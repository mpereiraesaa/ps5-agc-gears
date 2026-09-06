#include "ps5_bump_allocator.h"

#include <stdint.h>

int ps5_bump_allocator_init(Ps5BumpAllocator *allocator, void *base,
                            size_t bytes)
{
    if (!allocator || !base || bytes == 0u)
        return -1;
    allocator->base = base;
    allocator->bytes = bytes;
    allocator->used = 0u;
    return 0;
}

void *ps5_bump_allocate(Ps5BumpAllocator *allocator, size_t bytes,
                        size_t alignment)
{
    if (!allocator || !allocator->base || bytes == 0u || alignment == 0u ||
        (alignment & (alignment - 1u)) != 0u)
        return 0;
    const uintptr_t current = (uintptr_t)allocator->base + allocator->used;
    if (current < (uintptr_t)allocator->base ||
        current > UINTPTR_MAX - (alignment - 1u))
        return 0;
    const uintptr_t aligned = (current + alignment - 1u) & ~(alignment - 1u);
    if (aligned < (uintptr_t)allocator->base)
        return 0;
    const size_t offset = (size_t)(aligned - (uintptr_t)allocator->base);
    if (offset > allocator->bytes || bytes > allocator->bytes - offset)
        return 0;
    allocator->used = offset + bytes;
    return (void *)aligned;
}
