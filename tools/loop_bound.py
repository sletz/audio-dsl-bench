#!/usr/bin/env python3
"""tools/loop_bound.py BENCH [PROGRAM...] : machine bound of the sample loop of every compiled engine.

For each program and each ahead-of-time engine (the objects left in build/NAME by build.sh: the JIT engines
have no object to read), the audio functions are disassembled and their sample loop is isolated. A loop is
analysed when it is the only innermost loop of these functions, holds no call and no branch other than its
back edge, and advances one sample per iteration; the other cases are recorded with the reason.

The bound of an analysed loop, in cycles per sample, is the larger of
  - the throughput bound: the instructions of each class (FP/SIMD, integer, load, store, divide/sqrt, all)
    divided by the number this core issues per cycle;
  - the recurrence bound: the maximum cycle mean of the loop-carried dependencies. Register moves count
    0 cycles (move elimination); a value stored and reloaded from the same fixed address (state kept in
    memory) counts the store-to-reload latency.
Latencies and issue rates are measured on the machine by tools/m_latency.c. llvm-mca is not used: its Apple
model (apple-m4 in LLVM 21: fadd 4 cycles, fmadd 8, three FP pipes) is slower than the M5 (fadd 2, fmadd 3,
four FP pipes), and gave bounds above the measured times.
Cache misses, branch mispredictions, the front end and the code outside the loop are left out: the bound is
a floor for this instruction sequence, which the measured time cannot go below.

Writes results/bound.json (read by report/gen.py)."""
import json, math, os, re, subprocess, sys, tempfile

B = sys.argv[1]
OBJDUMP = os.environ.get('OBJDUMP', '/opt/local/libexec/llvm-21/bin/llvm-objdump')

# object file in build/NAME -> engine key of results/results.json (strict objects are the -ffp-contract=off pass)
ENGINES = [('Faust_scal_strict.o', 'cpp-scal-ieee'), ('Faust_vec_strict.o', 'cpp-vec-ieee'), ('Faust_ocpp_strict.o', 'ocpp-ieee'),
           ('Faust_ocpp48_strict.o', 'ocpp48-ieee'), ('onda_strict.o', 'onda'), ('onda_block_strict.o', 'onda-block'),
           ('Cmaj_strict.o', 'cmaj-cpp-ieee'), ('Cxx_strict.o', 'cxx-ieee'),
           ('Faust_scal_fast.o', 'cpp-scal-fm'), ('Faust_vec_fast.o', 'cpp-vec-fm'), ('Faust_vecacc_fast.o', 'cpp-vec-acc'),
           ('Faust_ocpp_fast.o', 'ocpp-fm'), ('Faust_best_fast.o', 'best-fm'), ('Faust_best48_fast.o', 'best48-fm'),
           ('onda_fast.o', 'onda-fm'), ('onda_block_fast.o', 'onda-block-fm'),
           ('Cmaj_fast.o', 'cmaj-cpp-fm'), ('Cxx_fast.o', 'cxx-fm')]
FUNC = re.compile(r'compute|process|advance')
SKIP_FUNC = re.compile(r'init|Init|7computeEdi')    # dsp::compute(double, ...) is a forwarding stub


def machine():
    """Latencies (cycles) and issue rates (per cycle) measured by tools/m_latency.c."""
    exe = os.path.join(tempfile.gettempdir(), 'audio_dsl_bench_m_latency')
    subprocess.run(['clang', '-O2', os.path.join(B, 'tools', 'm_latency.c'), '-o', exe], check=True)
    runs = []
    for _ in range(3):
        out = subprocess.run([exe], capture_output=True, text=True, check=True).stdout
        runs.append({l.split()[0]: float(l.split()[1]) for l in out.split('\n') if l.strip()})
    m = {k: sorted(r[k] for r in runs)[1] for k in runs[0]}     # median of three runs
    m['ghz'] = max(r['ghz_max'] for r in runs)                  # the highest clock seen: the cycles of a measured
    del m['ghz_max']                                            # time are not underestimated
    for k in m:
        if k.startswith('lat_'):                    # whole cycles, pairs of instructions halved later
            m[k] = float(round(m[k]))
        elif k.startswith('ipc_'):                  # rounded up: the measuring loop has its own overhead
            m[k] = float(math.ceil(m[k] - 0.1))
    return m


# ---- disassembly -------------------------------------------------------------------------------------------

