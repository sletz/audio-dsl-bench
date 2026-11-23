"""Builds report/audio-dsl-bench.html (onda, Faust, Cmajor, YUP YDSP and hand-written C++) from results/results.tsv (default clang contract)
and results/results_nocontract.tsv (C++ strict variants at -ffp-contract=off)."""
import collections, html, json, math, os, re, sys

B = sys.argv[1]


def load(path):
    R = collections.defaultdict(dict)
    for l in open(path):
        f = l.rstrip('\n').split('\t')
        if len(f) > 2:
            R[f[0]][f[1]] = float(f[2])
    return R


R = load(f"{B}/results/results.tsv")
for n, r in load(f"{B}/results/results_nocontract.tsv").items():
    for k in ('cpp-scal', 'cpp-vec', 'ocpp', 'ocpp48', 'cmaj-cpp', 'cxx'):
        if k in r:
            R[n][k + '-ieee'] = r[k]
json.dump(R, open(f"{B}/results/results.json", 'w'), indent=1)

# programs by DSP category; variants (other ways of writing the same program in onda) stay out of the means
CATS = [
    ('Oscillators and sources', [('sine', 'phasor + sin'), ('fm_pad', '2 FM voices + tanh'), ('additive64', '64 sine partials'),
                                 ('noise', 'LCG + one-pole lowpass'), ('bubble', 'gameaudio/bubble: one drop + freeverb'),
                                 ('virtual_analog', 'generator/virtualAnalog: VA oscillators')]),
    ('Nonlinearities and dynamics', [('tanh_drive', 'tanh saturation'), ('distortion', 'dynamic/distortion: ef.cubicnl_nodc'),
                                     ('compressor', 'dynamic/compressor: co.compressor_stereo'),
                                     ('noise_gate', 'dynamic/noiseGate: stereo gate'),
                                     ('exciter', 'psychoacoustic/harmonicExciter')]),
    ('Filters', [('biquad8', '8 RBJ biquads in series'), ('peq10', '10-band parametric EQ'),
                 ('svf_mod', 'TPT SVF, cutoff modulated per sample'), ('fir32c', '32-tap FIR (const def)'),
                 ('fir32', 'variant: taps computed at init'), ('crybaby', 'filtering/cryBaby: ve.crybaby wah'),
                 ('moog_vcf', 'filtering/moogVCF: ve.moog_vcf'),
                 ('diode_ladder', 'filtering/diodeLadder: ve.diodeLadder on a saw'),
                 ('filter_bank', 'filtering/filterBank: octave filter bank')]),
    ('Modulation and pitch', [('phaser', 'phasing/phaser: pf.phaser2_stereo'), ('flanger', 'phasing/flanger: pf.flanger_stereo'),
                              ('pitch_shifter', 'pitchShifting/pitchShifter: ef.transpose'),
                              ('autopan', 'misc/autopan: LFO stereo panner')]),
    ('Spatialization and mixing', [('hoa_decoder_x', '2D HOA decoder, order 3, 7→8'),
                            ('hoa_decoder', 'variant: dynamic ins/outs views'),
                            ('mixer16_x', '16-channel mixer, dB gain + pan'),
                            ('mixer16', 'variant: all in sample, ins view'),
                            ('hoa_octo', 'ambisonics/fourSourcesToOcto: 4 sources, order 3, 8 speakers')]),
    ('Delays and reverbs', [('freeverb', '8 combs + 4 allpasses'), ('fdn8', '8-line FDN, dense Hadamard'),
                                  ('diffuser32', '32 allpasses in series'), ('zita_rev', 're.zita_rev1_stereo'),
                                  ('dattorro', 'reverb/dattorro: re.dattorro_rev'),
                                  ('greyhole', 'reverb/greyhole: re.greyhole'),
                                  ('tapiir', 'delayEcho/tapiir: 6-tap feedback delay matrix')]),
    ('Physical models', [('clarinet', 'pm.clarinet_ui_MIDI, gate held'), ('violin', 'pm.violin_ui_MIDI, gate held'),
                         ('frenchbell', 'pm.frenchBell_ui, 50 modes, struck once'),
                         ('frenchbell_x', 'variant: mode sum in 4 partial sums'),
                         ('karplus', 'physicalModeling/karplus: pm.ks_ui_MIDI, plucked once'),
                         ('flute', 'physicalModeling/flute: pm.flute_ui, gate held')]),
]
VARIANTS = {'fir32', 'hoa_decoder', 'mixer16', 'frenchbell_x'}
# the "all engines" table: columns grouped by language
GROUPS = [('Faust 2.95.1', 'g-faust', [('cpp-scal-ieee', 'C++ IEEE'), ('cpp-scal-fm', 'C++ fast-math'), ('cpp-vec-fm', 'C++ -vec fast-math'),
                                      ('cpp-vec-acc', 'C++ -vec + Accelerate'), ('llvm-scal', 'LLVM'), ('llvm48', 'LLVM, fixed SR')]),
          ('Faust cert-7', 'g-faust', [('ocpp-ieee', 'ocpp IEEE'), ('ocpp48-ieee', 'ocpp IEEE, fixed SR'), ('ocpp-fm', 'ocpp fast-math'),
                                      ('best-fm', 'fcautotool pick'), ('best48-fm', 'pick, fixed SR')]),
          ('Onda', 'g-onda', [('onda', 'strict'), ('onda-fm', '--fast-math')]),
          ('Cmajor', 'g-cmaj', [('cmaj-cpp-ieee', 'C++ IEEE'), ('cmaj-cpp-fm', 'C++ fast-math'), ('cmaj-jit', 'JIT -O3'), ('cmaj-jit-fm', 'JIT -O4')]),
          ('YDSP', 'g-ydsp', [('ydsp', 'strict'), ('ydsp-fm', 'fastMath')]),
          ('C++', 'g-cxx', [('cxx-ieee', 'IEEE'), ('cxx-fm', 'fast-math')])]
