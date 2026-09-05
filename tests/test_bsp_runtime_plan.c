#include "../src/bsp_runtime_plan.h"

#include <assert.h>
#include <stdint.h>

int main(void)
{
    BspRuntimePlan plan;
    assert(bsp_runtime_plan(788512u, 3695u, &plan) == 0);
    assert(plan.bundle_bytes == 788512u);
    assert(plan.vertex_srds_offset == 788736u);
    assert(plan.clear_vertices_offset == 788768u);
    assert(plan.clear_indices_offset == 788864u);
    assert(plan.scene_draws_offset == 788880u);
    assert(plan.guard_offset == 1143744u);
    assert(plan.scene_draw_count == 3696u);
    assert(plan.allocation_bytes == 0x120000u);
    assert((plan.vertex_srds_offset & (BSP_RUNTIME_SRD_ALIGNMENT - 1u)) == 0u);
    assert((plan.allocation_bytes &
            (BSP_RUNTIME_ALLOCATION_ALIGNMENT - 1u)) == 0u);
    assert((plan.guard_offset & 63u) == 0u);
    assert(bsp_runtime_plan(0u, 1u, &plan) != 0);
    assert(bsp_runtime_plan(1u, 0u, &plan) != 0);
    assert(bsp_runtime_plan(SIZE_MAX, 1u, &plan) != 0);
    assert(bsp_runtime_plan(1u, UINT32_MAX, &plan) != 0);
    return 0;
}
