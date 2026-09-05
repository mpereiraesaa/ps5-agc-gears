#include "../src/ps5_direct_memory.h"

#include <assert.h>
#include <stdint.h>

static int step;
static int fail_at;
static int releases;
static unsigned char arena[0x4000];

static int reserve_virtual(void **address, size_t bytes, int flags,
                           size_t alignment)
{
    assert(++step == 1 && bytes == sizeof(arena) && flags == 0);
    assert(alignment == sizeof(arena));
    *address = arena;
    return fail_at == step ? -1 : 0;
}
static int allocate_direct(size_t bytes, size_t alignment, int type,
                           int64_t *offset)
{
    assert(++step == 2 && bytes == sizeof(arena));
    assert(alignment == sizeof(arena) && type == 3);
    *offset = 0x8000;
    return fail_at == step ? -1 : 0;
}
static int map_direct(void **address, size_t bytes, int protection,
                      int flags, int64_t offset, size_t alignment)
{
    assert(++step == 3 && *address == arena && bytes == sizeof(arena));
    assert(protection == 0x33 && flags == 0 && offset == 0x8000);
    assert(alignment == sizeof(arena));
    return fail_at == step ? -1 : 0;
}
static int unmap_direct(void *address, size_t bytes)
{
    assert(++step == 4 && address == arena && bytes == sizeof(arena));
    return fail_at == step ? -1 : 0;
}
static int release_direct(int64_t offset, size_t bytes)
{
    ++releases;
    assert(offset == 0x8000 && bytes == sizeof(arena));
    if (step < 4)
        ++step;
    else
        assert(++step == 5);
    return fail_at == step ? -1 : 0;
}

int main(void)
{
    const struct ps5_direct_memory_ops ops = {
        reserve_virtual, allocate_direct, map_direct,
        unmap_direct, release_direct,
    };
    struct ps5_direct_memory memory;
    step = fail_at = releases = 0;
    assert(ps5_direct_memory_open(&memory, &ops, sizeof(arena), sizeof(arena),
                                  3, 0x33) == PS5_DIRECT_MEMORY_OK);
    assert(memory.mapped && memory.allocated && !memory.retain);
    assert(ps5_direct_memory_close(&memory, &ops, 0) ==
           PS5_DIRECT_MEMORY_OWNED_BY_GPU);
    assert(memory.mapped && memory.allocated && memory.retain);

    step = fail_at = releases = 0;
    assert(ps5_direct_memory_open(&memory, &ops, sizeof(arena), sizeof(arena),
                                  3, 0x33) == PS5_DIRECT_MEMORY_OK);
    assert(ps5_direct_memory_close(&memory, &ops, 1) == PS5_DIRECT_MEMORY_OK);
    assert(!memory.mapped && !memory.allocated && releases == 1);

    step = releases = 0;
    fail_at = 3;
    assert(ps5_direct_memory_open(&memory, &ops, sizeof(arena), sizeof(arena),
                                  3, 0x33) == PS5_DIRECT_MEMORY_MAP_FAILED);
    assert(!memory.mapped && !memory.allocated && releases == 1);
    assert(ps5_direct_memory_open(&memory, &ops, 3u, 2u, 3, 0x33) ==
           PS5_DIRECT_MEMORY_PRECONDITION);
    return 0;
}
