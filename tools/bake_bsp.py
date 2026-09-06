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
LUMP_LIGHTING = 8
LUMP_EDGES = 12
LUMP_SURFEDGES = 13

BUNDLE_MAGIC = b"PS5BSP\0\0"
BUNDLE_VERSION = 2
BUNDLE_HEADER = struct.Struct("<8sIIII3f3f4I")
CHUNK_HEADER = struct.Struct("<4sIIIIIII")
VERTEX = struct.Struct("<3f2f2fI")
DRAW = struct.Struct("<8I")
INDEX = struct.Struct("<H")
IMAGE = struct.Struct("<4I")
TEXTURE = struct.Struct("<8I")

MAX_INPUT_BYTES = 512 * 1024 * 1024
MAX_OUTPUT_BYTES = 64 * 1024 * 1024
MAX_DECODED_TEXTURE_BYTES = 48 * 1024 * 1024
MAX_FACES = 1_000_000
MAX_FACE_EDGES = 4096
MAX_INDEXED_VERTICES = 1 << 16
PLAYER_EYE_HEIGHT = 28.0
ATLAS_MAX_DIMENSION = 2048
ATLAS_GUTTER = 1
IMAGE_FORMAT_RGBA8_UNORM = 1
TEXTURE_FLAG_TRANSPARENT = 1
TEXTURE_FLAG_FALLBACK = 2


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
class BaseTexture:
    name_hash: int
    width: int
    height: int
    rgba: bytes
    flags: int


@dataclass(frozen=True)
class Face:
    first_surfedge: int
    surfedge_count: int
    texinfo: int
    styles: tuple[int, int, int, int]
    light_offset: int


@dataclass(frozen=True)
class FaceGeometry:
    polygon: tuple[int, ...]
    texture_min_s: int
    texture_min_t: int
    light_width: int
    light_height: int


@dataclass(frozen=True)
class AtlasPlacement:
    x: int
    y: int
    width: int
    height: int


@dataclass(frozen=True)
class Chunk:
    tag: bytes
    data: bytes
    count: int
    stride: int
    alignment: int = 16


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


def _fnv1a32(data: bytes) -> int:
    value = 0x811C9DC5
    for byte in data:
        value = ((value ^ byte) * 0x01000193) & 0xFFFFFFFF
    return value


