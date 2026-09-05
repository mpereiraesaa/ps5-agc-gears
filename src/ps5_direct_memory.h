#ifndef PS5_AGC_GEARS_DIRECT_MEMORY_H
#define PS5_AGC_GEARS_DIRECT_MEMORY_H

#include <stddef.h>
#include <stdint.h>

typedef int (*ps5_reserve_virtual_fn)(void **address, size_t bytes,
                                      int flags, size_t alignment);
typedef int (*ps5_allocate_direct_fn)(size_t bytes, size_t alignment,
                                      int memory_type, int64_t *offset);
typedef int (*ps5_map_direct_fn)(void **address, size_t bytes,
                                 int protection, int flags, int64_t offset,
                                 size_t alignment);
typedef int (*ps5_unmap_direct_fn)(void *address, size_t bytes);
typedef int (*ps5_release_direct_fn)(int64_t offset, size_t bytes);

struct ps5_direct_memory_ops {
    ps5_reserve_virtual_fn reserve_virtual;
    ps5_allocate_direct_fn allocate_direct;
    ps5_map_direct_fn map_direct;
    ps5_unmap_direct_fn unmap_direct;
    ps5_release_direct_fn release_direct;
};

struct ps5_direct_memory {
    void *address;
    int64_t offset;
    size_t bytes;
    size_t alignment;
    int memory_type;
    int protection;
    int allocated;
    int mapped;
    int retain;
};

enum ps5_direct_memory_result {
    PS5_DIRECT_MEMORY_OK = 0,
    PS5_DIRECT_MEMORY_PRECONDITION = -1,
    PS5_DIRECT_MEMORY_RESERVE_FAILED = -2,
    PS5_DIRECT_MEMORY_ALLOCATE_FAILED = -3,
    PS5_DIRECT_MEMORY_MAP_FAILED = -4,
    PS5_DIRECT_MEMORY_OWNED_BY_GPU = -5,
    PS5_DIRECT_MEMORY_UNMAP_FAILED = -6,
    PS5_DIRECT_MEMORY_RELEASE_FAILED = -7,
};

int ps5_direct_memory_open(struct ps5_direct_memory *memory,
                           const struct ps5_direct_memory_ops *ops,
                           size_t bytes, size_t alignment,
                           int memory_type, int protection);
int ps5_direct_memory_close(struct ps5_direct_memory *memory,
                            const struct ps5_direct_memory_ops *ops,
                            int gpu_cleanup_allowed);

#endif
