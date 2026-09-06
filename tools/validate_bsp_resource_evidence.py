#!/usr/bin/env python3
"""Fail-closed validation for the Phase 2 resource-foundation PS5 run."""

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
BOOKEND_FRAMES = [0, 59_999]


def fail(message: str) -> None:
    raise EvidenceError(message)


def parse_fields(message: str) -> dict[str, str]:
    fields: dict[str, str] = {}
    for token in message.split()[1:]:
        if "=" not in token:
            fail(f"malformed marker field: {token}")
        key, value = token.split("=", 1)
        fields[key] = value
    return fields


def as_int(fields: dict[str, str], key: str) -> int:
    try:
        return int(fields[key], 0)
    except (KeyError, ValueError) as exc:
        fail(f"invalid integer field: {key}")
        raise AssertionError from exc


def one(messages: list[str], prefix: str) -> dict[str, str]:
    matches = [message for message in messages if message.startswith(prefix + " ")]
    if len(matches) != 1:
        fail(f"expected exactly one {prefix}, found {len(matches)}")
    return parse_fields(matches[0])


def many(messages: list[str], prefix: str) -> list[dict[str, str]]:
    return [parse_fields(message) for message in messages
            if message.startswith(prefix + " ")]


def validate(manifest_path: Path, *, bundle_sha256: str,
             bundle_bytes: int) -> dict[str, object]:
    manifest_path = manifest_path.resolve()
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        fail(f"invalid manifest: {exc}")
    identity = manifest.get("identity")
    if not isinstance(identity, dict) or identity.get("title") != "PPSA99997" or \
            identity.get("app") != "ps5-agc-gears":
        fail("title/app identity mismatch")
    if manifest.get("protocol") != "ps5log/1" or manifest.get("transport") != "tcp":
        fail("protocol/transport mismatch")
    if not all(manifest.get(key) for key in ("hello", "bye", "clean")):
        fail("run lacks clean HELLO/BYE completion")
    if manifest.get("gaps") != [] or manifest.get("raw_lines") != 0 or \
            manifest.get("oversized_lines") != 0:
        fail("run contains transport corruption")

    log_name = manifest.get("log_path")
    if not isinstance(log_name, str) or Path(log_name).name != log_name:
        fail("unsafe transcript path")
    log_path = (manifest_path.parent / log_name).resolve()
    if log_path.parent != manifest_path.parent:
        fail("transcript escaped manifest directory")
    try:
        data = log_path.read_bytes()
    except OSError as exc:
        fail(f"missing transcript: {exc}")
    if len(data) != manifest.get("bytes") or \
            hashlib.sha256(data).hexdigest() != manifest.get("sha256"):
        fail("transcript size/hash mismatch")
    lines = data.decode("utf-8").splitlines()
    expected_boot = str(identity.get("boot", ""))
    if not lines or not lines[0].startswith("HELLO ps5log/1 "):
        fail("HELLO line missing")
    hello = parse_fields("HELLO " + lines[0].split(" ", 2)[2])
    if hello.get("title") != "PPSA99997" or \
            hello.get("app") != "ps5-agc-gears" or \
            hello.get("boot") != expected_boot:
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
    if not records or [record[0] for record in records] != \
            list(range(1, len(records) + 1)):
        fail("transcript sequence is not contiguous")
    if len(records) != manifest.get("records") or \
            records[-1][0] != manifest.get("last_seq"):
        fail("manifest record count/last sequence mismatch")
    if any(level == "ERROR" for _, level, _ in records):
        fail("transcript contains ERROR records")
    messages = [message for _, _, message in records]
    if f"LOG_BOOT_MONOTONIC_NS={expected_boot}" not in messages:
        fail("structured boot token mismatch")
    reason = "bsp-resource-soak-complete"
    if lines[-1] != f"BYE seq={records[-1][0]} reason={reason}" or \
            manifest.get("bye_fields", {}).get("reason") != reason:
        fail("BYE reason/sequence mismatch")

    boot = one(messages, "BSP_RESOURCE_BOOT")
    if boot.get("schema") != "1" or boot.get("target") != "gfx1013" or \
            boot.get("constants") != "vsharp" or \
            as_int(boot, "transient_slots") != 2 or \
            as_int(boot, "pipelines") != 2 or boot.get("overlay") != "quad":
        fail("resource boot contract mismatch")
    if boot.get("bundle_sha256") != bundle_sha256 or \
            as_int(boot, "bundle_bytes") != bundle_bytes or \
            as_int(boot, "soak_frames") != 60_000:
        fail("private bundle or soak identity mismatch")

    heap = one(messages, "RESOURCE_HEAP_READY")
    if as_int(heap, "bytes") <= 0 or as_int(heap, "allocations") != 4 or \
            heap.get("pool") != "fence-retired" or \
            as_int(heap, "transient_slots") != 2 or as_int(heap, "slot_bytes") <= 0:
        fail("resource heap contract mismatch")
    layout = one(messages, "BSP_TEXTURE_TABLE_LAYOUT")
    textures = as_int(layout, "textures")
    descriptor_dwords = as_int(layout, "descriptor_dwords")
    if textures <= 0 or descriptor_dwords != textures * 24 or \
            layout.get("storage") != "per-frame-transient":
        fail("texture descriptor cardinality/storage mismatch")
    pipelines = one(messages, "RESOURCE_PIPELINES_READY")
    if as_int(pipelines, "count") != 2 or \
            as_int(pipelines, "map_gs_words") != 2 or \
            as_int(pipelines, "overlay_gs_words") != 1:
        fail("pipeline permutation contract mismatch")
    loop = one(messages, "BSP_LOOP_BEGIN")
    if loop.get("mode") != "resource-foundation-soak" or \
            as_int(loop, "buffers") != 2 or as_int(loop, "frames") != 60_000 or \
            loop.get("retirement") != "fence+videoout":
        fail("resource loop contract mismatch")

    ready = many(messages, "RESOURCE_FRAME_READY")
    sealed = many(messages, "RESOURCE_FRAME_SEALED")
    submitted = many(messages, "RESOURCE_FRAME_SUBMITTED")
    retired = many(messages, "RESOURCE_FRAME_RETIRED")
    video = many(messages, "BSP_VIDEOOUT_TOKEN")
    for name, markers in (("ready", ready), ("sealed", sealed),
                          ("submitted", submitted), ("retired", retired),
                          ("video", video)):
        if [as_int(marker, "frame") for marker in markers] != BOOKEND_FRAMES:
            fail(f"{name} frame bookends mismatch")
    for marker in ready:
        if as_int(marker, "transient_bytes") <= 0 or \
                as_int(marker, "constant_dwords") != 32 or \
                as_int(marker, "texture_descriptor_dwords") != descriptor_dwords or \
                as_int(marker, "overlay_vertices") != 4 or \
                as_int(marker, "overlay_indices") != 6 or \
                as_int(marker, "acquire_engine") != 1 or \
                marker.get("gcr") != "00009000" or \
                marker.get("poll_cycles") != "190" or \
                marker.get("tables_gpu_span") != "true":
            fail("per-frame resource contract mismatch")
    for index in range(2):
        if as_int(sealed[index], "token") != as_int(submitted[index], "token") or \
                as_int(submitted[index], "token") != as_int(retired[index], "token") or \
                as_int(sealed[index], "slot") != as_int(submitted[index], "slot") or \
                as_int(submitted[index], "slot") != as_int(retired[index], "slot") or \
                as_int(submitted[index], "command_dwords") <= 0 or \
                retired[index].get("fence") != "zero" or \
                retired[index].get("videoout_token") != "exact" or \
                as_int(video[index], "expected") != as_int(video[index], "observed") or \
                video[index].get("exact") != "true":
            fail("exact fence/token retirement mismatch")

    terminal = [marker for marker in many(messages, "BSP_FRAME")
                if marker.get("frame") == "59999"]
    if len(terminal) != 1 or as_int(terminal[0], "completed") != 60_000 or \
            as_int(terminal[0], "present_interval_over_budget") != 0 or \
            as_int(terminal[0], "errors") != 0:
        fail("terminal telemetry is not clean")
    ring = one(messages, "RESOURCE_RING_RETIRED")
    if as_int(ring, "slots") != 2 or ring.get("reusable") != "true" or \
            ring.get("tokens") != "exact" or as_int(ring, "last_token") <= 0:
        fail("transient ring did not retire for reuse")
    readback = one(messages, "BSP_RESOURCE_READBACK")
    if not all(HEX16.fullmatch(readback.get(key, ""))
               for key in ("buffer0", "buffer1")) or \
            min(as_int(readback, "bright_pixels0"),
                as_int(readback, "bright_pixels1")) <= 0 or \
            readback.get("guards") != "intact" or \
            as_int(readback, "frames") != 60_000 or as_int(readback, "errors") != 0:
        fail("resource readback/visibility contract mismatch")
    complete = one(messages, "BSP_RESOURCE_SOAK_COMPLETE")
    if as_int(complete, "frames") != 60_000 or \
            as_int(complete, "connected_frames") != 60_000 or \
            as_int(complete, "read_errors") != 0 or as_int(complete, "errors") != 0 or \
            as_int(complete, "textures") != textures or \
            as_int(complete, "descriptor_dwords") != descriptor_dwords or \
            complete.get("tokens") != "exact" or complete.get("guards") != "intact":
        fail("resource soak completion contract mismatch")
    pool = one(messages, "RESOURCE_POOL_RETIRED")
    if as_int(pool, "token") != as_int(ring, "last_token") or \
            as_int(pool, "reclaimed") != 4 or \
            pool.get("completion") != "fence+videoout":
        fail("resource pool reclamation contract mismatch")

    positions = [next(index for index, message in enumerate(messages)
                      if message.startswith(prefix + " "))
                 for prefix in ("BSP_RESOURCE_BOOT", "RESOURCE_HEAP_READY",
                                "RESOURCE_PIPELINES_READY", "BSP_LOOP_BEGIN",
                                "RESOURCE_RING_RETIRED", "BSP_RESOURCE_READBACK",
                                "BSP_RESOURCE_SOAK_COMPLETE", "RESOURCE_POOL_RETIRED")]
    if positions != sorted(positions):
        fail("resource evidence marker order mismatch")
    return {
        "run_id": manifest.get("run_id"),
        "records": len(records),
        "log_sha256": hashlib.sha256(data).hexdigest(),
        "frames": as_int(complete, "frames"),
        "textures": textures,
        "descriptor_dwords": descriptor_dwords,
        "last_token": as_int(pool, "token"),
        "reclaimed": as_int(pool, "reclaimed"),
        "readback0": readback["buffer0"],
        "readback1": readback["buffer1"],
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
        raise SystemExit(f"resource evidence validation failed: {exc}") from exc
    print(json.dumps(summary, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
