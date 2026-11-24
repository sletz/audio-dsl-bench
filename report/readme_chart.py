"""Draw benchmark charts from macOS results JSON or a Linux TSV."""
import argparse
import json
import math
from pathlib import Path
import sys
from xml.sax.saxutils import escape

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'linux'))
from summarize import FAUST, ONDA_EXPLICIT, ONDA_STANDARD, family_time, fastest_time, load_results

CATS = [('Oscillators and sources', ['sine', 'fm_pad', 'additive64', 'noise', 'bubble', 'virtual_analog']),
        ('Nonlinearities and dynamics', ['tanh_drive', 'distortion', 'compressor', 'noise_gate', 'exciter']),
        ('Filters', ['biquad8', 'peq10', 'svf_mod', 'fir32c', 'crybaby', 'moog_vcf', 'diode_ladder', 'filter_bank']),
        ('Modulation and pitch', ['phaser', 'flanger', 'pitch_shifter', 'autopan']),
        ('Spatialization and mixing', ['hoa_decoder_x', 'mixer16_x', 'hoa_octo']),
        ('Delays and reverbs', ['freeverb', 'fdn8', 'diffuser32', 'zita_rev', 'dattorro', 'greyhole', 'tapiir']),
        ('Physical models', ['clarinet', 'violin', 'frenchbell', 'karplus', 'flute'])]
NAMES = {'hoa_decoder_x': 'hoa_decoder', 'mixer16_x': 'mixer16'}
progs = [n for _, ns in CATS for n in ns]
FONT = "-apple-system, BlinkMacSystemFont, 'Segoe UI', Helvetica, Arial, sans-serif"
LANGS = [('onda', 'Onda'), ('cmaj', 'Cmajor'), ('ydsp', 'YDSP'), ('cxx', 'C++')]
EXPLICIT = ('onda_explicit', 'Onda with explicit loops')
L_THEMES = {  # categorical slots 1-4 of the dataviz reference palette, validated on adjacent pairs (fixed column order) in both modes
    'light': dict(bg='#ffffff', ink='#1f2328', ink2='#59636e', grid='#d1d9e0', one='#59636e', band='#f6f8fa',
                  onda='#2a78d6', cmaj='#eb6834', ydsp='#1baf7a', cxx='#eda100'),
    'dark': dict(bg='#0d1117', ink='#f0f6fc', ink2='#9198a1', grid='#3d444d', one='#9198a1', band='#161b22',
                 onda='#3987e5', cmaj='#d95926', ydsp='#199e70', cxx='#c98500'),
}
SUBTITLE = 'Speed relative to the fastest Faust build of the same contract (the ×1 line): up, faster than Faust; down, slower. Log scale.'


def geometric_means(data):
    means = {'s': {}, 'f': {}}
    for mode, values in means.items():
        for key, _ in LANGS:
            variants = (key, EXPLICIT[0]) if key == 'onda' else (key,)
            ratios = []
            for modes in data.values():
                measured = [modes[mode][variant] for variant in variants if variant in modes[mode]]
                if measured:
                    ratios.append(max(measured))
            if ratios:
                values[key] = math.exp(sum(map(math.log, ratios)) / len(ratios))
    return means


L_TICKS = [(0.125, '×⅛'), (0.25, '×¼'), (0.5, '×½'), (1, '×1'), (2, '×2'), (4, '×4'), (8, '×8')]
CAT_SHORT = {'Oscillators and sources': 'SOURCES', 'Nonlinearities and dynamics': 'DYNAMICS', 'Filters': 'FILTERS',
             'Modulation and pitch': 'MODULATION', 'Spatialization and mixing': 'SPATIAL', 'Delays and reverbs': 'REVERBS',
             'Physical models': 'PHYSICAL MODELS'}
L_W, L_PLOTL, L_PLOTR = 940, 64, 916      # plot area, left axis labels before it
L_PH, L_LABH, L_TOP = 190, 96, 128       # panel height, room for the rotated program names, first panel top
BAR, BGAP, CATGAP = 6, 1, 14              # column width, gap between the columns of a group, gap between categories
ROWS = [[0, 1, 2], [3, 4, 5, 6]]          # categories on each panel row of a contract; the mean ends the last row


def layout(cats, with_geo, slot, gw):
    """x positions of the groups of one panel row: categories, then the geometric mean if asked."""
    groups, x = [], L_PLOTL + 10
    for cat, ns in cats:
        x0 = x
        for n in ns:
            groups.append((n, x + (slot - gw) / 2)); x += slot
        groups.append(('|cat', (x0, x, cat))); x += CATGAP
    if with_geo:
        groups.append(('|geo', x + (slot - gw) / 2))
    return groups