cols = [c for _, _, cs in GROUPS for c in cs]
CMAJ_S, CMAJ_F = ['cmaj-cpp-ieee', 'cmaj-jit'], ['cmaj-cpp-fm', 'cmaj-jit-fm']
YDSP_S, YDSP_F = ['ydsp'], ['ydsp-fm']
NEW = {'crybaby', 'moog_vcf', 'compressor', 'distortion', 'phaser', 'flanger', 'pitch_shifter', 'dattorro', 'karplus', 'bubble',
       'virtual_analog', 'diode_ladder', 'filter_bank', 'noise_gate', 'exciter', 'autopan', 'greyhole', 'tapiir', 'hoa_octo', 'flute'}
IEEE = ['cpp-scal-ieee', 'cpp-vec-ieee', 'ocpp-ieee', 'ocpp48-ieee']
FAST = ['cpp-scal-fm', 'cpp-vec-fm', 'cpp-vec-acc', 'llvm-scal', 'llvm-vec', 'llvm48', 'ocpp-fm', 'best-fm', 'best48-fm']
IEEE_OLD = ['cpp-scal-ieee', 'cpp-vec-ieee']                                   # Faust 2.95.1
FAST_OLD = ['cpp-scal-fm', 'cpp-vec-fm', 'cpp-vec-acc', 'llvm-scal', 'llvm-vec']


def best(r, keys):
    return min(r[k] for k in keys if k in r)


def num(v):
    return f'{v:.2f}' if v < 100 else f'{v:.1f}'


def rat(x):
    c = 'win' if x >= 1.05 else ('loss' if x <= 0.95 else 'tie')
    return f'<td class="ratio {c}">×{x:.2f}</td>'


g = lambda a: math.exp(sum(map(math.log, a)) / len(a))

