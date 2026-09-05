/* Link-time facade for runtime resolution from libSceAgcDriver. */
#include "../../include/ps5_agc_driver.h"

int32_t sceAgcDriverSubmitDcb(void *description)
{
    (void)description;
    return -1;
}

uint32_t sceAgcDriverGetWaitRenderingPacketSizeInDwords(void)
{
    return 0u;
}

uint32_t sceAgcDriverWaitUntilSafeForRendering(
    uint32_t **command, uint32_t packet_size, uint32_t reserved,
    uint32_t video_handle, int32_t buffer_index)
{
    (void)command; (void)packet_size; (void)reserved;
    (void)video_handle; (void)buffer_index;
    return 0u;
}
