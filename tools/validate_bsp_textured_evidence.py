#!/usr/bin/env python3
"""Fail-closed validation for a private BSP textured ps5log/1 run."""

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
INPUT_FRAMES = list(range(599, 60_000, 600))


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
    if not records:
        fail("structured records missing")
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
    reason = "bsp-textured-soak-complete"
    expected_bye = f"BYE seq={records[-1][0]} reason={reason}"
    if lines[-1] != expected_bye or bye_fields.get("reason") != reason:
        fail("BYE reason/sequence mismatch")

    boot = one(messages, "BSP_TEXTURED_BOOT")
    if boot.get("schema") != "1" or boot.get("target") != "gfx1013":
        fail("textured boot schema/target mismatch")
    if boot.get("composition") != "base_x_lightmap":
        fail("textured boot composition mismatch")
    if boot.get("bundle_sha256") != bundle_sha256 or as_int(boot, "bundle_bytes") != bundle_bytes:
        fail("private bundle identity mismatch")
    if as_int(boot, "soak_frames") != 60_000:
        fail("soak frame contract mismatch")

    pad = one(messages, "BSP_NOCLIP_PAD_READY")
    if pad != {"sticks": "dual", "triggers": "vertical", "connected_required": "true"}:
        fail("pad contract mismatch")
    tables = one(messages, "BSP_TEXTURE_TABLES_READY")
    texture_count = as_int(tables, "textures")
    descriptor_dwords = as_int(tables, "descriptor_dwords")
    if texture_count <= 0 or descriptor_dwords != texture_count * 24:
        fail("texture descriptor cardinality mismatch")
    if not re.fullmatch(r"[1-9][0-9]*x[1-9][0-9]*", tables.get("lightmap", "")):
        fail("invalid lightmap dimensions")
    modes = {
        "base_mode": "repeat",
        "lightmap_mode": "clamp",
        "filter": "bilinear",
        "composition": "base_x_lightmap",
    }
    if any(tables.get(key) != value for key, value in modes.items()):
        fail("texture/sampler composition contract mismatch")
    loop = one(messages, "BSP_LOOP_BEGIN")
    if (loop.get("mode") != "textured-noclip-soak" or
            loop.get("composition") != "base_x_lightmap" or
            as_int(loop, "frames") != 60_000):
        fail("textured loop contract mismatch")

    tokens = [parse_fields(message) for message in messages if message.startswith("BSP_VIDEOOUT_TOKEN ")]
    if [as_int(token, "frame") for token in tokens] != [0, 59_999]:
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

    frame_markers = [parse_fields(message) for message in messages if message.startswith("BSP_FRAME ")]
    terminal = [fields for fields in frame_markers if fields.get("frame") == "59999"]
    if len(terminal) != 1:
        fail("terminal BSP_FRAME missing")
    if as_int(terminal[0], "completed") != 60_000:
        fail("terminal completed-frame count mismatch")
    if (as_int(terminal[0], "present_interval_over_budget") != 0 or
            as_int(terminal[0], "errors") != 0):
        fail("terminal telemetry is not clean")

    readback = one(messages, "BSP_TEXTURED_READBACK")
    if not all(HEX16.fullmatch(readback.get(key, "")) for key in ("buffer0", "buffer1")):
        fail("invalid readback hash")
    if min(as_int(readback, "bright_pixels0"), as_int(readback, "bright_pixels1")) <= 0:
        fail("readback contains no visible geometry")
    if (readback.get("guards") != "intact" or
            as_int(readback, "frames") != 60_000 or
            as_int(readback, "errors") != 0):
        fail("readback/guard contract mismatch")

    complete = one(messages, "BSP_TEXTURED_SOAK_COMPLETE")
    exact = {"frames": 60_000, "connected_frames": 60_000}
    for key, value in exact.items():
        if as_int(complete, key) != value:
            fail(f"completion threshold failed: {key}")
    final_input = {
        "moving_frames": "moving",
        "looking_frames": "looking",
        "distance_milli": "distance_milli",
        "input_changes": "changes",
    }
    for completion_key, heartbeat_key in final_input.items():
        if as_int(complete, completion_key) != previous[heartbeat_key]:
            fail(f"completion/input counter mismatch: {completion_key}")
    if (as_int(complete, "read_errors") != 0 or
            as_int(complete, "errors") != 0):
        fail("completion reports errors")
    if complete.get("tokens") != "exact" or complete.get("guards") != "intact":
        fail("completion token/guard contract mismatch")
    if complete.get("composition") != "base_x_lightmap":
        fail("completion composition mismatch")
    if (as_int(complete, "textures") != texture_count or
            as_int(complete, "descriptor_dwords") != descriptor_dwords):
        fail("completion texture metadata mismatch")
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
        "textures": texture_count,
        "descriptor_dwords": descriptor_dwords,
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
        summary = validate(
            args.manifest,
            bundle_sha256=args.bundle_sha256,
            bundle_bytes=args.bundle_bytes,
        )
    except EvidenceError as exc:
        raise SystemExit(f"textured evidence validation failed: {exc}") from exc
    print(json.dumps(summary, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
