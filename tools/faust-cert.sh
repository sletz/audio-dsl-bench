#!/bin/bash
# faust of the master-dev-certification-7 branch (2.94.1, built from /Users/letz/faust by
# tools/build-faust-cert.sh), with the libraries of the faustlibraries checkout.
# CERT_BIN=faust-cert4 selects another build.
exec "$(dirname "$0")/${CERT_BIN:-faust-cert7}" "$@" -I /Users/letz/Developpements/faustlibraries
