#!/bin/bash
# Builds tools/faust-cert4: the faust compiler of the master-dev-certification-4 branch,
# exported with git archive (the faust checkout's working tree is not touched) and built
# with the Makefile of build/ (configuring cmake by hand gave a binary that crashed).
#   FAUST_REPO=/Users/letz/faust BRANCH=master-dev-certification-4 tools/build-faust-cert4.sh
set -euo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
FAUST_REPO=${FAUST_REPO:-/Users/letz/faust}
BRANCH=${BRANCH:-master-dev-certification-4}
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
git -C "$FAUST_REPO" archive "$BRANCH" | tar -x -C "$WORK"
make -C "$WORK/build" cmake BACKENDS=regular.cmake TARGETS=regular.cmake > "$WORK/cmake.log"
make -C "$WORK/build" faust > "$WORK/make.log"
cp "$WORK/build/bin/faust" "$HERE/tools/faust-cert4"
echo "$(git -C "$FAUST_REPO" rev-parse --short "$BRANCH") $("$HERE/tools/faust-cert4" --version | head -1)"
