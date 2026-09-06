#ifndef PS5_AGC_GEARS_PLATFORM_H
#define PS5_AGC_GEARS_PLATFORM_H

/* Minimal clean-room platform ABI used by the native backend. */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ps5_batch_map_entry {
    void *address;
    int64_t physical;
    size_t length;
    uint8_t protection;
    uint8_t memory_type;
    uint16_t reserved;
    uint32_t operation;
};

struct ps5_agc_submit {
    const uint32_t *words;
    uint32_t count;
    uint8_t flag;
    uint8_t padding[3];
};

struct ps5_agc_command_buffer {
    uint32_t *bottom;
    uint32_t *top;
    uint32_t *up;
    uint32_t *down;
    uintptr_t callback;
    void *user_data;
    uint32_t reserved_dwords;
    uint32_t padding;
};

struct ps5_video_buffer {
    void *data;
    void *metadata;
    void *reserved0;
    void *reserved1;
};

struct ps5_video_attribute {
    uint8_t reserved[80];
};

int sceKernelReserveVirtualRange(void **address, size_t bytes,
                                 int flags, size_t alignment);
int sceKernelAllocateMainDirectMemory(size_t bytes, size_t alignment,
                                      int memory_type, int64_t *offset);
int sceKernelMapDirectMemory(void **address, size_t bytes, int protection,
                             int flags, int64_t offset, size_t alignment);
int sceKernelBatchMap(void *entries, int count, int *processed);
int sceKernelMunmap(void *address, size_t bytes);
int sceKernelReleaseDirectMemory(int64_t offset, size_t bytes);
int sceKernelCreateEqueue(void **queue, const char *name);
int sceKernelDeleteEqueue(void *queue);
int sceKernelWaitEqueue(void *queue, void *event, int count, int *out,
                        unsigned int *timeout_us);

int sceVideoOutOpen(int32_t user_id, int32_t bus_type, int32_t index,
                    const void *param);
int sceVideoOutClose(int32_t handle);
int sceVideoOutSetFlipRate(int32_t handle, int32_t rate);
int sceVideoOutAddFlipEvent(void *queue, int32_t handle, void *user_data);
int sceVideoOutDeleteFlipEvent(void *queue, int32_t handle);
int sceVideoOutGetEventData(const void *event, int64_t *flip_arg);
void sceVideoOutSetBufferAttribute2(void *attribute, uint64_t format,
                                    uint32_t tiling, uint32_t width,
                                    uint32_t height, uint64_t option,
                                    uint32_t reserved0, uint64_t reserved1);
int sceVideoOutRegisterBuffers2(int32_t handle, int32_t set_index,
                                int32_t start_index, void *buffers,
                                int32_t count, void *attribute,
                                int32_t option, void *reserved);
int sceVideoOutUnregisterBuffers(int32_t handle, int32_t set_index);

int sceSysmoduleLoadModuleInternal(unsigned int id, ...);
int sceSysmoduleUnloadModuleInternal(unsigned int id, ...);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#define PS5_PLATFORM_STATIC_ASSERT static_assert
#else
#define PS5_PLATFORM_STATIC_ASSERT _Static_assert
#endif

PS5_PLATFORM_STATIC_ASSERT(sizeof(struct ps5_batch_map_entry) == 0x20,
               "batch-map entry ABI");
PS5_PLATFORM_STATIC_ASSERT(offsetof(struct ps5_batch_map_entry, operation) == 0x1c,
               "batch-map operation ABI");
PS5_PLATFORM_STATIC_ASSERT(sizeof(struct ps5_agc_submit) == 0x10, "submit ABI");
PS5_PLATFORM_STATIC_ASSERT(sizeof(struct ps5_agc_command_buffer) == 0x38,
               "command-buffer writer ABI");
PS5_PLATFORM_STATIC_ASSERT(sizeof(struct ps5_video_buffer) == 0x20,
               "VideoOut buffer ABI");
PS5_PLATFORM_STATIC_ASSERT(sizeof(struct ps5_video_attribute) == 80,
               "VideoOut attribute ABI");

#undef PS5_PLATFORM_STATIC_ASSERT

#endif
