#include "ps5_direct_memory.h"

#include <string.h>

static int power_of_two(size_t value)
{
    return value && (value & (value - 1u)) == 0u;
}

static int valid_ops(const struct ps5_direct_memory_ops *ops)
{
    return ops && ops->reserve_virtual && ops->allocate_direct &&
           ops->map_direct && ops->unmap_direct && ops->release_direct;
}

int ps5_direct_memory_open(struct ps5_direct_memory *memory,
                           const struct ps5_direct_memory_ops *ops,
                           size_t bytes, size_t alignment,
                           int memory_type, int protection)
{
    if (!memory || !valid_ops(ops) || !bytes || !power_of_two(alignment) ||
        (bytes & (alignment - 1u)) != 0u)
        return PS5_DIRECT_MEMORY_PRECONDITION;
    memset(memory, 0, sizeof(*memory));
    memory->offset = -1;
    memory->bytes = bytes;
    memory->alignment = alignment;
    memory->memory_type = memory_type;
    memory->protection = protection;
    if (ops->reserve_virtual(&memory->address, bytes, 0, alignment) != 0 ||
        !memory->address)
        return PS5_DIRECT_MEMORY_RESERVE_FAILED;
    if (ops->allocate_direct(bytes, alignment, memory_type, &memory->offset) != 0 ||
        memory->offset < 0)
        return PS5_DIRECT_MEMORY_ALLOCATE_FAILED;
    memory->allocated = 1;
    void *mapped = memory->address;
    if (ops->map_direct(&mapped, bytes, protection, 0,
                        memory->offset, alignment) != 0 ||
        mapped != memory->address) {
        if (ops->release_direct(memory->offset, bytes) == 0)
            memory->allocated = 0;
        else
            memory->retain = 1;
        return PS5_DIRECT_MEMORY_MAP_FAILED;
    }
    memory->mapped = 1;
    return PS5_DIRECT_MEMORY_OK;
}

int ps5_direct_memory_close(struct ps5_direct_memory *memory,
                            const struct ps5_direct_memory_ops *ops,
                            int gpu_cleanup_allowed)
{
    if (!memory || !valid_ops(ops))
        return PS5_DIRECT_MEMORY_PRECONDITION;
    if (memory->retain || ((memory->mapped || memory->allocated) &&
                           !gpu_cleanup_allowed)) {
        memory->retain = 1;
        return PS5_DIRECT_MEMORY_OWNED_BY_GPU;
    }
    if (memory->mapped) {
        if (ops->unmap_direct(memory->address, memory->bytes) != 0) {
            memory->retain = 1;
            return PS5_DIRECT_MEMORY_UNMAP_FAILED;
        }
        memory->mapped = 0;
    }
    if (memory->allocated) {
        if (ops->release_direct(memory->offset, memory->bytes) != 0) {
            memory->retain = 1;
            return PS5_DIRECT_MEMORY_RELEASE_FAILED;
        }
        memory->allocated = 0;
    }
    memory->address = 0;
    memory->offset = -1;
    return PS5_DIRECT_MEMORY_OK;
}
