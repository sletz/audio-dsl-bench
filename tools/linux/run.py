#!/usr/bin/env python3
"""Build and time the audio-dsl-bench sources on x86-64 Linux."""

import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import re
import shlex
import struct
import subprocess


HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
ONDA_RELEASE = ROOT / "build" / "toolchains" / "onda-0.9.2-linux-x64"
HEADER = "workload\tengine\tns_per_frame\tmin\tmad\tmax_abs\trel_rms\tfinite\tref_rms\tworst_channel_rel_rms\n"


def command(*argv, **kwargs):
    return subprocess.run(argv, text=True, check=True, **kwargs)


def defaults(metadata):
    storage = bytearray(max(1, metadata["runtime"]["param_size_bytes"]))
    formats = {"f32": "f", "f64": "d", "i32": "i", "i64": "q", "bool": "?"}
    for param in metadata["metadata"]["params"]:
        kind = param["scalar"]
        for index, value in enumerate(param["default_reprs"] or []):
            offset = param["byte_offset"] + index * param["element_size_bytes"]
            number = (value == "true") if kind == "bool" else (float(value) if kind[0] == "f" else int(value))
            struct.pack_into("=" + formats[kind], storage, offset, number)
    return ",".join(map(str, storage))


def port_count(metadata, direction):
    count = 0
    for port in metadata["metadata"][direction]:
        if port["scalar"] != "f32" or port["slot_offset"] != count:
            raise ValueError(f"expected contiguous f32 audio ports in {direction}")
        count += port["array_len"]
    if count > 16:
        raise ValueError(f"too many {direction}")
    return count


def fixed_rate_libraries(original, target):
    target.mkdir()
    for src in original.glob("*.lib"):
        if src.name != "platform.lib":
            (target / src.name).write_bytes(src.read_bytes())
    platform = (original / "platform.lib").read_text()
    fixed, count = re.subn(r"^SR\s*=.*?;", "SR = 48000.0;", platform, flags=re.MULTILINE)
    if count != 1:
        raise ValueError("expected one SR definition in platform.lib")
    (target / "platform.lib").write_text(fixed)


