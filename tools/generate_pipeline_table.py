#!/usr/bin/env python3
"""Generate the checked two-pipeline runtime table from LLPC manifests."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
IDENTIFIER = re.compile(r"^[a-z][a-z0-9_]*$")


def fail(message: str) -> None:
    raise SystemExit(f"pipeline table: {message}")


def load(path: Path) -> dict[str, object]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        fail(f"object expected: {path}")
    return value


def render(spec: dict[str, object], manifest_dir: Path) -> str:
    if spec.get("schema") != 1 or spec.get("target") != "gfx1013":
        fail("unsupported permutation schema/target")
    entries = spec.get("pipelines")
    if not isinstance(entries, list) or len(entries) < 2:
        fail("at least two pipeline permutations are required")
    seen: set[str] = set()
    rows: list[str] = []
    enums: list[str] = []
    for index, entry in enumerate(entries):
        if not isinstance(entry, dict):
            fail("pipeline entry must be an object")
        name = entry.get("id")
        if not isinstance(name, str) or not IDENTIFIER.fullmatch(name) or name in seen:
            fail(f"invalid or duplicate pipeline id: {name}")
        seen.add(name)
        pipe = ROOT / str(entry.get("pipe", ""))
        if not pipe.is_file() or ROOT not in pipe.resolve().parents:
            fail(f"pipeline source is outside repository: {pipe}")
        manifest_path = manifest_dir / f"{name}.manifest.json"
        manifest = load(manifest_path)
        if (manifest.get("schema"), manifest.get("name"),
                manifest.get("target"), manifest.get("source"),
                manifest.get("no_relocations")) != (
                    1, name, "gfx1013", pipe.relative_to(ROOT).as_posix(), True):
            fail(f"manifest identity mismatch: {name}")
        if manifest.get("source_sha256") != hashlib.sha256(pipe.read_bytes()).hexdigest():
            fail(f"stale source hash: {name}")
        stages = manifest["stages"]
        hardware = manifest["hardware_stages"]
        gs_words = int(entry.get("gs_application_words", -1))
        ps_words = int(entry.get("ps_application_words", -1))
        if not 0 <= gs_words <= 24 or not 0 <= ps_words <= 24:
            fail(f"application word budget exceeded: {name}")
        macro = name.upper()
        enums.append(f"    PS5_PIPELINE_{macro} = {index},")
        rows.append(
            "    {\n"
            f'        .name = "{name}",\n'
            f"        .gs_isa_bytes = {int(stages['pre_raster_gs']['bytes'])}u,\n"
            f"        .ps_isa_bytes = {int(stages['pixel']['bytes'])}u,\n"
            f"        .gs_user_sgprs = {int(hardware['pre_raster_gs']['user_sgprs'])}u,\n"
            f"        .ps_user_sgprs = {int(hardware['pixel']['user_sgprs'])}u,\n"
            f"        .gs_application_words = {gs_words}u,\n"
            f"        .ps_application_words = {ps_words}u,\n"
            f'        .source_sha256 = "{manifest["source_sha256"]}",\n'
            "    },"
        )
    return """#ifndef PS5_AGC_PIPELINE_PERMUTATIONS_H
#define PS5_AGC_PIPELINE_PERMUTATIONS_H

#include <stdint.h>

typedef struct Ps5PipelinePermutationMetadata {
    const char *name;
    uint32_t gs_isa_bytes;
    uint32_t ps_isa_bytes;
    uint32_t gs_user_sgprs;
    uint32_t ps_user_sgprs;
    uint32_t gs_application_words;
    uint32_t ps_application_words;
    const char *source_sha256;
} Ps5PipelinePermutationMetadata;

enum ps5_pipeline_permutation_id {
""" + "\n".join(enums) + f"\n    PS5_PIPELINE_PERMUTATION_COUNT = {len(entries)}\n}};\n\n" + \
        "static const Ps5PipelinePermutationMetadata ps5_pipeline_permutations[] = {\n" + \
        "\n".join(rows) + "\n};\n\n#endif\n"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--spec", type=Path,
                        default=ROOT / "shaders/pipeline_permutations.json")
    parser.add_argument("--manifest-dir", type=Path,
                        default=ROOT / "build/shaders")
    parser.add_argument("--output", type=Path,
                        default=ROOT / "build/generated/pipeline_permutations.h")
    args = parser.parse_args()
    output = args.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(render(load(args.spec.resolve()),
                             args.manifest_dir.resolve()), encoding="utf-8")
    print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