# all engines, grouped by language
rows = []
ncols = len(cols) + 1
for cat, progs in CATS:
    rows.append(f'<tr class="cat"><th colspan="{ncols}" scope="rowgroup">{html.escape(cat)}</th></tr>')
    for n, lab in progs:
        r = R[n]
        lo = min(r[c] for c, _ in cols if c in r)
        cells = ''.join(('<td class="best">' if r.get(c) == lo else '<td>') + (num(r[c]) if c in r else '—') + '</td>' for c, _ in cols)
        cls = ' class="variant"' if n in VARIANTS else ''
        rows.append(f'<tr{cls}><th scope="row"><code>{n}</code><span class="lab">{html.escape(lab)}</span></th>{cells}</tr>')
head = ('<tr class="groups"><th></th>' + ''.join(f'<th colspan="{len(cs)}" class="{cl}">{html.escape(gname)}</th>' for gname, cl, cs in GROUPS)
        + '</tr>\n<tr><th scope="col" style="text-align:left">Program</th>'
        + ''.join(f'<th scope="col" class="{cl}">{html.escape(t)}</th>' for _, cl, cs in GROUPS for _, t in cs) + '</tr>')

# best engine of each language per contract, the ratios to the best Faust build, the fastest language per program
LANG_S = {'faust': IEEE, 'onda': ['onda'], 'cmaj': CMAJ_S, 'ydsp': YDSP_S, 'cxx': ['cxx-ieee']}
LANG_F = {'faust': FAST, 'onda': ['onda-fm'], 'cmaj': CMAJ_F, 'ydsp': YDSP_F, 'cxx': ['cxx-fm']}
drows, st = [], []
wins_s, wins_f = {k: 0 for k in LANG_S}, {k: 0 for k in LANG_S}
def cell(v, lo):
    return ('<td class="best">' if v == lo else '<td>') + num(v) + '</td>'
for cat, progs in CATS:
    drows.append(f'<tr class="cat"><th colspan="15" scope="rowgroup">{html.escape(cat)}</th></tr>')
    for n, lab in progs:
        if n in VARIANTS:
            continue
        r = R[n]
        bs = {k: best(r, ks) for k, ks in LANG_S.items()}
        bf = {k: best(r, ks) for k, ks in LANG_F.items()}
        los, lof = min(bs.values()), min(bf.values())
        wins_s[min(bs, key=bs.get)] += 1
        wins_f[min(bf, key=bf.get)] += 1
        drows.append(f'<tr><th scope="row"><code>{n}</code><span class="lab">{html.escape(lab)}</span></th>'
                     + ''.join(cell(bs[k], los) for k in LANG_S) + ''.join(cell(bf[k], lof) for k in LANG_F)
                     + ''.join(rat(bs['faust'] / bs[k]) for k in ('onda', 'cmaj', 'ydsp', 'cxx')) + '</tr>')
        st.append({'n': n, 'os': bs['faust'] / bs['onda'], 'of': bf['faust'] / bf['onda'], 'cs': bs['faust'] / bs['cmaj'],
                   'cf': bf['faust'] / bf['cmaj'], 'ys': bs['faust'] / bs['ydsp'], 'yf': bf['faust'] / bf['ydsp'],
                   'xs': bs['faust'] / bs['cxx'], 'xf': bf['faust'] / bf['cxx'],
                   'xcpp': r['cpp-scal-ieee'] / r['cxx-ieee'], 'xcmaj': r['cmaj-cpp-ieee'] / r['cxx-ieee'],
                   'oso': best(r, IEEE_OLD) / r['onda'], 'jit': r['cmaj-jit'] / r['cmaj-cpp-ieee']})
G = lambda k, sel=lambda x: True: g([x[k] for x in st if sel(x)])
ocpp_gain = g([R[n]['cpp-scal-ieee'] / R[n]['ocpp-ieee'] for _, p in CATS for n, _ in p if n not in VARIANTS and 'ocpp-ieee' in R[n]])
best_gain = g([R[n]['cpp-scal-fm'] / R[n]['best-fm'] for _, p in CATS for n, _ in p if n not in VARIANTS and 'best-fm' in R[n]])

