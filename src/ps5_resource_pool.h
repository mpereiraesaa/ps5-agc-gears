#ifndef PS5_AGC_GEARS_RESOURCE_POOL_H
#define PS5_AGC_GEARS_RESOURCE_POOL_H

#include <stddef.h>
#include <stdint.h>

enum {
    PS5_RESOURCE_POOL_MAX_ALLOCATIONS = 128,
};

enum ps5_resource_state {
    PS5_RESOURCE_UNUSED = 0,
    PS5_RESOURCE_ACTIVE = 1,
    PS5_RESOURCE_RETIRING = 2,
};

typedef struct Ps5ResourceAllocation {
    size_t offset;
    size_t bytes;
    uint32_t slot;
    uint32_t generation;
} Ps5ResourceAllocation;

typedef struct Ps5ResourceRecord {
    size_t offset;
    size_t bytes;
    uint64_t retire_token;
    uint32_t generation;
    uint8_t state;
} Ps5ResourceRecord;

typedef struct Ps5ResourcePool {
    uint8_t *base;
    size_t bytes;
    uint32_t next_generation;
    Ps5ResourceRecord records[PS5_RESOURCE_POOL_MAX_ALLOCATIONS];
} Ps5ResourcePool;

enum ps5_resource_pool_result {
    PS5_RESOURCE_POOL_OK = 0,
    PS5_RESOURCE_POOL_PRECONDITION = -1,
    PS5_RESOURCE_POOL_EXHAUSTED = -2,
    PS5_RESOURCE_POOL_STALE_HANDLE = -3,
    PS5_RESOURCE_POOL_ALREADY_RETIRING = -4,
    PS5_RESOURCE_POOL_COMPLETION_REQUIRED = -5,
};

int ps5_resource_pool_init(Ps5ResourcePool *pool, void *base, size_t bytes);
int ps5_resource_pool_allocate(Ps5ResourcePool *pool, size_t bytes,
                               size_t alignment,
                               Ps5ResourceAllocation *allocation);
void *ps5_resource_pool_pointer(const Ps5ResourcePool *pool,
                                const Ps5ResourceAllocation *allocation);

/* Use release_unsubmitted only before a resource has become GPU-visible. */
int ps5_resource_pool_release_unsubmitted(
    Ps5ResourcePool *pool, const Ps5ResourceAllocation *allocation);
int ps5_resource_pool_release_deferred(
    Ps5ResourcePool *pool, const Ps5ResourceAllocation *allocation,
    uint64_t retire_token);

/* completion_proven means the matching fence reached zero and the matching
 * VideoOut token was observed. A token is never reclaimed on fence alone. */
int ps5_resource_pool_reclaim(Ps5ResourcePool *pool, uint64_t retire_token,
                              int completion_proven,
                              uint32_t *reclaimed_count);

#endif
