# AGENTS.md - audio-dsl-bench

Notes for coding agents working in this repository. The README describes the project for readers;
this file says how to change it without breaking the measurements.

## What the project is

The same DSP programs written in Faust, onda, Cmajor, YDSP and plain C++ (`dsp/NAME.*`), built by
`build.sh` into one harness (`harness.cpp`) that checks every engine's output against Faust
`cpp-scal` and then times interleaved, shuffled rounds. `run_all.sh` measures every program in two
passes and regenerates the report. Measured on an Apple M5 (macOS); `tools/linux/` runs the same
programs on x86-64 Linux.

## Rules

- **Commit and push only when asked.** Keep the history linear: rebase or squash, no merge
  commits. Commit messages follow the existing style: one short imperative English line, a body
  only when it explains something.
- **Generated files are never edited by hand.** `results/*.tsv`, `results/results.json`,
  `results/bound.json`, `results/autotool/`, `report/audio-dsl-bench.html` and
  `report/languages-*.svg` come from `run_all.sh`, `report/gen.py`, `report/readme_chart.py`,
  `tools/loop_bound.py` and `tools/autotune_all.sh`. Change the template (`report/template.html`)
  or the scripts, then regenerate.
- **Numbers in prose come from the data.** The README and the report quote geometric means,
  win counts and per-program timings; after a new measurement, re-read them against
  `results/results.json` or let `gen.py` substitute them.
- **A port computes the same thing as the Faust program.** Same algorithm, default parameter
  values and 48 kHz constants; its `rel_rms` against `cpp-scal` must stay small (fast-math and
  chaotic programs such as `greyhole` diverge more, which is expected). The C++ ports are plain
  C++17 written for speed: no intrinsics, pragmas or libraries.
- **Pinned versions.** Onda 0.9.3 (processor ABI 7), Faust reference `0b21bae83` (2.95.3,
  `tools/faust-ref`), cert-7 `7db0ae9cd` (`tools/faust-cert7`), faustlibraries `cd7555d`,
  Cmajor 1.0.3209. Changing one means updating `README.md` and `tools/linux/README.md`, and
  remeasuring. `results/autotool/` was elected with that cert-7 build.
- **Unversioned build products.** `tools/faust-cert7`, `tools/faust-ref` and `sr48/` are
  rebuilt by `tools/build-faust-cert.sh` and `tools/make-sr48.sh`; `build/` is scratch.

## Measuring

- A full `./run_all.sh` takes about 30 minutes: run it in the background, on an idle machine on
  AC power. For a test run, set `RESULTS_DIR=build/some-dir` so that `results/` and the report
  are left alone; `KERNELS="sine zita_rev"` restricts the programs.
- Build output goes to `build/NAME.build.log` and is shown only when the build fails.
- Before trusting a change in timing, compare with the previous run and with earlier commits
  (`git show COMMIT:results/results.tsv`). A program slower by the same factor on every engine
  was measured while the machine was busy: re-run that program. A single engine that jumps with
  a small median absolute deviation can also be an outlier of one run; re-run it before
  concluding.
- The two passes: `results/results.tsv` uses clang's default contraction, and
  `results/results_nocontract.tsv` builds the strict C++ variants with `-ffp-contract=off` (the
  "IEEE" columns of the report).

## Where things are listed

Adding a program or an engine touches several lists that must agree:

- **Program**: the five sources in `dsp/`, `KERNELS` in `run_all.sh`, `CATS` in
  `report/gen.py` and `report/readme_chart.py`.
- **Engine**: `harness.cpp` (`add_faust` or an `Engine` subclass), `build.sh`, the engine lists of
  `report/gen.py` (`GROUPS`, `IEEE`, `FAST`, `LABEL`, `ALL_S`, `ALL_F`), `report/readme_chart.py`,
  `ENGINES` in `tools/loop_bound.py`, and `tools/linux/run.py` and `tools/linux/summarize.py`.
- **Harness output columns**: read by `report/gen.py` and `tools/linux/summarize.py`; change
  both with the `printf` in `harness.cpp`.

## Paths

`build.sh` defaults to the maintainer's checkouts (`../onda`, `../faustlibraries`, `../yup`,
MacPorts LLVM 21 in `/opt/local`, libfaust and Cmajor in `/usr/local`). Override them with
`ONDA`, `ONDA_INCLUDE`, `FAUST_REF`, `CERT_BIN`, `CMAJ` and `YUP_ROOT` rather than editing the
defaults.
