#!/usr/bin/env python3
"""Compile the project-owned LLPC pipe and extract its two gfx1013 stages."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import subprocess
from pathlib import Path

try:
    import yaml
except ImportError as error:
    raise SystemExit("PyYAML is required to decode PAL metadata") from error


ROOT = Path(__file__).resolve().parents[1]
TARGET = "gfx1013"
GFXIP = "10.1.3"
PUBLIC_NAME = re.compile(r"^[a-z][a-z0-9_]*$")


class PalMetadataLoader(yaml.SafeLoader):
    """Safe YAML loader with LLVM's scalar spelling tag."""


PalMetadataLoader.add_constructor(
    "!str", lambda loader, node: loader.construct_scalar(node)
)


def run(arguments: list[str], *, cwd: Path = ROOT) -> str:
    return subprocess.run(
        arguments, cwd=cwd, check=True, text=True, capture_output=True
    ).stdout


def symbol_extent(symbols: str, name: str) -> tuple[int, int]:
    match = re.search(
        rf"^\s*\d+:\s+([0-9a-fA-F]+)\s+(\d+)\s+FUNC.*\s{name}$",
        symbols,
        re.MULTILINE,
    )
    if match is None:
        raise ValueError(f"missing PAL shader symbol: {name}")
    return int(match.group(1), 16), int(match.group(2))


def checked_tool(path: Path, name: str) -> Path:
    if not path.is_file() or not os.access(path, os.X_OK):
        raise SystemExit(f"missing executable {name}: {path}")
    return path.resolve()


def checked_name(value: str) -> str:
    if not PUBLIC_NAME.fullmatch(value):
        raise SystemExit(f"invalid shader output name: {value}")
    return value


def decode_pal_metadata(notes: str) -> dict[str, object]:
    match = re.search(r"AMDGPU Metadata: ---\n(.*?)\n\.\.\.", notes, re.DOTALL)
    if match is None:
        raise ValueError("PAL metadata YAML was not decoded")
    metadata = yaml.load(match.group(1), Loader=PalMetadataLoader)
    pipeline = metadata["amdpal.pipelines"][0]
    stages = pipeline[".hardware_stages"]

    def stage(name: str) -> dict[str, object]:
        value = stages[name]
        return {
            "entry_point_symbol": value[".entry_point_symbol"],
            "sgpr_count": value[".sgpr_count"],
            "user_sgprs": value[".user_sgprs"],
            "vgpr_count": value[".vgpr_count"],
            "wavefront_size": value[".wavefront_size"],
            "wgp_mode": value.get(".wgp_mode", False),
            "scratch_enabled": value[".scratch_en"],
            "user_data_reg_map": value[".user_data_reg_map"],
        }

    return {
        "pipeline_type": pipeline[".type"],
        "hardware_stages": {
            "pre_raster_gs": stage(".gs"),
            "pixel": stage(".ps"),
        },
        "graphics_register_metadata": pipeline[".graphics_registers"],
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--pipe", type=Path, default=ROOT / "shaders/gears_lit.pipe")
    parser.add_argument("--name", help="stable output stem; defaults to pipe stem")
    parser.add_argument("--output-dir", type=Path, default=ROOT / "build/shaders")
    parser.add_argument("--amdllpc", type=Path, required=True)
    parser.add_argument("--readelf", type=Path, required=True)
    parser.add_argument("--objcopy", type=Path, default=Path("/usr/bin/llvm-objcopy"))
    args = parser.parse_args()

    pipe = args.pipe.resolve()
    if not pipe.is_file() or ROOT not in pipe.parents:
        raise SystemExit("shader input must be a file inside this repository")
    amdllpc = checked_tool(args.amdllpc, "amdllpc")
    readelf = checked_tool(args.readelf, "llvm-readelf")
    objcopy = checked_tool(args.objcopy, "llvm-objcopy")
    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    name = checked_name(args.name or pipe.stem)
    elf = output_dir / f"{name}.pal.elf"
    text_path = output_dir / f"{name}.text.bin"
    vertex_path = output_dir / f"{name}.gs.bin"
    pixel_path = output_dir / f"{name}.ps.bin"
    manifest_path = output_dir / f"{name}.manifest.json"

    run([str(amdllpc), f"-gfxip={GFXIP}", f"-o={elf}", str(pipe)])
    run([str(objcopy), f"--dump-section=.text={text_path}", str(elf)])
    header = run([str(readelf), "--file-header", str(elf)])
    sections = run(
        [str(readelf), "--sections", "--elf-output-style=GNU", str(elf)]
    )
    symbols = run(
        [str(readelf), "--symbols", "--elf-output-style=GNU", str(elf)]
    )
    notes = run(
        [str(readelf), "--notes", "--elf-output-style=LLVM", str(elf)]
    )
    if "AMDGPU" not in header or "0x42" not in header:
        raise SystemExit("compiler output is not the expected gfx1013 PAL ELF")
    if re.search(r"\]\s+\.rela?(?:\.|\s)", sections):
        raise SystemExit("shader ELF contains unresolved relocations")

    text = text_path.read_bytes()
    stages: dict[str, dict[str, object]] = {}
    for public_name, symbol, destination in (
        ("pre_raster_gs", "_amdgpu_gs_main", vertex_path),
        ("pixel", "_amdgpu_ps_main", pixel_path),
    ):
        offset, size = symbol_extent(symbols, symbol)
        if size <= 0 or offset + size > len(text):
            raise SystemExit(f"PAL symbol extent exceeds .text: {symbol}")
        blob = text[offset : offset + size]
        destination.write_bytes(blob)
        stages[public_name] = {
            "symbol": symbol,
            "offset": offset,
            "bytes": size,
            "sha256": hashlib.sha256(blob).hexdigest(),
        }

    pal = decode_pal_metadata(notes)
    manifest = {
        "schema": 1,
        "name": name,
        "target": TARGET,
        "gfxip": GFXIP,
        "source": pipe.relative_to(ROOT).as_posix(),
        "source_sha256": hashlib.sha256(pipe.read_bytes()).hexdigest(),
        "pal_elf_sha256": hashlib.sha256(elf.read_bytes()).hexdigest(),
        "elf_machine_flags": "0x42",
        "no_relocations": True,
        "stages": stages,
        **pal,
        "generated_files_are_not_source": True,
    }
    manifest_path.write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(manifest_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
