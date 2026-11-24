"""Build the Onda variants used by the macOS and Linux benchmark drivers."""

import json
from pathlib import Path
import struct
import subprocess
import sys


def defaults(metadata):
    storage = bytearray(max(1, metadata["runtime"]["param_size_bytes"]))
    formats = {"f32": "f", "f64": "d", "i32": "i", "i64": "q", "bool": "?"}
    byte_order = {"little_endian": "<", "big_endian": ">"}[metadata["target"]["byte_order"]]
    for param in metadata["metadata"]["params"]:
        kind = param["scalar"]
        for index, value in enumerate(param["default_reprs"] or []):
            offset = param["byte_offset"] + index * param["element_size_bytes"]
            number = (value == "true") if kind == "bool" else (float(value) if kind[0] == "f" else int(value))
            struct.pack_into(byte_order + formats[kind], storage, offset, number)
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


def build_onda(run, compiler, objcopy, source, build_dir, *, macho=False, emit_ir=False):
    variants = [("onda", source)]
    explicit = source.with_name(f"{source.stem}_explicit_loops.onda")
    if explicit.is_file():
        variants.append(("onda-block", explicit))
    declarations, registration, objects = [], [], []
    ports = None
    prefix = "_" if macho else ""
    for label, path in variants:
        for fast in (False, True):
            engine = f"{label}-fm" if fast else label
            symbol = engine.replace("-", "_")
            if macho:
                mode = "fast" if fast else "strict"
                stem = f"{label.replace('-', '_')}_{mode}"
            else:
                stem = engine
            options = [compiler, "compile", path, "--target-cpu", "host", "-b", "256", "--sr", "48000"]
            if fast:
                options.append("--fast-math")
            run(*options, "--emit", "obj", "-o", "raw.o", "--meta-out", f"{stem}.json")
            if emit_ir:
                run(*options, "--emit", "llvm-ir", "-o", f"{stem}.ll")
                run(*options, "--emit", "mir", "-o", f"{stem}.mir")
            metadata = json.loads((build_dir / f"{stem}.json").read_text())
            if metadata["abi_version"] != 7 or metadata["metadata"]["buffers"]:
                raise ValueError("expected Onda ABI 7 with no external buffers")
            shape = tuple(port_count(metadata, port) for port in ("inputs", "outputs"))
            if ports is not None and ports != shape:
                raise ValueError("Onda variants have different port counts")
            ports = shape
            exports = metadata["exports"]
            renamed = [f"{prefix}{exports[key]}={prefix}{symbol}_{key}" for key in ("init", "process")]
            # Keep event symbols unique when multiple objects expose the same event index.
            events = exports["events"] + exports.get("event_views", [])
            renamed.extend(f"{prefix}{name}={prefix}{symbol}_{name}" for name in events)
            obj = f"{stem}.o"
            run(objcopy, *(f"--redefine-sym={item}" for item in renamed), "raw.o", obj)
            objects.append(obj)
            declarations.extend([
                f'extern "C" decltype(onda_processor_init) {symbol}_init;',
                f'extern "C" decltype(onda_process) {symbol}_process;',
                f"const unsigned char {symbol}_defaults[] = {{{defaults(metadata)}}};",
            ])
            rt = metadata["runtime"]
            registration.append(
                f'    engines.push_back(std::make_unique<OndaEngine>("{engine}", {symbol}_init, {symbol}_process, '
                f'{rt["state_size_bytes"]}, {rt["state_align_bytes"]}, {rt["param_align_bytes"]}, '
                f'{symbol}_defaults, {rt["param_size_bytes"]}));'
            )
    return ports, declarations, registration, objects


def main():
    compiler, objcopy, source = sys.argv[1:]

    def run(*argv):
        subprocess.run(argv, stdout=sys.stderr, check=True)

    ports, declarations, registration, _ = build_onda(run, compiler, objcopy, Path(source), Path.cwd(), macho=True)
    print("\n".join([
        f"constexpr int input_count = {ports[0]}, output_count = {ports[1]};",
        *declarations,
        "static void add_onda_engines(std::vector<std::unique_ptr<Engine>>& engines) {",
        *registration, "}",
    ]))


if __name__ == "__main__":
    main()