def build(args, name, fixed_libs):
    build_dir = args.out / name
    build_dir.mkdir()
    log = (build_dir / "build.log").open("w")

    def run(*argv):
        log.write(shlex.join(map(str, argv)) + "\n")
        log.flush()
        command(*argv, cwd=build_dir, stdout=log, stderr=log)

    dsp_path = args.upstream / "dsp" / f"{name}.dsp"
    onda_path = dsp_path.with_suffix(".onda")
    cxx = [args.cxx, "-std=c++17", "-march=native", "-I", args.faust_include, "-I", ROOT / "tools"]
    declarations, registration, objects = [], [], []
    ports = None
    for fast in (False, True):
        engine = "onda-fm" if fast else "onda"
        symbol = engine.replace("-", "_")
        options = [args.onda, "compile", onda_path, "--target-cpu", "host", "-b", "256", "--sr", "48000"]
        if fast:
            options.append("--fast-math")
        run(*options, "--emit", "obj", "-o", "raw.o", "--meta-out", f"{engine}.json")
        if name in args.ir:
            run(*options, "--emit", "llvm-ir", "-o", f"{engine}.ll")
            run(*options, "--emit", "mir", "-o", f"{engine}.mir")
        metadata = json.loads((build_dir / f"{engine}.json").read_text())
        if metadata["abi_version"] != 7 or metadata["metadata"]["buffers"]:
            raise ValueError("expected Onda ABI 7 with no external buffers")
        shape = tuple(port_count(metadata, port) for port in ("inputs", "outputs"))
        if ports is not None and ports != shape:
            raise ValueError("Onda variants have different port counts")
        ports = shape
        exports = metadata["exports"]
        mapping = [f"{exports[key]}={symbol}_{key}" for key in ("init", "process")]
        obj = f"{engine}.o"
        run(args.objcopy, *(f"--redefine-sym={item}" for item in mapping), "raw.o", obj)
        # Keep event symbols from different objects separate even though this harness never calls them.
        run(args.objcopy, "--wildcard", "--localize-symbol=onda_event*", obj)
        objects.append(obj)
        for key in ("init", "process"):
            decltype = "decltype(onda_processor_init)" if key == "init" else "decltype(onda_process)"
            declarations.append(f'extern "C" {decltype} {symbol}_{key};')
        declarations.append(f"const unsigned char {symbol}_defaults[] = {{{defaults(metadata)}}};")
        rt = metadata["runtime"]
        registration.append(
            f'    engines.push_back(std::make_unique<OndaEngine>("{engine}", {symbol}_init, {symbol}_process, '
            f'{rt["state_size_bytes"]}, {rt["state_align_bytes"]}, {rt["param_align_bytes"]}, '
            f'{symbol}_defaults, {rt["param_size_bytes"]}));'
        )

    def compile_cpp(engine, source, flags):
        symbol = engine.replace("-", "_")
        obj = f"{engine}.o"
        cpp = f"{engine}.cpp"
        (build_dir / cpp).write_text(source)
        run(*cxx, "-O3", *flags, "-c", cpp, "-o", obj)
        if name in args.ir:
            run(*cxx, "-O3", *flags, "-S", "-emit-llvm", cpp, "-o", f"{engine}.ll")
        objects.append(obj)
        declarations.append(f"dsp* make_{symbol}();")
        registration.append(f'    add_faust("{engine}", make_{symbol}());')

    def add_cpp(engine, faust, libraries, options, flags, optional=False):
        symbol = engine.replace("-", "_")
        header = f"{engine}.h"
        try:
            run(faust, "-I", libraries, dsp_path, *options, "-cn", symbol, "-o", header)
        except subprocess.CalledProcessError:
            if optional:
                print(f"{name}: {engine} rejected by Faust (see {log.name})", flush=True)
                return
            raise
        compile_cpp(engine,
            "#include <faust/dsp/dsp.h>\n#include <faust/gui/UI.h>\n#include <faust/gui/meta.h>\n"
            f'#include "{header}"\ndsp* make_{symbol}() {{ return new {symbol}(); }}\n', flags,
        )

    strict_flags = ["-ffp-contract=off"]
    fast_flags = ["-ffast-math"]
    for suffix, libs in (("", args.faust_libraries), ("48", fixed_libs)):
        for mode, flags in (("scal", ["-scal"]), ("vec", ["-vec", "-vs", "32"])):
            cpp = ["-lang", "cpp", *flags]
            add_cpp(f"cpp{suffix}-{mode}", args.faust, libs, cpp, strict_flags)
            add_cpp(f"cpp{suffix}-{mode}-fm", args.faust, libs, cpp, fast_flags)
    if args.libmvec:
        add_cpp("cpp-vec-libmvec", args.faust, args.faust_libraries,
                ["-lang", "cpp", "-vec", "-vs", "32"], [*fast_flags, "-fveclib=libmvec"])
    if args.cert:
        choices = args.upstream / "results" / "autotool" / f"{name}.txt"
        recorded = choices.read_text() if choices.is_file() else ""
        winner = re.search(r"^fcautotool: winner .*? -- (.*?)  \[", recorded, re.MULTILINE)
        baseline = ["-lang", "ocpp"]
        best = shlex.split(winner[1]) if winner else baseline
        for engine, libs, options, flags in (
            ("ocpp", args.faust_libraries, baseline, strict_flags),
            ("ocpp-fm", args.faust_libraries, baseline, fast_flags),
            ("best-fm", args.faust_libraries, best, [*fast_flags, "-fbracket-depth=1024"]),
            ("ocpp48", fixed_libs, baseline, strict_flags),
            ("best48-fm", fixed_libs, best, [*fast_flags, "-fbracket-depth=1024"]),
        ):
            add_cpp(engine, args.cert, libs, options, flags, optional=True)

    cmajor_path = dsp_path.with_suffix(".cmajor")
    cmajor_flags = []
    if args.cmajor_library and cmajor_path.is_file():
        cmajor_flags = [f'-DCMAJ_SRC="{cmajor_path}"']
        (build_dir / "cmaj_src.cmajor").write_bytes(cmajor_path.read_bytes())
        (build_dir / "cmaj.cmajorpatch").write_text(json.dumps({
            "CmajorVersion": 1, "ID": f"bench.{name}", "version": "1.0", "name": name,
            "source": "cmaj_src.cmajor",
        }))
        try:
            run(args.cmajor or "cmaj", "generate", "--target=cpp", "--maxFramesPerBlock=256",
                "--output=cmaj_gen.h", "cmaj.cmajorpatch")
        except subprocess.CalledProcessError:
            print(f"{name}: Cmajor C++ generation failed (see {log.name})", flush=True)
        else:
            header = (build_dir / "cmaj_gen.h").read_text()
            processor = re.search(r"^struct ([A-Za-z_][A-Za-z_0-9]*)$", header, re.MULTILINE)
            if not processor:
                raise ValueError(f"{name}: no Cmajor processor class (see {log.name})")
            for engine, flags in (("cmaj-cpp", strict_flags), ("cmaj-cpp-fm", fast_flags)):
                symbol = engine.replace("-", "_")
                compile_cpp(engine, '#include "ext_dsp.h"\n#include "cmaj_gen.h"\n'
                            f"dsp* make_{symbol}() {{ return new cmaj_cpp_dsp<{symbol}_processor>(); }}\n",
                            [*flags, f"-D{processor[1]}={symbol}_processor"])
        registration.extend([
            '    add_faust("cmaj-jit", make_cmaj_jit(3));',
            '    add_faust("cmaj-jit-fm", make_cmaj_jit(4));',
        ])
        objects.append(args.out / "cmaj_jit.o")

    cxx_path = dsp_path.with_suffix(".cpp")
    if cxx_path.is_file():
        for engine, flags in (("cxx", strict_flags), ("cxx-fm", fast_flags)):
            symbol = engine.replace("-", "_")
            compile_cpp(engine, "#include <algorithm>\n#include <cmath>\n#include <cstdint>\n#include <cstring>\n"
                        '#include "ext_dsp.h"\n'
                        f'namespace {symbol} {{\n#include "{cxx_path}"\n}}\n'
                        f"dsp* make_{symbol}() {{ return new cxx_dsp<{symbol}::Dsp>(); }}\n", flags)
    (build_dir / "engines.inc").write_text("\n".join([
        f"constexpr int input_count = {ports[0]}, output_count = {ports[1]};",
        *declarations, "template <typename AddFaust>",
        "void add_engines(std::vector<std::unique_ptr<Engine>>& engines, AddFaust add_faust) {",
        *registration, "}", "",
    ]))
    ydsp_path = dsp_path.with_suffix(".ydsp")
    run(*cxx, "-I", build_dir, "-I", args.onda_include, "-I", ROOT / "tools" / "ydsp",
        "-DBENCH_GENERATED_ENGINES", *cmajor_flags, f'-DYDSP_SRC="{ydsp_path if ydsp_path.is_file() else ""}"',
        f'-DFAUSTLIB="{args.faust_libraries}"', f'-DSR48DIR="{fixed_libs}"',
        "-O2", "-ffp-contract=off", ROOT / "harness.cpp", *objects, args.ydsp_lib,
        args.libfaust, f"-Wl,-rpath,{args.libfaust.parent}", "-no-pie", "-pthread",
        "-ldl", "-lrt", "-lz", "-lm", "-o", "bench")
    log.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--upstream", type=Path, default=ROOT, help="benchmark source checkout")
    parser.add_argument("--faust", type=Path, required=True)
    parser.add_argument("--libfaust", type=Path, required=True, help="shared libfaust with LLVM")
    parser.add_argument("--faust-include", type=Path, required=True, help="directory containing faust/")
    parser.add_argument("--faust-libraries", type=Path, required=True, help="directory containing stdfaust.lib")
    parser.add_argument("--cert", type=Path, help="certification-branch Faust (upstream uses cert-7)")
    parser.add_argument("--cmajor", type=Path, help="Cmajor CLI (default: cmaj on PATH when Cmajor is enabled)")
    parser.add_argument("--cmajor-include", type=Path, help="directory containing the cmajor/ and choc/ headers")
    parser.add_argument("--cmajor-library", type=Path, help="libCmajPerformer.so for Cmajor JIT")
    parser.add_argument("--ydsp-lib", type=Path, default=ROOT / "build" / "linux-ydsp" / "libydsp_engine.a")
    parser.add_argument("--onda", type=Path, default=ONDA_RELEASE / "bin" / "onda")
    parser.add_argument("--onda-include", type=Path, default=ONDA_RELEASE / "include")
    parser.add_argument("--cxx", default="clang++")
    parser.add_argument("--objcopy", default="llvm-objcopy")
    parser.add_argument("--out", type=Path, required=True, help="new result directory")
    parser.add_argument("--rounds", type=int, default=21)
    parser.add_argument("--round-ms", type=int, default=40)
    parser.add_argument("--cpu", type=int, default=min(os.sched_getaffinity(0)))
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--ir", action="append", default=[], metavar="WORKLOAD")
    parser.add_argument("--libmvec", action="store_true")
    parser.add_argument("--flush-denormals", action="store_true", help="set FTZ/DAZ for every engine")
    parser.add_argument("--llvm-no-fast", action="store_true", help="Faust LLVM no-fast-math diagnostic run")
    parser.add_argument("workloads", nargs="*", help="default: workloads with all four source ports")
    args = parser.parse_args()
    for key in ("out", "upstream", "faust", "faust_libraries", "libfaust", "faust_include", "cert", "onda",
                "onda_include", "ydsp_lib", "cmajor", "cmajor_include", "cmajor_library"):
        if getattr(args, key):
            setattr(args, key, getattr(args, key).absolute())
    if args.rounds < 1 or args.round_ms < 1 or args.jobs < 1 or args.cpu not in os.sched_getaffinity(0):
        parser.error("invalid round count, round duration, build jobs or CPU selection")
    dsp = args.upstream / "dsp"
    names = args.workloads or sorted(path.stem for path in dsp.glob("*.onda")
                                    if all(path.with_suffix(ext).is_file() for ext in (".dsp", ".cpp", ".ydsp")))
    if not names or any(not all((dsp / f"{name}{ext}").is_file() for ext in (".dsp", ".onda")) for name in names):
        parser.error("expected workloads with Faust and Onda sources")
    if not args.ydsp_lib.is_file():
        parser.error(f"YDSP library missing: {args.ydsp_lib} (see tools/linux/README.md)")
    if not args.onda.is_file() or not (args.onda_include / "onda_processor_abi.h").is_file():
        parser.error("Onda compiler or ABI header missing (see tools/linux/README.md)")
    if bool(args.cmajor_include) != bool(args.cmajor_library) or (args.cmajor and not args.cmajor_library):
        parser.error("--cmajor-include and --cmajor-library must be supplied together")
    if args.cmajor_library and (
        not args.cmajor_library.is_file() or not (args.cmajor_include / "cmajor" / "API" / "cmaj_Engine.h").is_file()
    ):
        parser.error("Cmajor library or headers missing (see tools/linux/README.md)")
    if args.cmajor_library:
        version = command(args.cmajor or "cmaj", "--version", stdout=subprocess.PIPE).stdout.strip()
        print(version, flush=True)
    args.out.mkdir(parents=True, exist_ok=False)
    if args.cmajor_library:
        command(args.cxx, "-std=c++17", "-O2", "-I", args.cmajor_include,
                f'-DCMAJ_DYLIB="{args.cmajor_library}"', "-c", ROOT / "tools" / "cmaj_jit.cpp",
                "-o", args.out / "cmaj_jit.o")
    fixed_libs = args.out / "sr48"
    fixed_rate_libraries(args.faust_libraries, fixed_libs)
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        for name, _ in zip(names, pool.map(lambda name: build(args, name, fixed_libs), names)):
            print(f"Built {name}", flush=True)
    env = os.environ.copy()
    env.pop("FAUST_DEBUG", None)
    env.pop("FAUST_OPT", None)
    if args.llvm_no_fast:
        env["FAUST_OPT"] = "FAUST_LLVM_NO_FM"
    env["BENCH_FLUSH_DENORMALS"] = "1" if args.flush_denormals else "0"
    with (args.out / "results.tsv").open("w", buffering=1) as results:
        results.write(HEADER)
        for name in names:
            print(f"Timing {name} on CPU {args.cpu}", flush=True)
            build_dir = args.out / name
            with (build_dir / "run.log").open("w") as log:
                command("taskset", "-c", str(args.cpu), build_dir / "bench", dsp / f"{name}.dsp",
                        name, str(args.rounds), str(args.round_ms), str(build_dir) if name in args.ir else "",
                        stdout=results, stderr=log, cwd=build_dir, env=env)
            if (build_dir / "run.log").stat().st_size:
                print(f"{name}: engine diagnostics in {build_dir / 'run.log'}", flush=True)


if __name__ == "__main__":
    main()
