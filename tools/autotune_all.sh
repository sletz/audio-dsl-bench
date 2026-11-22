#!/bin/bash
# Elects the best Faust options for every kernel with fcautotool (faust of the certification-4 branch).
set -u
HERE="$(cd "$(dirname "$0")/.." && pwd)"
export FAUST="$HERE/tools/faust-cert4.sh" FCBENCH_ARCH_FLAGS="-mcpu=native"
mkdir -p "$HERE/results/autotool"
for n in ${KERNELS:-sine biquad8 tanh_drive noise additive64 fir32 fir32c svf_mod freeverb fdn8 zita_rev fm_pad hoa_decoder mixer16 diffuser32 peq10}; do
  fcautotool "$HERE/dsp/$n.dsp" > "$HERE/results/autotool/$n.txt" 2>&1
  echo "$n: $(grep -m1 -iE 'elected|winner|verdict' "$HERE/results/autotool/$n.txt")"
done
