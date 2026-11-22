#!/bin/bash
# faust of the master-dev-certification-4 branch with SR fixed at 48 kHz at compile time:
# sr48/ is a copy of the faustlibraries .lib files where platform.lib says SR = 48000.0
# (faust resolves platform.lib next to maths.lib first, so the whole set is copied).
H="$(cd "$(dirname "$0")/.." && pwd)"
exec "$H/tools/faust-cert4" "$@" -I "$H/sr48"
