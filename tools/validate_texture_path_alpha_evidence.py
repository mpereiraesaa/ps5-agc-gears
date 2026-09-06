#!/usr/bin/env python3
"""Fail-closed validation for the Phase 3 `{` alpha-test PS5 gate."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path


class EvidenceError(RuntimeError):
    pass


HEX64 = re.compile(r"[0-9a-f]{64}")
HEX16 = re.compile(r"[0-9a-f]{16}")
HEX8 = re.compile(r"[0-9a-f]{8}")


def fail(message: str) -> None:
    raise EvidenceError(message)


def fields(message: str) -> dict[str, str]:
    result: dict[str, str] = {}
    for token in message.split()[1:]:
        if "=" not in token:
            fail(f"malformed marker field: {token}")
        key, value = token.split("=", 1)
        result[key] = value
    return result


def number(item: dict[str, str], key: str) -> int:
    try:
        return int(item[key], 0)
    except (KeyError, ValueError) as exc:
        fail(f"invalid integer field: {key}")
        raise AssertionError from exc


def one(messages: list[str], prefix: str) -> dict[str, str]:
    matches = [fields(message) for message in messages
               if message.startswith(prefix + " ")]
    if len(matches) != 1:
        fail(f"expected exactly one {prefix}, found {len(matches)}")
    return matches[0]


def many(messages: list[str], prefix: str) -> list[dict[str, str]]:
    return [fields(message) for message in messages
            if message.startswith(prefix + " ")]


def transcript(path: Path) -> tuple[dict[str, object], list[str], bytes]:
    try:
        manifest = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        fail(f"invalid manifest: {exc}")
    identity = manifest.get("identity")
    if not isinstance(identity, dict) or identity.get("title") != "PPSA99997" \
            or identity.get("app") != "ps5-agc-gears":
        fail("title/app identity mismatch")
    if manifest.get("protocol") != "ps5log/1" or \
            manifest.get("transport") != "tcp" or \
            not all(manifest.get(key) for key in ("hello", "bye", "clean")) or \
            manifest.get("gaps") != [] or manifest.get("raw_lines") != 0 or \
            manifest.get("oversized_lines") != 0:
        fail("run is incomplete or transport-corrupt")
    name = manifest.get("log_path")
    if not isinstance(name, str) or Path(name).name != name:
        fail("unsafe transcript path")
    log = (path.parent / name).resolve()
    if log.parent != path.parent.resolve():
        fail("transcript escaped manifest directory")
    try:
        data = log.read_bytes()
    except OSError as exc:
        fail(f"missing transcript: {exc}")
    if len(data) != manifest.get("bytes") or \
            hashlib.sha256(data).hexdigest() != manifest.get("sha256"):
        fail("transcript size/hash mismatch")
    lines = data.decode("utf-8").splitlines()
    boot = str(identity.get("boot", ""))
    if not lines or not lines[0].startswith("HELLO ps5log/1 ") or \
            f"title=PPSA99997 app=ps5-agc-gears boot={boot}" not in lines[0]:
        fail("HELLO identity mismatch")
    records: list[tuple[int, str, str]] = []
    for line in lines[1:-1]:
        parts = line.split("\t", 3)
        if len(parts) != 4:
            fail("malformed structured record")
        try:
            sequence = int(parts[0])
        except ValueError:
            fail("invalid sequence number")
        records.append((sequence, parts[2], parts[3]))
    if not records or [row[0] for row in records] != \
            list(range(1, len(records) + 1)) or \
            len(records) != manifest.get("records") or \
            records[-1][0] != manifest.get("last_seq") or \
            any(level == "ERROR" for _, level, _ in records):
        fail("record sequence/count/error contract failed")
    reason = "bsp-texture-path-alpha-soak-complete"
    if lines[-1] != f"BYE seq={records[-1][0]} reason={reason}" or \
            manifest.get("bye_fields", {}).get("reason") != reason:
        fail("BYE reason/sequence mismatch")
    messages = [message for _, _, message in records]
    if f"LOG_BOOT_MONOTONIC_NS={boot}" not in messages:
        fail("structured boot token mismatch")
    return manifest, messages, data


def validate(path: Path, *, bundle_sha256: str,
             bundle_bytes: int) -> dict[str, object]:
    manifest, messages, data = transcript(path.resolve())
    boot = one(messages, "BSP_TEXTURE_PATH_BOOT")
    if boot.get("schema") != "1" or boot.get("slice") != "alpha-test" or \
            boot.get("target") != "gfx1013" or \
            number(boot, "transient_slots") != 2 or \
            boot.get("ownership") != "fence+videoout" or \
            boot.get("bundle_sha256") != bundle_sha256 or \
            number(boot, "bundle_bytes") != bundle_bytes or \
            number(boot, "soak_frames") != 10_000:
        fail("alpha-test boot contract mismatch")

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
    if number(pipelines, "count") != 3 or \
            pipelines.get("map") != "bsp_resource" or \
            pipelines.get("alpha_test") != "bsp_alpha_test" or \
            pipelines.get("overlay") != "bsp_overlay":
        fail("pipeline permutation contract mismatch")
    ready = one(messages, "ALPHA_TEST_READY")
    alpha_textures = number(ready, "textures")
    alpha_draws = number(ready, "draws")
    opaque_draws = number(ready, "opaque_draws")
    if alpha_textures <= 0 or alpha_draws <= 0 or opaque_draws <= 0 or \
            alpha_draws + opaque_draws <= alpha_draws or \
            ready.get("camera") != "nearest-centroid" or \
            ready.get("opaque_pipeline") != "bsp_resource" or \
            ready.get("alpha_pipeline") != "bsp_alpha_test" or \
            ready.get("kill_bit") != "0x40" or \
            not HEX8.fullmatch(ready.get("opaque_db", "")) or \
            not HEX8.fullmatch(ready.get("alpha_db", "")) or \
            int(ready["opaque_db"], 16) & 0x40 or \
            int(ready["alpha_db"], 16) != \
            (int(ready["opaque_db"], 16) | 0x40) or \
            ready.get("threshold") != "0.5" or \
            ready.get("depth_write") != "enabled" or \
            ready.get("blend") != "disabled":
        fail("alpha classification/register contract mismatch")
    loop = one(messages, "BSP_LOOP_BEGIN")
    if loop.get("mode") != "texture-path-alpha-soak" or \
            number(loop, "buffers") != 2 or number(loop, "frames") != 10_000 or \
            loop.get("opaque_pass") != "separate" or \
            loop.get("alpha_test_pass") != "separate" or \
            loop.get("sampler") != "anisotropic4x" or \
            loop.get("lightmap_pattern") != "paired" or \
            loop.get("retirement") != "fence+videoout":
        fail("alpha loop contract mismatch")

    alpha_rows = many(messages, "ALPHA_TEST_FRAME")
    if [number(row, "frame") for row in alpha_rows] != [0, 9_998, 9_999] or \
            [row.get("mode") for row in alpha_rows] != \
            ["alpha-test", "opaque-control", "alpha-test"] or \
            [number(row, "lightmap_pattern") for row in alpha_rows] != [0, 1, 1]:
        fail("alpha isolation frame sequence mismatch")
    for row in alpha_rows:
        if number(row, "opaque_draws") != opaque_draws or \
                number(row, "alpha_test_draws") != alpha_draws or \
                row.get("sampler") != "anisotropic4x":
            fail("alpha draw classification changed during soak")

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
    readback = one(messages, "ALPHA_TEST_READBACK")
    control_hash = readback.get("opaque_control_buffer", "")
    alpha_hash = readback.get("alpha_test_buffer", "")
    if not HEX16.fullmatch(control_hash) or not HEX16.fullmatch(alpha_hash) or \
            control_hash == alpha_hash or readback.get("paired") != "true" or \
            readback.get("framebuffer_distinct") != "true" or \
            readback.get("sampler") != "anisotropic4x" or \
            readback.get("guards") != "intact" or \
            number(readback, "frames") != 10_000:
        fail("GPU-visible alpha-test readback mismatch")
    resource_readback = one(messages, "BSP_RESOURCE_READBACK")
    if resource_readback.get("buffer0") != control_hash or \
            resource_readback.get("buffer1") != alpha_hash:
        fail("alpha readback does not match terminal buffers")
    complete = one(messages, "BSP_TEXTURE_PATH_ALPHA_COMPLETE")
    if number(complete, "frames") != 10_000 or \
            number(complete, "textures") != alpha_textures or \
            number(complete, "draws") != alpha_draws or \
            complete.get("pipeline") != "separate" or \
            complete.get("opaque_control") != "gpu-visible" or \
            complete.get("alpha_test") != "gpu-visible" or \
            complete.get("paired_lightmap") != "true" or \
            complete.get("tokens") != "exact" or \
            complete.get("guards") != "intact" or number(complete, "errors") != 0:
        fail("alpha completion contract mismatch")
    resource = one(messages, "BSP_RESOURCE_SOAK_COMPLETE")
    if number(resource, "frames") != 10_000 or \
            number(resource, "connected_frames") != 10_000 or \
            number(resource, "read_errors") != 0 or \
            number(resource, "pipelines") != 3 or \
            resource.get("tokens") != "exact" or \
            resource.get("guards") != "intact" or number(resource, "errors") != 0:
        fail("resource completion contract mismatch")
    pool = one(messages, "RESOURCE_POOL_RETIRED")
    if number(pool, "reclaimed") != 6 or \
            pool.get("completion") != "fence+videoout":
        fail("resource-pool retirement mismatch")
    return {
        "run_id": manifest.get("run_id"), "records": len(messages),
        "log_sha256": hashlib.sha256(data).hexdigest(), "frames": 10_000,
        "textures": alpha_textures, "draws": alpha_draws,
        "opaque_control_buffer": control_hash,
        "alpha_test_buffer": alpha_hash,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest", type=Path)
    parser.add_argument("--bundle-sha256", required=True)
    parser.add_argument("--bundle-bytes", type=int, required=True)
    args = parser.parse_args()
    if not HEX64.fullmatch(args.bundle_sha256):
        parser.error("--bundle-sha256 must be 64 lowercase hex digits")
    try:
        summary = validate(args.manifest, bundle_sha256=args.bundle_sha256,
                           bundle_bytes=args.bundle_bytes)
    except EvidenceError as exc:
        raise SystemExit(
            f"texture-path alpha evidence validation failed: {exc}"
        ) from exc
    print(json.dumps(summary, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
