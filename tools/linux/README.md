# Run the benchmark on x86-64 Linux

The Linux runner builds the root [benchmark harness](../../harness.cpp) with Onda AOT
objects, Faust C++ and LLVM engines, Cmajor generated C++ and LLVM JIT, the YUP YDSP
JIT, and the hand-written C++ ports. It selects the 38 shared workloads by default.

## Build the toolchains

Use Python 3, Clang C++, `llvm-objcopy`, `taskset`, CMake 3.31 or newer, Ninja, Make,
and the development dependencies for LLVM, Faust, and zlib. The runner defaults to the
published Onda 0.9.2 x86-64 Linux package and its bundled ABI header under
`build/toolchains/onda-0.9.2-linux-x64`. Use `--onda` and `--onda-include` together for
a separate installation. The runner supplies the strict or fast-math flag when that
flag is available.

From the benchmark checkout, put the dependencies in `build/toolchains/` or substitute
existing checkouts. The commands below use the compiler and library revisions tested on Linux.

```bash
deps="$PWD/build/toolchains"
mkdir -p "$deps"
curl -fL https://github.com/onda-lang/onda/releases/download/0.9.2/onda-0.9.2-linux-x64.tar.xz \
  -o "$deps/onda-0.9.2-linux-x64.tar.xz"
printf '%s  %s\n' '832bbaddad692590022c08a1c07b48d5942ed790abf37de9552bb9f42e69f0f7' \
  "$deps/onda-0.9.2-linux-x64.tar.xz" | sha256sum --check
tar -xJf "$deps/onda-0.9.2-linux-x64.tar.xz" -C "$deps"

git clone https://github.com/grame-cncm/faust "$deps/onda-faust-main"
git -C "$deps/onda-faust-main" checkout --detach 955f6e2ad
git -C "$deps/onda-faust-main" worktree add --detach "$deps/onda-faust-cert7" 06dd38660
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
build the CLI from the same tag with `-DWEBKIT2_GTK_VERSION=webkit2gtk-4.1`.
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

The harness first renders four seconds with each engine's default parameters. The summary
uses only finite rows with relative RMS error at most 0.001 against strict
scalar Faust C++. Each pair uses the workloads where both sides pass. The TSV retains all
timed rows. Check each workload's `build.log` and `run.log` for compiler or JIT diagnostics.

Append workload names to run a subset. `--ir NAME` writes Onda MIR and optimized LLVM
IR, LLVM IR from Clang for the C++ translation units, and IR from the timed Faust LLVM
factories. `--llvm-no-fast` requests a separate Faust LLVM diagnostic and labels its
rows `-no-fm`.
