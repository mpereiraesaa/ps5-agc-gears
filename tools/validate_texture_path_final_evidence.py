#!/usr/bin/env python3
"""Fail-closed validation for the final 60,000-frame Phase 3 soak."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from validate_texture_path_alpha_evidence import EvidenceError, fail, number, one, transcript
from validate_texture_path_accounting_evidence import validate as validate_accounting


FRAMES = 60_000
REASON = "bsp-texture-path-final-soak-complete"


def validate(path: Path, *, bundle_sha256: str,
             bundle_bytes: int) -> dict[str, object]:
    summary = validate_accounting(
        path, bundle_sha256=bundle_sha256, bundle_bytes=bundle_bytes,
        expected_frames=FRAMES, expected_slice="final",
        expected_reason=REASON, expected_loop_mode="texture-path-final-soak",
    )
    _, messages, _ = transcript(path.resolve(), reason=REASON)
    features = one(messages, "TEXTURE_FEATURES_READY")
    final = one(messages, "BSP_TEXTURE_PATH_FINAL_COMPLETE")
    if final.get("schema") != "1" or \
            number(final, "frames") != FRAMES or \
            number(final, "mip_chains") != number(features, "mip_chains") or \
            number(final, "opaque_draws") != number(features, "opaque_draws") or \
            number(final, "alpha_draws") != number(features, "alpha_draws") or \
            number(final, "sky_draws") != number(features, "sky_draws") or \
            number(final, "pipelines") != 4 or \
            final.get("sampler") != "anisotropic4x" or \
            final.get("dynamic_lightmap") != "bounded" or \
            number(final, "pool_resident_bytes") != \
                summary["pool_resident_bytes"] or \
            number(final, "texture_payload_bytes") != \
                summary["texture_payload_bytes"] or \
            number(final, "upload_bytes_total") != \
                summary["upload_bytes_total"] or \
            final.get("sequence_hash") != summary["sequence_hash"] or \
            final.get("readback") != "gpu-visible" or \
            final.get("input_dependency") != "none" or \
            final.get("tokens") != "exact" or \
            final.get("guards") != "intact" or \
            number(final, "errors") != 0:
        fail("final Phase 3 completion contract mismatch")
    summary.update({
        "mip_chains": number(final, "mip_chains"),
        "opaque_draws": number(final, "opaque_draws"),
        "alpha_draws": number(final, "alpha_draws"),
        "sky_draws": number(final, "sky_draws"),
    })
    return summary


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest", type=Path)
    parser.add_argument("--bundle-sha256", required=True)
    parser.add_argument("--bundle-bytes", required=True, type=int)
    args = parser.parse_args()
    try:
        if len(args.bundle_sha256) != 64 or any(
            char not in "0123456789abcdef" for char in args.bundle_sha256
        ):
            fail("invalid expected bundle SHA-256")
        print(json.dumps(validate(
            args.manifest, bundle_sha256=args.bundle_sha256,
            bundle_bytes=args.bundle_bytes,
        ), sort_keys=True))
    except EvidenceError as exc:
        raise SystemExit(f"final texture-path evidence rejected: {exc}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
