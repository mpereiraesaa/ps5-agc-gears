#include "ps5_transient_ring.h"

#include <limits.h>
#include <string.h>

static int power_of_two(size_t value)
{
    return value && (value & (value - 1u)) == 0u;
}

int ps5_transient_ring_init(Ps5TransientRing *ring, void *base, size_t bytes,
                            uint32_t slot_count, size_t slot_alignment)
{
    if (!ring || !base || bytes == 0u || slot_count == 0u ||
        slot_count > PS5_TRANSIENT_MAX_SLOTS ||
        !power_of_two(slot_alignment) ||
        ((uintptr_t)base & (slot_alignment - 1u)) != 0u)
        return PS5_TRANSIENT_PRECONDITION;
    const size_t raw_slot_bytes = bytes / slot_count;
    const size_t slot_bytes = raw_slot_bytes & ~(slot_alignment - 1u);
    if (slot_bytes == 0u || slot_bytes > SIZE_MAX / slot_count)
        return PS5_TRANSIENT_PRECONDITION;
    memset(ring, 0, sizeof(*ring));
    ring->base = base;
    ring->bytes = slot_bytes * slot_count;
    ring->slot_count = slot_count;
    for (uint32_t index = 0; index < slot_count; ++index) {
        ring->slots[index].offset = (size_t)index * slot_bytes;
        ring->slots[index].bytes = slot_bytes;
    }
    return PS5_TRANSIENT_OK;
}

int ps5_transient_ring_begin(Ps5TransientRing *ring, uint32_t slot_index,
                             uint64_t completed_token,
                             int completion_proven)
{
    if (!ring || !ring->base || slot_index >= ring->slot_count)
        return PS5_TRANSIENT_PRECONDITION;
    Ps5TransientSlot *slot = &ring->slots[slot_index];
    if (slot->state == PS5_TRANSIENT_OPEN)
        return PS5_TRANSIENT_SLOT_BUSY;
    if (slot->state == PS5_TRANSIENT_SEALED) {
        if (!completion_proven)
            return PS5_TRANSIENT_SLOT_BUSY;
        if (completed_token == 0u || completed_token != slot->retire_token)
            return PS5_TRANSIENT_TOKEN_MISMATCH;
    }
    slot->used = 0u;
    slot->retire_token = 0u;
    slot->state = PS5_TRANSIENT_OPEN;
    return PS5_TRANSIENT_OK;
}

int ps5_transient_ring_allocate(Ps5TransientRing *ring, uint32_t slot_index,
                                size_t bytes, size_t alignment,
                                Ps5TransientSlice *slice)
{
    if (!ring || !ring->base || !slice || slot_index >= ring->slot_count ||
        bytes == 0u || !power_of_two(alignment))
        return PS5_TRANSIENT_PRECONDITION;
    Ps5TransientSlot *slot = &ring->slots[slot_index];
    if (slot->state != PS5_TRANSIENT_OPEN)
        return PS5_TRANSIENT_SLOT_BUSY;
    const uintptr_t slot_base = (uintptr_t)ring->base + slot->offset;
    if (slot_base > UINTPTR_MAX - slot->used ||
        slot_base + slot->used > UINTPTR_MAX - (alignment - 1u))
        return PS5_TRANSIENT_EXHAUSTED;
    const uintptr_t aligned_address =
        (slot_base + slot->used + alignment - 1u) &
        ~(uintptr_t)(alignment - 1u);
    if (aligned_address < slot_base)
        return PS5_TRANSIENT_EXHAUSTED;
    const size_t local_offset = (size_t)(aligned_address - slot_base);
    if (local_offset > slot->bytes || bytes > slot->bytes - local_offset)
        return PS5_TRANSIENT_EXHAUSTED;
    slot->used = local_offset + bytes;
    *slice = (Ps5TransientSlice){
        .cpu = (void *)aligned_address,
        .offset = slot->offset + local_offset,
        .bytes = bytes,
    };
    return PS5_TRANSIENT_OK;
}

int ps5_transient_ring_seal(Ps5TransientRing *ring, uint32_t slot_index,
                            uint64_t retire_token)
{
    if (!ring || !ring->base || slot_index >= ring->slot_count ||
        retire_token == 0u)
        return PS5_TRANSIENT_PRECONDITION;
    Ps5TransientSlot *slot = &ring->slots[slot_index];
    if (slot->state != PS5_TRANSIENT_OPEN)
        return PS5_TRANSIENT_SLOT_BUSY;
    slot->retire_token = retire_token;
    slot->state = PS5_TRANSIENT_SEALED;
    return PS5_TRANSIENT_OK;
}

int ps5_transient_ring_abort_unsubmitted(Ps5TransientRing *ring,
                                         uint32_t slot_index)
{
    if (!ring || !ring->base || slot_index >= ring->slot_count)
        return PS5_TRANSIENT_PRECONDITION;
    Ps5TransientSlot *slot = &ring->slots[slot_index];
    if (slot->state != PS5_TRANSIENT_OPEN)
        return PS5_TRANSIENT_SLOT_BUSY;
    slot->used = 0u;
    slot->state = PS5_TRANSIENT_EMPTY;
    return PS5_TRANSIENT_OK;
}
