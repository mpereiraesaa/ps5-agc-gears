#!/usr/bin/env python3
"""Small fail-closed regressions for the PAL-to-AGC translator."""

import importlib.util
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.dont_write_bytecode = True
SPEC = importlib.util.spec_from_file_location(
    "generate_agc_metadata", ROOT / "tools/generate_agc_metadata.py"
)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def main() -> int:
    assert MODULE.pack([4, 0, 0, 0], 4) == 4
    assert MODULE.pack([15, 0, 0, 0], 4) == 15
    assert MODULE.bit(False) == 0 and MODULE.bit(True) == 1
    for manifest in (
        {},
        {"target": "gfx1013", "pipeline_type": "Ngg",
         "no_relocations": False},
    ):
        try:
            MODULE.derive(manifest)
        except ValueError:
            pass
        else:
            raise AssertionError("invalid PAL contract must fail")
    source = (ROOT / "tools/generate_agc_metadata.py").read_text()
    assert '("gfx1013", "Ngg", True)' in source
    sample = {
        "gs_isa_bytes": 1, "ps_isa_bytes": 2, "gs_rsrc1": 3,
        "gs_rsrc2": 4, "ps_rsrc1": 5, "ps_rsrc2": 6, "ge_cntl": 7,
        "shader_stages_en": 8, "draw_modifier": 5,
        "pre_raster_cx": [(1, 2)], "pixel_cx": [(3, 4)],
    }
    header = MODULE.render_header(sample, "BSP_FLAT", "ps5_bsp_flat")
    assert "PS5_BSP_FLAT_DRAW_MODIFIER" in header
    assert "ps5_bsp_flat_pre_raster_cx" in header
    default_header = MODULE.render_header(sample)
    assert "PS5_GEARS_DRAW_MODIFIER" in default_header
    try:
        MODULE.render_header(sample, "../BAD", "ps5_bsp_flat")
    except ValueError:
        pass
    else:
        raise AssertionError("unsafe metadata prefix must fail")
    print("AGC metadata translator unit tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