def svg_languages(theme, ratios, means, platform, machine, strict_label):
    t = L_THEMES[theme]
    bounds = (0.125, 10)
    log_lo, log_hi = map(math.log, bounds)
    series = LANGS.copy()
    if any(EXPLICIT[0] in values for modes in ratios.values() for values in modes.values()):
        series.insert(1, EXPLICIT)
    gw = len(series) * (BAR + BGAP) - BGAP
    rows_cats = [[CATS[i] for i in r] for r in ROWS]
    last = len(rows_cats) - 1
    # one column width for every row, set by the fullest row (its programs, categories and the mean)
    slot = min((L_PLOTR - L_PLOTL - 20 - CATGAP * len(c) - gw * (i == last)) / (sum(len(ns) for _, ns in c) + (i == last))
               for i, c in enumerate(rows_cats))
    panels = [(key, title, i) for key, title in (('s', strict_label), ('f', 'fast-math')) for i in range(len(rows_cats))]
    H = L_TOP + len(panels) * (L_PH + L_LABH) + 40
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {L_W} {H}" width="{L_W}" height="{H}" font-family="{FONT}">',
           f'<rect width="{L_W}" height="{H}" fill="{t["bg"]}"/>',
           f'<text x="24" y="34" font-size="19" font-weight="600" fill="{t["ink"]}">Audio DSL benchmark on {platform}</text>',
           f'<text x="24" y="56" font-size="13" fill="{t["ink2"]}">{SUBTITLE}</text>']

    def paint(key):
        if key == EXPLICIT[0]:
            return f'fill="none" stroke="{t["onda"]}" stroke-width="1.2"'
        return f'fill="{t[key]}"'

    legend_widths = {'onda': 35, 'onda_explicit': 135, 'cmaj': 41, 'ydsp': 34, 'cxx': 25}
    lx = 24
    for k, label in series:
        out.append(f'<rect x="{lx}" y="70" width="12" height="12" rx="2" {paint(k)}/>')
        out.append(f'<text x="{lx + 18}" y="80" font-size="12.5" fill="{t["ink"]}">{label}</text>')
        lx += 18 + legend_widths[k] + 12
    out.append(f'<line x1="{lx}" x2="{lx + 16}" y1="76" y2="76" stroke="{t["one"]}" stroke-width="1.5"/>')
    out.append(f'<text x="{lx + 24}" y="80" font-size="12.5" fill="{t["ink"]}">Fastest Faust (×1)</text>')
    for p, (key, title, row) in enumerate(panels):
        top = L_TOP + p * (L_PH + L_LABH)
        groups = layout(rows_cats[row], row == last, slot, gw)
        Y = lambda v: top + L_PH - (math.log(min(max(v, bounds[0]), bounds[1])) - log_lo) / (log_hi - log_lo) * L_PH
        g = means[key]
        if row > 0:
            out.append(f'<text x="{L_PLOTL}" y="{top - 10}" font-size="12" fill="{t["ink2"]}">{title}, continued</text>')
        if row == 0:
            summary = ' · '.join(f'{lname} ×{g[k]:.2f}' for k, lname in LANGS if k in g)
            out.append(f'<text x="{L_PLOTL}" y="{top - 10}" font-size="14" font-weight="600" fill="{t["ink"]}">{title}'
                       f'<tspan dx="12" font-size="12" font-weight="400" fill="{t["ink2"]}">geometric means: {summary}</tspan></text>')
        for v, lab in L_TICKS:
            out.append(f'<line x1="{L_PLOTL}" x2="{L_PLOTR}" y1="{Y(v):.1f}" y2="{Y(v):.1f}" stroke="{t["grid"]}" stroke-width="1"/>')
            out.append(f'<text x="{L_PLOTL - 8}" y="{Y(v) + 4:.1f}" font-size="11.5" text-anchor="end" fill="{t["ink2"]}">{lab}</text>')
        for name, gx in groups:
            if name == '|cat':
                x0, x1, cat = gx
                out.append(f'<text x="{(x0 + x1) / 2:.1f}" y="{top + 12}" font-size="10" font-weight="600" letter-spacing="0.5" '
                           f'text-anchor="middle" fill="{t["ink2"]}">{CAT_SHORT[cat]}</text>')
                continue
            if name == '|geo':
                vals, label = g, 'mean'
                out.append(f'<rect x="{gx - 6:.1f}" y="{top}" width="{gw + 12}" height="{L_PH}" rx="4" fill="{t["band"]}"/>')
                bars = LANGS
                bar_start = gx + (len(series) - len(bars)) * (BAR + BGAP) / 2
            else:
                vals, label = ratios.get(name, {}).get(key, {}), NAMES.get(name, name)
                bars = series
                bar_start = gx
            for i, (k, lname) in enumerate(bars):
                if k not in vals:
                    continue
                bx = bar_start + i * (BAR + BGAP)
                y0, y1 = sorted((Y(1), Y(vals[k])))
                out.append(f'<rect x="{bx:.1f}" y="{y0:.1f}" width="{BAR}" height="{max(1.5, y1 - y0):.1f}" rx="2" {paint(k)}>'
                           f'<title>{label}, {lname}: ×{vals[k]:.2f}</title></rect>')
            weight = '600' if name == '|geo' else '400'
            cx, ly = gx + gw / 2, top + L_PH + 14
            out.append(f'<text x="{cx:.1f}" y="{ly}" font-size="11.5" font-weight="{weight}" text-anchor="end" '
                       f'transform="rotate(-40 {cx:.1f} {ly})" fill="{t["ink"]}">{label}</text>')
        out.append(f'<line x1="{L_PLOTL}" x2="{L_PLOTR}" y1="{Y(1):.1f}" y2="{Y(1):.1f}" stroke="{t["one"]}" stroke-width="1.5"/>')
        cats = [gx for name, gx in groups if name == '|cat']
        for x0, x1, cat in cats[:-1]:     # dashed separators between categories
            sx = x1 + CATGAP / 2
            out.append(f'<line x1="{sx:.1f}" x2="{sx:.1f}" y1="{top}" y2="{top + L_PH}" stroke="{t["grid"]}" stroke-width="1" stroke-dasharray="3 3"/>')
    note1 = f'{machine + " · " if machine else ""}f32 · 48 kHz · 256-frame blocks. Cmajor uses its faster backend (C++ or LLVM JIT).'
    note2 = 'Bars outside the axis range are clipped.'
    out.append(f'<text x="24" y="{H - 24}" font-size="11.5" fill="{t["ink2"]}">{escape(note1)}</text>')
    out.append(f'<text x="24" y="{H - 8}" font-size="11.5" fill="{t["ink2"]}">{escape(note2)}</text>')
    out.append('</svg>')
    return '\n'.join(out)


