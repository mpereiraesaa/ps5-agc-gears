#include "ps5_gpu_span.h"

#include <stdint.h>

int ps5_gpu_span_visible(const void *mapping, size_t mapping_bytes,
                         const void *pointer, size_t bytes)
{
    const uintptr_t base = (uintptr_t)mapping;
    const uintptr_t address = (uintptr_t)pointer;
    if (!mapping || !pointer || !mapping_bytes || !bytes ||
        base > UINTPTR_MAX - mapping_bytes || address > UINTPTR_MAX - bytes)
        return 0;
    return address >= base && address + bytes <= base + mapping_bytes;
}
