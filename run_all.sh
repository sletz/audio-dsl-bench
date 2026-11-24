#!/bin/bash
# Builds and runs every program in two passes, then regenerates the report.
#   pass 1: default clang contract (results/results.tsv)
#   pass 2: C++ strict variants at -ffp-contract=off (results/results_nocontract.tsv, the "IEEE" columns)
#   ROUNDS=21 KERNELS="sine zita_rev" ./run_all.sh
# The fcautotool election (results/autotool/) is redone separately by tools/autotune_all.sh.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"; cd "$HERE"
KERNELS=${KERNELS:-"sine fm_pad additive64 noise tanh_drive biquad8 peq10 svf_mod fir32c fir32 hoa_decoder_x hoa_decoder mixer16_x mixer16 freeverb fdn8 diffuser32 zita_rev clarinet violin frenchbell frenchbell_x crybaby moog_vcf compressor distortion phaser flanger pitch_shifter dattorro karplus bubble virtual_analog diode_ladder filter_bank noise_gate exciter autopan greyhole tapiir hoa_octo flute"}
ROUNDS=${ROUNDS:-21}
RD=${RESULTS_DIR:-results}   # another directory (e.g. a run with another faust build, CERT_BIN=...) skips the report
mkdir -p "$RD" build
run_pass() {  # $1 = output file, $2 = extra flags for the strict C++ variants
  : > "$1"
  for n in $KERNELS; do
    # the build log is shown only when the build fails (libfaustwithllvm.a gives hundreds of ld warnings)
    STRICT_EXTRA="$2" ./build.sh "$n" > "build/$n.build.log" 2>&1 || { cat "build/$n.build.log" >&2; exit 1; }
    (cd "build/$n" && ./bench "$HERE/dsp/$n.dsp" "$n" "$ROUNDS") | tee -a "$1"
  done
}
run_pass "$RD/results.tsv" ""
run_pass "$RD/results_nocontract.tsv" "-ffp-contract=off"
[ "$RD" = results ] || { echo "results in $RD"; exit 0; }
python3 report/gen.py "$HERE"
python3 report/readme_chart.py "$HERE"
echo "report: $HERE/report/audio-dsl-bench.html"
