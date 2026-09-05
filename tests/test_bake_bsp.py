#!/usr/bin/env python3
"""Synthetic BSP v30 regressions for the Phase 1 host baker."""

from __future__ import annotations

import hashlib
import importlib.util
import struct
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.dont_write_bytecode = True
SPEC = importlib.util.spec_from_file_location("bake_bsp", ROOT / "tools/bake_bsp.py")
assert SPEC and SPEC.loader
BAKER = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = BAKER
SPEC.loader.exec_module(BAKER)


def tiny_bsp() -> bytes:
    entities = (
        b'{\n"classname" "worldspawn"\n}\n'
        b'{\n"classname" "info_player_start"\n'
        b'"origin" "32 16 8"\n"angle" "90"\n}\n\0'
    )
    texture = bytearray(struct.pack("<II", 1, 8))
    texture += struct.pack("<16sII4I", b"TEST", 64, 64, 0, 0, 0, 0)
    vertices = b"".join(struct.pack("<3f", *value) for value in (
        (0.0, 0.0, 0.0), (64.0, 0.0, 0.0),
        (64.0, 64.0, 0.0), (0.0, 64.0, 0.0),
    ))
    texinfo = struct.pack("<8fii", 1, 0, 0, 0, 0, 1, 0, 0, 0, 0)
    face = struct.pack("<Hhihh4Bi", 0, 0, 0, 4, 0, 0, 255, 255, 255, -1)
    edges = b"".join(struct.pack("<2H", *edge) for edge in (
        (0, 1), (1, 2), (2, 3), (3, 0),
    ))
    surfedges = b"".join(struct.pack("<i", value) for value in range(4))
    contents = {
        BAKER.LUMP_ENTITIES: entities,
        BAKER.LUMP_TEXTURES: bytes(texture),
        BAKER.LUMP_VERTICES: vertices,
        BAKER.LUMP_TEXINFO: texinfo,
        BAKER.LUMP_FACES: face,
        BAKER.LUMP_EDGES: edges,
        BAKER.LUMP_SURFEDGES: surfedges,
    }
    output = bytearray(BAKER.BSP_HEADER_BYTES)
    struct.pack_into("<I", output, 0, BAKER.BSP_VERSION)
    cursor = BAKER.BSP_HEADER_BYTES
    lumps = []
    for index in range(BAKER.BSP_LUMP_COUNT):
        while cursor % 4:
            output.append(0)
            cursor += 1
        payload = contents.get(index, b"")
        lumps.append((cursor, len(payload)))
        output.extend(payload)
        cursor += len(payload)
    for index, (offset, size) in enumerate(lumps):
        struct.pack_into("<II", output, 4 + index * 8, offset, size)
    return bytes(output)


def chunks(bundle: bytes) -> dict[bytes, tuple[int, int, int, int]]:
    header = BAKER.BUNDLE_HEADER.unpack_from(bundle)
    result = {}
    for index in range(header[11]):
        row = BAKER.CHUNK_HEADER.unpack_from(
            bundle, BAKER.BUNDLE_HEADER.size + index * BAKER.CHUNK_HEADER.size
        )
        result[row[0]] = (row[1], row[2], row[3], row[4])
    return result


def must_fail(payload: bytes, phrase: str) -> None:
    try:
        BAKER.bake(payload)
    except BAKER.BakeError as error:
        assert phrase in str(error)
    else:
        raise AssertionError("invalid BSP unexpectedly baked")


def main() -> int:
    source = tiny_bsp()
    first = BAKER.bake(source)
    second = BAKER.bake(source)
    assert first == second
    assert hashlib.sha256(first).hexdigest() == (
        "0b44128ed0964676ddcd16e4078a7a086c131597abf60bd79de3adba7eeada71"
    )

    header = BAKER.BUNDLE_HEADER.unpack_from(first)
    assert header[0] == BAKER.BUNDLE_MAGIC and header[1] == 1
    assert header[3] == len(first) and header[11] == 3
    assert header[5:8] == (32.0, 36.0, -16.0)
    assert abs(header[8]) < 1e-6 and abs(header[9]) < 1e-6
    assert abs(header[10] + 1.0) < 1e-6

    directory = chunks(first)
    assert directory[b"VERT"][2:] == (4, BAKER.VERTEX.size)
    assert directory[b"INDX"][2:] == (6, BAKER.INDEX.size)
    assert directory[b"DRAW"][2:] == (1, BAKER.DRAW.size)
    vertex_offset = directory[b"VERT"][0]
    assert BAKER.VERTEX.unpack_from(first, vertex_offset) == (
        0.0, 0.0, -0.0, 0.0, 0.0, 0.0, 0.0, 0,
    )
    index_offset = directory[b"INDX"][0]
    assert struct.unpack_from("<6I", first, index_offset) == (0, 1, 2, 0, 2, 3)
    draw_offset = directory[b"DRAW"][0]
    assert BAKER.DRAW.unpack_from(first, draw_offset)[:5] == (0, 6, 0, 0xFFFFFFFF, 0)

    wrong_version = bytearray(source)
    struct.pack_into("<I", wrong_version, 0, 29)
    must_fail(bytes(wrong_version), "expected BSP v30")
    outside = bytearray(source)
    struct.pack_into("<II", outside, 4 + BAKER.LUMP_VERTICES * 8,
                     len(outside) + 1, 12)
    must_fail(bytes(outside), "outside the file")
    no_spawn = source.replace(b"info_player_start", b"info_player_wrong")
    must_fail(no_spawn, "no player spawn")
    print("BSP baker tests passed: deterministic bundle, fan indices, spawn transform")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
