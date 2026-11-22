"""Draws report/chart-light.svg and report/chart-dark.svg for the README from
results/results.json (written by report/gen.py): onda's speedup over the best
Faust build, strict IEEE and fast-math, with Faust 2.95.1 alone as a marker."""
import json, math, os, sys

B = sys.argv[1]
R = json.load(open(f"{B}/results/results.json"))

CATS = [('Oscillators and sources', ['sine', 'fm_pad', 'additive64', 'noise']),
        ('Nonlinearities', ['tanh_drive']),
        ('Filters', ['biquad8', 'peq10', 'svf_mod', 'fir32c']),
        ('Matrices and mixing', ['hoa_decoder_x', 'mixer16_x']),
        ('Delays and reverbs', ['freeverb', 'fdn8', 'diffuser32', 'zita_rev']),
        ('Physical models', ['clarinet'])]
NAMES = {'hoa_decoder_x': 'hoa_decoder', 'mixer16_x': 'mixer16'}
IEEE = ['cpp-scal-ieee', 'cpp-vec-ieee', 'ocpp-ieee', 'ocpp48-ieee']
FAST = ['cpp-scal-fm', 'cpp-vec-fm', 'cpp-vec-acc', 'llvm-scal', 'llvm-vec', 'llvm48', 'ocpp-fm', 'best-fm', 'best48-fm']
IEEE_OLD = ['cpp-scal-ieee', 'cpp-vec-ieee']
FAST_OLD = ['cpp-scal-fm', 'cpp-vec-fm', 'cpp-vec-acc', 'llvm-scal', 'llvm-vec']
best = lambda r, ks: min(r[k] for k in ks if k in r)


def ratios(n):
    r = R[n]
    return {'s': best(r, IEEE) / r['onda'], 'f': best(r, FAST) / r['onda-fm'],
            'so': best(r, IEEE_OLD) / r['onda'], 'fo': best(r, FAST_OLD) / r['onda-fm']}


progs = [n for _, ns in CATS for n in ns]
data = {n: ratios(n) for n in progs}
g = lambda k: math.exp(sum(math.log(data[n][k]) for n in progs) / len(progs))
geo = {k: g(k) for k in ('s', 'f', 'so', 'fo')}

THEMES = {  # palette validated with the dataviz skill's validator (light and dark)
    'light': dict(bg='#ffffff', ink='#1f2328', ink2='#59636e', grid='#d1d9e0', one='#59636e',
                  onda='#008571', faust='#b9561a', ring='#1f2328'),
    'dark': dict(bg='#0d1117', ink='#f0f6fc', ink2='#9198a1', grid='#3d444d', one='#9198a1',
                 onda='#1fa58f', faust='#d67535', ring='#f0f6fc'),
}
FONT = "-apple-system, BlinkMacSystemFont, 'Segoe UI', Helvetica, Arial, sans-serif"
W, LEFT, PW, GAP, TOP = 940, 150, 360, 40, 112
ROW, CATH = 24, 26
LO, HI = math.log(0.3), math.log(4.5)
TICKS = [(0.5, '×0.5'), (1, '×1'), (2, '×2'), (4, '×4')]


