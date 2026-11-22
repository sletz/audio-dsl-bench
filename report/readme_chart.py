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
        ('Physical models', ['clarinet', 'violin', 'frenchbell'])]
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


# ---------------------------------------------------------------------------------------------------
# Four languages: report/languages-light.svg and report/languages-dark.svg. For each program, three
# columns give the speed of onda, of the best Cmajor engine and of YDSP relative to the best Faust build
# (the x1 baseline): up, faster than Faust; down, slower. Strict IEEE above, fast-math below.
CMAJ_S, CMAJ_F = ['cmaj-cpp-ieee', 'cmaj-jit'], ['cmaj-cpp-fm', 'cmaj-jit-fm']
LANGS = [('onda', 'onda'), ('cmaj', 'Cmajor'), ('ydsp', 'YDSP')]   # fixed order, left to right in each group
L_THEMES = {  # categorical slots 1-3 of the dataviz reference palette, validated all-pairs in both modes
    'light': dict(bg='#ffffff', ink='#1f2328', ink2='#59636e', grid='#d1d9e0', one='#59636e', band='#f6f8fa',
                  onda='#2a78d6', cmaj='#eb6834', ydsp='#1baf7a'),
    'dark': dict(bg='#0d1117', ink='#f0f6fc', ink2='#9198a1', grid='#3d444d', one='#9198a1', band='#161b22',
                 onda='#3987e5', cmaj='#d95926', ydsp='#199e70'),
}


def lang_ratios(n):
    r = R[n]
    return {'s': {'onda': best(r, IEEE) / r['onda'], 'cmaj': best(r, IEEE) / best(r, CMAJ_S), 'ydsp': best(r, IEEE) / r['ydsp']},
            'f': {'onda': best(r, FAST) / r['onda-fm'], 'cmaj': best(r, FAST) / best(r, CMAJ_F), 'ydsp': best(r, FAST) / r['ydsp-fm']}}


LD = {n: lang_ratios(n) for n in progs}
LGEO = {c: {k: math.exp(sum(math.log(LD[n][c][k]) for n in progs) / len(progs)) for k, _ in LANGS} for c in ('s', 'f')}
L_LO, L_HI = math.log(0.1), math.log(4.5)
L_TICKS = [(0.125, '×⅛'), (0.25, '×¼'), (0.5, '×½'), (1, '×1'), (2, '×2'), (4, '×4')]
CAT_SHORT = {'Oscillators and sources': 'SOURCES', 'Nonlinearities': 'NONLIN.', 'Filters': 'FILTERS',
             'Matrices and mixing': 'MIXING', 'Delays and reverbs': 'DELAYS AND REVERBS', 'Physical models': 'PHYSICAL MODELS'}
L_W, L_PLOTL, L_PLOTR = 940, 64, 916      # plot area, left axis labels before it
L_PH, L_LABH, L_TOP = 250, 96, 128       # panel height, room for the rotated program names, first panel top
BAR, BGAP, CATGAP = 12, 2, 14            # column width, gap between the columns of a group, gap between categories


