#!/usr/bin/env python3
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "generate_bsp_build_metadata", ROOT / "tools/generate_bsp_build_metadata.py"
)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def main() -> int:
    header = MODULE.render(b"deterministic-private-bundle")
    assert "PS5_BSP_BUNDLE_BYTES 28u" in header
    assert (
        'PS5_BSP_BUNDLE_SHA256 "f17de1b2a38040dc425382639ad5125dacca50eb'
        '429a47071091639a0b8368aa"'
    ) in header
    try:
        MODULE.render(b"")
    except ValueError:
        pass
    else:
        raise AssertionError("empty bundle must fail")
    print("BSP private-build metadata tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
