#ifndef PS5_AGC_GEARS_BUMP_ALLOCATOR_H
#define PS5_AGC_GEARS_BUMP_ALLOCATOR_H

#include <stddef.h>
#include <stdint.h>

typedef struct Ps5BumpAllocator {
    uint8_t *base;
    size_t bytes;
    size_t used;
} Ps5BumpAllocator;

int ps5_bump_allocator_init(Ps5BumpAllocator *allocator, void *base,
                            size_t bytes);
void *ps5_bump_allocate(Ps5BumpAllocator *allocator, size_t bytes,
                        size_t alignment);

#endif
