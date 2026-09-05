#ifndef PS5_AGC_GEARS_AGC_DRIVER_H
#define PS5_AGC_GEARS_AGC_DRIVER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int32_t sceAgcDriverSubmitDcb(void *description);
uint32_t sceAgcDriverGetWaitRenderingPacketSizeInDwords(void);
uint32_t sceAgcDriverWaitUntilSafeForRendering(
    uint32_t **command, uint32_t packet_size, uint32_t reserved,
    uint32_t video_handle, int32_t buffer_index);

#ifdef __cplusplus
}
#endif

#endif
