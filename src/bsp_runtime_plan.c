#include "bsp_runtime_plan.h"

#include "bsp_bundle.h"
#include "bsp_flat_draw.h"
#include "bsp_texture_descriptor.h"

#include <limits.h>
#include <string.h>

static int add_size(size_t left, size_t right, size_t *out)
{
    if (!out || right > SIZE_MAX - left)
        return -1;
    *out = left + right;
    return 0;
}

static int align_size(size_t value, size_t alignment, size_t *out)
{
    if (!out || alignment == 0u || (alignment & (alignment - 1u)) != 0u ||
        value > SIZE_MAX - (alignment - 1u))
        return -1;
    *out = (value + alignment - 1u) & ~(alignment - 1u);
    return 0;
}

static int place(size_t *cursor, size_t bytes, size_t alignment,
                 size_t *offset)
{
    size_t aligned = 0u;
    if (!cursor || !offset || bytes == 0u ||
        align_size(*cursor, alignment, &aligned) != 0 ||
        add_size(aligned, bytes, cursor) != 0)
        return -1;
    *offset = aligned;
    return 0;
}

static int plan_runtime(size_t bundle_bytes, uint32_t map_draw_count,
                        uint32_t texture_count, int textured,
                        BspRuntimePlan *plan)
{
    if (!plan || bundle_bytes == 0u || map_draw_count == 0u ||
        map_draw_count == UINT32_MAX ||
        (textured && (texture_count == 0u ||
         texture_count > UINT32_MAX / BSP_TEXTURE_TABLE_DWORDS)))
        return -1;
    memset(plan, 0, sizeof(*plan));
    plan->bundle_bytes = bundle_bytes;
    plan->scene_draw_count = map_draw_count + 1u;
    size_t cursor = bundle_bytes;
    if (place(&cursor, 8u * sizeof(uint32_t), BSP_RUNTIME_SRD_ALIGNMENT,
              &plan->vertex_srds_offset) != 0)
        return -1;
    if (textured) {
        plan->descriptor_table_dwords =
            texture_count * BSP_TEXTURE_TABLE_DWORDS;
        if (place(&cursor,
                  (size_t)plan->descriptor_table_dwords * sizeof(uint32_t),
                  BSP_RUNTIME_SRD_ALIGNMENT,
                  &plan->descriptor_tables_offset) != 0)
            return -1;
    }
    if (place(&cursor, 3u * sizeof(BspBundleVertex), 16u,
              &plan->clear_vertices_offset) != 0 ||
        place(&cursor, 3u * sizeof(uint16_t), 16u,
              &plan->clear_indices_offset) != 0 ||
        place(&cursor, (size_t)plan->scene_draw_count * sizeof(BspFlatDraw),
              16u, &plan->scene_draws_offset) != 0)
        return -1;
    if (textured &&
        place(&cursor,
              (size_t)plan->scene_draw_count * sizeof(const uint32_t *),
              _Alignof(const uint32_t *),
              &plan->texture_bindings_offset) != 0)
        return -1;
    if (place(&cursor, 64u, 64u, &plan->guard_offset) != 0 ||
        align_size(cursor, BSP_RUNTIME_ALLOCATION_ALIGNMENT,
                   &plan->allocation_bytes) != 0)
        return -1;
    return plan->allocation_bytes >= cursor ? 0 : -1;
}

int bsp_runtime_plan(size_t bundle_bytes, uint32_t map_draw_count,
                     BspRuntimePlan *plan)
{
    return plan_runtime(bundle_bytes, map_draw_count, 0u, 0, plan);
}

int bsp_runtime_plan_textured(size_t bundle_bytes, uint32_t map_draw_count,
                              uint32_t texture_count,
                              BspRuntimePlan *plan)
{
    return plan_runtime(bundle_bytes, map_draw_count, texture_count, 1, plan);
}
