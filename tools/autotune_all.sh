#!/bin/bash
# Elects the best Faust options for every kernel with fcautotool (faust of the certification-7 branch).
set -u
HERE="$(cd "$(dirname "$0")/.." && pwd)"
export FAUST="$HERE/tools/faust-cert.sh" FCBENCH_ARCH_FLAGS="-mcpu=native"
OUT="${AUTOTOOL_DIR:-$HERE/results/autotool}"   # AUTOTOOL_DIR=... with CERT_BIN=... for another cert build
mkdir -p "$OUT"
for n in ${KERNELS:-sine biquad8 tanh_drive noise additive64 fir32 fir32c svf_mod freeverb fdn8 zita_rev fm_pad hoa_decoder mixer16 diffuser32 peq10}; do
  fcautotool "$HERE/dsp/$n.dsp" > "$OUT/$n.txt" 2>&1
  echo "$n: $(grep -m1 -iE 'elected|winner|verdict' "$OUT/$n.txt")"
done
