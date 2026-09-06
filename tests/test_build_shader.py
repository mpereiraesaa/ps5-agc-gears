#!/usr/bin/env python3
"""Unit tests for parsing and source-level safety of build_shader.py."""

import importlib.util
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.dont_write_bytecode = True
SPEC = importlib.util.spec_from_file_location(
    "build_shader", ROOT / "tools/build_shader.py"
)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def main() -> int:
    symbols = """
       7: 0000000000000040 128 FUNC GLOBAL DEFAULT 2 _amdgpu_gs_main
       8: 0000000000000180  96 FUNC GLOBAL DEFAULT 2 _amdgpu_ps_main
    """
    assert MODULE.symbol_extent(symbols, "_amdgpu_gs_main") == (0x40, 128)
    assert MODULE.symbol_extent(symbols, "_amdgpu_ps_main") == (0x180, 96)
    assert not MODULE.re.search(r"\]\s+\.rela?(?:\.|\s)",
                                "[ 2] .text PROGBITS")
    assert MODULE.re.search(r"\]\s+\.rela?(?:\.|\s)",
                            "[ 3] .rela.text RELA")
    try:
        MODULE.decode_pal_metadata("not PAL metadata")
    except ValueError:
        pass
    else:
        raise AssertionError("missing PAL notes must fail")
    try:
        MODULE.symbol_extent(symbols, "missing")
    except ValueError:
        pass
    else:
        raise AssertionError("missing shader symbols must fail")
    source = (ROOT / "tools/build_shader.py").read_text(encoding="utf-8")
    assert 'TARGET = "gfx1013"' in source
    assert 'GFXIP = "10.1.3"' in source
    assert "gfx" + "1030" not in source
    print("shader build-tool unit tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
