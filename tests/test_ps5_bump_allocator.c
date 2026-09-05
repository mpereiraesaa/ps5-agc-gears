#include "../src/ps5_bump_allocator.h"

#include <assert.h>
#include <stdint.h>

int main(void)
{
    union { uint64_t align; uint8_t bytes[128]; } memory = {0};
    Ps5BumpAllocator allocator;
    assert(ps5_bump_allocator_init(&allocator, memory.bytes + 1, 100u) == 0);
    void *a = ps5_bump_allocate(&allocator, 7u, 16u);
    void *b = ps5_bump_allocate(&allocator, 32u, 32u);
    assert(a && b && ((uintptr_t)a & 15u) == 0u &&
           ((uintptr_t)b & 31u) == 0u && (uintptr_t)b > (uintptr_t)a);
    const size_t stable = allocator.used;
    assert(!ps5_bump_allocate(&allocator, 128u, 16u));
    assert(allocator.used == stable);
    assert(!ps5_bump_allocate(&allocator, 1u, 3u));
    assert(ps5_bump_allocator_init(0, memory.bytes, sizeof(memory.bytes)) != 0);
    return 0;
}
