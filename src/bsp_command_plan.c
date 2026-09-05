#include "bsp_command_plan.h"

#include "bsp_flat_draw.h"

#include <limits.h>
#include <string.h>

static int align_u32(uint32_t value, uint32_t alignment, uint32_t *out)
{
    if (!out || alignment == 0u || (alignment & (alignment - 1u)) != 0u ||
        value > UINT32_MAX - (alignment - 1u))
        return -1;
    *out = (value + alignment - 1u) & ~(alignment - 1u);
    return 0;
}

int bsp_command_plan(uint32_t draw_count, uint32_t fixed_dwords,
                     BspCommandPlan *plan)
{
    uint32_t draw_dwords = 0u;
    if (!plan || fixed_dwords == 0u ||
        bsp_flat_required_dwords(draw_count, &draw_dwords) != 0 ||
        draw_dwords > UINT32_MAX - fixed_dwords)
        return -1;
    memset(plan, 0, sizeof(*plan));
    plan->draw_dwords = draw_dwords;
    plan->frame_capacity_dwords = draw_dwords + fixed_dwords;
    if (plan->frame_capacity_dwords > UINT32_MAX / sizeof(uint32_t) ||
        align_u32(plan->frame_capacity_dwords * sizeof(uint32_t),
                  BSP_COMMAND_SLOT_ALIGNMENT, &plan->slot_bytes) != 0 ||
        plan->slot_bytes > UINT32_MAX / 2u)
        return -1;
    plan->slot_offsets[0] = 0u;
    plan->slot_offsets[1] = plan->slot_bytes;
    const uint32_t streams_end = plan->slot_bytes * 2u;
    if (align_u32(streams_end, BSP_COMMAND_FENCE_ALIGNMENT,
                  &plan->fence_offsets[0]) != 0 ||
        plan->fence_offsets[0] > UINT32_MAX - BSP_COMMAND_FENCE_ALIGNMENT)
        return -1;
    plan->fence_offsets[1] =
        plan->fence_offsets[0] + BSP_COMMAND_FENCE_ALIGNMENT;
    if (plan->fence_offsets[1] > UINT32_MAX - sizeof(uint64_t) ||
        align_u32(plan->fence_offsets[1] + sizeof(uint64_t),
                  BSP_COMMAND_SLOT_ALIGNMENT, &plan->allocation_bytes) != 0)
        return -1;
    return 0;
}
