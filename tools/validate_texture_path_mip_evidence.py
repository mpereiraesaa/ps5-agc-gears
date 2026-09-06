#!/usr/bin/env python3
"""Fail-closed validation for the Phase 3 mip/sampler PS5 gate."""

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
SAMPLES = (0, 1, 9_998, 9_999)


def fail(message: str) -> None:
    raise EvidenceError(message)


def fields(message: str) -> dict[str, str]:
    result = {}
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


def transcript(manifest_path: Path) -> tuple[dict[str, object], list[str], bytes]:
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        fail(f"invalid manifest: {exc}")
    identity = manifest.get("identity")
    if not isinstance(identity, dict) or identity.get("title") != "PPSA99997" or \
            identity.get("app") != "ps5-agc-gears":
        fail("title/app identity mismatch")
    if manifest.get("protocol") != "ps5log/1" or manifest.get("transport") != "tcp" or \
            not all(manifest.get(key) for key in ("hello", "bye", "clean")) or \
            manifest.get("gaps") != [] or manifest.get("raw_lines") != 0 or \
            manifest.get("oversized_lines") != 0:
        fail("run is incomplete or transport-corrupt")
    name = manifest.get("log_path")
    if not isinstance(name, str) or Path(name).name != name:
        fail("unsafe transcript path")
    path = (manifest_path.parent / name).resolve()
    if path.parent != manifest_path.parent.resolve():
        fail("transcript escaped manifest directory")
    try:
        data = path.read_bytes()
    except OSError as exc:
        fail(f"missing transcript: {exc}")
    if len(data) != manifest.get("bytes") or \
            hashlib.sha256(data).hexdigest() != manifest.get("sha256"):
        fail("transcript size/hash mismatch")
    lines = data.decode("utf-8").splitlines()
    boot_token = str(identity.get("boot", ""))
    if not lines or not lines[0].startswith("HELLO ps5log/1 ") or \
            f"title=PPSA99997 app=ps5-agc-gears boot={boot_token}" not in lines[0]:
        fail("HELLO identity mismatch")
    records = []
    for line in lines[1:-1]:
        parts = line.split("\t", 3)
        if len(parts) != 4:
            fail("malformed structured record")
        try:
            sequence = int(parts[0])
        except ValueError:
            fail("invalid sequence number")
        records.append((sequence, parts[2], parts[3]))
    if not records or [record[0] for record in records] != \
            list(range(1, len(records) + 1)) or \
            len(records) != manifest.get("records") or \
            records[-1][0] != manifest.get("last_seq") or \
            any(level == "ERROR" for _, level, _ in records):
        fail("record sequence/count/error contract failed")
    reason = "bsp-texture-path-mip-soak-complete"
    if lines[-1] != f"BYE seq={records[-1][0]} reason={reason}" or \
            manifest.get("bye_fields", {}).get("reason") != reason:
        fail("BYE reason/sequence mismatch")
    messages = [message for _, _, message in records]
    if f"LOG_BOOT_MONOTONIC_NS={boot_token}" not in messages:
        fail("structured boot token mismatch")
    return manifest, messages, data


