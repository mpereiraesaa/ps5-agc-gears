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
    assert "PS5_GEARS_DRAW_MODIFIER" in source
    print("AGC metadata translator unit tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
