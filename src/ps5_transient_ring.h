#ifndef PS5_AGC_GEARS_TRANSIENT_RING_H
#define PS5_AGC_GEARS_TRANSIENT_RING_H

#include <stddef.h>
#include <stdint.h>

enum {
    PS5_TRANSIENT_MAX_SLOTS = 4,
};

enum ps5_transient_slot_state {
    PS5_TRANSIENT_EMPTY = 0,
    PS5_TRANSIENT_OPEN = 1,
    PS5_TRANSIENT_SEALED = 2,
};

typedef struct Ps5TransientSlice {
    void *cpu;
    size_t offset;
    size_t bytes;
} Ps5TransientSlice;

typedef struct Ps5TransientSlot {
    size_t offset;
    size_t bytes;
    size_t used;
    uint64_t retire_token;
    uint8_t state;
} Ps5TransientSlot;

typedef struct Ps5TransientRing {
    uint8_t *base;
    size_t bytes;
    uint32_t slot_count;
    Ps5TransientSlot slots[PS5_TRANSIENT_MAX_SLOTS];
} Ps5TransientRing;

enum ps5_transient_result {
    PS5_TRANSIENT_OK = 0,
    PS5_TRANSIENT_PRECONDITION = -1,
    PS5_TRANSIENT_SLOT_BUSY = -2,
    PS5_TRANSIENT_TOKEN_MISMATCH = -3,
    PS5_TRANSIENT_EXHAUSTED = -4,
};

int ps5_transient_ring_init(Ps5TransientRing *ring, void *base, size_t bytes,
                            uint32_t slot_count, size_t slot_alignment);
int ps5_transient_ring_begin(Ps5TransientRing *ring, uint32_t slot_index,
                             uint64_t completed_token,
                             int completion_proven);
int ps5_transient_ring_allocate(Ps5TransientRing *ring, uint32_t slot_index,
                                size_t bytes, size_t alignment,
                                Ps5TransientSlice *slice);
int ps5_transient_ring_seal(Ps5TransientRing *ring, uint32_t slot_index,
                            uint64_t retire_token);
int ps5_transient_ring_abort_unsubmitted(Ps5TransientRing *ring,
                                         uint32_t slot_index);

#endif