# options elected by fcautotool
erows = []
for _, progs in CATS:
    for n, _ in progs:
        f = f"{B}/results/autotool/{n}.txt"
        if n.endswith('_x') or not os.path.exists(f):
            continue
        t = open(f).read()
        m = re.search(r'winner (\S+) \(([0-9.]+) ns/frame.*?-- (.*?)  \[', t)
        cpp = re.search(r'^\s+cpp\s+([0-9.]+) ns', t, re.M)
        if m and cpp:
            erows.append(f'<tr><th scope="row"><code>{n}</code></th><td style="text-align:left"><code>{html.escape(m.group(3))}</code></td>'
                         f'<td>{float(m.group(2)):.2f}</td><td>{float(cpp.group(1)):.2f}</td>'
                         f'<td class="ratio">×{float(cpp.group(1)) / float(m.group(2)):.2f}</td></tr>')

src = []
for cat, progs in CATS:
    for n, lab in progs:
        figs = ''
        for lang, ext in (('Faust', 'dsp'), ('Onda', 'onda'), ('Cmajor', 'cmajor'), ('YDSP', 'ydsp'), ('C++', 'cpp')):
            if os.path.exists(f"{B}/dsp/{n}.{ext}"):
                code = html.escape(open(f"{B}/dsp/{n}.{ext}").read())
                figs += f'<figure><figcaption>{lang} · {n}.{ext}</figcaption><pre><code>{code}</code></pre></figure>\n'
        op = ' open' if n == 'zita_rev' else ''
        src.append(f'''<details{op}><summary><code>{n}</code> — {html.escape(lab)}</summary><div class="pair">
{figs}</div></details>''')

fr = lambda x: f'{x:.2f}'
sub = {'ROWS': '\n'.join(rows), 'HEAD': head, 'SOURCES': '\n'.join(src), 'EROWS': '\n'.join(erows), 'DROWS': '\n'.join(drows),
       'NPROG': str(len(st)), 'GS': fr(G('os')), 'GF': fr(G('of')), 'GSO': fr(G('oso')), 'CS': fr(G('cs')), 'CF': fr(G('cf')),
       'YS': fr(G('ys')), 'YF': fr(G('yf')), 'XS': fr(G('xs')), 'XF': fr(G('xf')), 'ONEW': fr(G('os', lambda x: x['n'] in NEW)), 'JITCPP': fr(G('jit')),
       'OCPPGAIN': fr(ocpp_gain), 'BESTGAIN': fr(best_gain),
       'CXXCPP': fr(G('xcpp')), 'CXXCMAJ': fr(G('xcmaj')),
       'FWINS': str(wins_s['faust']), 'OWINS': str(wins_s['onda']), 'CWINS': str(wins_s['cmaj']), 'YWINS': str(wins_s['ydsp']), 'XWINS': str(wins_s['cxx']),
       'FFWINS': str(wins_f['faust']), 'OFWINS': str(wins_f['onda']), 'CFWINS': str(wins_f['cmaj']), 'YFWINS': str(wins_f['ydsp']), 'XFWINS': str(wins_f['cxx'])}
out = open(f"{B}/report/template.html").read()
for k, v in sub.items():
    out = out.replace(f'%%{k}%%', v)
assert not re.findall(r'%%[A-Z]+%%', out), re.findall(r'%%[A-Z]+%%', out)
open(f"{B}/report/audio-dsl-bench.html", 'w').write(out)
print(f"{sub['NPROG']} programs; best Faust / onda {sub['GS']} {sub['GF']}, Cmajor {sub['CS']} {sub['CF']}, YDSP {sub['YS']} {sub['YF']}, C++ {sub['XS']} {sub['XF']}; "
      f"fastest strict F/O/C/Y/X {sub['FWINS']}/{sub['OWINS']}/{sub['CWINS']}/{sub['YWINS']}/{sub['XWINS']}, "
      f"fast {sub['FFWINS']}/{sub['OFWINS']}/{sub['CFWINS']}/{sub['YFWINS']}/{sub['XFWINS']}")