def macos_timings(path):
    # The macOS run has separate rows for the IEEE C++ pass. Use those rows
    # for the strict columns and give them the common engine names.
    timings = json.loads(path.read_text())
    strict_cpp = {'cpp-scal', 'cpp-vec', 'ocpp', 'ocpp48', 'cmaj-cpp', 'cxx'}
    return {
        name: {(engine[:-5] if engine.endswith('-ieee') else engine): time for engine, time in rows.items()
               if engine not in strict_cpp and math.isfinite(time) and time > 0}
        for name, rows in timings.items()
    }


def language_ratios(times_by_workload):
    groups = {
        'cmaj': 'Cmajor', 'ydsp': 'YDSP', 'cxx': 'Hand-written C++',
    }
    ratios = {}
    for name in progs:
        times = times_by_workload.get(name, {})
        ratios[name] = {}
        for mode, key in enumerate(('s', 'f')):
            ratios[name][key] = values = {}
            reference = fastest_time(times, FAUST[mode])
            if reference is None:
                continue
            for group, family in groups.items():
                candidate = family_time(times, family, mode)
                if candidate is not None:
                    values[group] = reference / candidate
            for group, engines in (('onda', ONDA_STANDARD), (EXPLICIT[0], ONDA_EXPLICIT)):
                if engines[mode] in times:
                    values[group] = reference / times[engines[mode]]
    return ratios


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root', type=Path, help='benchmark checkout')
    parser.add_argument('--linux-results', type=Path)
    parser.add_argument('--machine', help='CPU model to show on the chart')
    parser.add_argument('--flush-denormals', action='store_true', help='label Linux timings recorded with FTZ/DAZ')
    args = parser.parse_args()
    if args.flush_denormals and not args.linux_results:
        parser.error('--flush-denormals requires --linux-results')

    if args.linux_results:
        timings, _ = load_results(args.linux_results)
        platform, filename = 'x86-64 Linux', 'linux'
        strict_label = 'Strict FTZ/DAZ' if args.flush_denormals else 'Strict'
    else:
        timings = macos_timings(args.root / 'results' / 'results.json')
        platform, filename, strict_label = 'macOS', 'languages', 'Strict IEEE'

    ratios = language_ratios(timings)
    means = geometric_means(ratios)
    for theme in L_THEMES:
        svg = svg_languages(theme, ratios, means, platform, args.machine, strict_label)
        (args.root / 'report' / f'{filename}-{theme}.svg').write_text(svg)


if __name__ == '__main__':
    main()
