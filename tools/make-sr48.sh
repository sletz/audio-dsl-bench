#!/bin/bash
# Builds sr48/: a copy of the official faustlibraries .lib files (those published on GitHub,
# tracked by origin/master) where platform.lib fixes SR at 48 kHz. faust resolves platform.lib
# next to maths.lib first, so the whole set is copied.
set -euo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
LIBS=${FAUSTLIB:-/Users/letz/Developpements/faustlibraries}
rm -rf "$HERE/sr48"; mkdir -p "$HERE/sr48"
git -C "$LIBS" ls-tree --name-only origin/master | grep '\.lib$' | while read -r f; do
  cp "$LIBS/$f" "$HERE/sr48/"
done
sed -i '' 's|^SR = min(192000.0, max(50.0, fconstant(int fSamplingFreq, <math.h>)));|SR = 48000.0; // fixed at compile time (sr48 copy of the bench)|' "$HERE/sr48/platform.lib"
grep -q '^SR = 48000.0;' "$HERE/sr48/platform.lib" || { echo "make-sr48: SR line of platform.lib not found" >&2; exit 1; }
echo "sr48: $(ls "$HERE/sr48" | wc -l | tr -d ' ') official libraries, SR = 48000.0"
