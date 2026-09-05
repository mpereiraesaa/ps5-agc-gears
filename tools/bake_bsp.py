#!/usr/bin/env python3
"""Bake a GoldSrc BSP v30 into a deterministic, runtime-only PS5 bundle.

Only public BSP structures are parsed here.  The output contains geometry and
the fixed spawn camera needed by Phase 1 gate 1; later texture/lightmap chunks
can be appended without changing the header or the existing chunk contracts.
"""

from __future__ import annotations

import argparse
import math
import re
import struct
import tempfile
import zlib
from dataclasses import dataclass
from pathlib import Path


BSP_VERSION = 30
BSP_LUMP_COUNT = 15
BSP_HEADER_BYTES = 4 + BSP_LUMP_COUNT * 8
LUMP_ENTITIES = 0
LUMP_TEXTURES = 2
LUMP_VERTICES = 3
LUMP_TEXINFO = 6
LUMP_FACES = 7
LUMP_EDGES = 12
LUMP_SURFEDGES = 13

BUNDLE_MAGIC = b"PS5BSP\0\0"
BUNDLE_VERSION = 1
BUNDLE_HEADER = struct.Struct("<8sIIII3f3f4I")
CHUNK_HEADER = struct.Struct("<4sIIIIIII")
VERTEX = struct.Struct("<3f2f2fI")
DRAW = struct.Struct("<8I")
INDEX = struct.Struct("<I")

MAX_INPUT_BYTES = 512 * 1024 * 1024
MAX_FACES = 1_000_000
MAX_FACE_EDGES = 4096
PLAYER_EYE_HEIGHT = 28.0


class BakeError(ValueError):
    """Input is not a supported, internally consistent BSP v30 file."""


@dataclass(frozen=True)
class Lump:
    offset: int
    size: int


@dataclass(frozen=True)
class TextureInfo:
    s: tuple[float, float, float, float]
    t: tuple[float, float, float, float]
    miptex: int


@dataclass(frozen=True)
class Face:
    first_surfedge: int
    surfedge_count: int
    texinfo: int


@dataclass(frozen=True)
class Chunk:
    tag: bytes
    data: bytes
    count: int
    stride: int


def _records(data: bytes, lump: Lump, record: struct.Struct, label: str):
    if lump.size % record.size:
        raise BakeError(f"{label} lump has a partial record")
    return [record.unpack_from(data, lump.offset + offset)
            for offset in range(0, lump.size, record.size)]


def _parse_lumps(data: bytes) -> list[Lump]:
    if len(data) < BSP_HEADER_BYTES:
        raise BakeError("truncated BSP header")
    version = struct.unpack_from("<I", data)[0]
    if version != BSP_VERSION:
        raise BakeError(f"expected BSP v{BSP_VERSION}, got v{version}")
    lumps = []
    for index in range(BSP_LUMP_COUNT):
        offset, size = struct.unpack_from("<II", data, 4 + index * 8)
        if offset < BSP_HEADER_BYTES or offset > len(data) or size > len(data) - offset:
            raise BakeError(f"lump {index} is outside the file")
        lumps.append(Lump(offset, size))
    return lumps


def _parse_texture_sizes(data: bytes, lump: Lump) -> list[tuple[int, int]]:
    if lump.size < 4:
        raise BakeError("texture lump is truncated")
    count = struct.unpack_from("<I", data, lump.offset)[0]
    if count > MAX_FACES or 4 + count * 4 > lump.size:
        raise BakeError("texture directory is truncated or excessive")
    sizes = []
    for index in range(count):
        relative = struct.unpack_from("<i", data, lump.offset + 4 + index * 4)[0]
        if relative < 0:
            sizes.append((1, 1))
            continue
        if relative > lump.size - 40:
            raise BakeError(f"miptex {index} header is outside the texture lump")
        width, height = struct.unpack_from("<II", data, lump.offset + relative + 16)
        if width == 0 or height == 0 or width > 16384 or height > 16384:
            raise BakeError(f"miptex {index} has invalid dimensions")
        sizes.append((width, height))
    return sizes