def svg(theme):
    t = THEMES[theme]
    # rows: category headers, programs, geometric mean
    layout, y = [], TOP
    for cat, ns in CATS:
        layout.append(('cat', cat, y)); y += CATH
        for n in ns:
            layout.append(('prog', n, y)); y += ROW
    y += 10
    layout.append(('geo', 'geometric mean', y)); y += ROW
    H = y + 62
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}" font-family="{FONT}">',
           f'<rect width="{W}" height="{H}" fill="{t["bg"]}"/>',
           f'<text x="24" y="34" font-size="19" font-weight="600" fill="{t["ink"]}">onda speedup over the best Faust build, like for like</text>',
           f'<text x="24" y="56" font-size="13" fill="{t["ink2"]}">ns per sample, Apple M5, f32, 48 kHz. Right of ×1, onda is faster; left, Faust is faster. Log scale.</text>']
    # legend
    lx = 24
    for kind, label in (('onda', 'onda faster'), ('faust', 'Faust faster')):
        out.append(f'<rect x="{lx}" y="70" width="12" height="12" rx="2" fill="{t[kind]}"/>')
        out.append(f'<text x="{lx + 18}" y="80" font-size="12.5" fill="{t["ink"]}">{label}</text>')
        lx += 18 + len(label) * 7 + 22
    out.append(f'<circle cx="{lx + 6}" cy="76" r="4.5" fill="{t["bg"]}" stroke="{t["ring"]}" stroke-width="1.6"/>')
    out.append(f'<text x="{lx + 18}" y="80" font-size="12.5" fill="{t["ink"]}">against Faust 2.95.1 alone (C++ and LLVM, without the certification-4 branch)</text>')
    ybot = layout[-1][2] + ROW
    for p, (key, title) in enumerate((('s', 'Strict IEEE'), ('f', 'fast-math'))):
        x0 = LEFT + p * (PW + GAP)
        X = lambda v: x0 + (math.log(v) - LO) / (HI - LO) * PW
        out.append(f'<text x="{x0}" y="{TOP - 8}" font-size="14" font-weight="600" fill="{t["ink"]}">{title}</text>')
        for v, lab in TICKS:
            out.append(f'<line x1="{X(v):.1f}" x2="{X(v):.1f}" y1="{TOP + 4}" y2="{ybot}" stroke="{t["one"] if v == 1 else t["grid"]}" stroke-width="{1.5 if v == 1 else 1}"/>')
            out.append(f'<text x="{X(v):.1f}" y="{ybot + 18}" font-size="11.5" text-anchor="middle" fill="{t["ink2"]}">{lab}</text>')
        for kind, name, yy in layout:
            if kind == 'cat':
                if p == 0:
                    out.append(f'<text x="24" y="{yy + 17}" font-size="11" font-weight="600" letter-spacing="0.6" fill="{t["ink2"]}">{name.upper()}</text>')
                continue
            v, vo = (geo[key], geo[key + 'o']) if kind == 'geo' else (data[name][key], data[name][key + 'o'])
            if p == 0:
                weight = '600' if kind == 'geo' else '400'
                out.append(f'<text x="{LEFT - 14}" y="{yy + 16}" font-size="12.5" font-weight="{weight}" text-anchor="end" fill="{t["ink"]}">{NAMES.get(name, name)}</text>')
            xa, xb = sorted((X(1), X(v)))
            col = t['onda'] if v >= 1 else t['faust']
            h = 12 if kind == 'prog' else 14
            out.append(f'<rect x="{xa:.1f}" y="{yy + 6 + (12 - h) / 2:.1f}" width="{max(1.5, xb - xa):.1f}" height="{h}" rx="3" fill="{col}"><title>{NAMES.get(name, name)}: ×{v:.2f}</title></rect>')
            out.append(f'<circle cx="{X(vo):.1f}" cy="{yy + 12}" r="4.5" fill="{t["bg"]}" stroke="{t["ring"]}" stroke-width="1.6"/>')
            # value label just past the bar end, or past the ring when the ring sits near that end
            out_dir = 1 if v >= 1 else -1
            tx = X(v) + 6 * out_dir
            if abs(X(vo) - X(v)) < 30:
                tx = max(tx, X(vo) + 10) if out_dir > 0 else min(tx, X(vo) - 10)
            anchor = 'start' if v >= 1 else 'end'
            weight = '600' if kind == 'geo' else '400'
            out.append(f'<text x="{tx:.1f}" y="{yy + 16}" font-size="11.5" font-weight="{weight}" text-anchor="{anchor}" fill="{t["ink2"]}">×{v:.2f}</text>')
    out.append(f'<text x="24" y="{H - 30}" font-size="11.5" fill="{t["ink2"]}">Best Faust build: fastest of 13 configurations (C++ scalar and vec, Accelerate, LLVM JIT, and from master-dev-certification-4:</text>')
    out.append(f'<text x="24" y="{H - 14}" font-size="11.5" fill="{t["ink2"]}">ocpp and the options elected by fcautotool, each also with SR fixed at compile time). Means over {len(progs)} programs.</text>')
    out.append('</svg>')
    return '\n'.join(out)


for theme in THEMES:
    open(f"{B}/report/chart-{theme}.svg", 'w').write(svg(theme))
print('geomean', {k: round(v, 3) for k, v in geo.items()})
