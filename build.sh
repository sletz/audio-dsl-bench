#!/bin/bash
# build.sh NAME : builds build/NAME/bench from dsp/NAME.dsp and dsp/NAME.onda, plus the optional
# Cmajor (dsp/NAME.cmajor) and YDSP (dsp/NAME.ydsp) ports
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ONDA="${ONDA:-/Users/letz/Developpements/onda/target/release/onda}"
FAUSTLIB=/Users/letz/Developpements/faustlibraries
OBJCOPY=/opt/local/libexec/llvm-21/bin/llvm-objcopy
CMAJ="${CMAJ:-cmaj}"
YUP_ROOT="${YUP_ROOT:-/Users/letz/Developpements/yup}"
N=$1
# generated prerequisites (not versioned)
[ -x "$HERE/tools/${CERT_BIN:-faust-cert7}" ] || { echo "build.sh: tools/${CERT_BIN:-faust-cert7} missing, run tools/build-faust-cert.sh" >&2; exit 1; }
[ -f "$HERE/sr48/platform.lib" ] || "$HERE/tools/make-sr48.sh"
# program-independent engines, built once: the YDSP JIT library (YUP, CMake) and the Cmajor JIT wrapper
# (SKIP_COMMON=1 when several builds run at once and these are already built)
if [ -z "${SKIP_COMMON:-}" ]; then
if [ ! -f "$HERE/build/_ydsp/build.ninja" ]; then
  cmake -S "$HERE/tools/ydsp" -B "$HERE/build/_ydsp" -G Ninja -DCMAKE_BUILD_TYPE=Release -DYUP_ROOT="$YUP_ROOT" > /dev/null
fi
cmake --build "$HERE/build/_ydsp" > /dev/null
mkdir -p "$HERE/build/_common"
if [ ! -f "$HERE/build/_common/cmaj_jit.o" ] || [ "$HERE/tools/cmaj_jit.cpp" -nt "$HERE/build/_common/cmaj_jit.o" ]; then
  clang++ -std=c++17 -O2 -mcpu=native -I/usr/local/include -c "$HERE/tools/cmaj_jit.cpp" -o "$HERE/build/_common/cmaj_jit.o"
fi
fi
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

# faust of the master-dev-certification-7 branch: -lang ocpp, and the options elected by fcautotool
FAUST_CERT="$HERE/tools/faust-cert.sh"
BEST=""
AUTOTOOL_DIR="${AUTOTOOL_DIR:-$HERE/results/autotool}"
[ -f "$AUTOTOOL_DIR/$N.txt" ] && BEST=$(sed -n 's/^fcautotool: winner .* -- \(.*\)  \[.*$/\1/p' "$AUTOTOOL_DIR/$N.txt" | head -1)
[ -n "$BEST" ] || BEST="-lang ocpp"
echo "$BEST" > best_options.txt
gen_cert() {  # $1 = class, $2 = make function, $3 = c++ flags, rest = faust options ($FC = faust wrapper)
  local cls=$1 mk=$2 fl=$3; shift 3
  if "${FC:-$FAUST_CERT}" "$HERE/dsp/$N.dsp" "$@" -cn $cls -o $cls.h 2> $cls.err; then
    printf '#include <faust/dsp/dsp.h>\n#include <faust/gui/UI.h>\n#include <faust/gui/meta.h>\n#include <cmath>\n#include <algorithm>\n#include "%s.h"\ndsp* %s() { return new %s(); }\n' $cls $mk $cls > $cls.cpp
  else  # this faust rejects the program: the engine is left out of the run
    echo "build.sh: $cls: $(head -1 $cls.err | cut -c1-120)" >&2
    printf '#include <faust/dsp/dsp.h>\ndsp* %s() { return nullptr; }\n' $mk > $cls.cpp
  fi
  $CXX $fl -c $cls.cpp -o $cls.o
}
gen_cert Faust_ocpp_strict make_faust_ocpp_strict "-O3 ${STRICT_EXTRA:-}" -lang ocpp
gen_cert Faust_ocpp_fast make_faust_ocpp_fast "-O3 -ffast-math" -lang ocpp
gen_cert Faust_best_fast make_faust_best_fast "-O3 -ffast-math -fbracket-depth=1024" $BEST
FC="$HERE/tools/faust-cert-sr48.sh" gen_cert Faust_ocpp48_strict make_faust_ocpp48_strict "-O3 ${STRICT_EXTRA:-}" -lang ocpp
FC="$HERE/tools/faust-cert-sr48.sh" gen_cert Faust_best48_fast make_faust_best48_fast "-O3 -ffast-math -fbracket-depth=1024" $BEST

