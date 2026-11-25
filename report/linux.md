# Onda results on x86-64 Linux

The run measures the reworked Onda ports for 38 workloads on an Intel Core i9-14900HX.
Ten workloads also have a `dsp/NAME_explicit_loops.onda` implementation measured as
`onda-block` and `onda-block-fm`.
[Raw timings](../results/linux-x86_64/results.tsv) · [Linux reproduction guide](../tools/linux/README.md)

## Results

The Onda summary uses the faster measured version for each workload and math mode. The
table shows the geometric mean of the fastest Faust time divided by that Onda time, and
the number of workloads where Onda was faster.

| | Strict vs best Faust | Fast-math vs best Faust |
| --- | ---: | ---: |
| Onda | ×1.562 (31/38) | ×1.302 (23/38) |

After the explicit-loop noise port changed, all engines were remeasured for `noise` once per run.
The other 37 workloads retain their original timings. Run 3 supplies the charts and gives
the median geometric mean in both math modes.
[Run 1](../results/linux-x86_64/repeats/run-1.tsv) · [Run 2](../results/linux-x86_64/repeats/run-2.tsv) · [Run 3](../results/linux-x86_64/results.tsv)

Explicit loops were faster on nine of the ten workloads in both math modes. Each TSV
includes output comparisons against strict scalar Faust for every timed engine.

## Measurement setup

Onda 0.9.3, 48 kHz, 256-frame blocks, 21 interleaved rounds targeting 12 ms for the
slowest engine, and x86 FTZ/DAZ enabled. The run was pinned to logical CPU 12 on
CachyOS Linux with Clang and LLVM 23.1.5 and glibc 2.44. The reproduction guide lists
the toolchain revisions and command line.