def functions(obj):
    """{symbol: [(label or None, mnemonic, operands)]} for the functions of obj that compute audio."""
    out = subprocess.run([OBJDUMP, '-d', '--no-show-raw-insn', '--symbolize-operands', obj],
                         capture_output=True, text=True, check=True).stdout
    funcs, cur, label = {}, None, None
    for l in out.split('\n'):
        m = re.match(r'^[0-9a-f]+ <(.+)>:$', l)
        if m:
            name = m.group(1)
            cur = funcs.setdefault(name, []) if FUNC.search(name) and not SKIP_FUNC.search(name) else None
            label = None
            continue
        if cur is None:
            continue
        m = re.match(r'^<(L\d+)>:$', l)
        if m:
            label = m.group(1)
            continue
        m = re.match(r'^\s+[0-9a-f]+:\s+(\S+)\s*(.*)$', l)
        if m:
            cur.append((label, m.group(1), m.group(2).split(';')[0].strip()))
            label = None
    return {k: v for k, v in funcs.items() if v}


BRANCHES = re.compile(r'^(b|b\..+|bc\..+|cbz|cbnz|tbz|tbnz|br|ret|bl|blr)$')


def find_loop(funcs):
    """The sample loop: the only innermost loop of the audio functions, or a reason why there is none."""
    loops = []
    for name, ins in funcs.items():
        pos = {lab: i for i, (lab, _, _) in enumerate(ins) if lab}
        for i, (_, mn, ops) in enumerate(ins):
            m = re.search(r'<(L\d+)>', ops)
            # a branch to itself is an unrelocated call or tail call (its target is in the relocations)
            if BRANCHES.match(mn) and mn not in ('bl', 'blr') and m and m.group(1) in pos and pos[m.group(1)] < i:
                loops.append((name, pos[m.group(1)], i))
    inner = [l for l in loops if not any(o is not l and o[0] == l[0] and l[1] <= o[1] and o[2] <= l[2] for o in loops)]
    if not inner:
        return None, 'no loop'
    if len(inner) > 1:
        return None, f'{len(inner)} innermost loops'
    name, a, b = inner[0]
    if any(o[0] == name and o[1] <= a and b <= o[2] and o != inner[0] for o in loops):
        return None, 'nested in another loop'        # stage by stage over the block, or in chunks: not one pass per sample
    body = funcs[name][a:b + 1]
    if any(mn in ('bl', 'blr') for _, mn, _ in body):
        return None, 'calls (libm or helper)'
    if any(BRANCHES.match(mn) for _, mn, _ in body[:-1]):
        return None, 'branches inside the loop'
    return body, None


# ---- instructions ------------------------------------------------------------------------------------------

def split_ops(s):
    out, depth, cur = [], 0, ''
    for c in s:
        if c in '[{':
            depth += 1
        elif c in ']}':
            depth -= 1
        if c == ',' and depth == 0:
            out.append(cur.strip()); cur = ''
        else:
            cur += c
    if cur.strip():
        out.append(cur.strip())
    return out


REG = re.compile(r'\b([wx](\d+)|sp|[bhsdqv](\d+))(\.\d*[bhsdq])?\b')


def regs(op):
    """Normalized registers named by an operand: x0..x30 and sp for integers, v0..v31 for FP/SIMD."""
    out = []
    for m in REG.finditer(op):
        t = m.group(1)
        out.append('sp' if t == 'sp' else ('x' + m.group(2) if t[0] in 'wx' else 'v' + m.group(3)))
    return out


def width(op):
    m = re.match(r'([wxbhsdq])\d+', op)
    return {'w': 4, 'x': 8, 'b': 1, 'h': 2, 's': 4, 'd': 8, 'q': 16}[m.group(1)] if m else 8


SETS_FLAGS = re.compile(r'^(cmp|cmn|tst|fcmp|fcmpe|ccmp|ccmn|fccmp|fccmpe|adds|subs|ands|bics|negs|adcs|sbcs)$')
READS_FLAGS = re.compile(r'^(csel|csinc|csinv|csneg|cset|csetm|cinc|cinv|cneg|fcsel|ccmp|ccmn|fccmp|fccmpe|adc|sbc|adcs|sbcs|b\..+)$')
NO_DEST = re.compile(r'^(cmp|cmn|tst|fcmp|fcmpe|ccmp|ccmn|fccmp|fccmpe|b|b\..+|cbz|cbnz|tbz|tbnz|prfm|prfum)$')
READS_DEST = re.compile(r'^(fmla|fmls|mla|mls|movk|bfi|bfxil|bfm|bsl|bit|bif|sli|sri|tbx|ins|saba|uaba|sdot|udot|fmlal2?|fmlsl2?)(\.\w+)?$')