def _fallback_texture(width: int, height: int) -> bytes:
    pixels = bytearray()
    for y in range(height):
        for x in range(width):
            bright = ((x // 8) ^ (y // 8)) & 1
            pixels += bytes((255, 0 if bright else 64, 255, 255))
    return bytes(pixels)


def _parse_base_textures(data: bytes, lump: Lump) -> list[BaseTexture]:
    if lump.size < 4:
        raise BakeError("texture lump is truncated")
    count = struct.unpack_from("<I", data, lump.offset)[0]
    if count > MAX_FACES or 4 + count * 4 > lump.size:
        raise BakeError("texture directory is truncated or excessive")
    textures: list[BaseTexture] = []
    decoded_bytes = 0
    for index in range(count):
        relative = struct.unpack_from("<i", data, lump.offset + 4 + index * 4)[0]
        if relative < 0:
            if relative != -1:
                raise BakeError(f"miptex {index} has an invalid directory offset")
            if 16 * 16 * 4 > MAX_DECODED_TEXTURE_BYTES - decoded_bytes:
                raise BakeError("decoded base textures exceed the Phase 1 size limit")
            decoded_bytes += 16 * 16 * 4
            name = f"missing-{index}".encode()
            textures.append(BaseTexture(_fnv1a32(name), 16, 16,
                                        _fallback_texture(16, 16),
                                        TEXTURE_FLAG_FALLBACK))
            continue
        if relative > lump.size - 40:
            raise BakeError(f"miptex {index} header is outside the texture lump")
        name_raw, width, height, *offsets = struct.unpack_from(
            "<16sII4I", data, lump.offset + relative)
        name = name_raw.split(b"\0", 1)[0]
        if not name:
            raise BakeError(f"miptex {index} has an empty name")
        if width == 0 or height == 0 or width > 16384 or height > 16384:
            raise BakeError(f"miptex {index} has invalid dimensions")
        rgba_bytes = width * height * 4
        if rgba_bytes > MAX_DECODED_TEXTURE_BYTES - decoded_bytes:
            raise BakeError("decoded base textures exceed the Phase 1 size limit")
        decoded_bytes += rgba_bytes
        name_hash = _fnv1a32(name)
        if name_hash == 0:
            raise BakeError(f"miptex {index} has an unsupported zero name hash")
        flags = TEXTURE_FLAG_TRANSPARENT if name.startswith(b"{") else 0
        if offsets[0] == 0:
            if any(offsets):
                raise BakeError(f"miptex {index} has an incomplete mip chain")
            textures.append(BaseTexture(name_hash, width, height,
                                        _fallback_texture(width, height),
                                        flags | TEXTURE_FLAG_FALLBACK))
            continue
        dimensions = [(max(1, width >> level), max(1, height >> level))
                      for level in range(4)]
        ranges = []
        for level, (offset, (mip_width, mip_height)) in enumerate(
                zip(offsets, dimensions)):
            mip_bytes = mip_width * mip_height
            mip_at = relative + offset
            if offset < 40 or mip_at > lump.size or \
                    mip_bytes > lump.size - mip_at:
                raise BakeError(f"miptex {index} mip {level} range is invalid")
            ranges.append((mip_at, mip_bytes))
        for level in range(1, 4):
            prior_at, prior_bytes = ranges[level - 1]
            if ranges[level][0] < prior_at + prior_bytes:
                raise BakeError(f"miptex {index} mip chain overlaps")
        mip_at, mip_bytes = ranges[0]
        palette_at = ranges[3][0] + ranges[3][1]
        if palette_at > lump.size - 2:
            raise BakeError(f"miptex {index} palette range is invalid")
        palette_count = struct.unpack_from("<H", data,
                                           lump.offset + palette_at)[0]
        if palette_count != 256 or \
                palette_count * 3 > lump.size - palette_at - 2:
            raise BakeError(f"miptex {index} palette is invalid")
        indices = data[lump.offset + mip_at:lump.offset + mip_at + mip_bytes]
        palette = data[lump.offset + palette_at + 2:
                       lump.offset + palette_at + 2 + palette_count * 3]
        rgba = bytearray()
        for palette_index in indices:
            if palette_index >= palette_count:
                raise BakeError(f"miptex {index} palette index is invalid")
            at = palette_index * 3
            alpha = 0 if flags & TEXTURE_FLAG_TRANSPARENT and palette_index == 255 else 255
            rgba += palette[at:at + 3] + bytes((alpha,))
        textures.append(BaseTexture(name_hash, width, height,
                                    bytes(rgba), flags))
    return textures


def _texture_chunks(textures: list[BaseTexture]) -> tuple[bytes, bytes]:
    if not textures:
        raise BakeError("BSP texture directory is empty")
    metadata = bytearray()
    pixels = bytearray()
    for texture in textures:
        aligned = _align(len(pixels), 256)
        pixels += bytes(aligned - len(pixels))
        offset = len(pixels)
        row_pitch = _align(texture.width * 4, 256)
        for row in range(texture.height):
            source = row * texture.width * 4
            pixels += texture.rgba[source:source + texture.width * 4]
            pixels += bytes(row_pitch - texture.width * 4)
        texture_bytes = row_pitch * texture.height
        metadata += TEXTURE.pack(offset, texture_bytes, texture.width,
                                 texture.height, row_pitch,
                                 IMAGE_FORMAT_RGBA8_UNORM,
                                 texture.name_hash, texture.flags)
    return bytes(metadata), bytes(pixels)


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


def _next_power_of_two(value: int) -> int:
    return 1 << max(0, value - 1).bit_length()


def _pack_atlas(sizes: list[tuple[int, int]]) -> tuple[int, int, list[AtlasPlacement | None]]:
    """Pack face lightmaps into deterministic shelves with one-texel gutters."""
    if not sizes:
        return 64, 1, []
    largest = max(width + 2 * ATLAS_GUTTER for width, _height in sizes)
    area = sum((width + 2 * ATLAS_GUTTER) *
               (height + 2 * ATLAS_GUTTER) for width, height in sizes)
    width = max(64, _next_power_of_two(max(largest, math.isqrt(area))))
    while width <= ATLAS_MAX_DIMENSION:
        placements: list[AtlasPlacement | None] = []
        x = y = row_height = 0
        failed = False
        for item_width, item_height in sizes:
            packed_width = item_width + 2 * ATLAS_GUTTER
            packed_height = item_height + 2 * ATLAS_GUTTER
            if packed_width > width:
                failed = True
                break
            if x + packed_width > width:
                x = 0
                y += row_height
                row_height = 0
            if y + packed_height > ATLAS_MAX_DIMENSION:
                failed = True
                break
            placements.append(AtlasPlacement(
                x + ATLAS_GUTTER, y + ATLAS_GUTTER,
                item_width, item_height,
            ))
            x += packed_width
            row_height = max(row_height, packed_height)
        if not failed:
            height = max(1, y + row_height)
            return width, height, placements
        width *= 2
    raise BakeError("lightmap atlas exceeds the 2048x2048 contract")


def _face_geometry(face_index: int, face: Face,
                   vertices: list[tuple[float, float, float]],
                   edges: list[tuple[int, int]], surfedges: list[int],
                   texture: TextureInfo) -> FaceGeometry:
    if face.surfedge_count < 3 or face.surfedge_count > MAX_FACE_EDGES:
        raise BakeError(f"face {face_index} has invalid edge count")
    if face.first_surfedge < 0 or face.first_surfedge > len(surfedges) - face.surfedge_count:
        raise BakeError(f"face {face_index} surfedge range is invalid")
    polygon: list[int] = []
    texture_s: list[float] = []
    texture_t: list[float] = []
    for surfedge in surfedges[face.first_surfedge:
                              face.first_surfedge + face.surfedge_count]:
        edge_index = abs(surfedge)
        if edge_index >= len(edges):
            raise BakeError(f"face {face_index} references an invalid edge")
        vertex_index = edges[edge_index][0 if surfedge >= 0 else 1]
        if vertex_index >= len(vertices):
            raise BakeError(f"face {face_index} references an invalid vertex")
        source = vertices[vertex_index]
        if not all(math.isfinite(value) for value in source):
            raise BakeError(f"face {face_index} has a non-finite vertex")
        polygon.append(vertex_index)
        texture_s.append(sum(source[i] * texture.s[i] for i in range(3)) + texture.s[3])
        texture_t.append(sum(source[i] * texture.t[i] for i in range(3)) + texture.t[3])
    minimum_s = math.floor(min(texture_s) / 16.0)
    minimum_t = math.floor(min(texture_t) / 16.0)
    maximum_s = math.ceil(max(texture_s) / 16.0)
    maximum_t = math.ceil(max(texture_t) / 16.0)
    light_width = maximum_s - minimum_s + 1
    light_height = maximum_t - minimum_t + 1
    if light_width <= 0 or light_height <= 0 or \
            light_width > ATLAS_MAX_DIMENSION - 2 or \
            light_height > ATLAS_MAX_DIMENSION - 2:
        raise BakeError(f"face {face_index} has invalid lightmap extents")
    return FaceGeometry(tuple(polygon), minimum_s * 16, minimum_t * 16,
                        light_width, light_height)


def _lightmap_atlas(faces: list[Face], geometry: list[FaceGeometry],
                    lighting: bytes) -> tuple[bytes, bytes, list[AtlasPlacement | None]]:
    lit_faces: list[int] = []
    sizes: list[tuple[int, int]] = []
    for face_index, (face, shape) in enumerate(zip(faces, geometry)):
        style_count = next((index for index, style in enumerate(face.styles)
                            if style == 255), len(face.styles))
        if face.light_offset < 0:
            continue
        if style_count == 0:
            raise BakeError(f"face {face_index} has light data without a style")
        sample_bytes = shape.light_width * shape.light_height * 3
        total_bytes = sample_bytes * style_count
        if face.light_offset > len(lighting) or total_bytes > len(lighting) - face.light_offset:
            raise BakeError(f"face {face_index} lightmap range is invalid")
        lit_faces.append(face_index)
        sizes.append((shape.light_width, shape.light_height))

    atlas_width, atlas_height, packed = _pack_atlas(sizes)
    pixels = bytearray([255]) * (atlas_width * atlas_height * 4)
    placements: list[AtlasPlacement | None] = [None] * len(faces)
    for face_index, placement in zip(lit_faces, packed):
        assert placement is not None
        placements[face_index] = placement
        face = faces[face_index]
        shape = geometry[face_index]
        source = lighting[face.light_offset:face.light_offset +
                          shape.light_width * shape.light_height * 3]
        for row in range(shape.light_height):
            for column in range(shape.light_width):
                source_at = (row * shape.light_width + column) * 3
                rgba = source[source_at:source_at + 3] + b"\xff"
                target = ((placement.y + row) * atlas_width +
                          placement.x + column) * 4
                pixels[target:target + 4] = rgba
        for row in range(-1, shape.light_height + 1):
            source_row = min(max(row, 0), shape.light_height - 1)
            for column in range(-1, shape.light_width + 1):
                if 0 <= row < shape.light_height and 0 <= column < shape.light_width:
                    continue
                source_column = min(max(column, 0), shape.light_width - 1)
                source_at = ((placement.y + source_row) * atlas_width +
                             placement.x + source_column) * 4
                target = ((placement.y + row) * atlas_width +
                          placement.x + column) * 4
                pixels[target:target + 4] = pixels[source_at:source_at + 4]
    header = IMAGE.pack(atlas_width, atlas_height, atlas_width * 4,
                        IMAGE_FORMAT_RGBA8_UNORM)
    return header, bytes(pixels), placements


def _chunk_bytes(chunks: list[Chunk], camera: tuple[float, float, float],
                 forward: tuple[float, float, float]) -> bytes:
    header_bytes = _align(BUNDLE_HEADER.size + len(chunks) * CHUNK_HEADER.size)
    offsets: list[int] = []
    cursor = header_bytes
    for chunk in chunks:
        cursor = _align(cursor, chunk.alignment)
        offsets.append(cursor)
        cursor += len(chunk.data)
    file_bytes = cursor
    if file_bytes > MAX_OUTPUT_BYTES:
        raise BakeError("bundle exceeds the native Phase 1 size limit")
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
    faces = [Face(row[2], row[3], row[4], tuple(row[5:9]), row[9])
             for row in raw_faces]
    base_textures = _parse_base_textures(data, lumps[LUMP_TEXTURES])
    texture_sizes = [(texture.width, texture.height)
                     for texture in base_textures]
    texture_metadata, texture_pixels = _texture_chunks(base_textures)
    lighting = data[lumps[LUMP_LIGHTING].offset:
                    lumps[LUMP_LIGHTING].offset + lumps[LUMP_LIGHTING].size]
    camera, forward = _spawn_camera(
        data[lumps[LUMP_ENTITIES].offset:
             lumps[LUMP_ENTITIES].offset + lumps[LUMP_ENTITIES].size])

    geometry: list[FaceGeometry] = []
    for face_index, face in enumerate(faces):
        if face.texinfo < 0 or face.texinfo >= len(texinfo):
            raise BakeError(f"face {face_index} texinfo is invalid")
        texture = texinfo[face.texinfo]
        if texture.miptex < 0 or texture.miptex >= len(texture_sizes):
            raise BakeError(f"face {face_index} miptex is invalid")
        geometry.append(_face_geometry(face_index, face, vertices, edges,
                                       surfedges, texture))
    light_header, light_pixels, light_placements = _lightmap_atlas(
        faces, geometry, lighting)
    atlas_width, atlas_height, _pitch, _format = IMAGE.unpack(light_header)

    vertex_blob = bytearray()
    index_blob = bytearray()
    draws: list[tuple[int, int, bytes]] = []
    emitted_vertices = 0
    emitted_indices = 0
    for face_index, (face, shape) in enumerate(zip(faces, geometry)):
        texture = texinfo[face.texinfo]
        width, height = texture_sizes[texture.miptex]
        if emitted_vertices + len(shape.polygon) > MAX_INDEXED_VERTICES:
            raise BakeError("renderable vertices exceed the uint16 index limit")

        first_index = emitted_indices
        placement = light_placements[face_index]
        for vertex_index in shape.polygon:
            source = vertices[vertex_index]
            texture_s = sum(source[i] * texture.s[i] for i in range(3)) + texture.s[3]
            texture_t = sum(source[i] * texture.t[i] for i in range(3)) + texture.t[3]
            base_s = texture_s / width
            base_t = texture_t / height
            if placement is None:
                light_s = 0.5 / atlas_width
                light_t = 0.5 / atlas_height
            else:
                light_s = (placement.x +
                           (texture_s - shape.texture_min_s) / 16.0 + 0.5) / atlas_width
                light_t = (placement.y +
                           (texture_t - shape.texture_min_t) / 16.0 + 0.5) / atlas_height
            converted = (source[0], source[2], -source[1])
            vertex_blob += VERTEX.pack(*converted, base_s, base_t,
                                       light_s, light_t, face_index)
        for corner in range(1, len(shape.polygon) - 1):
            for local_index in (0, corner, corner + 1):
                index_blob += INDEX.pack(emitted_vertices + local_index)
                emitted_indices += 1
        index_count = emitted_indices - first_index
        draw = DRAW.pack(first_index, index_count, texture.miptex,
                         0 if placement is not None else 0xFFFFFFFF,
                         face_index, 0, 0, 0)
        draws.append((texture.miptex, face_index, draw))
        emitted_vertices += len(shape.polygon)

    if not faces or emitted_indices == 0:
        raise BakeError("BSP contains no renderable faces")
    draws.sort(key=lambda item: (item[0], item[1]))
    draw_blob = b"".join(item[2] for item in draws)
    return _chunk_bytes([
        Chunk(b"VERT", bytes(vertex_blob), emitted_vertices, VERTEX.size),
        Chunk(b"INDX", bytes(index_blob), emitted_indices, INDEX.size),
        Chunk(b"DRAW", draw_blob, len(draws), DRAW.size),
        Chunk(b"LMHD", light_header, 1, IMAGE.size),
        Chunk(b"LMPX", light_pixels, atlas_width * atlas_height, 4, 256),
        Chunk(b"TEXM", texture_metadata, len(base_textures), TEXTURE.size),
        Chunk(b"TEXP", texture_pixels, len(texture_pixels), 1, 256),
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
