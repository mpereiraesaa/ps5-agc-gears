#include "../src/ps5_resource_pool.h"

#include <assert.h>
#include <stdint.h>

int main(void)
{
    union { uint64_t align; uint8_t bytes[512]; } memory = {0};
    Ps5ResourcePool pool;
    Ps5ResourceAllocation a;
    Ps5ResourceAllocation b;
    Ps5ResourceAllocation c;
    assert(ps5_resource_pool_init(&pool, memory.bytes + 1, 500u) == 0);
    assert(ps5_resource_pool_allocate(&pool, 31u, 32u, &a) == 0);
    assert(ps5_resource_pool_allocate(&pool, 64u, 64u, &b) == 0);
    assert(((uintptr_t)ps5_resource_pool_pointer(&pool, &a) & 31u) == 0u);
    assert(((uintptr_t)ps5_resource_pool_pointer(&pool, &b) & 63u) == 0u);

    assert(ps5_resource_pool_release_deferred(&pool, &a, 41u) == 0);
    assert(ps5_resource_pool_release_unsubmitted(&pool, &a) ==
           PS5_RESOURCE_POOL_ALREADY_RETIRING);
    uint32_t reclaimed = 99u;
    assert(ps5_resource_pool_reclaim(&pool, 41u, 0, &reclaimed) ==
           PS5_RESOURCE_POOL_COMPLETION_REQUIRED);
    assert(reclaimed == 0u);
    assert(ps5_resource_pool_pointer(&pool, &a) != 0);
    assert(ps5_resource_pool_reclaim(&pool, 40u, 1, &reclaimed) == 0);
    assert(reclaimed == 0u);
    assert(ps5_resource_pool_reclaim(&pool, 41u, 1, &reclaimed) == 0);
    assert(reclaimed == 1u);
    assert(ps5_resource_pool_pointer(&pool, &a) == 0);

    assert(ps5_resource_pool_allocate(&pool, 16u, 32u, &c) == 0);
    assert(c.offset == a.offset);
    assert(c.generation != a.generation);
    assert(ps5_resource_pool_release_unsubmitted(&pool, &a) ==
           PS5_RESOURCE_POOL_STALE_HANDLE);
    assert(ps5_resource_pool_release_unsubmitted(&pool, &b) == 0);
    assert(ps5_resource_pool_release_unsubmitted(&pool, &c) == 0);
    assert(ps5_resource_pool_allocate(&pool, 600u, 16u, &a) ==
           PS5_RESOURCE_POOL_EXHAUSTED);
    return 0;
}
