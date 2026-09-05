#include "../src/bsp_command_plan.h"

#include <assert.h>
#include <stdint.h>

int main(void)
{
    BspCommandPlan plan;
    assert(bsp_command_plan(4000u, 4096u, &plan) == 0);
    assert(plan.draw_dwords == 116000u);
    assert(plan.frame_capacity_dwords == 120096u);
    assert(plan.slot_bytes == 0x80000u);
    assert(plan.slot_offsets[0] == 0u && plan.slot_offsets[1] == 0x80000u);
    assert(plan.fence_offsets[0] == 0x100000u &&
           plan.fence_offsets[1] == 0x100040u);
    assert(plan.allocation_bytes == 0x110000u);
    assert((plan.slot_bytes & (BSP_COMMAND_SLOT_ALIGNMENT - 1u)) == 0u);
    assert((plan.fence_offsets[0] & (BSP_COMMAND_FENCE_ALIGNMENT - 1u)) == 0u);
    assert(bsp_command_plan(0u, 4096u, &plan) != 0);
    assert(bsp_command_plan(1u, 0u, &plan) != 0);
    assert(bsp_command_plan(UINT32_MAX, 4096u, &plan) != 0);
    return 0;
}
