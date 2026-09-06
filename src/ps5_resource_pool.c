#include "ps5_resource_pool.h"

#include <limits.h>
#include <string.h>

static int power_of_two(size_t value)
{
    return value && (value & (value - 1u)) == 0u;
}

static int aligned_offset(const Ps5ResourcePool *pool, size_t cursor,
                          size_t alignment, size_t *offset)
{
    if (!pool || !offset || cursor > pool->bytes || !power_of_two(alignment))
        return -1;
    const uintptr_t base = (uintptr_t)pool->base;
    if (base > UINTPTR_MAX - cursor)
        return -1;
    const uintptr_t address = base + cursor;
    if (address > UINTPTR_MAX - (alignment - 1u))
        return -1;
    const uintptr_t aligned =
        (address + alignment - 1u) & ~(uintptr_t)(alignment - 1u);
    if (aligned < base || aligned - base > SIZE_MAX)
        return -1;
    *offset = (size_t)(aligned - base);
    return *offset <= pool->bytes ? 0 : -1;
}

static int live(const Ps5ResourceRecord *record)
{
    return record->state == PS5_RESOURCE_ACTIVE ||
           record->state == PS5_RESOURCE_RETIRING;
}

static Ps5ResourceRecord *resolve(Ps5ResourcePool *pool,
                                  const Ps5ResourceAllocation *allocation)
{
    if (!pool || !allocation ||
        allocation->slot >= PS5_RESOURCE_POOL_MAX_ALLOCATIONS)
        return 0;
    Ps5ResourceRecord *record = &pool->records[allocation->slot];
    return live(record) && record->generation == allocation->generation &&
                   record->offset == allocation->offset &&
                   record->bytes == allocation->bytes
               ? record
               : 0;
}

int ps5_resource_pool_init(Ps5ResourcePool *pool, void *base, size_t bytes)
{
    if (!pool || !base || bytes == 0u)
        return PS5_RESOURCE_POOL_PRECONDITION;
    memset(pool, 0, sizeof(*pool));
    pool->base = base;
    pool->bytes = bytes;
    pool->next_generation = 1u;
    return PS5_RESOURCE_POOL_OK;
}

int ps5_resource_pool_allocate(Ps5ResourcePool *pool, size_t bytes,
                               size_t alignment,
                               Ps5ResourceAllocation *allocation)
{
    if (!pool || !pool->base || !allocation || bytes == 0u ||
        !power_of_two(alignment))
        return PS5_RESOURCE_POOL_PRECONDITION;

    uint32_t free_slot = PS5_RESOURCE_POOL_MAX_ALLOCATIONS;
    for (uint32_t index = 0; index < PS5_RESOURCE_POOL_MAX_ALLOCATIONS;
         ++index) {
        if (!live(&pool->records[index])) {
            free_slot = index;
            break;
        }
    }
    if (free_slot == PS5_RESOURCE_POOL_MAX_ALLOCATIONS)
        return PS5_RESOURCE_POOL_EXHAUSTED;

    size_t cursor = 0u;
    for (;;) {
        size_t candidate = 0u;
        if (aligned_offset(pool, cursor, alignment, &candidate) != 0 ||
            candidate > pool->bytes || bytes > pool->bytes - candidate)
            return PS5_RESOURCE_POOL_EXHAUSTED;

        size_t next_cursor = SIZE_MAX;
        int collision = 0;
        for (uint32_t index = 0; index < PS5_RESOURCE_POOL_MAX_ALLOCATIONS;
             ++index) {
            const Ps5ResourceRecord *record = &pool->records[index];
            if (!live(record))
                continue;
            const size_t candidate_end = candidate + bytes;
            const size_t record_end = record->offset + record->bytes;
            if (candidate < record_end && candidate_end > record->offset) {
                collision = 1;
                if (record_end < next_cursor)
                    next_cursor = record_end;
            }
        }
        if (!collision) {
            Ps5ResourceRecord *record = &pool->records[free_slot];
            uint32_t generation = pool->next_generation++;
            if (generation == 0u)
                generation = pool->next_generation++;
            *record = (Ps5ResourceRecord){
                .offset = candidate,
                .bytes = bytes,
                .generation = generation,
                .state = PS5_RESOURCE_ACTIVE,
            };
            *allocation = (Ps5ResourceAllocation){
                .offset = candidate,
                .bytes = bytes,
                .slot = free_slot,
                .generation = generation,
            };
            return PS5_RESOURCE_POOL_OK;
        }
        if (next_cursor == SIZE_MAX || next_cursor <= cursor)
            return PS5_RESOURCE_POOL_EXHAUSTED;
        cursor = next_cursor;
    }
}

void *ps5_resource_pool_pointer(const Ps5ResourcePool *pool,
                                const Ps5ResourceAllocation *allocation)
{
    Ps5ResourceRecord *record = resolve((Ps5ResourcePool *)pool, allocation);
    return record ? pool->base + record->offset : 0;
}

int ps5_resource_pool_release_unsubmitted(
    Ps5ResourcePool *pool, const Ps5ResourceAllocation *allocation)
{
    Ps5ResourceRecord *record = resolve(pool, allocation);
    if (!record)
        return PS5_RESOURCE_POOL_STALE_HANDLE;
    if (record->state != PS5_RESOURCE_ACTIVE)
        return PS5_RESOURCE_POOL_ALREADY_RETIRING;
    memset(record, 0, sizeof(*record));
    return PS5_RESOURCE_POOL_OK;
}

int ps5_resource_pool_release_deferred(
    Ps5ResourcePool *pool, const Ps5ResourceAllocation *allocation,
    uint64_t retire_token)
{
    Ps5ResourceRecord *record = resolve(pool, allocation);
    if (!record || retire_token == 0u)
        return record ? PS5_RESOURCE_POOL_PRECONDITION
                      : PS5_RESOURCE_POOL_STALE_HANDLE;
    if (record->state != PS5_RESOURCE_ACTIVE)
        return PS5_RESOURCE_POOL_ALREADY_RETIRING;
    record->state = PS5_RESOURCE_RETIRING;
    record->retire_token = retire_token;
    return PS5_RESOURCE_POOL_OK;
}

int ps5_resource_pool_reclaim(Ps5ResourcePool *pool, uint64_t retire_token,
                              int completion_proven,
                              uint32_t *reclaimed_count)
{
    if (!pool || !pool->base || !reclaimed_count || retire_token == 0u)
        return PS5_RESOURCE_POOL_PRECONDITION;
    *reclaimed_count = 0u;
    if (!completion_proven)
        return PS5_RESOURCE_POOL_COMPLETION_REQUIRED;
    for (uint32_t index = 0; index < PS5_RESOURCE_POOL_MAX_ALLOCATIONS;
         ++index) {
        Ps5ResourceRecord *record = &pool->records[index];
        if (record->state == PS5_RESOURCE_RETIRING &&
            record->retire_token == retire_token) {
            memset(record, 0, sizeof(*record));
            ++*reclaimed_count;
        }
    }
    return PS5_RESOURCE_POOL_OK;
}