# Cmajor: the C++ class generated by cmaj (renamed per variant, like faust -cn) and the source for its JIT
CMAJ_SRC=""; YDSP_SRC=""
[ -f "$HERE/dsp/$N.ydsp" ] && YDSP_SRC="$HERE/dsp/$N.ydsp"
if [ -f "$HERE/dsp/$N.cmajor" ]; then
  CMAJ_SRC="$HERE/dsp/$N.cmajor"
  cp "$CMAJ_SRC" cmaj_src.cmajor
  printf '{ "CmajorVersion": 1, "ID": "bench.%s", "version": "1.0", "name": "%s", "source": "cmaj_src.cmajor" }\n' $N $N > cmaj.cmajorpatch
  if "$CMAJ" generate --target=cpp --maxFramesPerBlock=256 --output=cmaj_gen.h cmaj.cmajorpatch > cmaj_gen.log 2>&1; then
    P=$(sed -n 's/^struct \([A-Za-z_0-9]*\)$/\1/p' cmaj_gen.h | head -1)
  else  # cmaj rejects the port: the cmaj-cpp engines are left out (cmaj-jit fails at run time the same way)
    echo "build.sh: cmaj generate: $(grep -m1 error: cmaj_gen.log | cut -c1-160)" >&2
    CMAJ_SRC=""
  fi
fi
for v in strict fast; do
  if [ -f cmaj_gen.h ] && [ -n "$CMAJ_SRC" ]; then
    perl -pe "s/\b$P\b/Cmaj_$v/g" cmaj_gen.h > Cmaj_$v.h
    printf '#include "ext_dsp.h"\n#include "Cmaj_%s.h"\ndsp* make_cmaj_cpp_%s() { return new cmaj_cpp_dsp<Cmaj_%s>(); }\n' $v $v $v > Cmaj_$v.cpp
  else
    printf '#include <faust/dsp/dsp.h>\ndsp* make_cmaj_cpp_%s() { return nullptr; }\n' $v > Cmaj_$v.cpp
  fi
  fl="-O3 ${STRICT_EXTRA:-}"; [ $v = fast ] && fl="-O3 -ffast-math"
  $CXX $fl -I"$HERE/tools" -c Cmaj_$v.cpp -o Cmaj_$v.o
done

$CXX -O2 -I. -I"$HERE/tools" -I"$HERE/tools/ydsp" -I/usr/local/include -DCMAJ_SRC=\"$CMAJ_SRC\" -DYDSP_SRC=\"$YDSP_SRC\" -DFAUSTLIB=\"$FAUSTLIB\" -DSR48DIR=\"$HERE/sr48\" -c "$HERE/harness.cpp" -o harness.o
$CXX -o bench harness.o Faust_*.o Cmaj_*.o onda_strict.o onda_fast.o "$HERE/build/_common/cmaj_jit.o" \
  "$HERE/build/_ydsp/libydsp_engine.a" /usr/local/lib/libfaustwithllvm.a \
  -L/opt/local/lib -lz -lzstd -lxml2 -lncurses -framework Accelerate \
  -framework Foundation -framework Cocoa -framework IOKit -framework Security -framework CoreAudio \
  -framework CoreMIDI -framework AudioToolbox -framework UserNotifications
echo "built $D/bench"
