#include "../src/ps5_gpu_span.h"

#include <assert.h>
#include <stdint.h>

int main(void)
{
    uint8_t mapping[64] = {0};
    assert(ps5_gpu_span_visible(mapping, sizeof(mapping), mapping, 1u));
    assert(ps5_gpu_span_visible(mapping, sizeof(mapping), mapping + 48, 16u));
    assert(!ps5_gpu_span_visible(mapping, sizeof(mapping), mapping + 64, 1u));
    assert(!ps5_gpu_span_visible(mapping, sizeof(mapping), mapping + 63, 2u));
    assert(!ps5_gpu_span_visible(mapping, sizeof(mapping), mapping, 0u));
    assert(!ps5_gpu_span_visible(0, sizeof(mapping), mapping, 1u));
    assert(!ps5_gpu_span_visible((void *)(uintptr_t)(UINTPTR_MAX - 7u),
                                 16u, mapping, 1u));
    return 0;
}
