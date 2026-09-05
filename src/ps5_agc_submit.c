#include "ps5_agc_submit.h"

#include "ps5_gpu_span.h"
#include "../include/ps5_platform.h"

int ps5_agc_submit_checked(const uint32_t *commands, uint32_t dwords,
                           void *opaque)
{
    const struct ps5_agc_submit_context *context = opaque;
    if (!context || !commands || dwords == 0u || !context->flush ||
        !context->submit)
        return PS5_AGC_SUBMIT_PRECONDITION;
    const size_t bytes = (size_t)dwords * sizeof(uint32_t);
    if (!ps5_gpu_span_visible(context->gpu_mapping,
                              context->gpu_mapping_bytes, commands, bytes))
        return PS5_AGC_SUBMIT_NOT_GPU_VISIBLE;

    context->flush(commands, bytes);
    struct ps5_agc_submit description = {commands, dwords, 0u, {0u, 0u, 0u}};
    return context->submit(&description);
}