def decode(mn, ops):
    """(defs, uses, address operand, data operands, is load, is store) of one instruction."""
    o = split_ops(ops)
    defs, uses = [], []
    mem = next((i for i, op in enumerate(o) if op.startswith('[')), None)
    if mem is not None:
        addr = o[mem]
        uses += regs(addr)
        base = regs(addr)[:1]
        if base and (addr.endswith('!') or mem + 1 < len(o)):   # pre-index or post-index writeback
            defs += base
        data = o[:mem]
        store = mn.startswith('st')
        for op in data:
            if store:
                uses += regs(op)
            else:
                defs += regs(op)
                if re.search(r'\}\[\d+\]$|\]\[\d+\]$', op):     # lane load reads its destination
                    uses += regs(op)
        return defs, uses, addr, data, not store, store
    if NO_DEST.match(mn):
        for op in o:
            uses += regs(op)
    elif o:
        defs += regs(o[0])
        if READS_DEST.match(mn) or re.search(r'\[\d+\]$', o[0]):  # accumulate, or write one lane
            uses += regs(o[0])
        for op in o[1:]:
            uses += regs(op)
    if SETS_FLAGS.match(mn):
        defs.append('nzcv')
    if READS_FLAGS.match(mn):
        uses.append('nzcv')
    return defs, uses, None, None, False, False


INT1 = re.compile(r'^(add|adds|sub|subs|and|ands|orr|orn|eor|eon|bic|bics|lsl|lsr|asr|ror|mov|movk|movz|movn|neg|negs|mvn|'
                  r'ubfx|sbfx|ubfiz|sbfiz|bfi|bfxil|extr|csel|csinc|csinv|csneg|cset|csetm|cinc|cinv|cneg|cmp|cmn|tst|'
                  r'ccmp|ccmn|sxtw|sxth|sxtb|uxtw|uxth|uxtb|adr|adrp|clz|rbit|rev)$')
FADD = re.compile(r'^(fadd|fsub|fabd|fabs|fneg|fmax|fmin|fmaxnm|fminnm|faddp|fmaxp|fminp|fmaxnmp|fminnmp)$')
FMUL = re.compile(r'^(fmul|fmulx|fnmul|fmadd|fmsub|fnmadd|fnmsub|fmla|fmls)$')
CVT = re.compile(r'^(scvtf|ucvtf|fcvtz[su]|fcvt[amnp][su])$')


def classify(mn, ops, M):
    """(latency in cycles, issue class) of one instruction, from the measured table; latency None when unknown."""
    base = mn.split('.')[0]
    o = split_ops(ops)
    vec = any(r[0] == 'v' for r in regs(ops))
    gpr = any(r[0] == 'x' for r in regs(ops))
    if base.startswith('ld'):
        return M['lat_ldr'], 'ld'
    if base.startswith('st'):
        return 0, 'st'
    if BRANCHES.match(mn):
        return 0, 'int'
    if base in ('mov', 'fmov') and len(o) == 2 and '[' not in ops and '#' not in ops:
        a, b = regs(o[0]), regs(o[1])
        if len(a) == 1 and len(b) == 1 and a[0][0] == b[0][0]:
            return 0, 'move'                              # removed by the renamer
        if len(a) == 1 and len(b) == 1:
            return M['lat_fmov_cross'] / 2, 'fp'          # between the integer and FP register files
    if not vec:
        if INT1.match(base):
            return 1, 'int'
        if base in ('mul', 'smull', 'umull', 'smulh', 'umulh', 'mneg'):
            return M['lat_mul'], 'int'
        if base in ('madd', 'msub', 'smaddl', 'umaddl', 'smsubl', 'umsubl'):
            return M['lat_madd'], 'int'
        return None, 'int'
    if FADD.match(base):
        return M['lat_fadd'], 'fp'
    if FMUL.match(base):
        return M['lat_fmadd'], 'fp'
    if base in ('fdiv',):
        return M['lat_fdiv'], 'div'
    if base in ('fsqrt',):
        return M['lat_fsqrt'], 'div'
    if base.startswith('frint'):
        return M['lat_frint'], 'fp'
    if base in ('fcmp', 'fcmpe', 'fccmp', 'fccmpe', 'fcsel'):
        return M['lat_fcmp_fcsel'] / 2, 'fp'
    if base == 'fcvt':
        return M['lat_fcvt_pair'] / 2, 'fp'
    if CVT.match(base):
        return (M['lat_fcvtzs_scvtf'] / 2 if gpr else M['lat_frint']), 'fp'
    if base in ('fmov', 'movi', 'mvni', 'dup', 'ext', 'zip1', 'zip2', 'uzp1', 'uzp2', 'trn1', 'trn2', 'rev64', 'ins', 'mov',
                'bsl', 'bit', 'bif', 'and', 'orr', 'eor', 'bic', 'not', 'mvn', 'neg', 'abs', 'add', 'sub', 'shl', 'ushr',
                'sshr', 'fcmeq', 'fcmge', 'fcmgt', 'fcmle', 'fcmlt', 'cmeq', 'cmge', 'cmgt', 'cmhi', 'cmhs', 'umov', 'smov'):
        return M['lat_vext'], 'fp'
    return None, 'fp'


