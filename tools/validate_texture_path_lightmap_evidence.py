#!/usr/bin/env python3
"""Fail-closed validation for the Phase 3 dynamic-lightmap PS5 gate."""

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
SAMPLED_FRAMES = [0, 1, 9_998, 9_999]
BOOKENDS = [0, 9_999]


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


def transcript_messages(manifest_path: Path) -> tuple[dict[str, object],
                                                        list[str], bytes]:
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        fail(f"invalid manifest: {exc}")
    identity = manifest.get("identity")
    if not isinstance(identity, dict) or identity.get("title") != "PPSA99997" or \
            identity.get("app") != "ps5-agc-gears":
        fail("title/app identity mismatch")
    if manifest.get("protocol") != "ps5log/1" or \
            manifest.get("transport") != "tcp":
        fail("protocol/transport mismatch")
    if not all(manifest.get(key) for key in ("hello", "bye", "clean")) or \
            manifest.get("gaps") != [] or manifest.get("raw_lines") != 0 or \
            manifest.get("oversized_lines") != 0:
        fail("run is incomplete or transport-corrupt")
    log_name = manifest.get("log_path")
    if not isinstance(log_name, str) or Path(log_name).name != log_name:
        fail("unsafe transcript path")
    log_path = (manifest_path.parent / log_name).resolve()
    if log_path.parent != manifest_path.parent.resolve():
        fail("transcript escaped manifest directory")
    try:
        data = log_path.read_bytes()
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
            records[-1][0] != manifest.get("last_seq") or \
            any(level == "ERROR" for _, level, _ in records):
        fail("record count or error-level contract failed")
    reason = "bsp-texture-path-lightmap-soak-complete"
    if lines[-1] != f"BYE seq={records[-1][0]} reason={reason}" or \
            manifest.get("bye_fields", {}).get("reason") != reason:
        fail("BYE reason/sequence mismatch")
    messages = [message for _, _, message in records]
    if f"LOG_BOOT_MONOTONIC_NS={boot_token}" not in messages:
        fail("structured boot token mismatch")
    return manifest, messages, data


