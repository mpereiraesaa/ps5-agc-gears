#ifndef PS5_AGC_GEARS_GPU_SPAN_H
#define PS5_AGC_GEARS_GPU_SPAN_H

#include <stddef.h>

int ps5_gpu_span_visible(const void *mapping, size_t mapping_bytes,
                         const void *pointer, size_t bytes);

#endif
