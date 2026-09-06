#ifndef PS5_AGC_GEARS_TRANSIENT_TABLE_H
#define PS5_AGC_GEARS_TRANSIENT_TABLE_H

#include "ps5_transient_ring.h"

#include <stddef.h>
#include <stdint.h>

typedef struct Ps5TransientTable {
    uint32_t *words;
    uint32_t dwords;
    uint32_t gpu_address_low;
    Ps5TransientSlice slice;
} Ps5TransientTable;

/* Descriptor tables are always allocated from an open frame slot, aligned to
 * 16 bytes, and rejected unless the complete table is in the GPU mapping. */
int ps5_transient_table_allocate(
    Ps5TransientRing *ring, uint32_t slot_index, uint32_t dwords,
    const void *gpu_mapping, size_t gpu_mapping_bytes,
    Ps5TransientTable *table);

#endif
