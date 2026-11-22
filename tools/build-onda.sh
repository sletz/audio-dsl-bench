#!/bin/bash
# Builds onda/target/release/onda against MacPorts LLVM 21 through the shadow prefix.
set -euo pipefail
ONDA_SRC=${ONDA_SRC:-/Users/letz/Developpements/onda}
P=$("$(dirname "$0")/make-llvm-shadow.sh")
cd "$ONDA_SRC"
LLVM_SYS_211_PREFIX="$P" ONDA_LLVM_LINK_MODE=static RUSTFLAGS="-L native=/opt/local/lib" \
  MACOSX_DEPLOYMENT_TARGET=26.0 cargo build -p onda_cli --release
