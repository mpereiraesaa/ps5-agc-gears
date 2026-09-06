#!/usr/bin/env python3
"""Fail-closed validation for a private BSP noclip ps5log/1 run."""

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
INPUT_FRAMES = list(range(599, 10_000, 600))


def fail(message: str) -> None:
    raise EvidenceError(message)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


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


def validate(
    manifest_path: Path,
    *,
    bundle_sha256: str,
    bundle_bytes: int,
) -> dict[str, object]:
    manifest_path = manifest_path.resolve()
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        fail(f"invalid manifest: {exc}")

    identity = manifest.get("identity")
    if not isinstance(identity, dict):
        fail("manifest identity missing")
    if identity.get("title") != "PPSA99997" or identity.get("app") != "ps5-agc-gears":
        fail("title/app identity mismatch")
    if manifest.get("protocol") != "ps5log/1" or manifest.get("transport") != "tcp":
        fail("protocol/transport mismatch")
    if not all(manifest.get(key) for key in ("hello", "bye", "clean")):
        fail("run lacks clean HELLO/BYE completion")
    if manifest.get("gaps") != [] or manifest.get("raw_lines") != 0:
        fail("run has sequence gaps or raw lines")
    if manifest.get("oversized_lines") != 0:
        fail("run has oversized records")

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
    if len(data) != manifest.get("bytes") or sha256(data) != manifest.get("sha256"):
        fail("transcript size/hash mismatch")

    lines = data.decode("utf-8").splitlines()
    if not lines or not lines[0].startswith("HELLO ps5log/1 "):
        fail("HELLO line missing")
    expected_boot = str(identity.get("boot", ""))
    hello_fields = parse_fields("HELLO " + lines[0].split(" ", 2)[2])
    if hello_fields.get("title") != "PPSA99997" or hello_fields.get("app") != "ps5-agc-gears":
        fail("HELLO identity mismatch")
    if hello_fields.get("boot") != expected_boot:
        fail("HELLO boot mismatch")

    records: list[tuple[int, str, str]] = []
    for line in lines[1:-1]:
        parts = line.split("\t", 3)
        if len(parts) != 4:
            fail("malformed structured record")
        try:
            seq = int(parts[0])
        except ValueError:
            fail("invalid sequence number")
        records.append((seq, parts[2], parts[3]))
    if [record[0] for record in records] != list(range(1, len(records) + 1)):
        fail("transcript sequence is not contiguous")
    if len(records) != manifest.get("records") or records[-1][0] != manifest.get("last_seq"):
        fail("manifest record count/last sequence mismatch")
    if any(level == "ERROR" for _, level, _ in records):
        fail("transcript contains ERROR records")
    messages = [message for _, _, message in records]

    if f"LOG_BOOT_MONOTONIC_NS={expected_boot}" not in messages:
        fail("structured boot token mismatch")
    bye_fields = manifest.get("bye_fields")
    if not isinstance(bye_fields, dict):
        fail("manifest BYE fields missing")
    expected_bye = f"BYE seq={records[-1][0]} reason=bsp-noclip-soak-complete"
    if lines[-1] != expected_bye or bye_fields.get("reason") != "bsp-noclip-soak-complete":
        fail("BYE reason/sequence mismatch")

    boot = one(messages, "BSP_NOCLIP_BOOT")
    if boot.get("schema") != "1" or boot.get("target") != "gfx1013":
        fail("noclip boot schema/target mismatch")
    if boot.get("bundle_sha256") != bundle_sha256 or as_int(boot, "bundle_bytes") != bundle_bytes:
        fail("private bundle identity mismatch")
    if as_int(boot, "soak_frames") != 10_000:
        fail("soak frame contract mismatch")

    pad = one(messages, "BSP_NOCLIP_PAD_READY")
    if pad != {"sticks": "dual", "triggers": "vertical", "connected_required": "true"}:
        fail("pad contract mismatch")
    loop = one(messages, "BSP_LOOP_BEGIN")
    if loop.get("mode") != "noclip-soak" or as_int(loop, "frames") != 10_000:
        fail("noclip loop contract mismatch")

    tokens = [parse_fields(message) for message in messages if message.startswith("BSP_VIDEOOUT_TOKEN ")]
    if [as_int(token, "frame") for token in tokens] != [0, 9999]:
        fail("VideoOut token bookends mismatch")
    for token in tokens:
        if token.get("exact") != "true" or as_int(token, "expected") != as_int(token, "observed"):
            fail("VideoOut token was not exact")

    inputs = [parse_fields(message) for message in messages if message.startswith("BSP_NOCLIP_INPUT ")]
    if [as_int(fields, "frame") for fields in inputs] != INPUT_FRAMES:
        fail("input heartbeat cadence mismatch")
    previous = {"connected": 0, "moving": 0, "looking": 0, "distance_milli": 0, "changes": 0}
    for fields in inputs:
        frame = as_int(fields, "frame")
        if as_int(fields, "connected") != frame + 1 or as_int(fields, "read_errors") != 0:
            fail("input continuity/read error mismatch")
        for key, value in previous.items():
            current = as_int(fields, key)
            if current < value:
                fail(f"input counter regressed: {key}")
            previous[key] = current
        if not HEX16.fullmatch(fields.get("hash", "")):
            fail("invalid input heartbeat hash")

    final_frames = [parse_fields(message) for message in messages if message.startswith("BSP_FRAME ")]
    final = [fields for fields in final_frames if fields.get("frame") == "9999"]
    if len(final) != 1:
        fail("terminal BSP_FRAME missing")
    final_frame = final[0]
    if as_int(final_frame, "completed") != 10_000:
        fail("terminal completed-frame count mismatch")
    if as_int(final_frame, "present_interval_over_budget") != 0 or as_int(final_frame, "errors") != 0:
        fail("terminal telemetry is not clean")

    readback = one(messages, "BSP_NOCLIP_READBACK")
    if readback.get("buffer0") != readback.get("buffer1") or not HEX16.fullmatch(readback.get("buffer0", "")):
        fail("readback hashes differ or are invalid")
    if min(as_int(readback, "bright_pixels0"), as_int(readback, "bright_pixels1")) <= 0:
        fail("readback contains no visible geometry")
    if readback.get("guards") != "intact" or as_int(readback, "frames") != 10_000 or as_int(readback, "errors") != 0:
        fail("readback/guard contract mismatch")

    complete = one(messages, "BSP_NOCLIP_SOAK_COMPLETE")
    requirements = {
        "frames": 10_000,
        "connected_frames": 10_000,
        "moving_frames": 600,
        "looking_frames": 120,
        "distance_milli": 100_000,
    }
    for key, minimum in requirements.items():
        value = as_int(complete, key)
        if value != minimum if key in ("frames", "connected_frames") else value < minimum:
            fail(f"completion threshold failed: {key}")
    if as_int(complete, "read_errors") != 0 or as_int(complete, "errors") != 0:
        fail("completion reports errors")
    if complete.get("tokens") != "exact" or complete.get("guards") != "intact":
        fail("completion token/guard contract mismatch")
    if not HEX16.fullmatch(complete.get("input_hash", "")):
        fail("invalid completion input hash")

    return {
        "run_id": manifest.get("run_id"),
        "records": len(records),
        "log_sha256": sha256(data),
        "frames": as_int(complete, "frames"),
        "moving_frames": as_int(complete, "moving_frames"),
        "looking_frames": as_int(complete, "looking_frames"),
        "distance_milli": as_int(complete, "distance_milli"),
        "input_hash": complete["input_hash"],
        "readback": readback["buffer0"],
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
        summary = validate(
            args.manifest,
            bundle_sha256=args.bundle_sha256,
            bundle_bytes=args.bundle_bytes,
        )
    except EvidenceError as exc:
        raise SystemExit(f"noclip evidence validation failed: {exc}") from exc
    print(json.dumps(summary, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
