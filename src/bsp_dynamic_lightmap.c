#include "bsp_dynamic_lightmap.h"

#include <float.h>
#include <limits.h>
#include <string.h>

enum { BSP_DYNAMIC_LIGHTMAP_GUARD_WORD = 0xd17e6a5bu };

static float dot3(const float a[3], const float b[3])
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static void subtract3(float out[3], const float a[3], const float b[3])
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

static void cross3(float out[3], const float a[3], const float b[3])
{
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

static float absolute(float value)
{
    return value < 0.0f ? -value : value;
}

static int ray_triangle(const float origin[3], const float direction[3],
                        const BspBundleVertex *a,
                        const BspBundleVertex *b,
                        const BspBundleVertex *c,
                        float *distance, float *weight_b, float *weight_c)
{
    float edge_ab[3];
    float edge_ac[3];
    float p[3];
    subtract3(edge_ab, b->position, a->position);
    subtract3(edge_ac, c->position, a->position);
    cross3(p, direction, edge_ac);
    const float determinant = dot3(edge_ab, p);
    if (absolute(determinant) < 0.000001f)
        return 0;
    const float inverse = 1.0f / determinant;
    float from_a[3];
    subtract3(from_a, origin, a->position);
    const float u = dot3(from_a, p) * inverse;
    if (u < 0.0f || u > 1.0f)
        return 0;
    float q[3];
    cross3(q, from_a, edge_ab);
    const float v = dot3(direction, q) * inverse;
    if (v < 0.0f || u + v > 1.0f)
        return 0;
    const float t = dot3(edge_ac, q) * inverse;
    if (t <= 0.0001f)
        return 0;
    *distance = t;
    *weight_b = u;
    *weight_c = v;
    return 1;
}

static uint64_t hash_bytes(uint64_t hash, const uint8_t *data, size_t bytes)
{
    for (size_t index = 0u; index < bytes; ++index) {
        hash ^= data[index];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static int layout_valid(const BspDynamicLightmapLayout *layout)
{
    if (!layout || layout->image_width == 0u ||
        layout->image_height == 0u || layout->row_pitch == 0u ||
        layout->patch_width == 0u || layout->patch_height == 0u ||
        layout->patch_x > layout->image_width ||
        layout->patch_width > layout->image_width - layout->patch_x ||
        layout->patch_y > layout->image_height ||
        layout->patch_height > layout->image_height - layout->patch_y ||
        layout->image_width > UINT32_MAX / 4u ||
        layout->row_pitch < layout->image_width * 4u ||
        layout->image_height > SIZE_MAX / layout->row_pitch ||
        layout->image_bytes !=
            (size_t)layout->image_height * layout->row_pitch ||
        layout->patch_height >
            SIZE_MAX / ((size_t)layout->patch_width * 4u) ||
        layout->patch_bytes !=
            (size_t)layout->patch_width * layout->patch_height * 4u)
        return 0;
    const size_t expected_offset =
        (size_t)layout->patch_y * layout->row_pitch +
        (size_t)layout->patch_x * 4u;
    const size_t expected_span =
        (size_t)(layout->patch_height - 1u) * layout->row_pitch +
        (size_t)layout->patch_width * 4u;
    return layout->dirty_offset == expected_offset &&
           layout->dirty_span_bytes == expected_span &&
           expected_offset <= layout->image_bytes &&
           expected_span <= layout->image_bytes - expected_offset;
}

int bsp_dynamic_lightmap_select(const BspBundleView *bundle,
                                BspDynamicLightmapLayout *layout)
{
    if (!bundle || !layout || !bundle->lightmap_image ||
        !bundle->lightmap_pixels || !bundle->vertices || !bundle->indices ||
        !bundle->draws || !bundle->textures || bundle->vertex_count == 0u ||
        bundle->index_count == 0u || bundle->draw_count == 0u ||
        bundle->texture_count == 0u)
        return -1;
    const BspBundleImage *const image = bundle->lightmap_image;
    if (image->format != BSP_BUNDLE_IMAGE_RGBA8_UNORM ||
        image->width == 0u || image->height == 0u ||
        image->width > UINT32_MAX / 4u ||
        image->row_pitch < image->width * 4u ||
        image->height > SIZE_MAX / image->row_pitch ||
        image->width > UINT32_MAX / image->height ||
        image->width * image->height != bundle->lightmap_pixel_count)
        return -1;

    float nearest = FLT_MAX;
    float selected_u = 0.0f;
    float selected_v = 0.0f;
    uint32_t selected_face = UINT32_MAX;
    for (uint32_t draw_index = 0u; draw_index < bundle->draw_count;
         ++draw_index) {
        const BspBundleDraw *const draw = &bundle->draws[draw_index];
        if (draw->lightmap == UINT32_MAX ||
            draw->base_texture >= bundle->texture_count ||
            (bundle->textures[draw->base_texture].flags &
             (BSP_BUNDLE_TEXTURE_TRANSPARENT | BSP_BUNDLE_TEXTURE_NODRAW)) !=
                0u ||
            draw->first_index > bundle->index_count ||
            draw->index_count > bundle->index_count - draw->first_index ||
            draw->index_count % 3u != 0u)
            continue;
        for (uint32_t local = 0u; local < draw->index_count; local += 3u) {
            const uint16_t ia = bundle->indices[draw->first_index + local];
            const uint16_t ib = bundle->indices[draw->first_index + local + 1u];
            const uint16_t ic = bundle->indices[draw->first_index + local + 2u];
            if (ia >= bundle->vertex_count || ib >= bundle->vertex_count ||
                ic >= bundle->vertex_count)
                return -1;
            const BspBundleVertex *const a = &bundle->vertices[ia];
            const BspBundleVertex *const b = &bundle->vertices[ib];
            const BspBundleVertex *const c = &bundle->vertices[ic];
            float distance = 0.0f;
            float weight_b = 0.0f;
            float weight_c = 0.0f;
            if (!ray_triangle(bundle->camera_position,
                              bundle->camera_forward, a, b, c,
                              &distance, &weight_b, &weight_c) ||
                distance >= nearest)
                continue;
            const float weight_a = 1.0f - weight_b - weight_c;
            selected_u = a->light_uv[0] * weight_a +
                         b->light_uv[0] * weight_b +
                         c->light_uv[0] * weight_c;
            selected_v = a->light_uv[1] * weight_a +
                         b->light_uv[1] * weight_b +
                         c->light_uv[1] * weight_c;
            nearest = distance;
            selected_face = draw->face_id;
        }
    }
    if (selected_face == UINT32_MAX || selected_u < 0.0f ||
        selected_u > 1.0f || selected_v < 0.0f || selected_v > 1.0f)
        return -2;

    uint32_t pixel_x = (uint32_t)(selected_u * (float)image->width);
    uint32_t pixel_y = (uint32_t)(selected_v * (float)image->height);
    if (pixel_x >= image->width)
        pixel_x = image->width - 1u;
    if (pixel_y >= image->height)
        pixel_y = image->height - 1u;
    const uint32_t patch_width = image->width < BSP_DYNAMIC_LIGHTMAP_PATCH_EDGE
                                     ? image->width
                                     : BSP_DYNAMIC_LIGHTMAP_PATCH_EDGE;
    const uint32_t patch_height =
        image->height < BSP_DYNAMIC_LIGHTMAP_PATCH_EDGE
            ? image->height
            : BSP_DYNAMIC_LIGHTMAP_PATCH_EDGE;
    uint32_t patch_x = pixel_x > patch_width / 2u
                           ? pixel_x - patch_width / 2u
                           : 0u;
    uint32_t patch_y = pixel_y > patch_height / 2u
                           ? pixel_y - patch_height / 2u
                           : 0u;
    if (patch_x > image->width - patch_width)
        patch_x = image->width - patch_width;
    if (patch_y > image->height - patch_height)
        patch_y = image->height - patch_height;
    *layout = (BspDynamicLightmapLayout){
        .image_width = image->width,
        .image_height = image->height,
        .row_pitch = image->row_pitch,
        .patch_x = patch_x,
        .patch_y = patch_y,
        .patch_width = patch_width,
        .patch_height = patch_height,
        .hit_face = selected_face,
        .image_bytes = (size_t)image->height * image->row_pitch,
        .patch_bytes = (size_t)patch_width * patch_height * 4u,
        .dirty_offset = (size_t)patch_y * image->row_pitch +
                        (size_t)patch_x * 4u,
        .dirty_span_bytes = (size_t)(patch_height - 1u) * image->row_pitch +
                            (size_t)patch_width * 4u,
    };
    return layout_valid(layout) ? 0 : -1;
}

int bsp_dynamic_lightmap_allocation_bytes(
    const BspDynamicLightmapLayout *layout, size_t *bytes)
{
    if (!bytes || !layout_valid(layout) ||
        layout->image_bytes >
            SIZE_MAX - 2u * BSP_DYNAMIC_LIGHTMAP_GUARD_BYTES)
        return -1;
    *bytes = layout->image_bytes + 2u * BSP_DYNAMIC_LIGHTMAP_GUARD_BYTES;
    return 0;
}

int bsp_dynamic_lightmap_guards_intact(
    const BspDynamicLightmapSlot *slot,
    const BspDynamicLightmapLayout *layout)
{
    size_t required = 0u;
    if (!slot || !slot->initialized || !slot->allocation || !slot->pixels ||
        bsp_dynamic_lightmap_allocation_bytes(layout, &required) != 0 ||
        slot->allocation_bytes < required ||
        slot->pixels !=
            slot->allocation + BSP_DYNAMIC_LIGHTMAP_GUARD_BYTES)
        return 0;
    const uint32_t *const before = (const uint32_t *)slot->allocation;
    const uint32_t *const after = (const uint32_t *)(
        slot->pixels + layout->image_bytes);
    for (size_t index = 0u;
         index < BSP_DYNAMIC_LIGHTMAP_GUARD_BYTES / sizeof(uint32_t);
         ++index)
        if (before[index] != BSP_DYNAMIC_LIGHTMAP_GUARD_WORD ||
            after[index] != BSP_DYNAMIC_LIGHTMAP_GUARD_WORD)
            return 0;
    return 1;
}

uint64_t bsp_dynamic_lightmap_patch_hash(
    const uint8_t *pixels, const BspDynamicLightmapLayout *layout)
{
    if (!pixels || !layout_valid(layout))
        return 0u;
    uint64_t hash = UINT64_C(14695981039346656037);
    for (uint32_t row = 0u; row < layout->patch_height; ++row) {
        const uint8_t *const begin = pixels + layout->dirty_offset +
                                     (size_t)row * layout->row_pitch;
        hash = hash_bytes(hash, begin, (size_t)layout->patch_width * 4u);
    }
    return hash;
}

uint64_t bsp_dynamic_lightmap_surrounding_hash(
    const uint8_t *pixels, const BspDynamicLightmapLayout *layout)
{
    if (!pixels || !layout_valid(layout))
        return 0u;
    uint64_t hash = UINT64_C(14695981039346656037);
    const size_t patch_begin = (size_t)layout->patch_x * 4u;
    const size_t patch_end =
        patch_begin + (size_t)layout->patch_width * 4u;
    for (uint32_t row = 0u; row < layout->image_height; ++row) {
        const uint8_t *const begin = pixels + (size_t)row * layout->row_pitch;
        if (row < layout->patch_y ||
            row >= layout->patch_y + layout->patch_height) {
            hash = hash_bytes(hash, begin, layout->row_pitch);
        } else {
            hash = hash_bytes(hash, begin, patch_begin);
            hash = hash_bytes(hash, begin + patch_end,
                              layout->row_pitch - patch_end);
        }
    }
    return hash;
}

int bsp_dynamic_lightmap_slot_init(
    BspDynamicLightmapSlot *slot, void *allocation, size_t allocation_bytes,
    const uint8_t *source_pixels, size_t source_bytes,
    const BspDynamicLightmapLayout *layout)
{
    size_t required = 0u;
    if (!slot || !allocation || !source_pixels ||
        bsp_dynamic_lightmap_allocation_bytes(layout, &required) != 0 ||
        allocation_bytes < required || source_bytes < layout->image_bytes ||
        ((uintptr_t)allocation & 255u) != 0u)
        return -1;
    memset(slot, 0, sizeof(*slot));
    slot->allocation = allocation;
    slot->allocation_bytes = allocation_bytes;
    slot->pixels = slot->allocation + BSP_DYNAMIC_LIGHTMAP_GUARD_BYTES;
    uint32_t *const before = (uint32_t *)slot->allocation;
    uint32_t *const after =
        (uint32_t *)(slot->pixels + layout->image_bytes);
    for (size_t index = 0u;
         index < BSP_DYNAMIC_LIGHTMAP_GUARD_BYTES / sizeof(uint32_t);
         ++index) {
        before[index] = BSP_DYNAMIC_LIGHTMAP_GUARD_WORD;
        after[index] = BSP_DYNAMIC_LIGHTMAP_GUARD_WORD;
    }
    memcpy(slot->pixels, source_pixels, layout->image_bytes);
    slot->surrounding_hash =
        bsp_dynamic_lightmap_surrounding_hash(slot->pixels, layout);
    slot->last_frame = UINT64_MAX;
    slot->last_pattern = UINT32_MAX;
    slot->first_upload_pending = 1u;
    slot->initialized = 1u;
    return bsp_dynamic_lightmap_guards_intact(slot, layout) ? 0 : -1;
}

int bsp_dynamic_lightmap_update(
    BspDynamicLightmapSlot *slot, const BspDynamicLightmapLayout *layout,
    Ps5TransientRing *ring, uint32_t slot_index, uint64_t frame_index,
    BspDynamicLightmapUpdate *update)
{
    return bsp_dynamic_lightmap_update_pattern(
        slot, layout, ring, slot_index, frame_index,
        (uint32_t)(frame_index & 1u), update);
}

int bsp_dynamic_lightmap_update_pattern(
    BspDynamicLightmapSlot *slot, const BspDynamicLightmapLayout *layout,
    Ps5TransientRing *ring, uint32_t slot_index, uint64_t frame_index,
    uint32_t pattern, BspDynamicLightmapUpdate *update)
{
    if (!slot || !slot->initialized || !layout_valid(layout) || !ring ||
        !update || pattern >= BSP_DYNAMIC_LIGHTMAP_PATTERN_COUNT ||
        slot_index >= ring->slot_count ||
        ring->slots[slot_index].state != PS5_TRANSIENT_OPEN ||
        !bsp_dynamic_lightmap_guards_intact(slot, layout))
        return -1;
    Ps5TransientSlice staging;
    if (ps5_transient_ring_allocate(ring, slot_index, layout->patch_bytes,
                                    256u, &staging) != PS5_TRANSIENT_OK)
        return -2;
    const uint8_t value = pattern == 0u ? 255u : 24u;
    uint8_t *const packed = staging.cpu;
    for (size_t pixel = 0u; pixel < layout->patch_bytes; pixel += 4u) {
        packed[pixel] = value;
        packed[pixel + 1u] = value;
        packed[pixel + 2u] = value;
        packed[pixel + 3u] = 255u;
    }
    for (uint32_t row = 0u; row < layout->patch_height; ++row) {
        uint8_t *const destination = slot->pixels + layout->dirty_offset +
                                     (size_t)row * layout->row_pitch;
        memcpy(destination,
               packed + (size_t)row * layout->patch_width * 4u,
               (size_t)layout->patch_width * 4u);
    }
    if (!bsp_dynamic_lightmap_guards_intact(slot, layout))
        return -3;
    const uint8_t first_upload = slot->first_upload_pending;
    *update = (BspDynamicLightmapUpdate){
        .staging_address = staging.cpu,
        .staging_bytes = staging.bytes,
        .written_address = first_upload
                               ? (const void *)slot->pixels
                               : (const void *)(slot->pixels +
                                                layout->dirty_offset),
        .written_span_bytes = first_upload ? layout->image_bytes
                                           : layout->dirty_span_bytes,
        .uploaded_bytes = first_upload ? layout->image_bytes
                                       : layout->patch_bytes,
        .patch_hash = bsp_dynamic_lightmap_patch_hash(slot->pixels, layout),
        .pattern = pattern,
        .first_upload = first_upload,
    };
    slot->first_upload_pending = 0u;
    slot->last_frame = frame_index;
    slot->last_pattern = pattern;
    return 0;
}
