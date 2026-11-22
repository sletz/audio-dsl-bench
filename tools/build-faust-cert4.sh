#!/bin/bash
# Builds tools/faust-cert4: the faust compiler of the master-dev-certification-4 branch (or of
# another branch into another name: BRANCH=origin/master-dev-certification-7 OUT=faust-cert7),
# exported with git archive (the faust checkout's working tree is not touched) and built
# with the Makefile of build/ (configuring cmake by hand gave a binary that crashed).
#   FAUST_REPO=/Users/letz/faust BRANCH=master-dev-certification-4 tools/build-faust-cert4.sh
set -euo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
FAUST_REPO=${FAUST_REPO:-/Users/letz/faust}
BRANCH=${BRANCH:-master-dev-certification-4}
OUT=${OUT:-faust-cert4}
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
git -C "$FAUST_REPO" archive "$BRANCH" | tar -x -C "$WORK"
make -C "$WORK/build" cmake BACKENDS=regular.cmake TARGETS=regular.cmake > "$WORK/cmake.log"
make -C "$WORK/build" faust > "$WORK/make.log"
cp "$WORK/build/bin/faust" "$HERE/tools/$OUT"
echo "$(git -C "$FAUST_REPO" rev-parse --short "$BRANCH") $("$HERE/tools/$OUT" --version | head -1)"