def svg_languages(theme):
    t = L_THEMES[theme]
    # x layout: groups of 3 columns per program, a gap between categories, the geometric mean last
    groups, x = [], L_PLOTL + 10
    gw = 3 * BAR + 2 * BGAP
    slot = (L_PLOTR - L_PLOTL - 20 - CATGAP * len(CATS) - gw) / len(progs)
    for ci, (cat, ns) in enumerate(CATS):
        x0 = x
        for n in ns:
            groups.append((n, x + (slot - gw) / 2)); x += slot
        groups.append(('|cat', (x0, x, cat))); x += CATGAP
    groups.append(('|geo', x + (slot - gw) / 2))
    H = L_TOP + 2 * (L_PH + L_LABH) + 40
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {L_W} {H}" width="{L_W}" height="{H}" font-family="{FONT}">',
           f'<rect width="{L_W}" height="{H}" fill="{t["bg"]}"/>',
           f'<text x="24" y="34" font-size="19" font-weight="600" fill="{t["ink"]}">onda, Cmajor and YDSP against the best Faust build</text>',
           f'<text x="24" y="56" font-size="13" fill="{t["ink2"]}">Speed relative to the fastest Faust build of the same contract (the ×1 line): up, faster than Faust; down, slower. Log scale.</text>']
    lx = 24
    for k, label in LANGS:
        out.append(f'<rect x="{lx}" y="70" width="12" height="12" rx="2" fill="{t[k]}"/>')
        out.append(f'<text x="{lx + 18}" y="80" font-size="12.5" fill="{t["ink"]}">{label}</text>')
        lx += 18 + len(label) * 7.5 + 22
    out.append(f'<line x1="{lx}" x2="{lx + 16}" y1="76" y2="76" stroke="{t["one"]}" stroke-width="1.5"/>')
    out.append(f'<text x="{lx + 24}" y="80" font-size="12.5" fill="{t["ink"]}">Faust: best of 13 builds (×1)</text>')
    for p, (key, title) in enumerate((('s', 'Strict IEEE'), ('f', 'fast-math'))):
        top = L_TOP + p * (L_PH + L_LABH)
        Y = lambda v: top + L_PH - (math.log(min(max(v, 0.1), 4.5)) - L_LO) / (L_HI - L_LO) * L_PH
        g = LGEO[key]
        means = ' · '.join(f'{lname} ×{g[k]:.2f}' for k, lname in LANGS)
        out.append(f'<text x="{L_PLOTL}" y="{top - 10}" font-size="14" font-weight="600" fill="{t["ink"]}">{title}'
                   f'<tspan dx="12" font-size="12" font-weight="400" fill="{t["ink2"]}">geometric means: {means}</tspan></text>')
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
            else:
                vals, label = LD[name][key], NAMES.get(name, name)
            for i, (k, lname) in enumerate(LANGS):
                bx = gx + i * (BAR + BGAP)
                y0, y1 = sorted((Y(1), Y(vals[k])))
                out.append(f'<rect x="{bx:.1f}" y="{y0:.1f}" width="{BAR}" height="{max(1.5, y1 - y0):.1f}" rx="2" fill="{t[k]}">'
                           f'<title>{label}, {lname}: ×{vals[k]:.2f}</title></rect>')
            weight = '600' if name == '|geo' else '400'
            cx, ly = gx + gw / 2, top + L_PH + 14
            out.append(f'<text x="{cx:.1f}" y="{ly}" font-size="11.5" font-weight="{weight}" text-anchor="end" '
                       f'transform="rotate(-40 {cx:.1f} {ly})" fill="{t["ink"]}">{label}</text>')
        out.append(f'<line x1="{L_PLOTL}" x2="{L_PLOTR}" y1="{Y(1):.1f}" y2="{Y(1):.1f}" stroke="{t["one"]}" stroke-width="1.5"/>')
        # category separators between the groups
        for name, gx in groups:
            if name == '|cat' and gx[1] < groups[-1][1] - 1:
                sx = gx[1] + CATGAP / 2
                out.append(f'<line x1="{sx:.1f}" x2="{sx:.1f}" y1="{top}" y2="{top + L_PH}" stroke="{t["grid"]}" stroke-width="1" stroke-dasharray="3 3"/>')
    out.append(f'<text x="24" y="{H - 24}" font-size="11.5" fill="{t["ink2"]}">Cmajor: the faster of its generated C++ (clang) and its LLVM JIT. YDSP: YUP\'s JIT, strict or fastMath. ns per sample, Apple M5,</text>')
    out.append(f'<text x="24" y="{H - 8}" font-size="11.5" fill="{t["ink2"]}">f32, 48 kHz, 256-frame blocks; means over {len(progs)} programs. Programs grouped by category; columns past ×⅛ are cut at the axis.</text>')
    out.append('</svg>')
    return '\n'.join(out)


for theme in L_THEMES:
    open(f"{B}/report/languages-{theme}.svg", 'w').write(svg_languages(theme))
print('languages', {c: {k: round(v, 2) for k, v in g.items()} for c, g in LGEO.items()})
