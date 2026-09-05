#include "../src/bsp_bundle.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

enum { HEADER = 160, VERTEX_OFFSET = 160, INDEX_OFFSET = 256,
       DRAW_OFFSET = 272, FILE_BYTES = 304 };

static void put_u32(uint8_t *at, uint32_t value)
{
    memcpy(at, &value, sizeof(value));
}

static void put_f32(uint8_t *at, float value)
{
    memcpy(at, &value, sizeof(value));
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

static void descriptor(uint8_t *data, unsigned index, const char tag[4],
                       uint32_t offset, uint32_t bytes, uint32_t count,
                       uint32_t stride)
{
    uint8_t *const row = data + 64u + index * 32u;
    memcpy(row, tag, 4u);
    put_u32(row + 4u, offset);
    put_u32(row + 8u, bytes);
    put_u32(row + 12u, count);
    put_u32(row + 16u, stride);
    put_u32(row + 20u, crc32_bytes(data + offset, bytes));
}

static void checksums(uint8_t *data)
{
    descriptor(data, 0u, "VERT", VERTEX_OFFSET, 96u, 3u, 32u);
    descriptor(data, 1u, "INDX", INDEX_OFFSET, 12u, 3u, 4u);
    descriptor(data, 2u, "DRAW", DRAW_OFFSET, 32u, 1u, 32u);
    put_u32(data + 20u, crc32_bytes(data + HEADER, FILE_BYTES - HEADER));
}

static void make_bundle(uint8_t data[FILE_BYTES])
{
    memset(data, 0, FILE_BYTES);
    memcpy(data, "PS5BSP\0\0", 8u);
    put_u32(data + 8u, 1u);
    put_u32(data + 12u, HEADER);
    put_u32(data + 16u, FILE_BYTES);
    put_f32(data + 24u, 1.0f);
    put_f32(data + 28u, 2.0f);
    put_f32(data + 32u, 3.0f);
    put_f32(data + 36u, 0.0f);
    put_f32(data + 40u, 0.0f);
    put_f32(data + 44u, -1.0f);
    put_u32(data + 48u, 3u);
    BspBundleVertex *const vertices = (BspBundleVertex *)(data + VERTEX_OFFSET);
    vertices[0].position[0] = 0.0f;
    vertices[1].position[0] = 1.0f;
    vertices[2].position[1] = 1.0f;
    uint32_t *const indices = (uint32_t *)(data + INDEX_OFFSET);
    indices[0] = 0u; indices[1] = 1u; indices[2] = 2u;
    BspBundleDraw *const draw = (BspBundleDraw *)(data + DRAW_OFFSET);
    draw->index_count = 3u;
    checksums(data);
}

int main(void)
{
    union { uint64_t align; uint8_t bytes[FILE_BYTES]; } storage;
    make_bundle(storage.bytes);
    BspBundleView view;
    assert(bsp_bundle_open(storage.bytes, sizeof(storage.bytes), &view) ==
           BSP_BUNDLE_OK);
    assert(view.vertex_count == 3u && view.index_count == 3u &&
           view.draw_count == 1u && view.camera_position[1] == 2.0f &&
           view.camera_forward[2] == -1.0f);

    uint8_t saved = storage.bytes[VERTEX_OFFSET];
    storage.bytes[VERTEX_OFFSET] ^= 1u;
    assert(bsp_bundle_open(storage.bytes, sizeof(storage.bytes), &view) ==
           BSP_BUNDLE_CHECKSUM_MISMATCH);
    storage.bytes[VERTEX_OFFSET] = saved;
    checksums(storage.bytes);

    uint32_t *const indices = (uint32_t *)(storage.bytes + INDEX_OFFSET);
    indices[2] = 3u;
    checksums(storage.bytes);
    assert(bsp_bundle_open(storage.bytes, sizeof(storage.bytes), &view) ==
           BSP_BUNDLE_GEOMETRY_INVALID);
    indices[2] = 2u;
    checksums(storage.bytes);

    put_u32(storage.bytes + 64u + 24u, 1u);
    assert(bsp_bundle_open(storage.bytes, sizeof(storage.bytes), &view) ==
           BSP_BUNDLE_DIRECTORY_INVALID);
    put_u32(storage.bytes + 64u + 24u, 0u);
    checksums(storage.bytes);
    assert(bsp_bundle_open(storage.bytes, sizeof(storage.bytes) - 1u, &view) ==
           BSP_BUNDLE_HEADER_INVALID);
    return 0;
}