def _entity_pairs(block: str) -> dict[str, str]:
    # GoldSrc entity text uses quoted, backslash-escaped key/value pairs.
    tokens = re.findall(r'"((?:\\.|[^"\\])*)"', block)
    if len(tokens) % 2:
        return {}
    def unescape(value: str) -> str:
        return re.sub(r'\\(["\\])', r'\1', value)
    return {unescape(tokens[i]): unescape(tokens[i + 1])
            for i in range(0, len(tokens), 2)}


def _spawn_camera(entity_bytes: bytes) -> tuple[tuple[float, float, float],
                                                 tuple[float, float, float]]:
    text = entity_bytes.rstrip(b"\0").decode("latin-1")
    selected: dict[str, str] | None = None
    for block in re.findall(r"\{([^{}]*)\}", text, re.DOTALL):
        values = _entity_pairs(block)
        if values.get("classname") in ("info_player_start", "info_player_deathmatch"):
            selected = values
            if values.get("classname") == "info_player_start":
                break
    if selected is None:
        raise BakeError("BSP has no player spawn entity")
    try:
        origin = tuple(float(value) for value in selected.get("origin", "0 0 0").split())
        if len(origin) != 3 or not all(math.isfinite(value) for value in origin):
            raise ValueError
        if "angles" in selected:
            angles = tuple(float(value) for value in selected["angles"].split())
            if len(angles) != 3:
                raise ValueError
            pitch, yaw, _roll = angles
        else:
            pitch, yaw = 0.0, float(selected.get("angle", "0"))
        if not math.isfinite(pitch) or not math.isfinite(yaw):
            raise ValueError
    except ValueError as error:
        raise BakeError("spawn entity has invalid origin or angles") from error

    # GoldSrc is Z-up.  The viewer is right-handed Y-up: (x, y, z) ->
    # (x, z, -y).  Store a direction vector so no runtime angle conversion is
    # needed.  The fixed camera starts at the conventional standing eye.
    source_eye = (origin[0], origin[1], origin[2] + PLAYER_EYE_HEIGHT)
    camera = (source_eye[0], source_eye[2], -source_eye[1])
    pitch_radians = math.radians(pitch)
    yaw_radians = math.radians(yaw)
    source_forward = (
        math.cos(pitch_radians) * math.cos(yaw_radians),
        math.cos(pitch_radians) * math.sin(yaw_radians),
        -math.sin(pitch_radians),
    )
    forward = (source_forward[0], source_forward[2], -source_forward[1])
    return camera, forward


def _align(value: int, alignment: int = 16) -> int:
    return (value + alignment - 1) & -alignment


def _chunk_bytes(chunks: list[Chunk], camera: tuple[float, float, float],
                 forward: tuple[float, float, float]) -> bytes:
    header_bytes = _align(BUNDLE_HEADER.size + len(chunks) * CHUNK_HEADER.size)
    offsets: list[int] = []
    cursor = header_bytes
    for chunk in chunks:
        cursor = _align(cursor)
        offsets.append(cursor)
        cursor += len(chunk.data)
    file_bytes = cursor
    output = bytearray(file_bytes)
    for chunk, offset in zip(chunks, offsets):
        output[offset:offset + len(chunk.data)] = chunk.data
    payload_crc = zlib.crc32(output[header_bytes:]) & 0xFFFFFFFF
    BUNDLE_HEADER.pack_into(
        output, 0, BUNDLE_MAGIC, BUNDLE_VERSION, header_bytes, file_bytes,
        payload_crc, *camera, *forward, len(chunks), 0, 0, 0,
    )
    descriptor = BUNDLE_HEADER.size
    for chunk, offset in zip(chunks, offsets):
        CHUNK_HEADER.pack_into(
            output, descriptor, chunk.tag, offset, len(chunk.data),
            chunk.count, chunk.stride, zlib.crc32(chunk.data) & 0xFFFFFFFF,
            0, 0,
        )
        descriptor += CHUNK_HEADER.size
    return bytes(output)


