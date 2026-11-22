#!/bin/bash
# Builds and runs every program in two passes, then regenerates the report.
#   pass 1: default clang contract (results/results.tsv)
#   pass 2: C++ strict variants at -ffp-contract=off (results/results_nocontract.tsv, the "IEEE" columns)
#   ROUNDS=21 KERNELS="sine zita_rev" ./run_all.sh
# The fcautotool election (results/autotool/) is redone separately by tools/autotune_all.sh.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"; cd "$HERE"
KERNELS=${KERNELS:-"sine fm_pad additive64 noise tanh_drive biquad8 peq10 svf_mod fir32c fir32 hoa_decoder_x hoa_decoder mixer16_x mixer16 freeverb fdn8 diffuser32 zita_rev clarinet violin frenchbell frenchbell_x"}
ROUNDS=${ROUNDS:-21}
mkdir -p results
run_pass() {  # $1 = output file, $2 = extra flags for the strict C++ variants
  : > "$1"
  for n in $KERNELS; do
    STRICT_EXTRA="$2" ./build.sh "$n" 2>&1 | grep -E "error" || true
    (cd "build/$n" && ./bench "$HERE/dsp/$n.dsp" "$n" "$ROUNDS" 2>/dev/null) | tee -a "$1"
  done
}
run_pass results/results.tsv ""
run_pass results/results_nocontract.tsv "-ffp-contract=off"
python3 report/gen.py "$HERE"
python3 report/readme_chart.py "$HERE"
echo "report: $HERE/report/audio-dsl-bench.html"
