#!/bin/bash
# Builds an LLVM prefix limited to AArch64/ARM/X86/WebAssembly on top of MacPorts LLVM 21.
# onda's native/target.cpp calls LLVMInitializeAll*, which with the full MacPorts LLVM
# references AMDGPU, etc., that onda's build.rs never links.
set -euo pipefail
R=${LLVM21:-/opt/local/libexec/llvm-21}
P=${1:-$HOME/.cache/llvm21-onda-shadow}
rm -rf "$P"; mkdir -p "$P/include/llvm/Config"
ln -s "$R/bin" "$P/bin"; ln -s "$R/lib" "$P/lib"
for d in "$R"/include/*; do [ "$(basename "$d")" = llvm ] || ln -s "$d" "$P/include/"; done
for d in "$R"/include/llvm/*; do [ "$(basename "$d")" = Config ] || ln -s "$d" "$P/include/llvm/"; done
for f in "$R"/include/llvm/Config/*; do ln -s "$f" "$P/include/llvm/Config/"; done
for f in Targets AsmPrinters AsmParsers Disassemblers TargetMCAs TargetExegesis; do
  src="$R/include/llvm/Config/$f.def"; [ -f "$src" ] || continue
  rm "$P/include/llvm/Config/$f.def"
  grep -vE "LLVM_[A-Z_]+\((AMDGPU|AVR|BPF|Hexagon|Lanai|LoongArch|Mips|MSP430|NVPTX|PowerPC|RISCV|Sparc|SPIRV|SystemZ|VE|XCore|M68k|Xtensa|ARC|CSKY|DirectX)\)" "$src" > "$P/include/llvm/Config/$f.def"
done
echo "$P"
