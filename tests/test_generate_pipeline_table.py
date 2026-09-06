#!/usr/bin/env python3
"""Fail-closed tests for the manifest-driven pipeline table."""

import hashlib
import importlib.util
import json
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.dont_write_bytecode = True
SPEC = importlib.util.spec_from_file_location(
    "generate_pipeline_table", ROOT / "tools/generate_pipeline_table.py"
)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def manifest(name: str, source: str) -> dict[str, object]:
    path = ROOT / source
    return {
        "schema": 1,
        "name": name,
        "target": "gfx1013",
        "source": source,
        "source_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "no_relocations": True,
        "stages": {
            "pre_raster_gs": {"bytes": 64},
            "pixel": {"bytes": 32},
        },
        "hardware_stages": {
            "pre_raster_gs": {"user_sgprs": 5},
            "pixel": {"user_sgprs": 2},
        },
    }


def main() -> int:
    specification = {
        "schema": 1,
        "target": "gfx1013",
        "pipelines": [
            {"id": "bsp_resource", "pipe": "shaders/bsp_resource.pipe",
             "gs_application_words": 2, "ps_application_words": 1},
            {"id": "bsp_alpha_test", "pipe": "shaders/bsp_alpha_test.pipe",
             "gs_application_words": 2, "ps_application_words": 1},
            {"id": "bsp_sky", "pipe": "shaders/bsp_sky.pipe",
             "gs_application_words": 2, "ps_application_words": 1},
            {"id": "bsp_overlay", "pipe": "shaders/bsp_overlay.pipe",
             "gs_application_words": 1, "ps_application_words": 0},
        ],
    }
    with tempfile.TemporaryDirectory(prefix="pipeline-table-") as directory:
        manifests = Path(directory)
        for entry in specification["pipelines"]:
            value = manifest(entry["id"], entry["pipe"])
            (manifests / f"{entry['id']}.manifest.json").write_text(
                json.dumps(value), encoding="utf-8"
            )
        header = MODULE.render(specification, manifests)
        assert "PS5_PIPELINE_PERMUTATION_COUNT = 4" in header
        assert all(name in header for name in (
            '"bsp_resource"', '"bsp_alpha_test"', '"bsp_sky"',
            '"bsp_overlay"'
        ))
        specification["pipelines"][3]["id"] = "bsp_resource"
        try:
            MODULE.render(specification, manifests)
        except SystemExit:
            pass
        else:
            raise AssertionError("duplicate pipeline id must fail")
    print("pipeline permutation table tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
