#!/usr/bin/env python3
"""Report Onda speedups against matched engine families after checking output."""

import argparse
from collections import defaultdict
import csv
from math import isfinite
from pathlib import Path
from statistics import geometric_mean


FAUST = (
    ("cpp-scal", "cpp-vec", "cpp48-scal", "cpp48-vec"),
    ("cpp-scal-fm", "cpp-vec-fm", "cpp48-scal-fm", "cpp48-vec-fm", "cpp-vec-libmvec",
     "llvm-scal", "llvm-vec", "llvm48", "llvm48-vec"),
)
CERT = (("ocpp", "ocpp48"), ("ocpp-fm", "best-fm", "best48-fm"))
FAUST_BEST = tuple(stock + cert for stock, cert in zip(FAUST, CERT))
CMAJOR = (("cmaj-cpp", "cmaj-jit"), ("cmaj-cpp-fm", "cmaj-jit-fm"))
YDSP = (("ydsp",), ("ydsp-fm",))
CXX = (("cxx",), ("cxx-fm",))
COMPARATORS = (
    ("Fastest Faust", FAUST_BEST),
    ("Cmajor", CMAJOR),
    ("YDSP", YDSP),
    ("Hand-written C++", CXX),
)
MAX_ERROR = 1e-3


def load_results(path, max_error=MAX_ERROR):
    accepted = defaultdict(dict)
    rejected = []
    with path.open() as data:
        for row in csv.DictReader(data, delimiter="\t"):
            name, engine = row["workload"], row["engine"]
            times = accepted[name]
            time = float(row["ns_per_frame"])
            error = float(row["rel_rms"])
            if row["finite"] == "finite" and 0 <= error <= max_error and isfinite(time) and time > 0:
                times[engine] = time
            else:
                rejected.append(f"{name}/{engine}")
    return accepted, rejected


def compare(accepted, against, fast):
    ratios = []
    onda = "onda-fm" if fast else "onda"
    for times in accepted.values():
        candidate = times.get(onda)
        reference = min((times[engine] for engine in against if engine in times), default=None)
        if reference is not None and candidate is not None:
            ratios.append(reference / candidate)
    if not ratios:
        return "—"
    wins = sum(ratio > 1 for ratio in ratios)
    return f"×{geometric_mean(ratios):.3f} ({wins}/{len(ratios)})"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("results", type=Path)
    parser.add_argument("--max-error", type=float, default=MAX_ERROR)
    args = parser.parse_args()
    accepted, rejected = load_results(args.results, args.max_error)
    print(f"{len(accepted)} workloads, {len(rejected)} rejected timing rows")
    print("| Compared with | Onda strict | Onda fast |")
    print("| --- | ---: | ---: |")
    for label, modes in COMPARATORS:
        cells = [compare(accepted, engines, fast) for fast, engines in enumerate(modes)]
        print(f"| {label} | {' | '.join(cells)} |")
    if rejected:
        print("\nRejected rows")
        for engine in rejected:
            print(f"- `{engine}`")


if __name__ == "__main__":
    main()
