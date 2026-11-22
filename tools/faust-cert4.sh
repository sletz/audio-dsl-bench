#!/bin/bash
# faust of the master-dev-certification-4 branch (2.92.5, built from /Users/letz/faust),
# with the libraries of the faustlibraries checkout. CERT_BIN=faust-cert7 selects another build.
exec "$(dirname "$0")/${CERT_BIN:-faust-cert4}" "$@" -I /Users/letz/Developpements/faustlibraries
