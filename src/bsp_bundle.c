#include "bsp_bundle.h"

#include <limits.h>
#include <string.h>

enum {
    BUNDLE_HEADER_BYTES = 64,
    BUNDLE_CHUNK_BYTES = 32,
    BUNDLE_VERSION = 1,
    MAX_CHUNKS = 64,
};

typedef struct ChunkView {
    uint32_t tag;
    uint32_t offset;
    uint32_t bytes;
    uint32_t count;
    uint32_t stride;
} ChunkView;

_Static_assert(sizeof(BspBundleVertex) == 32u, "bundle vertex ABI");
_Static_assert(sizeof(BspBundleDraw) == 32u, "bundle draw ABI");
_Static_assert(sizeof(BspBundleImage) == 16u, "bundle image ABI");
_Static_assert(sizeof(BspBundleTexture) == 32u, "bundle texture ABI");

static uint32_t load_u32(const uint8_t *at)
{
    uint32_t value;
    memcpy(&value, at, sizeof(value));
    return value;
}

static float load_f32(const uint8_t *at)
{
    float value;
    memcpy(&value, at, sizeof(value));
    return value;
}

static uint32_t crc32_bytes(const uint8_t *data, size_t bytes)
{
    uint32_t crc = UINT32_C(0xffffffff);
    for (size_t index = 0; index < bytes; ++index) {
        crc ^= data[index];
        for (unsigned bit = 0; bit < 8u; ++bit)
            crc = (crc >> 1) ^ (UINT32_C(0xedb88320) &
                                (uint32_t)-(int32_t)(crc & 1u));
    }
    return ~crc;
}

static int tag_equal(uint32_t tag, const char name[4])
{
    uint32_t expected;
    memcpy(&expected, name, sizeof(expected));
    return tag == expected;
}