def bake(data: bytes) -> bytes:
    if len(data) > MAX_INPUT_BYTES:
        raise BakeError("BSP exceeds the host baker size limit")
    lumps = _parse_lumps(data)
    vertices = _records(data, lumps[LUMP_VERTICES], struct.Struct("<3f"), "vertex")
    edges = _records(data, lumps[LUMP_EDGES], struct.Struct("<2H"), "edge")
    surfedges = [value[0] for value in
                 _records(data, lumps[LUMP_SURFEDGES], struct.Struct("<i"), "surfedge")]
    raw_texinfo = _records(data, lumps[LUMP_TEXINFO], struct.Struct("<8fii"), "texinfo")
    texinfo = [TextureInfo(tuple(row[0:4]), tuple(row[4:8]), row[8])
               for row in raw_texinfo]
    raw_faces = _records(data, lumps[LUMP_FACES], struct.Struct("<Hhihh4Bi"), "face")
    if len(raw_faces) > MAX_FACES:
        raise BakeError("face count exceeds the host baker limit")
    faces = [Face(row[2], row[3], row[4]) for row in raw_faces]
    texture_sizes = _parse_texture_sizes(data, lumps[LUMP_TEXTURES])
    camera, forward = _spawn_camera(
        data[lumps[LUMP_ENTITIES].offset:
             lumps[LUMP_ENTITIES].offset + lumps[LUMP_ENTITIES].size])

    vertex_blob = bytearray()
    index_blob = bytearray()
    draws: list[tuple[int, int, bytes]] = []
    emitted_vertices = 0
    emitted_indices = 0
    for face_index, face in enumerate(faces):
        if face.surfedge_count < 3 or face.surfedge_count > MAX_FACE_EDGES:
            raise BakeError(f"face {face_index} has invalid edge count")
        if face.first_surfedge < 0 or face.first_surfedge > len(surfedges) - face.surfedge_count:
            raise BakeError(f"face {face_index} surfedge range is invalid")
        if face.texinfo < 0 or face.texinfo >= len(texinfo):
            raise BakeError(f"face {face_index} texinfo is invalid")
        texture = texinfo[face.texinfo]
        if texture.miptex < 0 or texture.miptex >= len(texture_sizes):
            raise BakeError(f"face {face_index} miptex is invalid")
        width, height = texture_sizes[texture.miptex]
        polygon: list[int] = []
        for surfedge in surfedges[face.first_surfedge:
                                  face.first_surfedge + face.surfedge_count]:
            edge_index = abs(surfedge)
            if edge_index >= len(edges):
                raise BakeError(f"face {face_index} references an invalid edge")
            vertex_index = edges[edge_index][0 if surfedge >= 0 else 1]
            if vertex_index >= len(vertices):
                raise BakeError(f"face {face_index} references an invalid vertex")
            polygon.append(vertex_index)

        first_index = emitted_indices
        for vertex_index in polygon:
            source = vertices[vertex_index]
            if not all(math.isfinite(value) for value in source):
                raise BakeError(f"face {face_index} has a non-finite vertex")
            base_s = (sum(source[i] * texture.s[i] for i in range(3)) + texture.s[3]) / width
            base_t = (sum(source[i] * texture.t[i] for i in range(3)) + texture.t[3]) / height
            converted = (source[0], source[2], -source[1])
            vertex_blob += VERTEX.pack(*converted, base_s, base_t, 0.0, 0.0, face_index)
        for corner in range(1, len(polygon) - 1):
            for local_index in (0, corner, corner + 1):
                index_blob += INDEX.pack(emitted_vertices + local_index)
                emitted_indices += 1
        index_count = emitted_indices - first_index
        draw = DRAW.pack(first_index, index_count, texture.miptex, 0xFFFFFFFF,
                         face_index, 0, 0, 0)
        draws.append((texture.miptex, face_index, draw))
        emitted_vertices += len(polygon)

    if not faces or emitted_indices == 0:
        raise BakeError("BSP contains no renderable faces")
    draws.sort(key=lambda item: (item[0], item[1]))
    draw_blob = b"".join(item[2] for item in draws)
    return _chunk_bytes([
        Chunk(b"VERT", bytes(vertex_blob), emitted_vertices, VERTEX.size),
        Chunk(b"INDX", bytes(index_blob), emitted_indices, INDEX.size),
        Chunk(b"DRAW", draw_blob, len(draws), DRAW.size),
    ], camera, forward)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="private BSP v30 input")
    parser.add_argument("output", type=Path, help="generated .ps5bsp bundle")
    args = parser.parse_args()
    source = args.input.read_bytes()
    result = bake(source)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=args.output.parent, delete=False) as temporary:
        temporary.write(result)
        temporary_path = Path(temporary.name)
    temporary_path.replace(args.output)
    print(f"wrote {len(result)} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