def validate(manifest_path: Path, *, bundle_sha256: str,
             bundle_bytes: int) -> dict[str, object]:
    manifest, messages, data = transcript_messages(manifest_path.resolve())
    boot = one(messages, "BSP_TEXTURE_PATH_BOOT")
    if boot.get("schema") != "1" or boot.get("slice") != "dynamic-lightmap" or \
            boot.get("target") != "gfx1013" or \
            number(boot, "transient_slots") != 2 or \
            boot.get("ownership") != "fence+videoout" or \
            boot.get("bundle_sha256") != bundle_sha256 or \
            number(boot, "bundle_bytes") != bundle_bytes or \
            number(boot, "soak_frames") != 10_000:
        fail("texture-path boot contract mismatch")
    heap = one(messages, "RESOURCE_HEAP_READY")
    if number(heap, "bytes") <= 0 or number(heap, "allocations") != 6 or \
            heap.get("pool") != "fence-retired" or \
            number(heap, "transient_slots") != 2 or \
            number(heap, "slot_bytes") <= 0:
        fail("dynamic resource heap contract mismatch")
    ready = one(messages, "DYNAMIC_LIGHTMAP_READY")
    image_bytes = number(ready, "image_bytes")
    patch_bytes = number(ready, "patch_bytes")
    dirty_bytes = number(ready, "dirty_span_bytes")
    if image_bytes <= patch_bytes or patch_bytes != 8 * 8 * 4 or \
            dirty_bytes < patch_bytes or number(ready, "slots") != 2 or \
            number(ready, "guards") != 256 or \
            ready.get("selection") != "center-ray":
        fail("dynamic lightmap layout contract mismatch")
    loop = one(messages, "BSP_LOOP_BEGIN")
    if loop.get("mode") != "texture-path-lightmap-soak" or \
            number(loop, "buffers") != 2 or number(loop, "frames") != 10_000 or \
            loop.get("dynamic_lightmap") != "bounded" or \
            loop.get("overlay") != "fixed" or \
            loop.get("retirement") != "fence+videoout":
        fail("dynamic lightmap loop contract mismatch")

    samples = many(messages, "DYNAMIC_LIGHTMAP_FRAME")
    if [number(item, "frame") for item in samples] != SAMPLED_FRAMES:
        fail("dynamic lightmap sampled frames mismatch")
    hashes: dict[int, str] = {}
    prior_total = -1
    resident = number(samples[0], "resident_bytes")
    for index, item in enumerate(samples):
        frame = SAMPLED_FRAMES[index]
        pattern = frame & 1
        if number(item, "slot") != pattern or number(item, "pattern") != pattern or \
                number(item, "patch_bytes") != patch_bytes or \
                number(item, "resident_bytes") != resident or resident <= 0 or \
                number(item, "uploaded_bytes") != (
                    number(item, "transient_upload_bytes") +
                    number(item, "lightmap_upload_bytes")) or \
                number(item, "acquire_bytes") < number(item, "dirty_span_bytes") or \
                number(item, "acquire_bytes") > \
                    number(item, "dirty_span_bytes") + 510:
            fail("per-frame dynamic lightmap accounting mismatch")
        first = frame < 2
        if item.get("first_upload") != ("true" if first else "false") or \
                number(item, "lightmap_upload_bytes") != \
                    (image_bytes if first else patch_bytes) or \
                number(item, "dirty_span_bytes") != \
                    (image_bytes if first else dirty_bytes):
            fail("first/bounded upload contract mismatch")
        patch_hash = item.get("patch_hash", "")
        if not HEX16.fullmatch(patch_hash):
            fail("invalid patch hash")
        if pattern in hashes and hashes[pattern] != patch_hash:
            fail("pattern hash is not deterministic")
        hashes[pattern] = patch_hash
        total = number(item, "total_uploaded_bytes")
        if total <= prior_total:
            fail("uploaded-byte total is not monotonic")
        prior_total = total
    if len(hashes) != 2 or hashes[0] == hashes[1]:
        fail("alternating lightmap values are not distinct")

    for prefix in ("RESOURCE_FRAME_READY", "RESOURCE_FRAME_SEALED",
                   "RESOURCE_FRAME_SUBMITTED", "RESOURCE_FRAME_RETIRED",
                   "BSP_VIDEOOUT_TOKEN"):
        if [number(item, "frame") for item in many(messages, prefix)] != BOOKENDS:
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
                number(video[index], "expected") != number(video[index], "observed") or \
                video[index].get("exact") != "true":
            fail("exact fence/VideoOut retirement mismatch")

    terminal = [item for item in many(messages, "BSP_FRAME")
                if item.get("frame") == "9999"]
    if len(terminal) != 1 or number(terminal[0], "completed") != 10_000 or \
            number(terminal[0], "present_interval_over_budget") != 0 or \
            number(terminal[0], "errors") != 0:
        fail("terminal telemetry is not clean")
    ring = one(messages, "RESOURCE_RING_RETIRED")
    if number(ring, "slots") != 2 or ring.get("reusable") != "true" or \
            ring.get("tokens") != "exact" or number(ring, "last_token") <= 0:
        fail("transient ring retirement mismatch")
    readback = one(messages, "DYNAMIC_LIGHTMAP_READBACK")
    if readback.get("pattern0") != hashes[0] or \
            readback.get("pattern1") != hashes[1] or \
            not all(HEX16.fullmatch(readback.get(key, ""))
                    for key in ("gpu_buffer0", "gpu_buffer1")) or \
            readback.get("gpu_buffer0") == readback.get("gpu_buffer1") or \
            readback.get("buffers_distinct") != "true" or \
            readback.get("surrounding") != "stable" or \
            readback.get("guards") != "intact" or \
            number(readback, "frames") != 10_000:
        fail("GPU-visible dynamic lightmap readback mismatch")
    complete = one(messages, "BSP_TEXTURE_PATH_LIGHTMAP_COMPLETE")
    if number(complete, "frames") != 10_000 or \
            number(complete, "resident_bytes") != resident or \
            number(complete, "uploaded_bytes") != prior_total or \
            complete.get("patterns") != "alternating" or \
            complete.get("readback") != "gpu-visible" or \
            complete.get("surrounding") != "stable" or \
            complete.get("tokens") != "exact" or \
            complete.get("guards") != "intact" or number(complete, "errors") != 0:
        fail("dynamic lightmap completion contract mismatch")
    pool = one(messages, "RESOURCE_POOL_RETIRED")
    if number(pool, "token") != number(ring, "last_token") or \
            number(pool, "reclaimed") != 6 or \
            pool.get("completion") != "fence+videoout":
        fail("six-allocation pool retirement mismatch")
    return {
        "run_id": manifest.get("run_id"),
        "records": len(messages),
        "log_sha256": hashlib.sha256(data).hexdigest(),
        "frames": number(complete, "frames"),
        "patch_bytes": patch_bytes,
        "resident_bytes": resident,
        "uploaded_bytes": number(complete, "uploaded_bytes"),
        "pattern0": hashes[0],
        "pattern1": hashes[1],
        "reclaimed": number(pool, "reclaimed"),
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
        raise SystemExit(f"texture-path lightmap evidence validation failed: {exc}") from exc
    print(json.dumps(summary, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
