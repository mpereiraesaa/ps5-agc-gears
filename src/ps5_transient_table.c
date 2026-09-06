#include "ps5_transient_table.h"

#include "ps5_gpu_span.h"

#include <limits.h>

int ps5_transient_table_allocate(
    Ps5TransientRing *ring, uint32_t slot_index, uint32_t dwords,
    const void *gpu_mapping, size_t gpu_mapping_bytes,
    Ps5TransientTable *table)
{
    if (!ring || !table || dwords == 0u ||
        dwords > UINT32_MAX / sizeof(uint32_t) ||
        slot_index >= ring->slot_count)
        return -1;
    Ps5TransientSlot *slot = &ring->slots[slot_index];
    const size_t checkpoint = slot->used;
    Ps5TransientSlice slice;
    const size_t bytes = (size_t)dwords * sizeof(uint32_t);
    if (ps5_transient_ring_allocate(ring, slot_index, bytes, 16u,
                                    &slice) != PS5_TRANSIENT_OK)
        return -2;
    if (!ps5_gpu_span_visible(gpu_mapping, gpu_mapping_bytes,
                              slice.cpu, slice.bytes)) {
        slot->used = checkpoint;
        return -3;
    }
    *table = (Ps5TransientTable){
        .words = slice.cpu,
        .dwords = dwords,
        .gpu_address_low = (uint32_t)(uintptr_t)slice.cpu,
        .slice = slice,
    };
    return 0;
}
