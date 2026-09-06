#include "../src/ps5_transient_ring.h"

#include <assert.h>
#include <stdint.h>

int main(void)
{
    _Alignas(64) uint8_t memory[512] = {0};
    Ps5TransientRing ring;
    Ps5TransientSlice vertices;
    Ps5TransientSlice constants;
    assert(ps5_transient_ring_init(&ring, memory, sizeof(memory),
                                   2u, 64u) == 0);
    assert(ring.slots[0].bytes == 256u && ring.slots[1].offset == 256u);
    assert(ps5_transient_ring_begin(&ring, 0u, 0u, 0) == 0);
    assert(ps5_transient_ring_allocate(&ring, 0u, 33u, 16u,
                                       &vertices) == 0);
    assert(ps5_transient_ring_allocate(&ring, 0u, 64u, 64u,
                                       &constants) == 0);
    assert(((uintptr_t)vertices.cpu & 15u) == 0u);
    assert(((uintptr_t)constants.cpu & 63u) == 0u);
    assert(vertices.offset < constants.offset);
    assert(ps5_transient_ring_allocate(&ring, 0u, 256u, 1u,
                                       &vertices) ==
           PS5_TRANSIENT_EXHAUSTED);
    assert(ps5_transient_ring_seal(&ring, 0u, 1001u) == 0);
    assert(ps5_transient_ring_begin(&ring, 0u, 0u, 0) ==
           PS5_TRANSIENT_SLOT_BUSY);
    assert(ps5_transient_ring_begin(&ring, 0u, 1000u, 1) ==
           PS5_TRANSIENT_TOKEN_MISMATCH);
    assert(ps5_transient_ring_begin(&ring, 0u, 1001u, 1) == 0);
    assert(ring.slots[0].used == 0u);
    assert(ps5_transient_ring_abort_unsubmitted(&ring, 0u) == 0);

    assert(ps5_transient_ring_begin(&ring, 1u, 0u, 0) == 0);
    assert(ps5_transient_ring_seal(&ring, 1u, 1002u) == 0);
    assert(ps5_transient_ring_begin(&ring, 1u, 1002u, 0) ==
           PS5_TRANSIENT_SLOT_BUSY);
    return 0;
}
