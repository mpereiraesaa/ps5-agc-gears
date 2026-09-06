#include "../src/ps5_transient_table.h"

#include <assert.h>
#include <stdint.h>

int main(void)
{
    _Alignas(64) uint8_t mapping[512] = {0};
    Ps5TransientRing ring;
    Ps5TransientTable table;
    assert(ps5_transient_ring_init(&ring, mapping, sizeof(mapping),
                                   2u, 64u) == 0);
    assert(ps5_transient_ring_begin(&ring, 0u, 0u, 0) == 0);
    assert(ps5_transient_table_allocate(&ring, 0u, 24u, mapping,
                                        sizeof(mapping), &table) == 0);
    assert((void *)table.words == (void *)mapping && table.dwords == 24u);
    assert(((uintptr_t)table.words & 15u) == 0u);
    const size_t used = ring.slots[0].used;
    assert(ps5_transient_table_allocate(&ring, 0u, 4u, mapping + 256u,
                                        256u, &table) == -3);
    assert(ring.slots[0].used == used);
    assert(ps5_transient_table_allocate(&ring, 0u, 100u, mapping,
                                        sizeof(mapping), &table) == -2);
    return 0;
}
