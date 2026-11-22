#!/bin/bash
# Builds sr48/: a copy of the faustlibraries .lib files where platform.lib fixes SR at 48 kHz.
# faust resolves platform.lib next to maths.lib first, so the whole set is copied.
set -euo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
LIBS=${FAUSTLIB:-/Users/letz/Developpements/faustlibraries}
rm -rf "$HERE/sr48"; mkdir -p "$HERE/sr48"
cp "$LIBS"/*.lib "$HERE/sr48/"
sed -i '' 's|^SR = min(192000.0, max(50.0, fconstant(int fSamplingFreq, <math.h>)));|SR = 48000.0; // fixed at compile time (sr48 copy of the bench)|' "$HERE/sr48/platform.lib"
grep -q '^SR = 48000.0;' "$HERE/sr48/platform.lib" || { echo "make-sr48: SR line of platform.lib not found" >&2; exit 1; }
echo "sr48: $(ls "$HERE/sr48" | wc -l | tr -d ' ') libraries, SR = 48000.0"
