# Audio DSL bench: onda, Faust, Cmajor, YDSP

The same 38 DSP programs, written in four audio DSP languages and timed on one harness, with the
same input, block size and sample rate:

| Language | What it is | How it is measured here |
| --- | --- | --- |
| [Faust](https://faust.grame.fr) | functional block-diagram language, compiled to C++, LLVM and others | 13 builds: C++ (`-scal`, `-vec`, Accelerate), LLVM JIT, and the `master-dev-certification-4` branch (`-lang ocpp`, options elected by `fcautotool`, fixed sample rate) |
| [onda](https://github.com/onda-lang/onda) | DSP language with a Rust compiler and an LLVM backend | AOT object, strict and `--fast-math` |
| [Cmajor](https://cmajor.dev) | C-like language with processors, graphs and endpoints | the C++ from `cmaj generate`, compiled by clang, and Cmajor's own LLVM JIT |
| YDSP | the JIT-compiled DSP language of the [YUP](https://github.com/kunitoki/yup) framework (`yup_dsp_jit`), with Cmajor-style processors and a Faust-style graph algebra | its asmjit backend, strict and `fastMath` |

Faust is the reference: every result below is a speed relative to the fastest Faust build of the
same floating-point contract (strict IEEE, or fast-math). Measured in November 2026 on an Apple
M5, f32, 48 kHz, 256-frame blocks. Raw numbers in `results/`, full tables, findings and the
sources side by side in the report, `report/audio-dsl-bench.html`.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="report/languages-dark.svg">
  <img alt="Speed of onda, Cmajor and YDSP relative to the best Faust build, for 38 programs in seven categories, strict IEEE and fast-math, log scale. Geometric means, strict: onda x0.96, Cmajor x0.72, YDSP x0.65; fast-math: onda x0.81, Cmajor x0.62, YDSP x0.62. Faust is faster on most programs; the clearest wins over it are onda on additive64, noise_gate and karplus, Cmajor on tapiir, pitch_shifter and the violin, YDSP on frenchbell and diode_ladder and, with fastMath, tanh_drive." src="report/languages-light.svg">
</picture>

## Results

Geometric means; above ×1 the language is faster than Faust.

| | best Faust, strict | best Faust, fast-math | Faust 2.95.1 alone, strict | Faust 2.95.1 alone, fast-math |
| --- | ---: | ---: | ---: | ---: |
| onda | ×0.96 | ×0.81 | ×1.22 | ×1.06 |
| Cmajor (faster of its C++ and its JIT) | ×0.72 | ×0.62 | ×0.92 | ×0.81 |
| YDSP | ×0.65 | ×0.62 | ×0.83 | ×0.81 |

Over the 38 programs. "Faust 2.95.1 alone" leaves out the certification-4 branch (C++ and
LLVM, every option). On the 20 Faust examples alone, onda is level with the best Faust
(×1.02 strict) and Cmajor closer to it (×0.84) than on the 18 programs written for the
benchmark (onda ×0.90, Cmajor ×0.60); YDSP is at ×0.68 and ×0.62.

Fastest engine per program, strict: Faust 18, onda 13, Cmajor 6, YDSP 1; fast-math: Faust 24,
onda 9, Cmajor 4, YDSP 1. On virtual_analog, the fast-math builds of the certification-4 faust
produce a wrong output (0.88 relative RMS from every other engine, while its strict build
matches): they are left out of the comparisons.

- **Faust** is fastest on filters, delay lines and reverbs, mostly through the certification-4
  branch, which reorders operations and reads delay lines one sample ahead (dattorro: 3.4 ns for
  the options elected by fcautotool with fast-math, against 9 to 15 ns for onda, Cmajor, YDSP
  and Faust 2.95.1).
- **onda** wins as soon as it vectorizes (additive64 ×3.3 strict) and keeps its state in
  registers (noise_gate ×2.0, tanh_drive ×1.6, karplus ×1.5); it loses most on freeverb, peq10
  and hoa_decoder, and with fast-math on the delay-line programs (tapiir, dattorro, diffuser32).
- **Cmajor** is the fastest of all on tapiir (×1.9), pitch_shifter (×1.7), the violin and the
  flute in strict mode; its JIT is 7 % faster than its generated C++ on average. Its runtime runs
  the processor one frame at a time and exchanges audio by copies: about 2 ns per sample, much
  more with many channels (hoa_decoder ×0.12, mixer16 ×0.25).
- **YDSP** is the fastest of all on frenchbell in strict mode and, with `fastMath`, on
  tanh_drive (×1.5); it also beats the best Faust on diode_ladder in strict mode (×1.2). Strict, it keeps transcendentals on scalar libm, which costs
  it most on autopan, filter_bank and phaser; its FIR (fir32c) and its delay matrices (tapiir,
  hoa_decoder) are its weakest programs.

## Engines

Each benchmark executable links 21 engines and runs them on the same buffers (16 input and output
channels), interleaved and shuffled. The name is the one printed by `bench` and used in the report.

| Language | Engine | What runs |
| --- | --- | --- |
| Faust 2.95.1 | `cpp-scal`, `cpp-vec` | `faust -lang cpp`, `-scal` or `-vec -vs 32`, clang `-O3` with its default contract (FMA) or at `-ffp-contract=off` (the IEEE pass) |
| | `cpp-scal-fm`, `cpp-vec-fm`, `cpp-vec-acc` | the same with `-ffast-math`; `cpp-vec-acc` adds `-fveclib=Accelerate` |
| | `llvm-scal`, `llvm-vec`, `llvm48` | `libfaustwithllvm` JIT, `-scal` or `-vec`; `llvm48` with the sample rate fixed at 48 kHz |
| Faust cert-4 | `ocpp`, `ocpp-fm`, `ocpp48` | faust of the `master-dev-certification-4` branch (2.92.5), `-lang ocpp`, strict or `-ffast-math`, `ocpp48` with a fixed sample rate |
| | `best-fm`, `best48-fm` | the options elected by `fcautotool` (`results/autotool/`), compiled with `-ffast-math` |
| onda | `onda`, `onda-fm` | AOT object (`--emit obj -b 256 --sr 48000`), strict or `--fast-math`, called through the raw ABI |
| Cmajor | `cmaj-cpp`, `cmaj-cpp-fm` | the C++ class of `cmaj generate --target=cpp`, clang `-O3` (IEEE pass: `-ffp-contract=off`) or `-O3 -ffast-math` |
| | `cmaj-jit`, `cmaj-jit-fm` | Cmajor's LLVM JIT (`libCmajPerformer`), optimisation level 3, or 4 (fast maths) |
| YDSP | `ydsp`, `ydsp-fm` | YUP's JIT for the host CPU (NEON, 4 lanes), `fastMath` false or true (its default) |

Faust LLVM marks every operation fast-math: its columns count as fast-math. The certification-4
faust rejects the violin and the flute with the current libraries (its interval analysis cannot
bound the `de.fdelay4` index), so these two have no cert-4 engine.

## Programs

Thirty-eight programs in seven categories, each in `dsp/` as `NAME.dsp`, `NAME.onda`,
`NAME.cmajor` and `NAME.ydsp`. Eighteen were written for the benchmark; twenty reproduce
emblematic programs of the Faust distribution (`examples/` of the Faust repository, named in the
table), most of them demos of the Faust libraries written out with their default settings:

| Category | Written for the benchmark | From the Faust examples |
| --- | --- | --- |
| Oscillators and sources | `sine`, `fm_pad` (2 FM voices + tanh), `additive64` (64 partials), `noise` (LCG + lowpass) | `virtual_analog` (generator/virtualAnalog), `bubble` (gameaudio/bubble, one drop into a freeverb) |
| Nonlinearities and dynamics | `tanh_drive` | `distortion` (dynamic/distortion), `compressor` (dynamic/compressor), `noise_gate` (dynamic/noiseGate), `exciter` (psychoacoustic/harmonicExciter) |
| Filters | `biquad8` (8 biquads in series), `peq10` (10-band parametric EQ), `svf_mod` (SVF with a modulated cutoff), `fir32c` (32-tap FIR) | `crybaby` (filtering/cryBaby), `moog_vcf` (filtering/moogVCF), `diode_ladder` (filtering/diodeLadder), `filter_bank` (filtering/filterBank, 10 octave bands) |
| Modulation and pitch | | `phaser` (phasing/phaser), `flanger` (phasing/flanger), `pitch_shifter` (pitchShifting/pitchShifter), `autopan` (misc/autopan) |
| Spatialization and mixing | `hoa_decoder_x` (order-3 2D ambisonic decoder, 7 to 8 channels), `mixer16_x` (16 channels, gain and pan) | `hoa_octo` (ambisonics/fourSourcesToOcto: 4 sources, order 3, 8 speakers) |
| Delays and reverbs | `freeverb`, `fdn8` (8-line FDN), `diffuser32` (32 allpasses), `zita_rev` (`re.zita_rev1_stereo`) | `dattorro` (reverb/dattorro), `greyhole` (reverb/greyhole), `tapiir` (delayEcho/tapiir, 6-tap feedback matrix) |
| Physical models | `clarinet`, `violin`, `frenchbell` (`pm.clarinet_ui_MIDI`, `pm.violin_ui_MIDI`, `pm.frenchBell_ui`) | `karplus` (physicalModeling/karplus), `flute` (physicalModeling/flute) |

The ports follow the Faust program, the Cmajor and YDSP ones after the onda port: same
equations, same delay lengths, same order of operations where the language allows. For the
examples, bypass switches are dropped, mode switches stay on their default branch, and gate
buttons become a slider held at 1 (the string is plucked once, the bubble dropped once, the wind
instruments play a sustained note). Four examples get other defaults, because theirs make the
program trivial: `pitch_shifter` shifts by 5 semitones (default 0), `autopan` has its amount at 1
(default 0), `tapiir` has 0.05·(n+1) s delays and 0.1 tap gains (defaults 0), and `noise_gate`
keeps its -30 dB threshold, so with the benchmark's noise input it stays open and mostly measures
its detector. Four alternative onda writings (`fir32`, `hoa_decoder`, `mixer16`, `frenchbell_x`)
are measured but kept out of the means; they have no Cmajor or YDSP version.

Every engine's output is compared with Faust C++ over 4 s. In the IEEE pass, where neither side
contracts multiply-adds, the strict onda, Cmajor and YDSP ports stay within 8·10⁻⁴ relative RMS
of Faust, the largest on additive64 (f32 phase drift); several are bit-exact. In the default pass,
where clang fuses multiply-adds in the Faust reference, greyhole differs by 0.2 for every engine
that does not fuse, Faust's own included: its prime-length smoothing recursion amplifies the last
bit into slowly wandering delay lengths.

## What the ports taught

Each port is written the way its compiler handles best, while staying ordinary code in that
language. These measurements, taken while porting, decided the form of the ports:

- **onda**: the `ins[i]`/`outs[i]` views cost a factor of 50 on hoa_decoder (named ports
  gathered in a local array instead), and control-rate math left in `sample:` a factor of 40 on
  mixer16 (computed in `block:` instead).
- **Cmajor**: parameters are `input event` endpoints whose handlers recompute the derived
  coefficients, Cmajor's way to keep control-rate math out of the sample loop (the generated C++
  ignores the `init` annotation of `input value` endpoints and reads a value endpoint through a
  per-frame ramp). The JIT prefers scalar state: biquad8 with its state in two `float[8]` took
  19.5 ns, in 16 scalars 7.2 ns (the generated C++ stayed at 18 ns: its state lives in memory
  from one frame to the next).
- **YDSP**: scalar state is kept in registers across the sample loop, and a delay line whose
  counter wraps at a constant (`i += 1; if (i >= 1008) { i = 0; }`) has its bounds proven. fdn8
  as a struct array of lines with a nested Hadamard loop took 44 ns, with named lines and scalar
  state 5.7 ns; freeverb and diffuser32 as graphs of one processor with a run-time `size`
  parameter took 14.8 and 33.9 ns, as single processors 4.4 and 19.2 ns.
- On peq10 the scalar form was slower in both Cmajor and YDSP, so that port keeps arrays.
- **YDSP and libm**: strict YDSP pays its scalar transcendentals more than the others: the 10
  `pow` of filter_bank cost about 5 ns each, the two `tanh` of autopan 4 ns each, and compressor
  falls from 10 to 5.8 ns without its per-sample `pow`. The ports keep the same equations as
  the other languages rather than rewriting them for YDSP.
- **greyhole** has 24 allpass lines of 16384 floats. Placed back to back, they sit exactly 64 KB
  apart and all fall into the same L1 cache sets; the onda, Cmajor and YDSP ports pad each line
  with 16 floats (onda: 121 to 43 ns, YDSP: 137 to 73 ns). Faust's generated C++ escapes this
  only partly, because small arrays happen to sit between its lines. This is a memory-layout
  choice the three languages let the author make, not a change of algorithm.

## Requirements

- **onda**: `tools/build-onda.sh` builds `../onda/target/release/onda` against MacPorts LLVM 21,
  through a "shadow" prefix (`tools/make-llvm-shadow.sh`) limited to AArch64/ARM/X86/WebAssembly,
  because onda's `native/target.cpp` shim initializes every target of the LLVM installation
  (AMDGPU, etc.), which `build.rs` does not link. Otherwise: `ONDA=/path/to/onda ./build.sh ...`.
  `llvm-objcopy` from LLVM 21 (`/opt/local/libexec/llvm-21/bin`) renames the symbols of the
  strict and fast-math objects, which export the same names.
- **Faust**: `faust` and `/usr/local/lib/libfaustwithllvm.a` (with libz, libzstd, libxml2,
  libncurses from MacPorts in `/opt/local/lib`), and the libraries of the `../faustlibraries`
  checkout (`FAUSTLIB` in `build.sh`). `tools/faust-cert4`, the faust of the
  `master-dev-certification-4` branch, is not versioned: `tools/build-faust-cert4.sh` rebuilds it
  from `/Users/letz/faust` (`git archive`, the working tree is not touched). `sr48/`, the
  libraries with `SR = 48000.0` in `platform.lib`, is recreated by `tools/make-sr48.sh`, which
  `build.sh` calls when it is missing.
- **Cmajor** 1.0.3209: `cmaj` in the `PATH` (`CMAJ=...` otherwise), the API headers in
  `/usr/local/include/cmajor` and `/usr/local/include/choc`, and
  `/usr/local/lib/libCmajPerformer.dylib`, loaded at run time by the `cmaj-jit` engines.
- **YDSP**: a YUP checkout (`git clone https://github.com/kunitoki/yup ../yup`, or
  `YUP_ROOT=...`), CMake ≥ 3.31 and Ninja. `build.sh` builds `build/_ydsp/libydsp_engine.a` once
  with YUP's own CMake (a few minutes the first time); YUP fetches the missing upstream sources
  of its third-party modules.

## Usage

```bash
./build.sh zita_rev                  # build/zita_rev/bench
cd build/zita_rev && ./bench ../../dsp/zita_rev.dsp zita_rev 21
```

Output, one line per engine: program, engine, median ns/sample, minimum, median absolute
deviation, maximum and relative RMS difference against `cpp-scal`, finiteness, RMS of the
reference. A Cmajor or YDSP port that does not compile only drops its engines.

```bash
./run_all.sh                         # the 42 programs in two passes, ~25 min
```

`run_all.sh` regenerates `results/`, `report/audio-dsl-bench.html` and the README chart. The
second pass compiles the strict C++ variants (Faust and Cmajor) with `-ffp-contract=off`, the
"IEEE" columns. `tools/autotune_all.sh` redoes the `fcautotool` election (~6 min). Measure on an
idle machine, on AC power.

## Files

| File | Role |
| --- | --- |
| `dsp/NAME.dsp`, `.onda`, `.cmajor`, `.ydsp` | The programs in the four languages |
| `harness.cpp` | Links the 21 engines, compares their outputs over 4 s, then times interleaved, shuffled rounds |
| `build.sh` | One program: onda objects, Faust and Cmajor C++ classes, harness |
| `run_all.sh` | The 42 programs in two passes, then the report and the chart |
| `tools/ext_dsp.h` | Cmajor and YDSP engines behind the Faust `dsp` interface |
| `tools/cmaj_jit.cpp`, `tools/cmaj_jit.h` | Cmajor LLVM JIT engine (C interface over the Cmajor API) |
| `tools/ydsp/` | YDSP engine: C interface over `yup_dsp_jit`, and the CMake project that builds it as a static library |
| `report/gen.py`, `report/template.html` | Merge the two passes (`results/results.json`) and generate the HTML report |
| `report/readme_chart.py` | The README chart, `report/languages-light.svg` and `report/languages-dark.svg` |
| `tools/faust-cert4*.sh` | cert-4 faust with the faustlibraries libraries, or with `sr48/` |
| `tools/build-faust-cert4.sh`, `tools/make-sr48.sh` | Recreate the two generated, unversioned items |
| `tools/autotune_all.sh` | Elects the options of each program with `fcautotool` |
| `tools/dumpir.cpp` | Writes the LLVM IR of a Faust LLVM factory (`writeDSPFactoryToIRFile`) |
| `tools/ctime.cpp` | Time to create a Faust LLVM factory and an instance |

## Method notes

- onda fixes the block size (256) and the sample rate (48 kHz) at compile time, and the Cmajor
  JIT gets the sample rate at build time. Faust (`init(48000)`, `compute(256, ...)`), the Cmajor
  C++ class (`initialise(0, 48000)`) and YDSP (`prepare(48000, 256)`) get it at run time, except
  the fixed-SR Faust builds.
- Cmajor exchanges audio by copies (`setInputFrames`, `advance`, `copyOutputFrames`): they are
  part of its measured time. The other engines process the harness buffers in place.
- clang contracts into FMA by default; the strict columns use the `-ffp-contract=off` pass.
  Strict onda and strict YDSP do not contract; the Cmajor JIT at `-O3` reproduces strict onda's
  output on most programs.
- Parameters stay at their defaults (no automation); a single block size and a single machine
  (ARM64, 128-bit NEON).
- `zita_rev` ports `re.zita_rev1_stereo(60, f1, f2, t60dc, t60m, 48000)` with delay lengths
  computed for 48 kHz; the onda, Cmajor and YDSP versions are not valid at another rate.
