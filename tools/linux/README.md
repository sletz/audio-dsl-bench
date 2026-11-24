# Run the benchmark on x86-64 Linux

The Linux runner builds the root [benchmark harness](../../harness.cpp) with Onda AOT
objects, Faust C++ and LLVM engines, Cmajor generated C++ and LLVM JIT, the YUP YDSP
JIT, and the hand-written C++ ports. It selects the 38 shared workloads by default.

## Build the toolchains

Use Python 3.8 or newer, Clang C++, `llvm-objcopy`, `taskset`, CMake 3.31 or newer, Ninja, Make,
and the development dependencies for LLVM, Faust, and zlib. Unpack the x86-64 Linux
package from the [latest Onda release](https://github.com/onda-lang/onda/releases/latest)
and add its `bin/` directory to `PATH`. The runner finds the bundled ABI header and
prints the compiler version. Use `ONDA` or `--onda` to select a different compiler,
and `ONDA_INCLUDE` or `--onda-include` if its matching ABI header lives elsewhere.
The strict build uses the release's default arithmetic. The fast build passes `--fast-math`.

From the benchmark checkout, put the dependencies in `build/toolchains/` or substitute
existing checkouts. The Faust revisions match the upstream report and `fcautotool` picks.

```bash
deps="$PWD/build/toolchains"
mkdir -p "$deps"

git clone https://github.com/grame-cncm/faust "$deps/onda-faust-main"
git -C "$deps/onda-faust-main" checkout --detach 0b21bae83
git -C "$deps/onda-faust-main" worktree add --detach "$deps/onda-faust-cert7" 7db0ae9cd
git clone https://github.com/grame-cncm/faustlibraries "$deps/onda-faust-libraries"
git -C "$deps/onda-faust-libraries" checkout --detach c957169
git clone https://github.com/kunitoki/yup "$deps/yup"
git -C "$deps/yup" checkout --detach db4e7c2

cmake -S "$deps/onda-faust-main/build" -B "$deps/onda-faust-main-cmake" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ \
  -DINCLUDE_LLVM=ON -DLINK_LLVM_STATIC=OFF -DINCLUDE_DYNAMIC=ON \
  '-DLLVM_BACKEND=COMPILER DYNAMIC' '-DCPP_BACKEND=COMPILER DYNAMIC' \
  '-DOLDCPP_BACKEND=COMPILER DYNAMIC' -DINCLUDE_OSC=OFF -DINCLUDE_HTTP=OFF \
  -DINCLUDE_EMCC=OFF -DINCLUDE_WASM_GLUE=OFF
cmake --build "$deps/onda-faust-main-cmake" --target faust dynamiclib --parallel 6

make -C "$deps/onda-faust-cert7/build" cmake BACKENDS=regular.cmake TARGETS=regular.cmake
make -C "$deps/onda-faust-cert7/build" faust BUILDOPT='--parallel 6'

cmake -S tools/ydsp -B build/linux-ydsp -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DYUP_ROOT="$deps/yup"
cmake --build build/linux-ydsp --parallel 6
```

Use Cmajor 1.0.3209 and its matching headers and CHOC submodule. The release `cmaj`
binary requires WebKitGTK 4.0 and libsoup 2.4. On systems with WebKitGTK 4.1, you can
build the CLI from the same tag with `-DWEBKIT2_GTK_VERSION=webkit2gtk-4.1`
and pass that executable to `--cmajor`.
[Cmajor release and source](https://github.com/cmajor-lang/cmajor/releases/tag/1.0.3209)

```bash
cmajor="$deps/cmajor-1.0.3209"
mkdir -p "$cmajor"
curl -fL https://github.com/cmajor-lang/cmajor/releases/download/1.0.3209/cmajor.linux.x64.zip \
  -o "$cmajor/cmajor.linux.x64.zip"
printf '%s  %s\n' 'b7da026020c6afe823ff134d943631b741f531993292a38263736d7f08a47e7a' \
  "$cmajor/cmajor.linux.x64.zip" | sha256sum --check
unzip "$cmajor/cmajor.linux.x64.zip" 'linux/x64/cmaj' 'linux/x64/libCmajPerformer.so' -d "$cmajor"
git clone --depth 1 --branch 1.0.3209 https://github.com/cmajor-lang/cmajor "$cmajor/source"
git -C "$cmajor/source" -c submodule.3rdParty/choc.url=https://github.com/Tracktion/choc.git \
  submodule update --init include/choc
```

## Measure and inspect

Choose an available performance core for `--cpu`. The Linux results used CPU 12.
`--libmvec` adds a Faust vector build using glibc's vector functions, analogous
to the Accelerate variant on macOS. `--cert` adds the cert-7 C++ builds and reuses the
upstream `fcautotool` selections. Omitting either option leaves out that group.
The Cmajor include and library options enable both Cmajor backends. The CLI defaults
to `cmaj` on the path when `--cmajor` is omitted.

```bash
python3 tools/linux/run.py \
  --faust "$deps/onda-faust-main/build/bin/faust" \
  --faust-include "$deps/onda-faust-main/architecture" \
  --libfaust "$deps/onda-faust-main/build/lib/libfaust.so" \
  --faust-libraries "$deps/onda-faust-libraries" \
  --cert "$deps/onda-faust-cert7/build/bin/faust" \
  --cmajor "$cmajor/linux/x64/cmaj" \
  --cmajor-include "$cmajor/source/include" \
  --cmajor-library "$cmajor/linux/x64/libCmajPerformer.so" \
  --cpu 12 --rounds 21 --round-ms 12 --jobs 6 --flush-denormals --libmvec \
  --ir tanh_drive --ir zita_rev --out build/linux-run
python3 tools/linux/summarize.py build/linux-run/results.tsv
```

The harness runs interleaved rounds at 48 kHz with 256-frame blocks. Onda uses the raw
processor ABI with both block-boundary flags.
Every engine has the same x86 denormal setting for the run. `--flush-denormals` enables
FTZ/DAZ and the default preserves denormals.

The harness first renders four seconds with each engine's default parameters. The TSV
reports the maximum absolute difference and relative RMS difference against strict scalar
Faust C++, whether the output is finite, and the RMS of the reference. As in the macOS
report, these diagnostics do not filter the timing summary. The summary compares the fastest
engine in each language family against the fastest Faust engine in the same math mode. Each
cell shows the geometric mean speedup and win count for workloads with timings on both sides.
Check each workload's `build.log` and `run.log` for compiler or JIT diagnostics.

Append workload names to run a subset. `--ir NAME` writes Onda MIR and optimized LLVM
IR, LLVM IR from Clang for the C++ translation units, and IR from the timed Faust LLVM
factories. `--llvm-no-fast` requests a separate Faust LLVM diagnostic and labels its
rows `-no-fm`.
