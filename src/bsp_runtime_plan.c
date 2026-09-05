#include "bsp_runtime_plan.h"

#include "bsp_bundle.h"
#include "bsp_flat_draw.h"

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

int bsp_runtime_plan(size_t bundle_bytes, uint32_t map_draw_count,
                     BspRuntimePlan *plan)
{
    if (!plan || bundle_bytes == 0u || map_draw_count == 0u ||
        map_draw_count == UINT32_MAX)
        return -1;
    memset(plan, 0, sizeof(*plan));
    plan->bundle_bytes = bundle_bytes;
    plan->scene_draw_count = map_draw_count + 1u;
    size_t cursor = bundle_bytes;
    if (place(&cursor, 8u * sizeof(uint32_t), BSP_RUNTIME_SRD_ALIGNMENT,
              &plan->vertex_srds_offset) != 0 ||
        place(&cursor, 3u * sizeof(BspBundleVertex), 16u,
              &plan->clear_vertices_offset) != 0 ||
        place(&cursor, 3u * sizeof(uint32_t), 16u,
              &plan->clear_indices_offset) != 0 ||
        place(&cursor, (size_t)plan->scene_draw_count * sizeof(BspFlatDraw),
              16u, &plan->scene_draws_offset) != 0 ||
        place(&cursor, 64u, 64u, &plan->guard_offset) != 0 ||
        align_size(cursor, BSP_RUNTIME_ALLOCATION_ALIGNMENT,
                   &plan->allocation_bytes) != 0)
        return -1;
    return plan->allocation_bytes >= cursor ? 0 : -1;
}