def validate(manifest_path: Path, *, bundle_sha256: str,
             bundle_bytes: int) -> dict[str, object]:
    manifest, messages, data = transcript(manifest_path.resolve())
    boot = one(messages, "BSP_TEXTURE_PATH_BOOT")
    if boot.get("schema") != "1" or boot.get("slice") != "mip-sampler" or \
            boot.get("target") != "gfx1013" or \
            number(boot, "transient_slots") != 2 or \
            boot.get("ownership") != "fence+videoout" or \
            boot.get("bundle_sha256") != bundle_sha256 or \
            number(boot, "bundle_bytes") != bundle_bytes or \
            number(boot, "soak_frames") != 10_000:
        fail("mip gate boot contract mismatch")

    chains = one(messages, "MIP_CHAINS_READY")
    texture_count = number(chains, "textures")
    if texture_count <= 0 or chains.get("layout") != "addr-sw-linear" or \
            chains.get("order") != "smallest-to-base" or \
            number(chains, "pitch_alignment") != 256 or \
            number(chains, "levels_min") < 2 or \
            number(chains, "levels_max") > 15 or \
            number(chains, "levels_min") > number(chains, "levels_max") or \
            number(chains, "chain_bytes") <= 0 or \
            chains.get("deterministic") != "box-rne":
        fail("mip-chain layout contract mismatch")
    tables = one(messages, "BSP_TEXTURE_TABLE_LAYOUT")
    if number(tables, "textures") != texture_count or \
            number(tables, "descriptor_dwords") != texture_count * 24 or \
            tables.get("storage") != "per-frame-transient" or \
            tables.get("base_filter") != "alternating-trilinear-anisotropic4x" or \
            tables.get("lightmap_filter") != "bilinear":
        fail("mip descriptor-table contract mismatch")
    loop = one(messages, "BSP_LOOP_BEGIN")
    if loop.get("mode") != "texture-path-mip-soak" or \
            number(loop, "buffers") != 2 or number(loop, "frames") != 10_000 or \
            loop.get("mip_layout") != "addr-sw-linear" or \
            loop.get("samplers") != "trilinear+anisotropic4x" or \
            loop.get("lightmap_pattern") != "paired" or \
            loop.get("retirement") != "fence+videoout":
        fail("mip loop contract mismatch")

    sampler_rows = many(messages, "MIP_SAMPLER_FRAME")
    if [number(row, "frame") for row in sampler_rows] != list(SAMPLES):
        fail("mip sampler sample frames mismatch")
    expected_patterns = (0, 0, 1, 1)
    for row, frame, pattern in zip(sampler_rows, SAMPLES, expected_patterns):
        trilinear = frame % 2 == 0
        if number(row, "slot") != frame % 2 or \
                number(row, "lightmap_pattern") != pattern or \
                row.get("filter") != ("trilinear" if trilinear else "anisotropic4x") or \
                not all(HEX8.fullmatch(row.get(key, "")) for key in ("s0", "s1", "s2", "s3")):
            fail("mip sampler frame identity mismatch")
        words = [int(row[f"s{index}"], 16) for index in range(4)]
        if trilinear:
            valid = words[0] == 0 and words[2] == 0x08500000 and words[3] == 0 and \
                    words[1] & 0x0ff00000 and words[1] >> 24 == 0
        else:
            valid = words[0] == 0x00410400 and words[2] == 0x28f00000 and \
                    words[3] == 0 and words[1] >> 24 == 8
        if not valid:
            fail("T#/S# sampler encoding mismatch")
    if (int(sampler_rows[0]["s1"], 16) & 0x00ffffff) != \
            (int(sampler_rows[1]["s1"], 16) & 0x00ffffff):
        fail("sampler variants use different MAX_LOD")

    light_rows = many(messages, "DYNAMIC_LIGHTMAP_FRAME")
    if [number(row, "frame") for row in light_rows] != list(SAMPLES) or \
            [number(row, "pattern") for row in light_rows] != list(expected_patterns):
        fail("paired lightmap isolation mismatch")
    pattern_hashes: dict[int, str] = {}
    for row in light_rows:
        pattern = number(row, "pattern")
        digest = row.get("patch_hash", "")
        if not HEX16.fullmatch(digest) or \
                (pattern in pattern_hashes and pattern_hashes[pattern] != digest):
            fail("paired lightmap hash mismatch")
        pattern_hashes[pattern] = digest
    if len(pattern_hashes) != 2 or pattern_hashes[0] == pattern_hashes[1]:
        fail("dynamic lightmap patterns are not distinct")

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
            dynamic.get("surrounding") != "stable" or dynamic.get("guards") != "intact":
        fail("final paired lightmap mismatch")
    readback = one(messages, "MIP_SAMPLER_READBACK")
    if not HEX16.fullmatch(readback.get("trilinear_buffer", "")) or \
            not HEX16.fullmatch(readback.get("anisotropic4x_buffer", "")) or \
            readback.get("trilinear_buffer") == readback.get("anisotropic4x_buffer") or \
            readback.get("paired") != "true" or \
            readback.get("framebuffer_distinct") != "true" or \
            readback.get("guards") != "intact" or number(readback, "frames") != 10_000:
        fail("GPU-visible sampler readback mismatch")
    complete = one(messages, "BSP_TEXTURE_PATH_MIP_COMPLETE")
    if number(complete, "frames") != 10_000 or number(complete, "textures") != texture_count or \
            complete.get("layout") != "addr-sw-linear" or complete.get("t_sharp") != "mip-aware" or \
            complete.get("samplers") != "trilinear+anisotropic4x" or \
            complete.get("readback") != "gpu-visible" or complete.get("paired_lightmap") != "true" or \
            complete.get("tokens") != "exact" or complete.get("guards") != "intact" or \
            number(complete, "errors") != 0:
        fail("mip completion contract mismatch")
    pool = one(messages, "RESOURCE_POOL_RETIRED")
    if number(pool, "reclaimed") != 6 or pool.get("completion") != "fence+videoout":
        fail("resource-pool retirement mismatch")
    return {
        "run_id": manifest.get("run_id"), "records": len(messages),
        "log_sha256": hashlib.sha256(data).hexdigest(), "frames": 10_000,
        "textures": texture_count, "mip_levels_min": number(chains, "levels_min"),
        "mip_levels_max": number(chains, "levels_max"),
        "trilinear_buffer": readback["trilinear_buffer"],
        "anisotropic4x_buffer": readback["anisotropic4x_buffer"],
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
        raise SystemExit(f"texture-path mip evidence validation failed: {exc}") from exc
    print(json.dumps(summary, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
