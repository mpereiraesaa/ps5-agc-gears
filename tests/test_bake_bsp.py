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
    mip_offsets = (40, 4136, 5160, 5416)
    mip0 = bytes(index & 255 for index in range(64 * 64))
    mip1 = bytes(32 * 32)
    mip2 = bytes(16 * 16)
    mip3 = bytes(8 * 8)
    palette = b"".join(bytes((index, (index + 1) & 255, 255 - index))
                       for index in range(256))
    texture = bytearray(struct.pack("<II", 1, 8))
    texture += struct.pack("<16sII4I", b"{TEST", 64, 64, *mip_offsets)
    texture += mip0 + mip1 + mip2 + mip3 + struct.pack("<H", 256) + palette
    vertices = b"".join(struct.pack("<3f", *value) for value in (
        (0.0, 0.0, 0.0), (64.0, 0.0, 0.0),
        (64.0, 64.0, 0.0), (0.0, 64.0, 0.0),
    ))
    texinfo = struct.pack("<8fii", 1, 0, 0, 0, 0, 1, 0, 0, 0, 0)
    face = struct.pack("<Hhihh4Bi", 0, 0, 0, 4, 0, 0, 255, 255, 255, 0)
    lighting = b"".join(bytes((value, value + 1, value + 2))
                        for value in range(0, 75, 3))
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
        BAKER.LUMP_LIGHTING: lighting,
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
        "b68cba3218caa281f596f683e8cd5ba019a939b937bfc068f6d97b1a83811476"
    )

    header = BAKER.BUNDLE_HEADER.unpack_from(first)
    assert header[0] == BAKER.BUNDLE_MAGIC and header[1] == 3
    assert header[3] == len(first) and header[11] == 7
    assert header[5:8] == (32.0, 36.0, -16.0)
    assert abs(header[8]) < 1e-6 and abs(header[9]) < 1e-6
    assert abs(header[10] + 1.0) < 1e-6

    directory = chunks(first)
    assert directory[b"VERT"][2:] == (4, BAKER.VERTEX.size)
    assert directory[b"INDX"][2:] == (6, BAKER.INDEX.size)
    assert directory[b"DRAW"][2:] == (1, BAKER.DRAW.size)
    assert directory[b"LMHD"][2:] == (1, BAKER.IMAGE.size)
    assert directory[b"LMPX"][2:] == (448, 4)
    assert directory[b"TEXM"][2:] == (1, BAKER.TEXTURE.size)
    assert directory[b"TEXP"][2:] == (32512, 1)
    assert directory[b"LMPX"][0] % 256 == 0
    assert directory[b"TEXP"][0] % 256 == 0
    vertex_offset = directory[b"VERT"][0]
    assert BAKER.VERTEX.unpack_from(first, vertex_offset) == (
        0.0, 0.0, -0.0, 0.0, 0.0, 0.0234375,
        0.2142857164144516, 0,
    )
    index_offset = directory[b"INDX"][0]
    assert struct.unpack_from("<6H", first, index_offset) == (0, 1, 2, 0, 2, 3)
    draw_offset = directory[b"DRAW"][0]
    assert BAKER.DRAW.unpack_from(first, draw_offset)[:5] == (0, 6, 0, 0, 0)
    light_header = BAKER.IMAGE.unpack_from(first, directory[b"LMHD"][0])
    assert light_header == (64, 7, 256, BAKER.IMAGE_FORMAT_RGBA8_UNORM)
    texture_header = BAKER.TEXTURE.unpack_from(first, directory[b"TEXM"][0])
    assert texture_header == (
        0, 32512, 64, 64, 256, BAKER.IMAGE_FORMAT_RGBA8_UNORM,
        BAKER._fnv1a32(b"{TEST"), BAKER.TEXTURE_FLAG_TRANSPARENT,
        7, 0, 0, 0,
    )
    texture_at = directory[b"TEXP"][0]
    # AddrLib linear order is mip6..mip0.  Mip0 begins after 16,128 bytes.
    assert first[texture_at:texture_at + 4] == bytes((128, 128, 128, 254))
    mip0_at = texture_at + 16128
    assert first[mip0_at:mip0_at + 4] == bytes((0, 1, 255, 255))
    assert first[mip0_at + 255 * 4:mip0_at + 256 * 4] == \
        bytes((255, 0, 0, 0))

    texture_lump_offset = struct.unpack_from(
        "<I", source, 4 + BAKER.LUMP_TEXTURES * 8)[0]
    external = bytearray(source)
    struct.pack_into("<4I", external, texture_lump_offset + 8 + 24,
                     0, 0, 0, 0)
    external_bundle = BAKER.bake(bytes(external))
    external_directory = chunks(external_bundle)
    external_texture = BAKER.TEXTURE.unpack_from(
        external_bundle, external_directory[b"TEXM"][0])
    assert external_texture[7] == (BAKER.TEXTURE_FLAG_TRANSPARENT |
                                    BAKER.TEXTURE_FLAG_FALLBACK)

    sky = bytearray(source)
    sky[texture_lump_offset + 8:texture_lump_offset + 24] = \
        struct.pack("<16s", b"sky")
    sky_bundle = BAKER.bake(bytes(sky))
    sky_directory = chunks(sky_bundle)
    sky_texture = BAKER.TEXTURE.unpack_from(
        sky_bundle, sky_directory[b"TEXM"][0])
    assert sky_texture[7] == BAKER.TEXTURE_FLAG_SKY

    nodraw = bytearray(source)
    nodraw[texture_lump_offset + 8:texture_lump_offset + 24] = \
        struct.pack("<16s", b"aaatrigger")
    must_fail(bytes(nodraw), "no renderable faces")

    wrong_version = bytearray(source)
    struct.pack_into("<I", wrong_version, 0, 29)
    must_fail(bytes(wrong_version), "expected BSP v30")
    outside = bytearray(source)
    struct.pack_into("<II", outside, 4 + BAKER.LUMP_VERTICES * 8,
                     len(outside) + 1, 12)
    must_fail(bytes(outside), "outside the file")
    no_spawn = source.replace(b"info_player_start", b"info_player_wrong")
    must_fail(no_spawn, "no player spawn")
    original_index_limit = BAKER.MAX_INDEXED_VERTICES
    BAKER.MAX_INDEXED_VERTICES = 3
    try:
        must_fail(source, "uint16 index limit")
    finally:
        BAKER.MAX_INDEXED_VERTICES = original_index_limit
    bad_light = bytearray(source)
    face_offset = struct.unpack_from("<I", source,
                                     4 + BAKER.LUMP_FACES * 8)[0]
    struct.pack_into("<i", bad_light, face_offset + 16, 1024)
    must_fail(bytes(bad_light), "lightmap range")
    bad_palette = bytearray(source)
    struct.pack_into("<H", bad_palette, texture_lump_offset + 8 + 5480, 0)
    must_fail(bytes(bad_palette), "palette is invalid")
    print("BSP baker tests passed: deterministic geometry, lightmaps and base textures")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