int bsp_bundle_open(const void *opaque, size_t bytes, BspBundleView *view)
{
    static const uint8_t magic[8] = {'P','S','5','B','S','P',0,0};
    if (!opaque || !view)
        return BSP_BUNDLE_PRECONDITION;
    memset(view, 0, sizeof(*view));
    if (bytes < BUNDLE_HEADER_BYTES || bytes > UINT32_MAX)
        return BSP_BUNDLE_HEADER_INVALID;
    const uint8_t *const data = opaque;
    const uint32_t version = load_u32(data + 8u);
    const uint32_t header_bytes = load_u32(data + 12u);
    const uint32_t file_bytes = load_u32(data + 16u);
    const uint32_t payload_crc = load_u32(data + 20u);
    const uint32_t chunk_count = load_u32(data + 48u);
    if (memcmp(data, magic, sizeof(magic)) != 0 ||
        version != BUNDLE_VERSION || file_bytes != bytes ||
        chunk_count == 0u || chunk_count > MAX_CHUNKS ||
        header_bytes < BUNDLE_HEADER_BYTES + chunk_count * BUNDLE_CHUNK_BYTES ||
        header_bytes > bytes || (header_bytes & 15u) != 0u ||
        load_u32(data + 52u) != 0u || load_u32(data + 56u) != 0u ||
        load_u32(data + 60u) != 0u)
        return BSP_BUNDLE_HEADER_INVALID;
    if (crc32_bytes(data + header_bytes, bytes - header_bytes) != payload_crc)
        return BSP_BUNDLE_CHECKSUM_MISMATCH;

    ChunkView chunks[MAX_CHUNKS];
    memset(chunks, 0, sizeof(chunks));
    for (uint32_t index = 0; index < chunk_count; ++index) {
        const uint8_t *const row = data + BUNDLE_HEADER_BYTES +
                                   index * BUNDLE_CHUNK_BYTES;
        ChunkView *const chunk = &chunks[index];
        chunk->tag = load_u32(row);
        chunk->offset = load_u32(row + 4u);
        chunk->bytes = load_u32(row + 8u);
        chunk->count = load_u32(row + 12u);
        chunk->stride = load_u32(row + 16u);
        if (chunk->tag == 0u || chunk->offset < header_bytes ||
            (chunk->offset & 15u) != 0u || chunk->bytes == 0u ||
            chunk->stride == 0u || chunk->count == 0u ||
            chunk->count > UINT32_MAX / chunk->stride ||
            chunk->count * chunk->stride != chunk->bytes ||
            chunk->offset > bytes || chunk->bytes > bytes - chunk->offset ||
            load_u32(row + 24u) != 0u || load_u32(row + 28u) != 0u)
            return BSP_BUNDLE_DIRECTORY_INVALID;
        if (crc32_bytes(data + chunk->offset, chunk->bytes) !=
            load_u32(row + 20u))
            return BSP_BUNDLE_CHECKSUM_MISMATCH;
        for (uint32_t prior = 0; prior < index; ++prior) {
            const uint64_t begin = chunk->offset;
            const uint64_t end = begin + chunk->bytes;
            const uint64_t prior_begin = chunks[prior].offset;
            const uint64_t prior_end = prior_begin + chunks[prior].bytes;
            if (chunk->tag == chunks[prior].tag ||
                (begin < prior_end && prior_begin < end))
                return BSP_BUNDLE_DIRECTORY_INVALID;
        }
    }

    const ChunkView *vertex_chunk = 0;
    const ChunkView *index_chunk = 0;
    const ChunkView *draw_chunk = 0;
    const ChunkView *lightmap_header_chunk = 0;
    const ChunkView *lightmap_pixels_chunk = 0;
    const ChunkView *texture_metadata_chunk = 0;
    const ChunkView *texture_pixels_chunk = 0;
    for (uint32_t index = 0; index < chunk_count; ++index) {
        if (tag_equal(chunks[index].tag, "VERT")) vertex_chunk = &chunks[index];
        if (tag_equal(chunks[index].tag, "INDX")) index_chunk = &chunks[index];
        if (tag_equal(chunks[index].tag, "DRAW")) draw_chunk = &chunks[index];
        if (tag_equal(chunks[index].tag, "LMHD")) lightmap_header_chunk = &chunks[index];
        if (tag_equal(chunks[index].tag, "LMPX")) lightmap_pixels_chunk = &chunks[index];
        if (tag_equal(chunks[index].tag, "TEXM")) texture_metadata_chunk = &chunks[index];
        if (tag_equal(chunks[index].tag, "TEXP")) texture_pixels_chunk = &chunks[index];
    }
    if (!vertex_chunk || !index_chunk || !draw_chunk ||
        vertex_chunk->stride != sizeof(BspBundleVertex) ||
        index_chunk->stride != sizeof(uint32_t) ||
        draw_chunk->stride != sizeof(BspBundleDraw) ||
        index_chunk->count % 3u != 0u)
        return BSP_BUNDLE_GEOMETRY_INVALID;
    if (!!lightmap_header_chunk != !!lightmap_pixels_chunk)
        return BSP_BUNDLE_GEOMETRY_INVALID;
    if (lightmap_header_chunk) {
        if (lightmap_header_chunk->count != 1u ||
            lightmap_header_chunk->stride != sizeof(BspBundleImage) ||
            lightmap_pixels_chunk->stride != 4u)
            return BSP_BUNDLE_GEOMETRY_INVALID;
        const BspBundleImage *const image =
            (const BspBundleImage *)(data + lightmap_header_chunk->offset);
        if (image->width == 0u || image->height == 0u ||
            image->width > 2048u || image->height > 2048u ||
            (lightmap_pixels_chunk->offset & 255u) != 0u ||
            image->row_pitch != image->width * 4u ||
            (image->row_pitch & 255u) != 0u ||
            image->format != BSP_BUNDLE_IMAGE_RGBA8_UNORM ||
            image->width > UINT32_MAX / image->height ||
            image->width * image->height != lightmap_pixels_chunk->count)
            return BSP_BUNDLE_GEOMETRY_INVALID;
    }
    if (!!texture_metadata_chunk != !!texture_pixels_chunk)
        return BSP_BUNDLE_GEOMETRY_INVALID;
    if (texture_metadata_chunk) {
        if (texture_metadata_chunk->stride != sizeof(BspBundleTexture) ||
            texture_pixels_chunk->stride != 1u ||
            (texture_pixels_chunk->offset & 255u) != 0u)
            return BSP_BUNDLE_GEOMETRY_INVALID;
        const BspBundleTexture *const textures = (const BspBundleTexture *)(
            data + texture_metadata_chunk->offset);
        for (uint32_t index = 0; index < texture_metadata_chunk->count; ++index) {
            const BspBundleTexture *const texture = &textures[index];
            if (texture->width == 0u || texture->height == 0u ||
                texture->width > 16384u || texture->height > 16384u ||
                texture->row_pitch < texture->width * 4u ||
                (texture->row_pitch & 255u) != 0u ||
                texture->format != BSP_BUNDLE_IMAGE_RGBA8_UNORM ||
                (texture->flags & ~(BSP_BUNDLE_TEXTURE_TRANSPARENT |
                                    BSP_BUNDLE_TEXTURE_FALLBACK)) != 0u ||
                texture->name_hash == 0u ||
                texture->height > UINT32_MAX / texture->row_pitch ||
                texture->bytes != texture->height * texture->row_pitch ||
                (texture->offset & 255u) != 0u ||
                texture->offset > texture_pixels_chunk->bytes ||
                texture->bytes > texture_pixels_chunk->bytes - texture->offset)
                return BSP_BUNDLE_GEOMETRY_INVALID;
            if (index > 0u) {
                const BspBundleTexture *const prior = &textures[index - 1u];
                if (texture->offset < prior->offset + prior->bytes)
                    return BSP_BUNDLE_GEOMETRY_INVALID;
            }
        }
    }

    const uint32_t *const indices =
        (const uint32_t *)(data + index_chunk->offset);
    const BspBundleDraw *const draws =
        (const BspBundleDraw *)(data + draw_chunk->offset);
    for (uint32_t index = 0; index < index_chunk->count; ++index)
        if (indices[index] >= vertex_chunk->count)
            return BSP_BUNDLE_GEOMETRY_INVALID;
    for (uint32_t index = 0; index < draw_chunk->count; ++index) {
        const BspBundleDraw *const draw = &draws[index];
        if (draw->index_count == 0u || draw->index_count % 3u != 0u ||
            draw->first_index > index_chunk->count ||
            draw->index_count > index_chunk->count - draw->first_index ||
            draw->reserved[0] != 0u || draw->reserved[1] != 0u ||
            (!lightmap_header_chunk && draw->lightmap != UINT32_MAX) ||
            (lightmap_header_chunk && draw->lightmap != 0u &&
             draw->lightmap != UINT32_MAX) ||
            (texture_metadata_chunk &&
             draw->base_texture >= texture_metadata_chunk->count) ||
            (index > 0u && (draw[-1].base_texture > draw->base_texture ||
             (draw[-1].base_texture == draw->base_texture &&
              draw[-1].face_id >= draw->face_id))))
            return BSP_BUNDLE_GEOMETRY_INVALID;
    }

    view->data = data;
    view->bytes = bytes;
    view->vertices = (const BspBundleVertex *)(data + vertex_chunk->offset);
    view->vertex_count = vertex_chunk->count;
    view->indices = indices;
    view->index_count = index_chunk->count;
    view->draws = draws;
    view->draw_count = draw_chunk->count;
    if (lightmap_header_chunk) {
        view->lightmap_image = (const BspBundleImage *)(
            data + lightmap_header_chunk->offset);
        view->lightmap_pixels = data + lightmap_pixels_chunk->offset;
        view->lightmap_pixel_count = lightmap_pixels_chunk->count;
    }
    if (texture_metadata_chunk) {
        view->textures = (const BspBundleTexture *)(
            data + texture_metadata_chunk->offset);
        view->texture_count = texture_metadata_chunk->count;
        view->texture_pixels = data + texture_pixels_chunk->offset;
        view->texture_pixel_bytes = texture_pixels_chunk->bytes;
    }
    for (uint32_t vertex = 0; vertex < vertex_chunk->count; ++vertex) {
        const BspBundleVertex *const item = &view->vertices[vertex];
        for (unsigned component = 0; component < 3u; ++component)
            if (!__builtin_isfinite(item->position[component]))
                return BSP_BUNDLE_GEOMETRY_INVALID;
        for (unsigned component = 0; component < 2u; ++component)
            if (!__builtin_isfinite(item->base_uv[component]) ||
                !__builtin_isfinite(item->light_uv[component]))
                return BSP_BUNDLE_GEOMETRY_INVALID;
        if (lightmap_header_chunk &&
            (item->light_uv[0] < 0.0f || item->light_uv[0] > 1.0f ||
             item->light_uv[1] < 0.0f || item->light_uv[1] > 1.0f))
            return BSP_BUNDLE_GEOMETRY_INVALID;
    }
    for (unsigned component = 0; component < 3u; ++component) {
        view->camera_position[component] = load_f32(data + 24u + component * 4u);
        view->camera_forward[component] = load_f32(data + 36u + component * 4u);
        if (!__builtin_isfinite(view->camera_position[component]) ||
            !__builtin_isfinite(view->camera_forward[component]))
            return BSP_BUNDLE_GEOMETRY_INVALID;
    }
    return BSP_BUNDLE_OK;
}