# ---- bound -------------------------------------------------------------------------------------------------

def bound(body, M):
    info = [classify(mn, ops, M) for _, mn, ops in body]
    unknown = sorted(set(mn for (_, mn, _), (l, _) in zip(body, info) if l is None))
    lat = [3.0 if l is None else l for l, _ in info]   # an instruction outside the table: 3 cycles (and listed)
    cls = [c for _, c in info]
    n = {c: cls.count(c) for c in ('fp', 'int', 'ld', 'st', 'div')}
    thr = {'fp': (n['fp'] + n['div']) / M['ipc_fp'], 'int': n['int'] / M['ipc_int'], 'ld': n['ld'] / M['ipc_ld'],
           'st': n['st'] / M['ipc_st'], 'div': n['div'] / M['ipc_fdiv'], 'issue': len(body) / M['ipc_mix']}
    tclass = max(thr, key=thr.get)
    rec = recurrence(body, lat, M)
    t = thr[tclass]
    return {'insts': len(body), 'throughput': round(t, 2), 'throughput_class': tclass, 'recurrence': round(rec, 2),
            'bound_cycles': round(max(t, rec), 2), 'limit': 'recurrence' if rec > t else tclass, 'unknown': unknown}


def recurrence(body, lat, M):
    """Maximum cycle mean (cycles per iteration) of the loop-carried dependencies.

    A loaded value depends on the stored value when the address is fixed (state kept in memory), with the
    store-to-reload latency, and on nothing otherwise (delay lines, tables): the address computation is
    left out because this core predicts load addresses (a pointer chase on one address runs at 3 loads per
    cycle in tools/m_latency.c), so it is not a dependency the hardware must wait for."""
    dec = [decode(mn, ops) for _, mn, ops in body]
    defined = set(d for x in dec for d in x[0])
    insts = []                                     # ([(def, uses, latency)], slots read, slots written, store data)
    for (defs, uses, addr, data, ld, st), l in zip(dec, lat):
        if addr is None:
            insts.append(([(d, uses, l) for d in defs], [], [], []))
            continue
        rd, wr = [], []
        m = re.match(r'^\[(x\d+|sp)(?:,\s*#(-?(?:0x)?[0-9a-f]+))?\]$', addr.rstrip('!'))
        base = regs(m.group(1))[0] if m else None
        if m and base not in defined:              # a fixed address
            off = int(m.group(2), 0) if m.group(2) else 0
            for op in data:
                (rd if ld else wr).append((base, off))
                off += width(op)
        wb = regs(addr)[0] if regs(addr) and regs(addr)[0] in defs else None
        out = [(wb, [wb], 1)] if wb else []        # writeback: an address addition
        if ld:
            lane = [r for op in data if re.search(r'\]\[\d+\]$|\}\[\d+\]$', op) for r in regs(op)]
            out += [(d, lane, 0) for op in data for d in regs(op)]
            insts.append((out, rd, [], []))
        else:
            insts.append((out, [], wr, [r for op in data for r in regs(op)]))
    written = set(d for i in insts for d, _, _ in i[0]) | set(s for i in insts for s in i[2])
    seen, carried = set(), []
    for defs, rd, wr, sdata in insts:              # carried: read before being written, and written in the body
        for v in [u for _, us, _ in defs for u in us] + rd + sdata:
            if v in written and v not in seen and v not in carried:
                carried.append(v)
        seen.update(d for d, _, _ in defs); seen.update(wr)
    if not carried:
        return 0.0
    NEG = -1e18
    fwd = M['lat_store_reload']
    W = {}
    for c in carried:                              # longest path from c (iteration i) to each carried value (i + 1)
        cur = {c: 0.0}
        for defs, rd, wr, sdata in insts:
            t_mem = max([cur.get(s, NEG) for s in rd] + [NEG])
            for d, us, l in defs:
                t_in = max([cur.get(u, NEG) for u in us] + [NEG])
                t = max(t_in + l if t_in > NEG / 2 else NEG, t_mem + fwd if t_mem > NEG / 2 else NEG)
                cur[d] = t
            if wr:                                 # a store: the slot gets the time of the stored data
                t_st = max([cur.get(u, NEG) for u in sdata] + [NEG])
                for s in wr:
                    cur[s] = t_st
        W[c] = {c2: cur[c2] for c2 in carried if cur.get(c2, NEG) > NEG / 2}
    idx = {c: i for i, c in enumerate(carried)}
    n = len(carried)
    edges = [(idx[a], idx[b], w) for a, row in W.items() for b, w in row.items()]
    D = [[NEG] * n for _ in range(n + 1)]          # Karp's maximum mean cycle, every edge one iteration long
    D[0] = [0.0] * n
    for k in range(1, n + 1):
        Dk, Dp = D[k], D[k - 1]
        for a, b, w in edges:
            if Dp[a] > NEG / 2 and Dp[a] + w > Dk[b]:
                Dk[b] = Dp[a] + w
    best = 0.0
    for v in range(n):
        if D[n][v] > NEG / 2:
            best = max(best, min((D[n][v] - D[k][v]) / (n - k) for k in range(n) if D[k][v] > NEG / 2))
    return best


