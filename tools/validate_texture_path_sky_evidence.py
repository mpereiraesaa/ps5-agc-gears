#!/usr/bin/env python3
"""Fail-closed validation for the Phase 3 separate-sky PS5 gate."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

from validate_texture_path_alpha_evidence import (
    EvidenceError, HEX16, fail, many, number, one, transcript,
)


def validate(path: Path, *, bundle_sha256: str,
             bundle_bytes: int) -> dict[str, object]:
    manifest, messages, data = transcript(
        path.resolve(), reason="bsp-texture-path-sky-soak-complete"
    )
    boot = one(messages, "BSP_TEXTURE_PATH_BOOT")
    if boot.get("schema") != "1" or boot.get("slice") != "sky" or \
            boot.get("target") != "gfx1013" or \
            number(boot, "transient_slots") != 2 or \
            boot.get("ownership") != "fence+videoout" or \
            boot.get("bundle_sha256") != bundle_sha256 or \
            number(boot, "bundle_bytes") != bundle_bytes or \
            number(boot, "soak_frames") != 10_000:
        fail("sky boot contract mismatch")

    chains = one(messages, "MIP_CHAINS_READY")
    texture_count = number(chains, "textures")
    if texture_count <= 0 or chains.get("layout") != "addr-sw-linear" or \
            chains.get("order") != "smallest-to-base" or \
            number(chains, "pitch_alignment") != 256 or \
            number(chains, "levels_min") < 2 or \
            number(chains, "levels_max") > 15 or \
            number(chains, "chain_bytes") <= 0 or \
            chains.get("deterministic") != "box-rne":
        fail("inherited mip-chain contract mismatch")
    tables = one(messages, "BSP_TEXTURE_TABLE_LAYOUT")
    if number(tables, "textures") != texture_count or \
            number(tables, "descriptor_dwords") != texture_count * 24 or \
            tables.get("base_filter") != "anisotropic4x" or \
            tables.get("lightmap_filter") != "bilinear":
        fail("fixed sampler/table contract mismatch")
    pipelines = one(messages, "RESOURCE_PIPELINES_READY")
    if number(pipelines, "count") != 4 or \
            pipelines.get("map") != "bsp_resource" or \
            pipelines.get("alpha_test") != "bsp_alpha_test" or \
            pipelines.get("sky") != "bsp_sky" or \
            pipelines.get("overlay") != "bsp_overlay":
        fail("four-pipeline permutation contract mismatch")

    ready = one(messages, "SKY_PASS_READY")
    sky_textures = number(ready, "textures")
    sky_draws = number(ready, "draws")
    opaque_draws = number(ready, "opaque_draws")
    alpha_draws = number(ready, "alpha_draws")
    if sky_textures <= 0 or sky_draws <= 0 or opaque_draws <= 0 or \
            sky_draws + opaque_draws + alpha_draws <= sky_draws or \
            ready.get("camera") != "nearest-centroid-standoff" or \
            ready.get("pipeline") != "bsp_sky" or \
            ready.get("composition") != "base-unlit" or \
            ready.get("depth_write") != "enabled" or \
            ready.get("blend") != "disabled" or \
            number(ready, "opaque_ps_bytes") <= 0 or \
            number(ready, "sky_ps_bytes") <= 0 or \
            number(ready, "opaque_ps_bytes") == number(ready, "sky_ps_bytes") or \
            ready.get("shader_distinct") != "true":
        fail("sky classification/shader contract mismatch")

    loop = one(messages, "BSP_LOOP_BEGIN")
    if loop.get("mode") != "texture-path-sky-soak" or \
            number(loop, "buffers") != 2 or number(loop, "frames") != 10_000 or \
            loop.get("opaque_pass") != "separate" or \
            loop.get("alpha_test_pass") != "separate" or \
            loop.get("sky_pass") != "separate" or \
            loop.get("sampler") != "anisotropic4x" or \
            loop.get("lightmap_pattern") != "paired" or \
            loop.get("retirement") != "fence+videoout":
        fail("sky loop contract mismatch")

    sky_rows = many(messages, "SKY_PASS_FRAME")
    if [number(row, "frame") for row in sky_rows] != [0, 9_998, 9_999] or \
            [row.get("mode") for row in sky_rows] != \
            ["sky-pass", "skip-control", "sky-pass"] or \
            [number(row, "sky_draws") for row in sky_rows] != \
            [sky_draws, 0, sky_draws] or \
            [number(row, "lightmap_pattern") for row in sky_rows] != [0, 1, 1]:
        fail("sky isolation frame sequence mismatch")
    for row in sky_rows:
        if number(row, "expected_sky_draws") != sky_draws or \
                row.get("sampler") != "anisotropic4x":
            fail("sky draw classification changed during soak")

    light_rows = many(messages, "DYNAMIC_LIGHTMAP_FRAME")
    if [number(row, "frame") for row in light_rows] != [0, 1, 9_998, 9_999] or \
            [number(row, "pattern") for row in light_rows] != [0, 0, 1, 1]:
        fail("paired lightmap isolation mismatch")
    for prefix in ("RESOURCE_FRAME_READY", "RESOURCE_FRAME_SEALED",
                   "RESOURCE_FRAME_SUBMITTED", "RESOURCE_FRAME_RETIRED",
                   "BSP_VIDEOOUT_TOKEN"):
        if [number(row, "frame") for row in many(messages, prefix)] != [0, 9_999]:
            fail(f"{prefix} bookends mismatch")
    sealed = many(messages, "RESOURCE_FRAME_SEALED")
    submitted = many(messages, "RESOURCE_FRAME_SUBMITTED")
    retired = many(messages, "RESOURCE_FRAME_RETIRED")
    video = many(messages, "BSP_VIDEOOUT_TOKEN")
    for index in range(2):
        token = number(sealed[index], "token")
        if token != number(submitted[index], "token") or \
                token != number(retired[index], "token") or \
                retired[index].get("fence") != "zero" or \
                retired[index].get("videoout_token") != "exact" or \
                number(video[index], "expected") != number(video[index], "observed"):
            fail("exact fence/VideoOut retirement mismatch")
    terminal = [row for row in many(messages, "BSP_FRAME")
                if row.get("frame") == "9999"]
    if len(terminal) != 1 or number(terminal[0], "completed") != 10_000 or \
            number(terminal[0], "present_interval_over_budget") != 0 or \
            number(terminal[0], "errors") != 0:
        fail("terminal telemetry is not clean")
    ring = one(messages, "RESOURCE_RING_RETIRED")
    if ring.get("reusable") != "true" or ring.get("tokens") != "exact":
        fail("transient ring retirement mismatch")
    dynamic = one(messages, "DYNAMIC_LIGHTMAP_READBACK")
    if dynamic.get("slot0") != dynamic.get("slot1") or \
            dynamic.get("slots_equal") != "true" or \
            number(dynamic, "final_pattern") != 1 or \
            dynamic.get("surrounding") != "stable" or \
            dynamic.get("guards") != "intact":
        fail("final paired lightmap mismatch")

    readback = one(messages, "SKY_PASS_READBACK")
    control_hash = readback.get("skip_control_buffer", "")
    sky_hash = readback.get("sky_pass_buffer", "")
    if not HEX16.fullmatch(control_hash) or not HEX16.fullmatch(sky_hash) or \
            control_hash == sky_hash or readback.get("paired") != "true" or \
            readback.get("framebuffer_distinct") != "true" or \
            readback.get("sampler") != "anisotropic4x" or \
            readback.get("guards") != "intact" or \
            number(readback, "frames") != 10_000:
        fail("GPU-visible sky-pass readback mismatch")
    resource_readback = one(messages, "BSP_RESOURCE_READBACK")
    if resource_readback.get("buffer0") != control_hash or \
            resource_readback.get("buffer1") != sky_hash:
        fail("sky readback does not match terminal buffers")
    complete = one(messages, "BSP_TEXTURE_PATH_SKY_COMPLETE")
    if number(complete, "frames") != 10_000 or \
            number(complete, "textures") != sky_textures or \
            number(complete, "draws") != sky_draws or \
            complete.get("pipeline") != "separate" or \
            complete.get("skip_control") != "gpu-visible" or \
            complete.get("sky_pass") != "gpu-visible" or \
            complete.get("paired_lightmap") != "true" or \
            complete.get("tokens") != "exact" or \
            complete.get("guards") != "intact" or number(complete, "errors") != 0:
        fail("sky completion contract mismatch")
    resource = one(messages, "BSP_RESOURCE_SOAK_COMPLETE")
    if number(resource, "frames") != 10_000 or \
            number(resource, "connected_frames") != 10_000 or \
            number(resource, "read_errors") != 0 or \
            number(resource, "pipelines") != 4 or \
            resource.get("tokens") != "exact" or \
            resource.get("guards") != "intact" or number(resource, "errors") != 0:
        fail("resource completion contract mismatch")
    pool = one(messages, "RESOURCE_POOL_RETIRED")
    if number(pool, "reclaimed") != 6 or pool.get("completion") != "fence+videoout":
        fail("resource pool reclamation mismatch")
    return {
        "run_id": manifest.get("run_id"), "records": manifest.get("records"),
        "frames": 10_000, "textures": sky_textures, "draws": sky_draws,
        "skip_control_buffer": control_hash, "sky_pass_buffer": sky_hash,
        "log_sha256": hashlib.sha256(data).hexdigest(),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest", type=Path)
    parser.add_argument("--bundle-sha256", required=True)
    parser.add_argument("--bundle-bytes", required=True, type=int)
    args = parser.parse_args()
    try:
        if len(args.bundle_sha256) != 64 or \
                any(char not in "0123456789abcdef" for char in args.bundle_sha256):
            fail("invalid expected bundle SHA-256")
        print(json.dumps(validate(
            args.manifest, bundle_sha256=args.bundle_sha256,
            bundle_bytes=args.bundle_bytes,
        ), sort_keys=True))
    except EvidenceError as exc:
        raise SystemExit(f"evidence rejected: {exc}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
