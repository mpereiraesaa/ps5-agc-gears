#!/usr/bin/env python3
"""Generate private-build metadata without copying bundle bytes into source."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path


def render(bundle: bytes) -> str:
    if not bundle:
        raise ValueError("empty BSP bundle")
    digest = hashlib.sha256(bundle).hexdigest()
    return f"""#ifndef PS5_BSP_BUILD_METADATA_H
#define PS5_BSP_BUILD_METADATA_H
#define PS5_BSP_BUNDLE_BYTES {len(bundle)}u
#define PS5_BSP_BUNDLE_SHA256 \"{digest}\"
#endif
"""


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--bundle", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    content = args.bundle.read_bytes()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(render(content), encoding="utf-8")
    print(args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