def samples_per_iteration(body):
    """1 when the loop counter, or the pointer compared at the back edge, advances by one sample."""
    for _, mn, ops in reversed(body[:-1]):
        if mn in ('subs', 'adds'):
            m = re.search(r'#(0x[0-9a-f]+|\d+)$', ops)
            return 1 if m and int(m.group(1), 0) == 1 else None
        if mn in ('cmp', 'cmn'):
            ctl = set(regs(ops))
            break
    else:
        _, bmn, bops = body[-1]
        if bmn not in ('cbz', 'cbnz'):
            return None
        ctl = set(regs(split_ops(bops)[0]))
    vector = any(re.search(r'\.(4s|2d|2s|8h|16b)', ops) for _, mn, ops in body if mn.startswith('f'))
    for _, mn, ops in body:
        o = split_ops(ops)
        if mn in ('add', 'sub') and len(o) == 3 and o[2].startswith('#') and regs(o[0]) and regs(o[0])[0] in ctl:
            step = int(o[2][1:], 0)
            return 1 if step == 1 or (step == 4 and not vector) else None
    return None


def analyse(prog, M):
    res = {}
    for obj, key in ENGINES:
        path = os.path.join(B, 'build', prog, obj)
        if not os.path.exists(path):
            continue
        body, why = find_loop(functions(path))
        if body is None:
            res[key] = {'status': why}
        elif samples_per_iteration(body) != 1:
            res[key] = {'status': 'not one sample per iteration'}
        else:
            res[key] = {'status': 'ok', **bound(body, M)}
    return res


if __name__ == '__main__':
    R = json.load(open(os.path.join(B, 'results', 'results.json')))
    progs = sys.argv[2:] or sorted(R)
    M = machine()
    ghz = M['ghz']
    out = {'_machine': M}
    for p in progs:
        out[p] = analyse(p, M)
        for key, v in out[p].items():
            if v['status'] == 'ok':
                v['bound_ns'] = round(v['bound_cycles'] / ghz, 3)
                if key in R.get(p, {}):
                    v['measured_ns'] = R[p][key]
                    v['measured_cycles'] = round(R[p][key] * ghz, 2)
        ok = {k: v for k, v in out[p].items() if v['status'] == 'ok'}
        print(p, ' '.join(f"{k}:{v['bound_cycles']}{v['limit'][0]}/{v.get('measured_cycles', '?')}" for k, v in ok.items()) or '-')
    print('machine', M)
    if len(sys.argv) == 2:
        json.dump(out, open(os.path.join(B, 'results', 'bound.json'), 'w'), indent=1)
