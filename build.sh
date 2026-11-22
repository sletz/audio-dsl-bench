#!/bin/bash
# build.sh NAME : builds build/NAME/bench from dsp/NAME.dsp and dsp/NAME.onda
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ONDA="${ONDA:-/Users/letz/Developpements/onda/target/release/onda}"
FAUSTLIB=/Users/letz/Developpements/faustlibraries
OBJCOPY=/opt/local/libexec/llvm-21/bin/llvm-objcopy
N=$1
# generated prerequisites (not versioned)
[ -x "$HERE/tools/faust-cert4" ] || { echo "build.sh: tools/faust-cert4 missing, run tools/build-faust-cert4.sh" >&2; exit 1; }
[ -f "$HERE/sr48/platform.lib" ] || "$HERE/tools/make-sr48.sh"
D="$HERE/build/$N"
rm -rf "$D"; mkdir -p "$D"; cd "$D"
CXX="clang++ -std=c++17 -mcpu=native"

for v in strict fast; do
  fm=""; [ $v = fast ] && fm="--fast-math"
  "$ONDA" compile "$HERE/dsp/$N.onda" --emit obj --target-cpu host -b 256 --sr 48000 $fm \
    -o onda_$v.raw.o --meta-out onda_$v.json > /dev/null
  $OBJCOPY --redefine-sym _onda_processor_init=_onda_init_$v --redefine-sym _onda_process=_onda_process_$v \
    onda_$v.raw.o onda_$v.o
done

python3 - onda_strict.json > onda_meta.h <<'EOF'
import json, sys
d = json.load(open(sys.argv[1]))
rt = d["runtime"]; md = d["metadata"]
print("#pragma once\n#include <cstring>")
print(f"#define ONDA_STATE_SIZE {rt['state_size_bytes']}")
print(f"#define ONDA_STATE_ALIGN {max(16, rt['state_align_bytes'])}")
print(f"#define ONDA_PARAM_SIZE {max(1, rt['param_size_bytes'])}")
print("static inline void onda_write_defaults(unsigned char* p) {")
for prm in md["params"]:
    sc = prm["scalar"]; off = prm["byte_offset"]
    for i, r in enumerate(prm["default_reprs"] or []):
        o = off + i * prm["element_size_bytes"]
        ctype = {"f32": "float", "f64": "double", "i32": "int", "i64": "long long", "bool": "unsigned char"}[sc]
        val = {"true": "1", "false": "0"}.get(r, r)
        print(f"  {{ {ctype} v = ({ctype})({val}); memcpy(p + {o}, &v, sizeof v); }}")
print("}")
EOF

for mode in scal vec; do
  opt="-scal"; [ $mode = vec ] && opt="-vec -vs 32"
  for v in strict fast; do
    cls="Faust_${mode}_${v}"
    faust -I $FAUSTLIB "$HERE/dsp/$N.dsp" -lang cpp $opt -cn $cls -o $cls.h
    cat > $cls.cpp <<EOF
#include <faust/dsp/dsp.h>
#include <faust/gui/UI.h>
#include <faust/gui/meta.h>
#include <cmath>
#include <algorithm>
#include "$cls.h"
dsp* make_faust_${mode}_${v}() { return new $cls(); }
EOF
    fl="-O3 ${STRICT_EXTRA:-}"; [ $v = fast ] && fl="-O3 -ffast-math"
    $CXX $fl -c $cls.cpp -o $cls.o
  done
done

# Faust -vec with clang mapping libm calls to Accelerate's vector functions (vsinf, vtanhf, ...)
cls=Faust_vecacc_fast
faust -I $FAUSTLIB "$HERE/dsp/$N.dsp" -lang cpp -vec -vs 32 -cn $cls -o $cls.h
sed -e 's/Faust_vec_fast/Faust_vecacc_fast/g' -e 's/make_faust_vec_fast/make_faust_vecacc_fast/' Faust_vec_fast.cpp > $cls.cpp
$CXX -O3 -ffast-math -fveclib=Accelerate -c $cls.cpp -o $cls.o

# faust of the master-dev-certification-4 branch: -lang ocpp, and the options elected by fcautotool
FAUST_CERT4="$HERE/tools/faust-cert4.sh"
BEST=$(sed -n 's/^fcautotool: winner .* -- \(.*\)  \[.*$/\1/p' "$HERE/results/autotool/$N.txt" 2>/dev/null | head -1)
[ -n "$BEST" ] || BEST="-lang ocpp"
echo "$BEST" > best_options.txt
gen_cert4() {  # $1 = class, $2 = make function, $3 = c++ flags, rest = faust options ($FC = faust wrapper)
  local cls=$1 mk=$2 fl=$3; shift 3
  "${FC:-$FAUST_CERT4}" "$HERE/dsp/$N.dsp" "$@" -cn $cls -o $cls.h
  printf '#include <faust/dsp/dsp.h>\n#include <faust/gui/UI.h>\n#include <faust/gui/meta.h>\n#include <cmath>\n#include <algorithm>\n#include "%s.h"\ndsp* %s() { return new %s(); }\n' $cls $mk $cls > $cls.cpp
  $CXX $fl -c $cls.cpp -o $cls.o
}
gen_cert4 Faust_ocpp_strict make_faust_ocpp_strict "-O3 ${STRICT_EXTRA:-}" -lang ocpp
gen_cert4 Faust_ocpp_fast make_faust_ocpp_fast "-O3 -ffast-math" -lang ocpp
gen_cert4 Faust_best_fast make_faust_best_fast "-O3 -ffast-math -fbracket-depth=1024" $BEST
FC="$HERE/tools/faust-cert4-sr48.sh" gen_cert4 Faust_ocpp48_strict make_faust_ocpp48_strict "-O3 ${STRICT_EXTRA:-}" -lang ocpp
FC="$HERE/tools/faust-cert4-sr48.sh" gen_cert4 Faust_best48_fast make_faust_best48_fast "-O3 -ffast-math -fbracket-depth=1024" $BEST

$CXX -O2 -I. -DFAUSTLIB=\"$FAUSTLIB\" -DSR48DIR=\"$HERE/sr48\" -c "$HERE/harness.cpp" -o harness.o
$CXX -o bench harness.o Faust_*.o onda_strict.o onda_fast.o /usr/local/lib/libfaustwithllvm.a \
  -L/opt/local/lib -lz -lzstd -lxml2 -lncurses -framework Accelerate
echo "built $D/bench"
