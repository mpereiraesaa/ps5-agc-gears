#!/usr/bin/env python3
"""Fail-closed validation for Phase 3 texture residency/upload accounting."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

from validate_texture_path_alpha_evidence import (
    EvidenceError, HEX16, fail, many, number, one, transcript,
)


def validate(path: Path, *, bundle_sha256: str,
             bundle_bytes: int, expected_frames: int = 10_000,
             expected_slice: str = "accounting",
             expected_reason: str =
                 "bsp-texture-path-accounting-soak-complete",
             expected_loop_mode: str =
                 "texture-path-accounting-soak") -> dict[str, object]:
    if expected_frames < 4:
        fail("accounting evidence requires at least four frames")
    sampled_frames = [0, 1, expected_frames - 2, expected_frames - 1]
    manifest, messages, data = transcript(
        path.resolve(), reason=expected_reason
    )
    boot = one(messages, "BSP_TEXTURE_PATH_BOOT")
    if boot.get("schema") != "1" or \
            boot.get("slice") != expected_slice or \
            boot.get("target") != "gfx1013" or \
            number(boot, "transient_slots") != 2 or \
            boot.get("ownership") != "fence+videoout" or \
            boot.get("bundle_sha256") != bundle_sha256 or \
            number(boot, "bundle_bytes") != bundle_bytes or \
            number(boot, "soak_frames") != expected_frames or \
            boot.get("input_gate") != "not-repeated":
        fail("accounting boot contract mismatch")
    pad = one(messages, "BSP_NOCLIP_PAD_READY")
    if pad.get("sticks") != "dual" or pad.get("triggers") != "vertical" or \
            pad.get("connected_required") != "false":
        fail("texture-only input-independence contract mismatch")

    residency = one(messages, "TEXTURE_RESIDENCY_READY")
    if residency.get("schema") != "1" or \
            number(residency, "lightmap_slots") != 2 or \
            number(residency, "allocations") != 6 or \
            residency.get("accounting") != "exact":
        fail("residency schema/ownership mismatch")
    resident = number(residency, "pool_resident_bytes")
    capacity = number(residency, "pool_capacity_bytes")
    allocation_total = sum(number(residency, key) for key in (
        "bsp_allocation_bytes", "shader_allocation_bytes",
        "depth_allocation_bytes", "transient_allocation_bytes",
        "lightmap_allocation_bytes",
    ))
    texture_payload = number(residency, "texture_payload_bytes")
    payload_total = sum(number(residency, key) for key in (
        "base_texture_bytes", "source_lightmap_bytes",
        "dynamic_lightmap_image_bytes",
    ))
    if resident <= 0 or capacity < resident or resident != allocation_total or \
            texture_payload <= 0 or texture_payload != payload_total or \
            texture_payload > resident:
        fail("resident-byte partition mismatch")

    dynamic_ready = one(messages, "DYNAMIC_LIGHTMAP_READY")
    image_bytes = number(dynamic_ready, "image_bytes")
    patch_bytes = number(dynamic_ready, "patch_bytes")
    if image_bytes <= patch_bytes or patch_bytes != 8 * 8 * 4 or \
            number(dynamic_ready, "slots") != 2 or \
            number(dynamic_ready, "guards") != 256 or \
            number(residency, "source_lightmap_bytes") != image_bytes or \
            number(residency, "dynamic_lightmap_image_bytes") != \
                image_bytes * 2 or \
            number(residency, "lightmap_allocation_bytes") != \
                (image_bytes + 512) * 2:
        fail("lightmap residency partition mismatch")

    chains = one(messages, "MIP_CHAINS_READY")
    texture_count = number(chains, "textures")
    if texture_count <= 0 or chains.get("layout") != "addr-sw-linear" or \
            chains.get("order") != "smallest-to-base" or \
            number(chains, "pitch_alignment") != 256 or \
            number(chains, "chain_bytes") != \
                number(residency, "base_texture_bytes") or \
            chains.get("deterministic") != "box-rne":
        fail("mip residency identity mismatch")
    pipelines = one(messages, "RESOURCE_PIPELINES_READY")
    if number(pipelines, "count") != 4 or \
            [pipelines.get(key) for key in
             ("map", "alpha_test", "sky", "overlay")] != \
            ["bsp_resource", "bsp_alpha_test", "bsp_sky", "bsp_overlay"]:
        fail("inherited pipeline contract mismatch")
    features = one(messages, "TEXTURE_FEATURES_READY")
    opaque = number(features, "opaque_draws")
    alpha = number(features, "alpha_draws")
    sky = number(features, "sky_draws")
    if number(features, "mip_chains") != texture_count or \
            min(opaque, alpha, sky) <= 0 or \
            opaque + alpha + sky != number(features, "total_draws") or \
            number(features, "pipelines") != 4 or \
            features.get("composition") != "opaque+alpha+sky" or \
            features.get("sampler") != "anisotropic4x":
        fail("inherited texture-feature accounting mismatch")

    loop = one(messages, "BSP_LOOP_BEGIN")
    if loop.get("mode") != expected_loop_mode or \
            number(loop, "buffers") != 2 or \
            number(loop, "frames") != expected_frames or \
            loop.get("opaque_pass") != "separate" or \
            loop.get("alpha_test_pass") != "separate" or \
            loop.get("sky_pass") != "separate" or \
            loop.get("sampler") != "anisotropic4x" or \
            loop.get("dynamic_lightmap") != "bounded" or \
            loop.get("residency") != "partitioned" or \
            loop.get("uploads") != "checked-per-frame" or \
            loop.get("input_dependency") != "none" or \
            loop.get("retirement") != "fence+videoout":
        fail("accounting loop contract mismatch")

    dynamic_rows = many(messages, "DYNAMIC_LIGHTMAP_FRAME")
    upload_rows = many(messages, "TEXTURE_UPLOAD_FRAME")
    if [number(row, "frame") for row in dynamic_rows] != sampled_frames or \
            [number(row, "frame") for row in upload_rows] != sampled_frames:
        fail("upload sample sequence mismatch")
    last_cumulative = -1
    last_hash = ""
    transient_per_frame = number(upload_rows[0], "transient_bytes")
    pattern_hashes: dict[int, str] = {}
    for index, (dynamic, upload) in enumerate(zip(dynamic_rows, upload_rows)):
        frame = sampled_frames[index]
        first = frame < 2
        pattern = frame & 1
        expected_lightmap = image_bytes if first else patch_bytes
        sequence_hash = upload.get("sequence_hash", "")
        if upload.get("schema") != "1" or \
                number(upload, "slot") != pattern or \
                number(dynamic, "slot") != pattern or \
                number(dynamic, "pattern") != pattern or \
                upload.get("first_upload") != ("true" if first else "false") or \
                number(upload, "transient_bytes") != transient_per_frame or \
                number(upload, "lightmap_bytes") != expected_lightmap or \
                number(upload, "frame_bytes") != \
                    transient_per_frame + expected_lightmap or \
                number(dynamic, "transient_upload_bytes") != \
                    number(upload, "transient_bytes") or \
                number(dynamic, "lightmap_upload_bytes") != \
                    number(upload, "lightmap_bytes") or \
                number(dynamic, "uploaded_bytes") != \
                    number(upload, "frame_bytes") or \
                number(dynamic, "total_uploaded_bytes") != \
                    number(upload, "cumulative_bytes") or \
                dynamic.get("first_upload") != upload.get("first_upload") or \
                number(dynamic, "resident_bytes") != resident or \
                not HEX16.fullmatch(sequence_hash) or \
                upload.get("accounting") != "checked-u64":
            fail("per-frame upload partition mismatch")
        patch_hash = dynamic.get("patch_hash", "")
        if not HEX16.fullmatch(patch_hash) or \
                (pattern in pattern_hashes and
                 pattern_hashes[pattern] != patch_hash):
            fail("dynamic lightmap pattern hash mismatch")
        pattern_hashes[pattern] = patch_hash
        cumulative = number(upload, "cumulative_bytes")
        if cumulative <= last_cumulative:
            fail("sampled upload total is not monotonic")
        last_cumulative = cumulative
        last_hash = sequence_hash

    summary = one(messages, "TEXTURE_UPLOAD_SUMMARY")
    expected_transient_total = transient_per_frame * expected_frames
    expected_bounded_frames = expected_frames - 2
    expected_lightmap_total = image_bytes * 2 + \
        patch_bytes * expected_bounded_frames
    expected_upload_total = expected_transient_total + expected_lightmap_total
    if summary.get("schema") != "1" or \
            number(summary, "frames") != expected_frames or \
            number(summary, "transient_bytes_per_frame") != \
                transient_per_frame or \
            number(summary, "bounded_lightmap_bytes_per_frame") != \
                patch_bytes or \
            number(summary, "transient_bytes_total") != \
                expected_transient_total or \
            number(summary, "lightmap_bytes_total") != \
                expected_lightmap_total or \
            number(summary, "upload_bytes_total") != expected_upload_total or \
            number(summary, "frame_bytes_min") != \
                transient_per_frame + patch_bytes or \
            number(summary, "frame_bytes_max") != \
                transient_per_frame + image_bytes or \
            number(summary, "full_upload_frames") != 2 or \
            number(summary, "bounded_upload_frames") != \
                expected_bounded_frames or \
            summary.get("sequence_hash") != last_hash or \
            summary.get("sequence") != "gap-free" or \
            summary.get("accounting") != "checked-u64":
        fail("complete upload summary mismatch")

    for prefix in ("RESOURCE_FRAME_READY", "RESOURCE_FRAME_SEALED",
                   "RESOURCE_FRAME_SUBMITTED", "RESOURCE_FRAME_RETIRED",
                   "BSP_VIDEOOUT_TOKEN"):
        if [number(row, "frame") for row in many(messages, prefix)] != \
                [0, expected_frames - 1]:
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
                if row.get("frame") == str(expected_frames - 1)]
    if len(terminal) != 1 or \
            number(terminal[0], "completed") != expected_frames or \
            number(terminal[0], "present_interval_over_budget") != 0 or \
            number(terminal[0], "errors") != 0:
        fail("terminal telemetry is not clean")
    ring = one(messages, "RESOURCE_RING_RETIRED")
    if ring.get("reusable") != "true" or ring.get("tokens") != "exact":
        fail("transient ring retirement mismatch")
    readback = one(messages, "DYNAMIC_LIGHTMAP_READBACK")
    if len(pattern_hashes) != 2 or pattern_hashes[0] == pattern_hashes[1] or \
            readback.get("pattern0") != pattern_hashes[0] or \
            readback.get("pattern1") != pattern_hashes[1] or \
            not all(HEX16.fullmatch(readback.get(key, "")) for key in
                    ("gpu_buffer0", "gpu_buffer1")) or \
            readback.get("pattern0") == readback.get("pattern1") or \
            readback.get("gpu_buffer0") == readback.get("gpu_buffer1") or \
            readback.get("buffers_distinct") != "true" or \
            readback.get("surrounding") != "stable" or \
            readback.get("guards") != "intact" or \
            number(readback, "frames") != expected_frames:
        fail("dynamic lightmap/readback inheritance mismatch")
    resource_readback = one(messages, "BSP_RESOURCE_READBACK")
    if resource_readback.get("buffer0") != readback.get("gpu_buffer0") or \
            resource_readback.get("buffer1") != readback.get("gpu_buffer1") or \
            number(resource_readback, "bytes") <= 0 or \
            number(resource_readback, "bright_pixels0") <= 0 or \
            number(resource_readback, "bright_pixels1") <= 0 or \
            resource_readback.get("guards") != "intact" or \
            number(resource_readback, "frames") != expected_frames or \
            number(resource_readback, "errors") != 0 or \
            resource_readback.get("overlay") != "transient":
        fail("dynamic lightmap readback does not match terminal buffers")
    lightmap_complete = one(messages, "BSP_TEXTURE_PATH_LIGHTMAP_COMPLETE")
    if number(lightmap_complete, "frames") != expected_frames or \
            number(lightmap_complete, "resident_bytes") != resident or \
            number(lightmap_complete, "uploaded_bytes") != \
                expected_upload_total or \
            lightmap_complete.get("patterns") != "alternating" or \
            lightmap_complete.get("readback") != "gpu-visible" or \
            lightmap_complete.get("surrounding") != "stable" or \
            lightmap_complete.get("tokens") != "exact" or \
            lightmap_complete.get("guards") != "intact" or \
            number(lightmap_complete, "errors") != 0:
        fail("dynamic-lightmap completion inheritance mismatch")
    complete = one(messages, "BSP_TEXTURE_PATH_ACCOUNTING_COMPLETE")
    if number(complete, "frames") != expected_frames or \
            number(complete, "pool_resident_bytes") != resident or \
            number(complete, "texture_payload_bytes") != texture_payload or \
            number(complete, "upload_bytes_total") != expected_upload_total or \
            complete.get("sequence_hash") != last_hash or \
            number(complete, "allocations") != 6 or \
            complete.get("accounting") != "exact+gap-free" or \
            complete.get("tokens") != "exact" or \
            complete.get("guards") != "intact" or \
            complete.get("input_dependency") != "none" or \
            number(complete, "errors") != 0:
        fail("accounting completion contract mismatch")
    resource = one(messages, "BSP_RESOURCE_SOAK_COMPLETE")
    if number(resource, "frames") != expected_frames or \
            number(resource, "connected_frames") > expected_frames or \
            number(resource, "read_errors") != 0 or \
            number(resource, "pipelines") != 4 or \
            resource.get("input_dependency") != "none" or \
            resource.get("tokens") != "exact" or \
            resource.get("guards") != "intact" or \
            number(resource, "errors") != 0:
        fail("resource completion contract mismatch")
    pool = one(messages, "RESOURCE_POOL_RETIRED")
    if number(pool, "token") != number(ring, "last_token") or \
            number(pool, "reclaimed") != 6 or \
            pool.get("completion") != "fence+videoout":
        fail("resource pool reclamation mismatch")
    return {
        "run_id": manifest.get("run_id"),
        "records": manifest.get("records"),
        "frames": expected_frames,
        "pool_resident_bytes": resident,
        "texture_payload_bytes": texture_payload,
        "upload_bytes_total": expected_upload_total,
        "sequence_hash": last_hash,
        "log_sha256": hashlib.sha256(data).hexdigest(),
    }


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
        raise SystemExit(f"evidence rejected: {exc}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
