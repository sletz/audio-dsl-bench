#!/usr/bin/env python3
"""Report each engine family's speedup against the fastest Faust build."""

import argparse
from collections import defaultdict
import csv
from math import isfinite
from pathlib import Path
from statistics import geometric_mean


FAUST = (
    ("cpp-scal", "cpp-vec", "cpp48-scal", "cpp48-vec", "ocpp", "ocpp48"),
    ("cpp-scal-fm", "cpp-vec-fm", "cpp48-scal-fm", "cpp48-vec-fm", "cpp-vec-libmvec", "cpp-vec-acc",
     "llvm-scal", "llvm-vec", "llvm48", "llvm48-vec", "ocpp-fm", "best-fm", "best48-fm"),
)
ENGINE_FAMILIES = (
    ("Onda", (("onda",), ("onda-fm",))),
    ("Cmajor", (("cmaj-cpp", "cmaj-jit"), ("cmaj-cpp-fm", "cmaj-jit-fm"))),
    ("YDSP", (("ydsp",), ("ydsp-fm",))),
    ("Hand-written C++", (("cxx",), ("cxx-fm",))),
)


def load_results(path):
    accepted = defaultdict(dict)
    rejected = []
    with path.open() as data:
        for row in csv.DictReader(data, delimiter="\t"):
            name, engine = row["workload"], row["engine"]
            times = accepted[name]
            time = float(row["ns_per_frame"])
            if isfinite(time) and time > 0:
                times[engine] = time
            else:
                rejected.append(f"{name}/{engine}")
    return accepted, rejected


def fastest_time(times, engines):
    return min((times[engine] for engine in engines if engine in times), default=None)


def compare(accepted, engines, faust_engines):
    ratios = []
    for times in accepted.values():
        candidate = fastest_time(times, engines)
        reference = fastest_time(times, faust_engines)
        if reference is not None and candidate is not None:
            ratios.append(reference / candidate)
    if not ratios:
        return "—"
    wins = sum(ratio > 1 for ratio in ratios)
    return f"×{geometric_mean(ratios):.3f} ({wins}/{len(ratios)})"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("results", type=Path)
    args = parser.parse_args()
    accepted, rejected = load_results(args.results)
    print(f"{len(accepted)} workloads, {len(rejected)} rejected timing rows")
    print("| Engine family | Strict vs best Faust | Fast-math vs best Faust |")
    print("| --- | ---: | ---: |")
    for label, modes in ENGINE_FAMILIES:
        cells = [compare(accepted, engines, faust_engines) for engines, faust_engines in zip(modes, FAUST)]
        print(f"| {label} | {' | '.join(cells)} |")
    if rejected:
        print("\nRows with invalid timings")
        for engine in rejected:
            print(f"- `{engine}`")


if __name__ == "__main__":
    main()
