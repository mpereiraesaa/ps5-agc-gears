#include "../include/ps5_agc.h"
#include "../include/ps5_agc_driver.h"
#include "../include/ps5_platform.h"
#include "../src/gears_frame_runner.h"
#include "../src/gears_mesh.h"
#include "../src/gears_renderer.h"
#include "../src/gears_rt_clear.h"
#include "../src/bsp_bundle.h"
#include "../src/bsp_command_plan.h"
#include "../src/bsp_flat_scene.h"
#include "../src/bsp_noclip.h"
#include "../src/bsp_runtime_plan.h"
#include "../src/bsp_texture_descriptor.h"
#include "../src/bsp_textured_draw.h"
#include "../src/bsp_resource_draw.h"
#include "../src/bsp_resource_frame.h"
#include "../src/ps5_agc_submit.h"
#include "../src/ps5_cache_contract.h"
#include "../src/ps5_color_target.h"
#include "../src/ps5_depth_target.h"
#include "../src/ps5_event_adapter.h"
#include "../src/ps5_gfx1013_descriptor.h"
#include "../src/ps5_pipeline.h"
#include "../src/ps5_resource_pool.h"
#include "../src/ps5_shader_header.h"
#include "../src/ps5_submission.h"
#include "../src/ps5_transient_ring.h"
#include "../src/ps5_surface.h"
#include "../src/ps5_videoout.h"
#include "ps5_agc_native.h"
#include "ps5log/ps5log.h"
#include "gears_shader_metadata.h"
#ifdef PS5_BSP_VIEWER
#include "bsp_build_metadata.h"
#include "bsp_flat_shader_metadata.h"
#ifdef PS5_BSP_TEXTURED
#include "bsp_textured_shader_metadata.h"
#ifdef PS5_RESOURCE_FOUNDATION
#include "bsp_resource_shader_metadata.h"
#include "bsp_overlay_shader_metadata.h"
#include "pipeline_permutations.h"
#endif
#endif
#endif

#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

extern const uint8_t ps5_gears_gs_start[], ps5_gears_gs_end[];
extern const uint8_t ps5_gears_ps_start[], ps5_gears_ps_end[];
#ifdef PS5_BSP_VIEWER
extern const uint8_t ps5_bsp_flat_gs_start[], ps5_bsp_flat_gs_end[];
extern const uint8_t ps5_bsp_flat_ps_start[], ps5_bsp_flat_ps_end[];
#ifdef PS5_BSP_TEXTURED
extern const uint8_t ps5_bsp_textured_gs_start[], ps5_bsp_textured_gs_end[];
extern const uint8_t ps5_bsp_textured_ps_start[], ps5_bsp_textured_ps_end[];
#ifdef PS5_RESOURCE_FOUNDATION
extern const uint8_t ps5_bsp_resource_gs_start[], ps5_bsp_resource_gs_end[];
extern const uint8_t ps5_bsp_resource_ps_start[], ps5_bsp_resource_ps_end[];
extern const uint8_t ps5_bsp_overlay_gs_start[], ps5_bsp_overlay_gs_end[];
extern const uint8_t ps5_bsp_overlay_ps_start[], ps5_bsp_overlay_ps_end[];
#endif
#endif
#endif

enum {
    AGC_MODULE = 0x80000094u,
    COMMAND_BYTES = 0x20000u,
    COMMAND_ALIGNMENT = 0x10000u,
    COMMAND_SLOT_BYTES = 0x1000u,
    FENCE0_OFFSET = 0x1100u,
    FENCE1_OFFSET = 0x1200u,
    SHADER_BYTES = 0x40000u,
    SHADER_ALIGNMENT = 0x4000u,
    GS_HEADER_OFFSET = 0x0000u,
    PS_HEADER_OFFSET = 0x0200u,
    GS_CODE_OFFSET = 0x1000u,
    PS_CODE_OFFSET = 0x1200u,
    LINKED_CX_OFFSET = 0x2000u,
    LINKED_UC_OFFSET = 0x2200u,
    PIPELINE_OFFSET = 0x3000u,
    DEPTH_REGISTERS_OFFSET = 0x3e00u,
#ifdef PS5_RESOURCE_FOUNDATION
    OVERLAY_GS_HEADER_OFFSET = 0x4000u,
    OVERLAY_PS_HEADER_OFFSET = 0x4200u,
    OVERLAY_GS_CODE_OFFSET = 0x5000u,
    OVERLAY_PS_CODE_OFFSET = 0x5200u,
    OVERLAY_LINKED_CX_OFFSET = 0x6000u,
    OVERLAY_LINKED_UC_OFFSET = 0x6200u,
    OVERLAY_PIPELINE_OFFSET = 0x7000u,
    RESOURCE_TRANSIENT_BYTES = 0x40000u,
    RESOURCE_HEAP_ALIGNMENT = 0x10000u,
#endif
    GEAR0_OFFSET = 0x4000u,
    GEAR1_OFFSET = 0x10000u,
    GEAR2_OFFSET = 0x16000u,
    GEAR_SRD_OFFSET = 0x1c000u,
    CLEAR_SRD_OFFSET = 0x1c100u,
    CLEAR_VERTEX_OFFSET = 0x1c200u,
    SHADER_FOOTER_BYTES = 0x30u,
    DEPTH_BYTES = 0x870000u,
    DEPTH_ALLOCATION_BYTES = 0x900000u,
    DEPTH_ALIGNMENT = 0x10000u,
    GUARD_WORD = 0x51a6c3d9u,
    BSP_FIXED_COMMAND_DWORDS = 4096u,
    BSP_MAX_BUNDLE_BYTES = 64u * 1024u * 1024u,
#ifdef PS5_BSP_TEXTURED
    BSP_GATE_FRAME_COUNT = 60000u,
    BSP_NOCLIP_MIN_MOVING_FRAMES = 600u,
    BSP_NOCLIP_MIN_LOOKING_FRAMES = 120u,
#elif defined(PS5_BSP_NOCLIP)
    BSP_GATE_FRAME_COUNT = 10000u,
    BSP_NOCLIP_MIN_MOVING_FRAMES = 600u,
    BSP_NOCLIP_MIN_LOOKING_FRAMES = 120u,
#else
    BSP_GATE_FRAME_COUNT = 600u,
#endif
};

struct native_resources {
    uint64_t agc_state;
    void *command;
    int64_t command_offset;
    void *framebuffer;
    int64_t framebuffer_offset;
    void *shader;
    int64_t shader_offset;
    void *depth;
    int64_t depth_offset;
    void *bsp;
    int64_t bsp_offset;
#ifdef PS5_RESOURCE_FOUNDATION
    void *resource_heap;
    int64_t resource_heap_offset;
    size_t resource_heap_bytes;
    void *transient;
    Ps5ResourcePool resource_pool;
    Ps5ResourceAllocation bsp_allocation;
    Ps5ResourceAllocation shader_allocation;
    Ps5ResourceAllocation depth_allocation;
    Ps5ResourceAllocation transient_allocation;
#endif
    size_t command_bytes;
    size_t bsp_bytes;
    int32_t pad_handle;
    struct ps5_videoout video;
    struct ps5_surface_plan surface;
    int agc_loaded;
    int command_reserved;
    int command_allocated;
    int command_mapped;
    int framebuffer_allocated;
    int framebuffer_mapped;
    int shader_allocated;
    int shader_mapped;
    int depth_allocated;
    int depth_mapped;
    int bsp_allocated;
    int bsp_mapped;
#ifdef PS5_RESOURCE_FOUNDATION
    int resource_heap_allocated;
    int resource_heap_mapped;
#endif
    int pad_opened;
};

struct native_renderer {
    uint32_t *commands[2];
    uint32_t *cursor[2];
    volatile uint64_t *fences[2];
    struct ps5_pipeline_registers *pipelines[2];
#ifdef PS5_RESOURCE_FOUNDATION
    struct ps5_pipeline_registers *overlay_pipelines[2];
    Ps5TransientRing transient_ring;
    BspResourceFrame resource_frames[2];
    Ps5CpuToGpuPlan cache_plans[2];
    uint64_t completed_tokens[2];
    uint64_t last_completed_token;
    uint64_t overlay_draw_modifier;
    BspBundleVertex *clear_vertices;
    uint16_t *clear_indices;
#endif
    ps5_agc_register *depth_registers;
    GearsSceneDraw clear_draw;
    uint32_t srd_tables[GEARS_SCENE_DRAW_COUNT];
    uint32_t vertex_counts[GEARS_SCENE_DRAW_COUNT];
    uint64_t draw_modifier;
    size_t command_slot_bytes;
#ifdef PS5_BSP_VIEWER
    BspBundleView bsp_bundle;
    BspRuntimePlan bsp_plan;
    BspFlatDraw *bsp_draws;
    uint32_t bsp_draw_count;
#ifdef PS5_BSP_TEXTURED
    const uint32_t **bsp_texture_bindings;
#endif
#ifdef PS5_BSP_NOCLIP
    BspNoclipCamera noclip;
    float last_camera_time;
    uint64_t pad_read_errors;
#endif
#endif
    struct ps5_agc_submit_context submit;
    struct native_resources *resources;
    int transaction_started;
};

static struct native_resources resources;
static struct native_renderer renderer;

static uint64_t now_ns(void *unused)
{
    struct timespec value = {0, 0};
    (void)unused;
    (void)clock_gettime(CLOCK_MONOTONIC, &value);
    return (uint64_t)value.tv_sec * UINT64_C(1000000000) +
           (uint64_t)value.tv_nsec;
}

static void log_result(const char *name, int result)
{
    (void)ps5log_printf(result == 0 ? PS5LOG_INFO : PS5LOG_ERR,
                        "%s=0x%08x", name, (uint32_t)result);
}

static int allocate_direct(void **address, int64_t *offset, size_t bytes,
                           size_t alignment)
{
    int result = sceKernelAllocateMainDirectMemory(bytes, alignment, 0x0c,
                                                    offset);
    if (result != 0)
        return result;
    result = sceKernelMapDirectMemory(address, bytes, 0x33, 0, *offset,
                                      alignment);
    return result != 0 || !*address ? (result != 0 ? result : -1) : 0;
}

static int map_command(void)
{
    int result = sceKernelReserveVirtualRange(&resources.command,
                                               resources.command_bytes, 0,
                                               COMMAND_ALIGNMENT);
    if (result != 0 || !resources.command)
        return result != 0 ? result : -1;
    resources.command_reserved = 1;
    result = sceKernelAllocateMainDirectMemory(resources.command_bytes,
                                                COMMAND_ALIGNMENT, 0x0c,
                                                &resources.command_offset);
    if (result != 0)
        return result;
    resources.command_allocated = 1;
    struct ps5_batch_map_entry entry = {
        resources.command, resources.command_offset, resources.command_bytes,
        0xf2, 0x0c, 0, 0
    };
    int processed = -1;
    result = sceKernelBatchMap(&entry, 1, &processed);
    if (result != 0 || processed != 1)
        return result != 0 ? result : -1;
    resources.command_mapped = 1;
    memset(resources.command, 0, resources.command_bytes);
    return 0;
}

#ifdef PS5_RESOURCE_FOUNDATION
static int add_aligned(size_t *total, size_t bytes, size_t alignment)
{
    if (!total || !bytes || !alignment ||
        (alignment & (alignment - 1u)) != 0u ||
        *total > SIZE_MAX - (alignment - 1u))
        return -1;
    const size_t aligned = (*total + alignment - 1u) & ~(alignment - 1u);
    if (bytes > SIZE_MAX - aligned)
        return -1;
    *total = aligned + bytes;
    return 0;
}

static int init_resource_heap(size_t bsp_bytes)
{
    size_t heap_bytes = 0u;
    if (add_aligned(&heap_bytes, bsp_bytes,
                    BSP_RUNTIME_ALLOCATION_ALIGNMENT) != 0 ||
        add_aligned(&heap_bytes, SHADER_BYTES, SHADER_ALIGNMENT) != 0 ||
        add_aligned(&heap_bytes, DEPTH_ALLOCATION_BYTES,
                    DEPTH_ALIGNMENT) != 0 ||
        add_aligned(&heap_bytes, RESOURCE_TRANSIENT_BYTES,
                    RESOURCE_HEAP_ALIGNMENT) != 0 ||
        add_aligned(&heap_bytes, 64u, RESOURCE_HEAP_ALIGNMENT) != 0)
        return -1;
    heap_bytes = (heap_bytes + RESOURCE_HEAP_ALIGNMENT - 1u) &
                 ~(size_t)(RESOURCE_HEAP_ALIGNMENT - 1u);
    int result = allocate_direct(&resources.resource_heap,
                                 &resources.resource_heap_offset,
                                 heap_bytes, RESOURCE_HEAP_ALIGNMENT);
    if (result != 0)
        return result;
    resources.resource_heap_allocated = resources.resource_heap_mapped = 1;
    resources.resource_heap_bytes = heap_bytes;
    memset(resources.resource_heap, 0, heap_bytes);
    if (ps5_resource_pool_init(&resources.resource_pool,
                               resources.resource_heap, heap_bytes) != 0 ||
        ps5_resource_pool_allocate(
            &resources.resource_pool, bsp_bytes,
            BSP_RUNTIME_ALLOCATION_ALIGNMENT,
            &resources.bsp_allocation) != 0 ||
        ps5_resource_pool_allocate(
            &resources.resource_pool, SHADER_BYTES, SHADER_ALIGNMENT,
            &resources.shader_allocation) != 0 ||
        ps5_resource_pool_allocate(
            &resources.resource_pool, DEPTH_ALLOCATION_BYTES,
            DEPTH_ALIGNMENT, &resources.depth_allocation) != 0 ||
        ps5_resource_pool_allocate(
            &resources.resource_pool, RESOURCE_TRANSIENT_BYTES,
            RESOURCE_HEAP_ALIGNMENT,
            &resources.transient_allocation) != 0)
        return -2;
    resources.bsp = ps5_resource_pool_pointer(
        &resources.resource_pool, &resources.bsp_allocation);
    resources.shader = ps5_resource_pool_pointer(
        &resources.resource_pool, &resources.shader_allocation);
    resources.depth = ps5_resource_pool_pointer(
        &resources.resource_pool, &resources.depth_allocation);
    resources.transient = ps5_resource_pool_pointer(
        &resources.resource_pool, &resources.transient_allocation);
    if (!resources.bsp || !resources.shader || !resources.depth ||
        !resources.transient ||
        ((uintptr_t)resources.resource_heap >> 32) != UINT64_C(2) ||
        ps5_transient_ring_init(
            &renderer.transient_ring, resources.transient,
            RESOURCE_TRANSIENT_BYTES, 2u, 256u) != 0)
        return -3;
    resources.bsp_bytes = bsp_bytes;
    return 0;
}
#endif

#ifdef PS5_BSP_VIEWER
static int read_exact(int fd, void *buffer, size_t bytes)
{
    uint8_t *cursor = buffer;
    while (bytes != 0u) {
        const ssize_t got = read(fd, cursor, bytes);
        if (got <= 0)
            return -1;
        cursor += (size_t)got;
        bytes -= (size_t)got;
    }
    return 0;
}

static int load_bsp_bundle(void)
{
    const int fd = open("/app0/map.ps5bsp", O_RDONLY);
    if (fd < 0)
        return -1;
    struct stat status;
    if (fstat(fd, &status) != 0 || status.st_size <= 0 ||
        (uint64_t)status.st_size > BSP_MAX_BUNDLE_BYTES ||
        (uint64_t)status.st_size != PS5_BSP_BUNDLE_BYTES) {
        (void)close(fd);
        return -2;
    }
    const size_t file_bytes = (size_t)status.st_size;
    BspRuntimePlan upper;
    const uint32_t maximum_draws =
        (uint32_t)(file_bytes / sizeof(BspBundleDraw));
#ifdef PS5_BSP_TEXTURED
    const uint32_t maximum_textures =
        (uint32_t)(file_bytes / sizeof(BspBundleTexture));
    const int upper_result = bsp_runtime_plan_textured(
        file_bytes, maximum_draws, maximum_textures, &upper);
#else
    const int upper_result = bsp_runtime_plan(file_bytes, maximum_draws,
                                               &upper);
#endif
    if (upper_result != 0) {
        (void)close(fd);
        return -3;
    }
    int result;
#ifdef PS5_RESOURCE_FOUNDATION
    result = init_resource_heap(upper.allocation_bytes);
#else
    result = allocate_direct(&resources.bsp, &resources.bsp_offset,
                             upper.allocation_bytes,
                             BSP_RUNTIME_ALLOCATION_ALIGNMENT);
#endif
    if (result != 0) {
        (void)close(fd);
        return result;
    }
#ifndef PS5_RESOURCE_FOUNDATION
    resources.bsp_allocated = resources.bsp_mapped = 1;
    resources.bsp_bytes = upper.allocation_bytes;
#endif
    memset(resources.bsp, 0, resources.bsp_bytes);
    result = read_exact(fd, resources.bsp, file_bytes);
    const int close_result = close(fd);
    if (result != 0 || close_result != 0)
        return -4;
    if (bsp_bundle_open(resources.bsp, file_bytes,
                        &renderer.bsp_bundle) != BSP_BUNDLE_OK)
        return -5;
#ifdef PS5_BSP_TEXTURED
    result = bsp_runtime_plan_textured(
        file_bytes, renderer.bsp_bundle.draw_count,
        renderer.bsp_bundle.texture_count, &renderer.bsp_plan);
#else
    result = bsp_runtime_plan(file_bytes, renderer.bsp_bundle.draw_count,
                              &renderer.bsp_plan);
#endif
    if (result != 0 ||
        renderer.bsp_plan.allocation_bytes > resources.bsp_bytes)
        return -5;
    return 0;
}

static int write_vertex_srd(uint32_t table[4], const void *vertices,
                            uint32_t vertex_count)
{
    if (!table || !vertices || vertex_count == 0u ||
        ((uintptr_t)table >> 32) != UINT64_C(2))
        return -1;
    return ps5_gfx1013_build_vsharp(table, (uintptr_t)vertices,
                                    sizeof(BspBundleVertex),
                                    vertex_count);
}

static int prepare_bsp_scene(void)
{
    uint8_t *const base = resources.bsp;
    uint32_t *const srds = (uint32_t *)(base +
        renderer.bsp_plan.vertex_srds_offset);
    BspBundleVertex *const clear_vertices = (BspBundleVertex *)(base +
        renderer.bsp_plan.clear_vertices_offset);
    uint16_t *const clear_indices = (uint16_t *)(base +
        renderer.bsp_plan.clear_indices_offset);
#ifdef PS5_RESOURCE_FOUNDATION
    renderer.clear_vertices = clear_vertices;
    renderer.clear_indices = clear_indices;
#endif
    BspFlatDraw *const draws = (BspFlatDraw *)(base +
        renderer.bsp_plan.scene_draws_offset);
    if (write_vertex_srd(srds, renderer.bsp_bundle.vertices,
                         renderer.bsp_bundle.vertex_count) != 0 ||
        write_vertex_srd(srds + 4u, clear_vertices, 3u) != 0 ||
        bsp_flat_build_clear(&draws[0], clear_vertices, clear_indices,
                             (uint32_t)(uintptr_t)(srds + 4u)) != 0 ||
        bsp_flat_build_scene(draws + 1u, renderer.bsp_bundle.draw_count,
                             &renderer.bsp_bundle,
                             (uint32_t)(uintptr_t)srds,
                             (float)resources.surface.width /
                                 (float)resources.surface.height) != 0)
        return -1;
#if defined(PS5_BSP_TEXTURED) && !defined(PS5_RESOURCE_FOUNDATION)
    uint32_t *const descriptor_tables = (uint32_t *)(base +
        renderer.bsp_plan.descriptor_tables_offset);
    uint32_t written_dwords = 0u;
    const uintptr_t descriptor_address = (uintptr_t)descriptor_tables;
    if ((descriptor_address >> 32) != UINT64_C(2) ||
        bsp_texture_build_tables(
            descriptor_tables, renderer.bsp_plan.descriptor_table_dwords,
            &renderer.bsp_bundle,
            (uintptr_t)renderer.bsp_bundle.texture_pixels,
            (uintptr_t)renderer.bsp_bundle.lightmap_pixels,
            &written_dwords) != 0 ||
        written_dwords != renderer.bsp_plan.descriptor_table_dwords)
        return -2;
    const uint32_t **const bindings = (const uint32_t **)(base +
        renderer.bsp_plan.texture_bindings_offset);
    bindings[0] = descriptor_tables;
    for (uint32_t index = 0; index < renderer.bsp_bundle.draw_count; ++index) {
        const uint32_t texture =
            renderer.bsp_bundle.draws[index].base_texture;
        if (texture >= renderer.bsp_bundle.texture_count)
            return -3;
        bindings[index + 1u] = descriptor_tables +
            texture * BSP_TEXTURE_TABLE_DWORDS;
    }
    renderer.bsp_texture_bindings = bindings;
#endif
    renderer.bsp_draws = draws;
    renderer.bsp_draw_count = renderer.bsp_plan.scene_draw_count;
    volatile uint32_t *guard = (volatile uint32_t *)(base +
        renderer.bsp_plan.guard_offset);
    for (unsigned word = 0; word < 16u; ++word)
        guard[word] = GUARD_WORD;
    ps5_native_cache_flush(resources.bsp, resources.bsp_bytes);
    return 0;
}

#ifdef PS5_BSP_NOCLIP
static int open_noclip_pad(void)
{
    int32_t user_id = -1;
    int result = sceUserServiceInitialize(0);
    if (result != 0)
        return result;
    result = sceUserServiceGetForegroundUser(&user_id);
    if (result != 0 || user_id == -1)
        return result != 0 ? result : -1;
    result = scePadInit();
    if (result != 0)
        return result;
    resources.pad_handle = scePadOpen(user_id, 0, 0, 0);
    if (resources.pad_handle <= 0)
        return resources.pad_handle != 0 ? resources.pad_handle : -1;
    resources.pad_opened = 1;
    return bsp_noclip_init(&renderer.noclip,
                           renderer.bsp_bundle.camera_position,
                           renderer.bsp_bundle.camera_forward);
}

static int update_noclip_camera(struct native_renderer *state,
                                const GearsAnimationFrame *frame)
{
    struct ps5_pad_data pad;
    memset(&pad, 0, sizeof(pad));
    const int result = scePadReadState(state->resources->pad_handle, &pad);
    if (result != 0)
        ++state->pad_read_errors;
    const BspNoclipInput input = {
        pad.left_stick.x, pad.left_stick.y,
        pad.right_stick.x, pad.right_stick.y,
        pad.l2, pad.r2, result == 0 && pad.connected != 0,
    };
    const float delta = frame->frame_index == 0u ? 1.0f / 60.0f
        : frame->time_seconds - state->last_camera_time;
    state->last_camera_time = frame->time_seconds;
    if (bsp_noclip_step(&state->noclip, &input, delta) != 0
#ifndef PS5_RESOURCE_FOUNDATION
        ||
        bsp_flat_update_camera(state->bsp_draws + 1u,
            state->bsp_draw_count - 1u, state->noclip.position,
            state->noclip.forward,
            (float)state->resources->surface.width /
                (float)state->resources->surface.height) != 0
#endif
        )
        return -1;
    if ((frame->frame_index + 1u) % 600u == 0u)
        (void)ps5log_printf(PS5LOG_MARK,
            "BSP_NOCLIP_INPUT frame=%llu connected=%llu moving=%llu "
            "looking=%llu distance_milli=%llu changes=%llu hash=%016llx "
            "read_errors=%llu",
            (unsigned long long)frame->frame_index,
            (unsigned long long)state->noclip.connected_frames,
            (unsigned long long)state->noclip.moving_frames,
            (unsigned long long)state->noclip.looking_frames,
            (unsigned long long)(state->noclip.distance_travelled * 1000.0f),
            (unsigned long long)state->noclip.input_changes,
            (unsigned long long)state->noclip.input_hash,
            (unsigned long long)state->pad_read_errors);
    return 0;
}
#endif
#endif

static const struct ps5_videoout_ops video_ops = {
    sceVideoOutOpen, sceVideoOutClose, sceVideoOutSetFlipRate,
    sceKernelCreateEqueue, sceKernelDeleteEqueue,
    sceVideoOutAddFlipEvent, sceVideoOutDeleteFlipEvent,
    sceVideoOutSetBufferAttribute2, sceVideoOutRegisterBuffers2,
    sceVideoOutUnregisterBuffers
};

#ifdef PS5_RESOURCE_FOUNDATION
static int resource_compose_fail(struct native_renderer *state,
                                 uint32_t slot, int result)
{
    (void)ps5_transient_ring_abort_unsubmitted(&state->transient_ring, slot);
    return result;
}
#endif

static int frame_compose(const GearsAnimationFrame *frame, void *opaque)
{
    struct native_renderer *state = opaque;
    if (!frame || !state || frame->buffer >= 2u)
        return -1;
#ifdef PS5_BSP_NOCLIP
    if (update_noclip_camera(state, frame) != 0)
        return -9;
#endif
#ifdef PS5_RESOURCE_FOUNDATION
    const uint32_t resource_slot = frame->buffer;
    const uint64_t completed = state->completed_tokens[resource_slot];
    if (ps5_transient_ring_begin(
            &state->transient_ring, resource_slot, completed,
            completed != 0u) != PS5_TRANSIENT_OK)
        return -8;
    if (bsp_resource_frame_build(
            &state->resource_frames[resource_slot],
            &state->transient_ring, resource_slot,
            state->resources->resource_heap,
            state->resources->resource_heap_bytes,
            &state->bsp_bundle, state->clear_vertices,
            state->clear_indices, state->noclip.position,
            state->noclip.forward,
            (float)state->resources->surface.width /
                (float)state->resources->surface.height,
            frame->frame_index) != 0) {
        return resource_compose_fail(state, resource_slot, -9);
    }
    Ps5TransientSlot *const transient_slot =
        &state->transient_ring.slots[resource_slot];
    const void *const transient_begin = state->transient_ring.base +
        transient_slot->offset;
    if (ps5_cache_cpu_to_gpu_plan(
            state->resources->resource_heap,
            state->resources->resource_heap_bytes, transient_begin,
            transient_slot->used,
            &state->cache_plans[resource_slot]) != 0) {
        return resource_compose_fail(state, resource_slot, -9);
    }
    ps5_native_cache_flush(
        state->cache_plans[resource_slot].flush_address,
        state->cache_plans[resource_slot].flush_bytes);
    if (frame->frame_index == 0u ||
        frame->frame_index + 1u == BSP_GATE_FRAME_COUNT)
        (void)ps5log_printf(PS5LOG_MARK,
            "RESOURCE_FRAME_READY frame=%llu slot=%u transient_bytes=%llu "
            "constant_dwords=%u texture_descriptor_dwords=%u "
            "overlay_vertices=%u overlay_indices=%u acquire_engine=%u "
            "gcr=%08x poll_cycles=%x "
            "tables_gpu_span=true",
            (unsigned long long)frame->frame_index, resource_slot,
            (unsigned long long)
                state->resource_frames[resource_slot].transient_bytes,
            BSP_RESOURCE_CONSTANT_DWORDS,
            state->resource_frames[resource_slot].texture_table_dwords,
            BSP_RESOURCE_OVERLAY_VERTICES,
            BSP_RESOURCE_OVERLAY_INDICES,
            state->cache_plans[resource_slot].engine,
            state->cache_plans[resource_slot].gcr_control,
            state->cache_plans[resource_slot].poll_cycles);
#endif
    uint32_t *const begin = state->commands[frame->buffer];
    uint32_t *const end = begin +
        state->command_slot_bytes / sizeof(uint32_t);
    uint32_t *cursor = begin;
    memset(begin, 0, state->command_slot_bytes);
    int result = ps5_native_wait_rendering(
        &cursor, (uint32_t)(end - cursor), 0u,
        state->resources->video.handle, (int32_t)frame->buffer);
    if (result != 0)
#ifdef PS5_RESOURCE_FOUNDATION
        return resource_compose_fail(state, resource_slot, -10);
#else
        return -10;
#endif
#ifdef PS5_RESOURCE_FOUNDATION
    result = ps5_native_acquire_mem(
        &cursor, (uint32_t)(end - cursor),
        state->cache_plans[resource_slot].acquire_base,
        state->cache_plans[resource_slot].acquire_bytes,
        state->cache_plans[resource_slot].engine,
        state->cache_plans[resource_slot].gcr_control,
        state->cache_plans[resource_slot].poll_cycles,
        state->resources->resource_heap,
        state->resources->resource_heap_bytes);
    if (result != 0)
        return resource_compose_fail(state, resource_slot, -10);
#endif
    result = ps5_native_fill_depth(
        &cursor, (uint32_t)(end - cursor), state->resources->depth,
        UINT32_C(0x3f800000), DEPTH_BYTES, state->resources->depth,
        DEPTH_ALLOCATION_BYTES);
    if (result != 0)
#ifdef PS5_RESOURCE_FOUNDATION
        return resource_compose_fail(state, resource_slot, -11);
#else
        return -11;
#endif
    result = ps5_native_set_indirect(
        &cursor, (uint32_t)(end - cursor), state->depth_registers,
        PS5_DEPTH_REGISTER_COUNT, state->resources->shader, SHADER_BYTES,
        PS5_NATIVE_REGISTERS_CX);
    if (result != 0)
#ifdef PS5_RESOURCE_FOUNDATION
        return resource_compose_fail(state, resource_slot, -12);
#else
        return -12;
#endif
    struct ps5_pipeline_registers *pipeline = state->pipelines[frame->buffer];
    result = ps5_native_set_indirect(
        &cursor, (uint32_t)(end - cursor), pipeline->cx,
        PS5_PIPELINE_CX_REGISTERS, state->resources->shader, SHADER_BYTES,
        PS5_NATIVE_REGISTERS_CX);
    if (result != 0)
#ifdef PS5_RESOURCE_FOUNDATION
        return resource_compose_fail(state, resource_slot, -13);
#else
        return -13;
#endif
    result = ps5_native_set_indirect(
        &cursor, (uint32_t)(end - cursor), pipeline->uc,
        PS5_PIPELINE_UC_REGISTERS, state->resources->shader, SHADER_BYTES,
        PS5_NATIVE_REGISTERS_UC);
    if (result != 0)
#ifdef PS5_RESOURCE_FOUNDATION
        return resource_compose_fail(state, resource_slot, -14);
#else
        return -14;
#endif
    result = ps5_native_set_indirect(
        &cursor, (uint32_t)(end - cursor), pipeline->sh,
        PS5_PIPELINE_SH_REGISTERS, state->resources->shader, SHADER_BYTES,
        PS5_NATIVE_REGISTERS_SH);
    if (result != 0)
#ifdef PS5_RESOURCE_FOUNDATION
        return resource_compose_fail(state, resource_slot, -15);
#else
        return -15;
#endif
#ifdef PS5_BSP_VIEWER
#ifdef PS5_RESOURCE_FOUNDATION
    BspResourceComposeResult resource_composed = {0};
    result = bsp_resource_compose_map(
        &cursor, end, &state->resource_frames[resource_slot],
        &state->bsp_bundle, state->clear_indices,
        state->resources->resource_heap,
        state->resources->resource_heap_bytes, state->draw_modifier,
        ps5_native_set_sh_direct, ps5_native_draw_index,
        &resource_composed);
    struct ps5_pipeline_registers *overlay =
        state->overlay_pipelines[resource_slot];
    if (result == 0)
        result = ps5_native_set_indirect(
            &cursor, (uint32_t)(end - cursor), overlay->cx,
            PS5_PIPELINE_CX_REGISTERS, state->resources->shader,
            SHADER_BYTES, PS5_NATIVE_REGISTERS_CX);
    if (result == 0)
        result = ps5_native_set_indirect(
            &cursor, (uint32_t)(end - cursor), overlay->uc,
            PS5_PIPELINE_UC_REGISTERS, state->resources->shader,
            SHADER_BYTES, PS5_NATIVE_REGISTERS_UC);
    if (result == 0)
        result = ps5_native_set_indirect(
            &cursor, (uint32_t)(end - cursor), overlay->sh,
            PS5_PIPELINE_SH_REGISTERS, state->resources->shader,
            SHADER_BYTES, PS5_NATIVE_REGISTERS_SH);
    if (result == 0)
        result = bsp_resource_compose_overlay(
            &cursor, end, &state->resource_frames[resource_slot],
            state->resources->resource_heap,
            state->resources->resource_heap_bytes,
            state->overlay_draw_modifier, ps5_native_set_sh_direct,
            ps5_native_draw_index, &resource_composed);
    if (result != 0 ||
        resource_composed.map_draws != state->bsp_bundle.draw_count ||
        resource_composed.overlay_draws != 1u)
        return resource_compose_fail(state, resource_slot, -16);
#elif defined(PS5_BSP_TEXTURED)
    BspFlatComposeResult composed = {0, 0};
    result = bsp_textured_compose(
        &cursor, end, state->bsp_draws, state->bsp_texture_bindings,
        state->bsp_draw_count, state->resources->bsp,
        state->resources->bsp_bytes, state->draw_modifier,
        ps5_native_set_sh_direct, ps5_native_draw_index, &composed);
    uint32_t expected_dwords = 0u;
    if (result != 0 ||
        bsp_textured_required_dwords(state->bsp_draw_count,
                                     &expected_dwords) != 0 ||
        composed.draws != state->bsp_draw_count ||
        composed.command_dwords != expected_dwords)
        return -16;
#else
    BspFlatComposeResult composed = {0, 0};
    result = bsp_flat_compose(
        &cursor, end, state->bsp_draws, state->bsp_draw_count,
        state->resources->bsp, state->resources->bsp_bytes,
        state->draw_modifier, ps5_native_set_sh_direct,
        ps5_native_draw_index, &composed);
    uint32_t expected_dwords = 0u;
    if (result != 0 ||
        bsp_flat_required_dwords(state->bsp_draw_count,
                                 &expected_dwords) != 0 ||
        composed.draws != state->bsp_draw_count ||
        composed.command_dwords != expected_dwords)
        return -16;
#endif
#else
    GearsRendererComposeResult composed = {0, 0};
    result = gears_renderer_compose_frame(
        &cursor, end, &state->clear_draw, frame->draws,
        state->draw_modifier, ps5_native_set_sh_direct,
        ps5_native_draw_auto, &composed);
    if (result != 0 || composed.draws != 4u ||
        composed.command_dwords != 120u)
        return -16;
#endif
    state->cursor[frame->buffer] = cursor;
    return 0;
}

static int frame_submit(const GearsAnimationFrame *frame, void *opaque)
{
    struct native_renderer *state = opaque;
    if (!frame || !state || frame->buffer >= 2u ||
        !state->cursor[frame->buffer])
        return -1;
    const unsigned slot = frame->buffer;
#ifdef PS5_RESOURCE_FOUNDATION
    if (ps5_transient_ring_seal(&state->transient_ring, slot,
                                frame->token) != PS5_TRANSIENT_OK)
        return -2;
    if (frame->frame_index == 0u ||
        frame->frame_index + 1u == BSP_GATE_FRAME_COUNT)
        (void)ps5log_printf(PS5LOG_MARK,
            "RESOURCE_FRAME_SEALED frame=%llu slot=%u token=%llu "
            "retirement=fence+videoout",
            (unsigned long long)frame->frame_index, slot,
            (unsigned long long)frame->token);
#endif
    struct ps5_present_stream stream = {
        state->commands[slot],
        state->commands[slot] +
            state->command_slot_bytes / sizeof(uint32_t),
        state->cursor[slot], 0
    };
    struct ps5_submission_input input = {
        &stream, ps5_native_set_flip, ps5_agc_submit_checked, &state->submit,
        state->fences[slot], state->resources->video.handle, (int32_t)slot,
        state->resources->surface.flip_mode, frame->token, 1, 1, 1
    };
    struct ps5_submission_state submitted;
    state->transaction_started = 1;
    const int result = ps5_submission_build_and_submit(&input, &submitted);
    if (result != PS5_SUBMISSION_OK || !submitted.submit_called ||
        !submitted.retain_all_resources)
        return result != 0 ? result : -2;
    state->cursor[slot] = stream.cursor;
    return 0;
}

static int frame_wait_gpu(const GearsAnimationFrame *frame, void *opaque)
{
    struct native_renderer *state = opaque;
    if (!frame || !state || frame->buffer >= 2u)
        return -1;
    const uint64_t deadline = now_ns(0) + UINT64_C(2000000000);
    while (__atomic_load_n(state->fences[frame->buffer], __ATOMIC_ACQUIRE) !=
               0u && now_ns(0) < deadline) {
        const struct timespec delay = {0, 1000000};
        (void)nanosleep(&delay, 0);
    }
    return __atomic_load_n(state->fences[frame->buffer], __ATOMIC_ACQUIRE) ==
                   0u ? 0 : -1;
}

static int frame_wait_video(const GearsAnimationFrame *frame,
                            uint64_t *observed_token, void *opaque)
{
    struct native_renderer *state = opaque;
    if (!frame || !state || !observed_token || frame->buffer >= 2u)
        return -1;
    struct ps5_frame_completion completion;
    if (ps5_frame_completion_begin(&completion, frame->token, 1) !=
        PS5_FRAME_WAITING)
        return -2;
    uint64_t event_storage[4] = {0, 0, 0, 0};
    struct ps5_event_diagnostics diagnostics = {0};
    const uint64_t deadline = now_ns(0) + UINT64_C(2000000000);
    for (;;) {
        const int expired = now_ns(0) >= deadline;
        const struct ps5_event_poll poll = {
            state->resources->video.equeue, event_storage,
            state->fences[frame->buffer], 10000u, 0, expired,
            sceKernelWaitEqueue, sceVideoOutGetEventData, &diagnostics
        };
        const int result = ps5_event_poll_completion(&completion, &poll);
        if (result == PS5_FRAME_DONE) {
            *observed_token = frame->token;
#ifdef PS5_RESOURCE_FOUNDATION
            if (!ps5_cache_gpu_to_cpu_complete(
                    __atomic_load_n(state->fences[frame->buffer],
                                    __ATOMIC_ACQUIRE),
                    frame->token, *observed_token))
                return -3;
            state->completed_tokens[frame->buffer] = frame->token;
            state->last_completed_token = frame->token;
            if (frame->frame_index == 0u ||
                frame->frame_index + 1u == BSP_GATE_FRAME_COUNT)
                (void)ps5log_printf(PS5LOG_MARK,
                    "RESOURCE_FRAME_RETIRED frame=%llu slot=%u token=%llu "
                    "fence=zero videoout_token=exact",
                    (unsigned long long)frame->frame_index, frame->buffer,
                    (unsigned long long)frame->token);
#endif
#ifdef PS5_BSP_VIEWER
            if (frame->frame_index == 0u ||
                frame->frame_index + 1u == BSP_GATE_FRAME_COUNT)
                (void)ps5log_printf(PS5LOG_MARK,
                    "BSP_VIDEOOUT_TOKEN frame=%llu buffer=%u expected=%llu "
                    "observed=%llu exact=true",
                    (unsigned long long)frame->frame_index, frame->buffer,
                    (unsigned long long)frame->token,
                    (unsigned long long)*observed_token);
#endif
            return 0;
        }
        if (result < 0)
            return result;
    }
}

static void frame_telemetry(uint64_t frame,
                            const GearsTelemetrySnapshot *snapshot,
                            int terminal_error, void *opaque)
{
    (void)opaque;
    if (!snapshot)
        return;
#ifdef PS5_BSP_VIEWER
    const char *const event = "BSP_FRAME";
#else
    const char *const event = "GEARS_FRAME";
#endif
    (void)ps5log_printf(terminal_error ? PS5LOG_ERR : PS5LOG_INFO,
        "%s frame=%llu completed=%llu compose_avg_ns=%llu "
        "gpu_wait_avg_ns=%llu video_wait_avg_ns=%llu "
        "present_interval_avg_ns=%llu present_interval_max_ns=%llu "
        "present_interval_over_budget=%llu "
        "errors=%llu terminal=%d",
        event, (unsigned long long)frame,
        (unsigned long long)snapshot->frames_completed,
        (unsigned long long)snapshot->compose_ns_average,
        (unsigned long long)snapshot->gpu_wait_ns_average,
        (unsigned long long)snapshot->video_wait_ns_average,
        (unsigned long long)snapshot->present_interval_ns_average,
        (unsigned long long)snapshot->present_interval_ns_max,
        (unsigned long long)snapshot->present_interval_over_budget,
        (unsigned long long)snapshot->errors, terminal_error);
}

static int guards_intact(void)
{
    const uint64_t footprint = resources.surface.tiled_footprint;
    const uint64_t offsets[3] = {
        footprint,
        PS5_SURFACE_BUFFER_STRIDE - 64u,
        PS5_SURFACE_BUFFER_STRIDE + footprint,
    };
    for (unsigned zone = 0; zone < 3u; ++zone) {
        const volatile uint32_t *guard = (const volatile uint32_t *)(
            (const uint8_t *)resources.framebuffer + offsets[zone]);
        for (unsigned word = 0; word < 16u; ++word)
            if (guard[word] != GUARD_WORD)
                return 0;
    }
    const volatile uint32_t *depth_guard = (const volatile uint32_t *)(
        (const uint8_t *)resources.depth + DEPTH_BYTES);
    for (unsigned word = 0; word < 16u; ++word)
        if (depth_guard[word] != GUARD_WORD)
            return 0;
#ifdef PS5_BSP_VIEWER
    const volatile uint32_t *bsp_guard = (const volatile uint32_t *)(
        (const uint8_t *)resources.bsp + renderer.bsp_plan.guard_offset);
    for (unsigned word = 0; word < 16u; ++word)
        if (bsp_guard[word] != GUARD_WORD)
            return 0;
#endif
    return 1;
}

static int cleanup(void)
{
    int result;
#ifdef PS5_BSP_NOCLIP
    if (resources.pad_opened &&
        (result = scePadClose(resources.pad_handle)) != 0)
        return result;
    resources.pad_opened = 0;
    resources.pad_handle = -1;
#endif
    if (resources.video.handle >= 0) {
        result = ps5_videoout_close(&resources.video, &video_ops);
        log_result("videoout_close_chain", result);
        if (result != 0)
            return result;
    }
    if (resources.framebuffer_mapped &&
        (result = sceKernelMunmap(resources.framebuffer,
                                  PS5_SURFACE_ALLOCATION_BYTES)) != 0)
        return result;
    resources.framebuffer_mapped = 0;
    if (resources.framebuffer_allocated &&
        (result = sceKernelReleaseDirectMemory(resources.framebuffer_offset,
                         PS5_SURFACE_ALLOCATION_BYTES)) != 0)
        return result;
    resources.framebuffer_allocated = 0;
#ifdef PS5_RESOURCE_FOUNDATION
    if (resources.resource_heap_mapped) {
        (void)ps5_resource_pool_release_unsubmitted(
            &resources.resource_pool, &resources.transient_allocation);
        (void)ps5_resource_pool_release_unsubmitted(
            &resources.resource_pool, &resources.depth_allocation);
        (void)ps5_resource_pool_release_unsubmitted(
            &resources.resource_pool, &resources.shader_allocation);
        (void)ps5_resource_pool_release_unsubmitted(
            &resources.resource_pool, &resources.bsp_allocation);
        if ((result = sceKernelMunmap(resources.resource_heap,
                                      resources.resource_heap_bytes)) != 0)
            return result;
        resources.resource_heap_mapped = 0;
    }
    if (resources.resource_heap_allocated &&
        (result = sceKernelReleaseDirectMemory(
             resources.resource_heap_offset,
             resources.resource_heap_bytes)) != 0)
        return result;
    resources.resource_heap_allocated = 0;
#else
    if (resources.depth_mapped &&
        (result = sceKernelMunmap(resources.depth,
                                  DEPTH_ALLOCATION_BYTES)) != 0)
        return result;
    resources.depth_mapped = 0;
    if (resources.depth_allocated &&
        (result = sceKernelReleaseDirectMemory(resources.depth_offset,
                                                DEPTH_ALLOCATION_BYTES)) != 0)
        return result;
    resources.depth_allocated = 0;
#ifdef PS5_BSP_VIEWER
    if (resources.bsp_mapped &&
        (result = sceKernelMunmap(resources.bsp, resources.bsp_bytes)) != 0)
        return result;
    resources.bsp_mapped = 0;
    if (resources.bsp_allocated &&
        (result = sceKernelReleaseDirectMemory(resources.bsp_offset,
                                                resources.bsp_bytes)) != 0)
        return result;
    resources.bsp_allocated = 0;
#endif
    if (resources.shader_mapped &&
        (result = sceKernelMunmap(resources.shader, SHADER_BYTES)) != 0)
        return result;
    resources.shader_mapped = 0;
    if (resources.shader_allocated &&
        (result = sceKernelReleaseDirectMemory(resources.shader_offset,
                                                SHADER_BYTES)) != 0)
        return result;
    resources.shader_allocated = 0;
#endif
    if (resources.command_mapped) {
        struct ps5_batch_map_entry entry = {
            resources.command, 0, resources.command_bytes, 0xf2, 0x0c, 0, 1
        };
        int processed = -1;
        result = sceKernelBatchMap(&entry, 1, &processed);
        if (result != 0 || processed != 1)
            return result != 0 ? result : -1;
        resources.command_mapped = 0;
    }
    if (resources.command_allocated &&
        (result = sceKernelReleaseDirectMemory(resources.command_offset,
                                                resources.command_bytes)) != 0)
        return result;
    resources.command_allocated = 0;
    if (resources.command_reserved &&
        (result = sceKernelMunmap(resources.command,
                                  resources.command_bytes)) != 0)
        return result;
    resources.command_reserved = 0;
    if (resources.agc_loaded &&
        (result = sceSysmoduleUnloadModuleInternal(AGC_MODULE)) != 0)
        return result;
    resources.agc_loaded = 0;
    return 0;
}

static void park(const char *reason)
{
    (void)ps5log_printf(PS5LOG_ERR, "PARKED retain_all_resources=true reason=%s",
                        reason ? reason : "unknown");
    ps5log_close("parked-retain");
    for (;;)
        (void)pause();
}

#ifdef PS5_BSP_VIEWER
static uint64_t readback_hash(const void *data, size_t bytes)
{
    const uint8_t *cursor = data;
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t index = 0; index < bytes; ++index) {
        hash ^= cursor[index];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t bright_pixel_count(const void *data, size_t bytes)
{
    const uint8_t *pixels = data;
    uint64_t bright = 0u;
    for (size_t offset = 0; offset + 3u < bytes; offset += 4u)
        if (pixels[offset] > 32u || pixels[offset + 1u] > 32u ||
            pixels[offset + 2u] > 32u)
            ++bright;
    return bright;
}

static void park_complete(void)
{
#ifdef PS5_RESOURCE_FOUNDATION
    ps5log_close("bsp-resource-soak-complete");
#elif defined(PS5_BSP_TEXTURED)
    ps5log_close("bsp-textured-soak-complete");
#elif defined(PS5_BSP_NOCLIP)
    ps5log_close("bsp-noclip-soak-complete");
#else
    ps5log_close("bsp-gate1-complete");
#endif
    for (;;)
        (void)pause();
}
#endif

static int fail_pre_submit(const char *step, int result)
{
    log_result(step, result);
    const int cleanup_result = cleanup();
    log_result("pre_submit_cleanup", cleanup_result);
    ps5log_close("pre-submit-failure");
    return result != 0 ? result : -1;
}

int main(void)
{
    memset(&resources, 0, sizeof(resources));
    resources.command_offset = resources.framebuffer_offset =
        resources.shader_offset = resources.depth_offset = -1;
    resources.bsp_offset = -1;
#ifdef PS5_RESOURCE_FOUNDATION
    resources.resource_heap_offset = -1;
#endif
    resources.pad_handle = -1;
    resources.video.handle = -1;
    ps5log_config log_config;
    const char *log_path = 0;
    const uint64_t boot_token = now_ns(0);
    ps5log_config_defaults(&log_config);
    const int config_result = ps5log_load_config(
        ps5log_default_conf_paths, ps5log_default_conf_path_count,
        &log_config, &log_path);
    const int log_result = config_result == 0
        ? ps5log_init(&log_config, "PPSA99997", "ps5-agc-gears", boot_token)
        : 1;
    (void)ps5log_line(PS5LOG_INFO, "LOG_SCHEMA=3");
    (void)ps5log_line(PS5LOG_INFO,
                      "LOG_TRANSPORT=ps5log/1 tcp structured");
    (void)ps5log_line(PS5LOG_INFO, "LOG_FS_SINKS=disabled");
    (void)ps5log_hex64(PS5LOG_INFO, "LOG_BOOT_MONOTONIC_NS", boot_token);
    (void)ps5log_printf(PS5LOG_INFO,
                        "LOG_CONFIG_RESULT=%d LOG_INIT_RESULT=%d path=%s",
                        config_result, log_result,
                        log_path ? log_path : "unavailable");
#ifdef PS5_BSP_VIEWER
#ifdef PS5_RESOURCE_FOUNDATION
    (void)ps5log_printf(PS5LOG_MARK,
        "BSP_RESOURCE_BOOT schema=1 target=gfx1013 fw=12.02 "
        "constants=vsharp transient_slots=2 pipelines=2 overlay=quad "
        "bundle_sha256=%s bundle_bytes=%u soak_frames=%u",
        PS5_BSP_BUNDLE_SHA256, PS5_BSP_BUNDLE_BYTES,
        BSP_GATE_FRAME_COUNT);
#elif defined(PS5_BSP_TEXTURED)
    (void)ps5log_printf(PS5LOG_MARK,
        "BSP_TEXTURED_BOOT schema=1 target=gfx1013 fw=12.02 "
        "composition=base_x_lightmap bundle_sha256=%s bundle_bytes=%u "
        "soak_frames=%u",
        PS5_BSP_BUNDLE_SHA256, PS5_BSP_BUNDLE_BYTES,
        BSP_GATE_FRAME_COUNT);
#elif defined(PS5_BSP_NOCLIP)
    (void)ps5log_printf(PS5LOG_MARK,
        "BSP_NOCLIP_BOOT schema=1 target=gfx1013 fw=12.02 "
        "bundle_sha256=%s bundle_bytes=%u soak_frames=%u",
        PS5_BSP_BUNDLE_SHA256, PS5_BSP_BUNDLE_BYTES,
        BSP_GATE_FRAME_COUNT);
#else
    (void)ps5log_printf(PS5LOG_MARK,
        "BSP_BOOT schema=1 target=gfx1013 fw=12.02 color_clear=rt_draw "
        "bundle_sha256=%s bundle_bytes=%u",
        PS5_BSP_BUNDLE_SHA256, PS5_BSP_BUNDLE_BYTES);
#endif
#else
    (void)ps5log_line(PS5LOG_MARK,
        "GEARS_BOOT schema=1 target=gfx1013 fw=12.02 color_clear=rt_draw");
#endif

    int result = sceSysmoduleLoadModuleInternal(AGC_MODULE);
    if (result != 0)
        return fail_pre_submit("agc_load", result);
    resources.agc_loaded = 1;
    result = sceAgcInit(&resources.agc_state, sizeof(resources.agc_state));
    if (result != 0)
        return fail_pre_submit("agc_init", result);
#ifdef PS5_BSP_VIEWER
    result = load_bsp_bundle();
    if (result != 0)
        return fail_pre_submit("bsp_bundle_load", result);
#ifdef PS5_BSP_NOCLIP
    result = open_noclip_pad();
    if (result != 0)
        return fail_pre_submit("noclip_pad_open", result);
    (void)ps5log_line(PS5LOG_MARK,
        "BSP_NOCLIP_PAD_READY sticks=dual triggers=vertical "
        "connected_required=true");
#endif
    BspCommandPlan command_plan;
#ifdef PS5_BSP_TEXTURED
    const int command_plan_result = bsp_command_plan_with_stride(
        renderer.bsp_plan.scene_draw_count,
        BSP_TEXTURED_DWORDS_PER_DRAW, BSP_FIXED_COMMAND_DWORDS,
        &command_plan);
#else
    const int command_plan_result = bsp_command_plan(
        renderer.bsp_plan.scene_draw_count, BSP_FIXED_COMMAND_DWORDS,
        &command_plan);
#endif
    if (command_plan_result != 0)
        return fail_pre_submit("bsp_command_plan", -1);
    resources.command_bytes = command_plan.allocation_bytes;
    renderer.command_slot_bytes = command_plan.slot_bytes;
#else
    resources.command_bytes = COMMAND_BYTES;
    renderer.command_slot_bytes = COMMAND_SLOT_BYTES;
#endif
    result = map_command();
    if (result != 0)
        return fail_pre_submit("command_map", result);
    result = allocate_direct(&resources.framebuffer,
                             &resources.framebuffer_offset,
                             PS5_SURFACE_ALLOCATION_BYTES,
                             PS5_SURFACE_ALIGNMENT);
    if (result != 0)
        return fail_pre_submit("framebuffer_map", result);
    resources.framebuffer_allocated = resources.framebuffer_mapped = 1;
    result = ps5_surface_make_plan(0u, &resources.surface);
    if (result != 0)
        return fail_pre_submit("surface_plan", result);

    memset(resources.framebuffer, 0, PS5_SURFACE_ALLOCATION_BYTES);
    const uint64_t guard_offsets[3] = {
        resources.surface.tiled_footprint,
        PS5_SURFACE_BUFFER_STRIDE - 64u,
        PS5_SURFACE_BUFFER_STRIDE + resources.surface.tiled_footprint,
    };
    for (unsigned zone = 0; zone < 3u; ++zone) {
        uint32_t *guard = (uint32_t *)((uint8_t *)resources.framebuffer +
                                       guard_offsets[zone]);
        for (unsigned word = 0; word < 16u; ++word)
            guard[word] = GUARD_WORD;
        ps5_native_cache_flush(guard, 64u);
    }
    result = ps5_videoout_open(&resources.video, &video_ops,
                               &resources.surface, resources.framebuffer);
    if (result != 0)
        return fail_pre_submit("videoout_open_chain", result);

#ifndef PS5_RESOURCE_FOUNDATION
    result = allocate_direct(&resources.shader, &resources.shader_offset,
                             SHADER_BYTES, SHADER_ALIGNMENT);
    if (result != 0)
        return fail_pre_submit("shader_map", result);
    resources.shader_allocated = resources.shader_mapped = 1;
    result = allocate_direct(&resources.depth, &resources.depth_offset,
                             DEPTH_ALLOCATION_BYTES, DEPTH_ALIGNMENT);
    if (result != 0)
        return fail_pre_submit("depth_map", result);
    resources.depth_allocated = resources.depth_mapped = 1;
#endif
    memset(resources.shader, 0, SHADER_BYTES);

#ifdef PS5_BSP_VIEWER
#ifdef PS5_BSP_TEXTURED
#ifdef PS5_RESOURCE_FOUNDATION
    const struct ps5_shader_metadata metadata = {
        PS5_BSP_RESOURCE_GS_RSRC1, PS5_BSP_RESOURCE_GS_RSRC2,
        PS5_BSP_RESOURCE_PS_RSRC1, PS5_BSP_RESOURCE_PS_RSRC2,
        PS5_BSP_RESOURCE_GE_CNTL, PS5_BSP_RESOURCE_SHADER_STAGES_EN,
        PS5_BSP_RESOURCE_GS_OUT_PRIM_TYPE, PS5_BSP_RESOURCE_DRAW_MODIFIER,
        ps5_bsp_resource_pre_raster_cx,
        sizeof(ps5_bsp_resource_pre_raster_cx) /
            sizeof(ps5_bsp_resource_pre_raster_cx[0]),
        ps5_bsp_resource_pixel_cx,
        sizeof(ps5_bsp_resource_pixel_cx) /
            sizeof(ps5_bsp_resource_pixel_cx[0])
    };
    const uint8_t *const embedded_gs_start = ps5_bsp_resource_gs_start;
    const uint8_t *const embedded_gs_end = ps5_bsp_resource_gs_end;
    const uint8_t *const embedded_ps_start = ps5_bsp_resource_ps_start;
    const uint8_t *const embedded_ps_end = ps5_bsp_resource_ps_end;
    const size_t expected_gs_isa = PS5_BSP_RESOURCE_GS_ISA_BYTES;
    const size_t expected_ps_isa = PS5_BSP_RESOURCE_PS_ISA_BYTES;
#else
    const struct ps5_shader_metadata metadata = {
        PS5_BSP_TEXTURED_GS_RSRC1, PS5_BSP_TEXTURED_GS_RSRC2,
        PS5_BSP_TEXTURED_PS_RSRC1, PS5_BSP_TEXTURED_PS_RSRC2,
        PS5_BSP_TEXTURED_GE_CNTL, PS5_BSP_TEXTURED_SHADER_STAGES_EN,
        PS5_BSP_TEXTURED_GS_OUT_PRIM_TYPE, PS5_BSP_TEXTURED_DRAW_MODIFIER,
        ps5_bsp_textured_pre_raster_cx,
        sizeof(ps5_bsp_textured_pre_raster_cx) /
            sizeof(ps5_bsp_textured_pre_raster_cx[0]),
        ps5_bsp_textured_pixel_cx,
        sizeof(ps5_bsp_textured_pixel_cx) /
            sizeof(ps5_bsp_textured_pixel_cx[0])
    };
    const uint8_t *const embedded_gs_start = ps5_bsp_textured_gs_start;
    const uint8_t *const embedded_gs_end = ps5_bsp_textured_gs_end;
    const uint8_t *const embedded_ps_start = ps5_bsp_textured_ps_start;
    const uint8_t *const embedded_ps_end = ps5_bsp_textured_ps_end;
    const size_t expected_gs_isa = PS5_BSP_TEXTURED_GS_ISA_BYTES;
    const size_t expected_ps_isa = PS5_BSP_TEXTURED_PS_ISA_BYTES;
#endif
#else
    const struct ps5_shader_metadata metadata = {
        PS5_BSP_FLAT_GS_RSRC1, PS5_BSP_FLAT_GS_RSRC2,
        PS5_BSP_FLAT_PS_RSRC1, PS5_BSP_FLAT_PS_RSRC2,
        PS5_BSP_FLAT_GE_CNTL, PS5_BSP_FLAT_SHADER_STAGES_EN,
        PS5_BSP_FLAT_GS_OUT_PRIM_TYPE, PS5_BSP_FLAT_DRAW_MODIFIER,
        ps5_bsp_flat_pre_raster_cx,
        sizeof(ps5_bsp_flat_pre_raster_cx) /
            sizeof(ps5_bsp_flat_pre_raster_cx[0]),
        ps5_bsp_flat_pixel_cx,
        sizeof(ps5_bsp_flat_pixel_cx) / sizeof(ps5_bsp_flat_pixel_cx[0])
    };
    const uint8_t *const embedded_gs_start = ps5_bsp_flat_gs_start;
    const uint8_t *const embedded_gs_end = ps5_bsp_flat_gs_end;
    const uint8_t *const embedded_ps_start = ps5_bsp_flat_ps_start;
    const uint8_t *const embedded_ps_end = ps5_bsp_flat_ps_end;
    const size_t expected_gs_isa = PS5_BSP_FLAT_GS_ISA_BYTES;
    const size_t expected_ps_isa = PS5_BSP_FLAT_PS_ISA_BYTES;
#endif
#else
    const struct ps5_shader_metadata metadata = {
        PS5_GEARS_GS_RSRC1, PS5_GEARS_GS_RSRC2,
        PS5_GEARS_PS_RSRC1, PS5_GEARS_PS_RSRC2,
        PS5_GEARS_GE_CNTL, PS5_GEARS_SHADER_STAGES_EN,
        PS5_GEARS_GS_OUT_PRIM_TYPE, PS5_GEARS_DRAW_MODIFIER,
        ps5_gears_pre_raster_cx, 10u, ps5_gears_pixel_cx, 9u
    };
    const uint8_t *const embedded_gs_start = ps5_gears_gs_start;
    const uint8_t *const embedded_gs_end = ps5_gears_gs_end;
    const uint8_t *const embedded_ps_start = ps5_gears_ps_start;
    const uint8_t *const embedded_ps_end = ps5_gears_ps_end;
    const size_t expected_gs_isa = PS5_GEARS_GS_ISA_BYTES;
    const size_t expected_ps_isa = PS5_GEARS_PS_ISA_BYTES;
#endif
    uint8_t *base = resources.shader;
    struct ps5_shader_arena *gs_arena =
        (struct ps5_shader_arena *)(base + GS_HEADER_OFFSET);
    struct ps5_shader_arena *ps_arena =
        (struct ps5_shader_arena *)(base + PS_HEADER_OFFSET);
    uint8_t *gs_code = base + GS_CODE_OFFSET;
    uint8_t *ps_code = base + PS_CODE_OFFSET;
    const size_t gs_isa = (size_t)(embedded_gs_end - embedded_gs_start);
    const size_t ps_isa = (size_t)(embedded_ps_end - embedded_ps_start);
    const uint32_t gs_size = (uint32_t)gs_isa + SHADER_FOOTER_BYTES;
    const uint32_t ps_size = (uint32_t)ps_isa + SHADER_FOOTER_BYTES;
    if (gs_isa != expected_gs_isa || ps_isa != expected_ps_isa ||
        ps5_shader_header_build(gs_arena, PS5_SHADER_PRE_RASTER, gs_size,
                                &metadata) != 0 ||
        ps5_shader_header_build(ps_arena, PS5_SHADER_PIXEL, ps_size,
                                &metadata) != 0)
        return fail_pre_submit("shader_header", -1);
    memcpy(gs_code, embedded_gs_start, gs_isa);
    memcpy(ps_code, embedded_ps_start, ps_isa);
    memcpy(gs_code + gs_size - SHADER_FOOTER_BYTES, "barefoot", 8u);
    memcpy(ps_code + ps_size - SHADER_FOOTER_BYTES, "barefoot", 8u);
    void *gs_object = 0;
    void *ps_object = 0;
    result = sceAgcCreateShader(&gs_object, gs_arena, gs_code);
    if (result != 0 || gs_object != gs_arena)
        return fail_pre_submit("create_gs", result != 0 ? result : -1);
    result = sceAgcCreateShader(&ps_object, ps_arena, ps_code);
    if (result != 0 || ps_object != ps_arena)
        return fail_pre_submit("create_ps", result != 0 ? result : -1);
    struct ps5_agc_linked_cx *linked_cx =
        (struct ps5_agc_linked_cx *)(base + LINKED_CX_OFFSET);
    struct ps5_agc_linked_uc *linked_uc =
        (struct ps5_agc_linked_uc *)(base + LINKED_UC_OFFSET);
    result = sceAgcLinkShaders(linked_cx, linked_uc, 0, gs_object, ps_object,
                               4u);
    if (result != 0)
        return fail_pre_submit("link_shaders", result);

    ps5_agc_register defaults[PS5_COLOR_REGISTER_COUNT];
    result = ps5_color_select_runtime_defaults(
        defaults, (const struct ps5_agc_register_defaults *)
                      sceAgcGetRegisterDefaults());
    if (result != 0)
        return fail_pre_submit("color_defaults", result);
    struct ps5_pipeline_registers *pipelines =
        (struct ps5_pipeline_registers *)(base + PIPELINE_OFFSET);
    for (unsigned slot = 0; slot < 2u; ++slot) {
        ps5_agc_register color[PS5_COLOR_REGISTER_COUNT];
        const uintptr_t address = (uintptr_t)resources.framebuffer +
            resources.surface.buffer_offsets[slot];
        if (ps5_color_build_target(color, defaults, address,
                                   resources.surface.width,
                                   resources.surface.height) != 0 ||
            ps5_pipeline_build(
                &pipelines[slot], color, linked_cx, linked_uc,
                gs_arena->cx, ps_arena->cx, gs_arena->sh, ps_arena->sh,
                resources.surface.width, resources.surface.height) != 0)
            return fail_pre_submit("pipeline_build", -1);
    }
#ifdef PS5_RESOURCE_FOUNDATION
    const struct ps5_shader_metadata overlay_metadata = {
        PS5_BSP_OVERLAY_GS_RSRC1, PS5_BSP_OVERLAY_GS_RSRC2,
        PS5_BSP_OVERLAY_PS_RSRC1, PS5_BSP_OVERLAY_PS_RSRC2,
        PS5_BSP_OVERLAY_GE_CNTL, PS5_BSP_OVERLAY_SHADER_STAGES_EN,
        PS5_BSP_OVERLAY_GS_OUT_PRIM_TYPE, PS5_BSP_OVERLAY_DRAW_MODIFIER,
        ps5_bsp_overlay_pre_raster_cx,
        sizeof(ps5_bsp_overlay_pre_raster_cx) /
            sizeof(ps5_bsp_overlay_pre_raster_cx[0]),
        ps5_bsp_overlay_pixel_cx,
        sizeof(ps5_bsp_overlay_pixel_cx) /
            sizeof(ps5_bsp_overlay_pixel_cx[0])
    };
    struct ps5_shader_arena *overlay_gs_arena =
        (struct ps5_shader_arena *)(base + OVERLAY_GS_HEADER_OFFSET);
    struct ps5_shader_arena *overlay_ps_arena =
        (struct ps5_shader_arena *)(base + OVERLAY_PS_HEADER_OFFSET);
    uint8_t *overlay_gs_code = base + OVERLAY_GS_CODE_OFFSET;
    uint8_t *overlay_ps_code = base + OVERLAY_PS_CODE_OFFSET;
    const size_t overlay_gs_isa =
        (size_t)(ps5_bsp_overlay_gs_end - ps5_bsp_overlay_gs_start);
    const size_t overlay_ps_isa =
        (size_t)(ps5_bsp_overlay_ps_end - ps5_bsp_overlay_ps_start);
    const uint32_t overlay_gs_size =
        (uint32_t)overlay_gs_isa + SHADER_FOOTER_BYTES;
    const uint32_t overlay_ps_size =
        (uint32_t)overlay_ps_isa + SHADER_FOOTER_BYTES;
    if (overlay_gs_isa != PS5_BSP_OVERLAY_GS_ISA_BYTES ||
        overlay_ps_isa != PS5_BSP_OVERLAY_PS_ISA_BYTES ||
        ps5_shader_header_build(overlay_gs_arena, PS5_SHADER_PRE_RASTER,
                                overlay_gs_size, &overlay_metadata) != 0 ||
        ps5_shader_header_build(overlay_ps_arena, PS5_SHADER_PIXEL,
                                overlay_ps_size, &overlay_metadata) != 0)
        return fail_pre_submit("overlay_shader_header", -1);
    memcpy(overlay_gs_code, ps5_bsp_overlay_gs_start, overlay_gs_isa);
    memcpy(overlay_ps_code, ps5_bsp_overlay_ps_start, overlay_ps_isa);
    memcpy(overlay_gs_code + overlay_gs_size - SHADER_FOOTER_BYTES,
           "barefoot", 8u);
    memcpy(overlay_ps_code + overlay_ps_size - SHADER_FOOTER_BYTES,
           "barefoot", 8u);
    void *overlay_gs_object = 0;
    void *overlay_ps_object = 0;
    result = sceAgcCreateShader(&overlay_gs_object, overlay_gs_arena,
                                overlay_gs_code);
    if (result != 0 || overlay_gs_object != overlay_gs_arena)
        return fail_pre_submit("create_overlay_gs",
                               result != 0 ? result : -1);
    result = sceAgcCreateShader(&overlay_ps_object, overlay_ps_arena,
                                overlay_ps_code);
    if (result != 0 || overlay_ps_object != overlay_ps_arena)
        return fail_pre_submit("create_overlay_ps",
                               result != 0 ? result : -1);
    struct ps5_agc_linked_cx *overlay_linked_cx =
        (struct ps5_agc_linked_cx *)(base + OVERLAY_LINKED_CX_OFFSET);
    struct ps5_agc_linked_uc *overlay_linked_uc =
        (struct ps5_agc_linked_uc *)(base + OVERLAY_LINKED_UC_OFFSET);
    result = sceAgcLinkShaders(overlay_linked_cx, overlay_linked_uc, 0,
                               overlay_gs_object, overlay_ps_object, 4u);
    if (result != 0)
        return fail_pre_submit("link_overlay_shaders", result);
    struct ps5_pipeline_registers *overlay_pipelines =
        (struct ps5_pipeline_registers *)(base + OVERLAY_PIPELINE_OFFSET);
    for (unsigned slot = 0; slot < 2u; ++slot) {
        ps5_agc_register color[PS5_COLOR_REGISTER_COUNT];
        const uintptr_t address = (uintptr_t)resources.framebuffer +
            resources.surface.buffer_offsets[slot];
        if (ps5_color_build_target(color, defaults, address,
                                   resources.surface.width,
                                   resources.surface.height) != 0 ||
            ps5_pipeline_build(
                &overlay_pipelines[slot], color, overlay_linked_cx,
                overlay_linked_uc, overlay_gs_arena->cx,
                overlay_ps_arena->cx, overlay_gs_arena->sh,
                overlay_ps_arena->sh, resources.surface.width,
                resources.surface.height) != 0)
            return fail_pre_submit("overlay_pipeline_build", -1);
    }
#endif
    ps5_agc_register *depth_registers =
        (ps5_agc_register *)(base + DEPTH_REGISTERS_OFFSET);
    if (ps5_depth_build_d32_no_htile(depth_registers,
                                     (uintptr_t)resources.depth,
                                     resources.surface.width,
                                     resources.surface.height) != 0)
        return fail_pre_submit("depth_state", -1);

#ifdef PS5_BSP_VIEWER
    if (prepare_bsp_scene() != 0)
        return fail_pre_submit("bsp_scene", -1);
#ifdef PS5_BSP_TEXTURED
    const uint32_t bsp_draw_dwords =
        renderer.bsp_draw_count * BSP_TEXTURED_DWORDS_PER_DRAW;
#else
    const uint32_t bsp_draw_dwords =
        renderer.bsp_draw_count * BSP_FLAT_DWORDS_PER_DRAW;
#endif
    (void)ps5log_printf(PS5LOG_MARK,
        "BSP_BUNDLE_READY vertices=%u indices=%u map_draws=%u "
        "scene_draws=%u draw_dwords=%u command_slot_bytes=%llu "
        "allocation_bytes=%llu",
        renderer.bsp_bundle.vertex_count, renderer.bsp_bundle.index_count,
        renderer.bsp_bundle.draw_count, renderer.bsp_draw_count,
        bsp_draw_dwords,
        (unsigned long long)renderer.command_slot_bytes,
        (unsigned long long)resources.bsp_bytes);
#ifdef PS5_RESOURCE_FOUNDATION
    (void)ps5log_printf(PS5LOG_MARK,
        "RESOURCE_HEAP_READY bytes=%llu allocations=4 "
        "pool=fence-retired transient_slots=2 slot_bytes=%llu",
        (unsigned long long)resources.resource_heap_bytes,
        (unsigned long long)renderer.transient_ring.slots[0].bytes);
    (void)ps5log_printf(PS5LOG_MARK,
        "BSP_TEXTURE_TABLE_LAYOUT textures=%u descriptor_dwords=%u "
        "storage=per-frame-transient lightmap=%ux%u "
        "base_mode=repeat lightmap_mode=clamp filter=bilinear "
        "composition=base_x_lightmap",
        renderer.bsp_bundle.texture_count,
        renderer.bsp_plan.descriptor_table_dwords,
        renderer.bsp_bundle.lightmap_image->width,
        renderer.bsp_bundle.lightmap_image->height);
#elif defined(PS5_BSP_TEXTURED)
    (void)ps5log_printf(PS5LOG_MARK,
        "BSP_TEXTURE_TABLES_READY textures=%u descriptor_dwords=%u "
        "lightmap=%ux%u base_mode=repeat lightmap_mode=clamp "
        "filter=bilinear composition=base_x_lightmap",
        renderer.bsp_bundle.texture_count,
        renderer.bsp_plan.descriptor_table_dwords,
        renderer.bsp_bundle.lightmap_image->width,
        renderer.bsp_bundle.lightmap_image->height);
#endif
#else
    static const size_t vertex_offsets[3] = {
        GEAR0_OFFSET, GEAR1_OFFSET, GEAR2_OFFSET
    };
    static const GearSpec specs[3] = {
        {0.2500f, 0.9125f, 1.0875f, 0.250f, 20u},
        {0.1250f, 0.4125f, 0.5875f, 0.500f, 10u},
        {0.3250f, 0.4125f, 0.5875f, 0.125f, 10u},
    };
    for (unsigned gear = 0; gear < 3u; ++gear) {
        GearsVertex *vertices = (GearsVertex *)(base + vertex_offsets[gear]);
        const size_t count = gears_mesh_vertex_count(specs[gear].teeth);
        if (gears_build_mesh(vertices, count, &specs[gear]) != count)
            return fail_pre_submit("gear_mesh", -1);
        uint32_t *srd = (uint32_t *)(base + GEAR_SRD_OFFSET + gear * 16u);
        const uintptr_t vertex_address = (uintptr_t)vertices;
        const uintptr_t table_address = (uintptr_t)srd;
        if ((table_address >> 32) != UINT64_C(2))
            return fail_pre_submit("srd_aperture", -1);
        srd[0] = (uint32_t)vertex_address;
        srd[1] = (uint32_t)((vertex_address >> 32) & 0xffffu) | (24u << 16);
        srd[2] = (uint32_t)count;
        srd[3] = UINT32_C(0x11014fac);
        renderer.srd_tables[gear] = (uint32_t)table_address;
        renderer.vertex_counts[gear] = (uint32_t)count;
    }
    GearsRtClearVertex *clear_vertices =
        (GearsRtClearVertex *)(base + CLEAR_VERTEX_OFFSET);
    uint32_t *clear_srd = (uint32_t *)(base + CLEAR_SRD_OFFSET);
    const uintptr_t clear_vertex_address = (uintptr_t)clear_vertices;
    const uintptr_t clear_table_address = (uintptr_t)clear_srd;
    if ((clear_table_address >> 32) != UINT64_C(2))
        return fail_pre_submit("clear_srd_aperture", -1);
    clear_srd[0] = (uint32_t)clear_vertex_address;
    clear_srd[1] = (uint32_t)((clear_vertex_address >> 32) & 0xffffu) |
                   (24u << 16);
    clear_srd[2] = GEARS_RT_CLEAR_VERTEX_COUNT;
    clear_srd[3] = UINT32_C(0x11014fac);
    const float black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    if (gears_rt_clear_build(clear_vertices, &renderer.clear_draw,
                             (uint32_t)clear_table_address, black) != 0)
        return fail_pre_submit("rt_clear_build", -1);
#endif

    volatile uint32_t *depth_guard = (volatile uint32_t *)(
        (uint8_t *)resources.depth + DEPTH_BYTES);
    for (unsigned word = 0; word < 16u; ++word)
        depth_guard[word] = GUARD_WORD;
    ps5_native_cache_flush((const void *)depth_guard, 64u);
    ps5_native_cache_flush(resources.shader, SHADER_BYTES);

    renderer.resources = &resources;
#ifdef PS5_BSP_VIEWER
    renderer.commands[0] = (uint32_t *)((uint8_t *)resources.command +
                                        command_plan.slot_offsets[0]);
    renderer.commands[1] = (uint32_t *)((uint8_t *)resources.command +
                                        command_plan.slot_offsets[1]);
    renderer.fences[0] = (volatile uint64_t *)(
        (uint8_t *)resources.command + command_plan.fence_offsets[0]);
    renderer.fences[1] = (volatile uint64_t *)(
        (uint8_t *)resources.command + command_plan.fence_offsets[1]);
#else
    renderer.commands[0] = resources.command;
    renderer.commands[1] = (uint32_t *)((uint8_t *)resources.command +
                                         0x2000u);
    renderer.fences[0] = (volatile uint64_t *)((uint8_t *)resources.command +
                                               FENCE0_OFFSET);
    renderer.fences[1] = (volatile uint64_t *)((uint8_t *)resources.command +
                                               FENCE1_OFFSET);
#endif
    renderer.pipelines[0] = &pipelines[0];
    renderer.pipelines[1] = &pipelines[1];
#ifdef PS5_RESOURCE_FOUNDATION
    renderer.overlay_pipelines[0] = &overlay_pipelines[0];
    renderer.overlay_pipelines[1] = &overlay_pipelines[1];
    renderer.overlay_draw_modifier = overlay_metadata.draw_modifier;
    if (PS5_PIPELINE_PERMUTATION_COUNT != 2 ||
        ps5_pipeline_permutations[PS5_PIPELINE_BSP_RESOURCE]
                .gs_application_words != 2u ||
        ps5_pipeline_permutations[PS5_PIPELINE_BSP_OVERLAY]
                .gs_application_words != 1u)
        return fail_pre_submit("pipeline_permutation_table", -1);
    (void)ps5log_printf(PS5LOG_MARK,
        "RESOURCE_PIPELINES_READY count=%u map=%s overlay=%s "
        "map_gs_words=%u overlay_gs_words=%u",
        PS5_PIPELINE_PERMUTATION_COUNT,
        ps5_pipeline_permutations[PS5_PIPELINE_BSP_RESOURCE].name,
        ps5_pipeline_permutations[PS5_PIPELINE_BSP_OVERLAY].name,
        ps5_pipeline_permutations[PS5_PIPELINE_BSP_RESOURCE]
            .gs_application_words,
        ps5_pipeline_permutations[PS5_PIPELINE_BSP_OVERLAY]
            .gs_application_words);
#endif
    renderer.depth_registers = depth_registers;
    renderer.draw_modifier = metadata.draw_modifier;
    ps5_native_submit_context_init(&renderer.submit, resources.command,
                                   resources.command_bytes);

    GearsFrameRunnerInput input = {0};
    input.start_ns = now_ns(0);
    input.first_flip_token = UINT64_C(0x0000420000000001);
    input.present_interval_budget_ns = UINT64_C(17000000);
#ifdef PS5_BSP_VIEWER
    for (unsigned index = 0; index < GEARS_SCENE_DRAW_COUNT; ++index) {
        input.srd_tables[index] = 1u;
        input.vertex_counts[index] = 3u;
    }
#else
    memcpy(input.srd_tables, renderer.srd_tables, sizeof(input.srd_tables));
    memcpy(input.vertex_counts, renderer.vertex_counts,
           sizeof(input.vertex_counts));
#endif
    input.now_ns = now_ns;
    input.compose = frame_compose;
    input.submit = frame_submit;
    input.wait_gpu = frame_wait_gpu;
    input.wait_videoout = frame_wait_video;
    input.telemetry = frame_telemetry;
    input.user = &renderer;
#ifdef PS5_BSP_VIEWER
#ifdef PS5_RESOURCE_FOUNDATION
    (void)ps5log_line(PS5LOG_MARK,
        "BSP_LOOP_BEGIN mode=resource-foundation-soak buffers=2 "
        "color_dma=false depth_dma=true indexed=true frames=60000 "
        "constants=per-frame descriptors=per-frame overlay=transient "
        "retirement=fence+videoout");
#elif defined(PS5_BSP_TEXTURED)
    (void)ps5log_line(PS5LOG_MARK,
        "BSP_LOOP_BEGIN mode=textured-noclip-soak buffers=2 "
        "color_dma=false depth_dma=true indexed=true frames=60000 "
        "composition=base_x_lightmap");
#elif defined(PS5_BSP_NOCLIP)
    (void)ps5log_line(PS5LOG_MARK,
        "BSP_LOOP_BEGIN mode=noclip-soak buffers=2 "
        "color_dma=false depth_dma=true indexed=true frames=10000");
#else
    (void)ps5log_line(PS5LOG_MARK,
        "BSP_LOOP_BEGIN mode=fixed-camera buffers=2 "
        "color_dma=false depth_dma=true indexed=true");
#endif
#else
    (void)ps5log_line(PS5LOG_MARK,
        "GEARS_LOOP_BEGIN mode=continuous buffers=2 "
        "color_dma=false depth_dma=true");
#endif
    GearsFrameLoop loop;
    if (gears_frame_loop_init(&loop, &input) != 0)
        return fail_pre_submit("frame_loop_init", -1);
#ifdef PS5_BSP_VIEWER
    for (unsigned frame = 0; frame < BSP_GATE_FRAME_COUNT; ++frame) {
        result = gears_frame_loop_step(&loop);
        if (result != 0)
            park("bsp-submit-or-ownership-failure");
        if ((frame + 1u) % 600u == 0u && !guards_intact())
            park("guard-corruption");
    }
    result = gears_frame_loop_drain(&loop);
    GearsFrameRunnerResult run = {0};
    if (result != 0 || gears_frame_loop_result(&loop, &run) != 0 ||
        run.state != GEARS_RUN_COMPLETE ||
        run.frames_completed != BSP_GATE_FRAME_COUNT)
        park("bsp-drain-or-ownership-failure");
    if (!guards_intact())
        park("guard-corruption");
#ifdef PS5_RESOURCE_FOUNDATION
    int resource_ring_reusable = renderer.last_completed_token != 0u;
    for (uint32_t slot = 0u; slot < 2u; ++slot) {
        if (ps5_transient_ring_begin(
                &renderer.transient_ring, slot,
                renderer.completed_tokens[slot],
                renderer.completed_tokens[slot] != 0u) !=
                PS5_TRANSIENT_OK ||
            ps5_transient_ring_abort_unsubmitted(
                &renderer.transient_ring, slot) != PS5_TRANSIENT_OK)
            resource_ring_reusable = 0;
    }
    (void)ps5log_printf(resource_ring_reusable ? PS5LOG_MARK : PS5LOG_ERR,
        "RESOURCE_RING_RETIRED slots=2 reusable=%s tokens=exact "
        "last_token=%llu",
        resource_ring_reusable ? "true" : "false",
        (unsigned long long)renderer.last_completed_token);
    if (!resource_ring_reusable)
        park("resource-ring-retirement-gate-failure");
#endif
    const size_t readback_bytes = resources.surface.tiled_footprint;
    uint8_t *const first = resources.framebuffer;
    uint8_t *const second = (uint8_t *)resources.framebuffer +
        PS5_SURFACE_BUFFER_STRIDE;
    ps5_native_cache_flush(first, readback_bytes);
    ps5_native_cache_flush(second, readback_bytes);
    const uint64_t first_hash = readback_hash(first, readback_bytes);
    const uint64_t second_hash = readback_hash(second, readback_bytes);
    const uint64_t first_bright = bright_pixel_count(first, readback_bytes);
    const uint64_t second_bright = bright_pixel_count(second, readback_bytes);
#ifdef PS5_BSP_NOCLIP
#ifdef PS5_RESOURCE_FOUNDATION
    (void)ps5log_printf(PS5LOG_MARK,
        "BSP_RESOURCE_READBACK buffer0=%016llx buffer1=%016llx bytes=%llu "
        "bright_pixels0=%llu bright_pixels1=%llu guards=intact "
        "frames=%llu errors=%llu overlay=transient",
        (unsigned long long)first_hash, (unsigned long long)second_hash,
        (unsigned long long)readback_bytes,
        (unsigned long long)first_bright,
        (unsigned long long)second_bright,
        (unsigned long long)run.frames_completed,
        (unsigned long long)run.telemetry.errors);
#elif defined(PS5_BSP_TEXTURED)
    (void)ps5log_printf(PS5LOG_MARK,
        "BSP_TEXTURED_READBACK buffer0=%016llx buffer1=%016llx bytes=%llu "
        "bright_pixels0=%llu bright_pixels1=%llu guards=intact "
        "frames=%llu errors=%llu",
        (unsigned long long)first_hash, (unsigned long long)second_hash,
        (unsigned long long)readback_bytes,
        (unsigned long long)first_bright,
        (unsigned long long)second_bright,
        (unsigned long long)run.frames_completed,
        (unsigned long long)run.telemetry.errors);
#else
    (void)ps5log_printf(PS5LOG_MARK,
        "BSP_NOCLIP_READBACK buffer0=%016llx buffer1=%016llx bytes=%llu "
        "bright_pixels0=%llu bright_pixels1=%llu guards=intact "
        "frames=%llu errors=%llu",
        (unsigned long long)first_hash, (unsigned long long)second_hash,
        (unsigned long long)readback_bytes,
        (unsigned long long)first_bright,
        (unsigned long long)second_bright,
        (unsigned long long)run.frames_completed,
        (unsigned long long)run.telemetry.errors);
#endif
    const int input_continuity_valid =
        run.telemetry.errors == 0u &&
        run.telemetry.present_interval_over_budget == 0u &&
        renderer.pad_read_errors == 0u &&
        renderer.noclip.sampled_frames == BSP_GATE_FRAME_COUNT &&
        renderer.noclip.connected_frames == BSP_GATE_FRAME_COUNT;
#ifdef PS5_RESOURCE_FOUNDATION
    const int resource_valid =
        input_continuity_valid &&
        first_bright != 0u && second_bright != 0u &&
        renderer.bsp_plan.descriptor_table_dwords ==
            renderer.bsp_bundle.texture_count * BSP_TEXTURE_TABLE_DWORDS &&
        renderer.resource_frames[0].transient_bytes != 0u &&
        renderer.resource_frames[1].transient_bytes != 0u &&
        renderer.last_completed_token != 0u;
    if (!resource_valid)
        park("resource-render-or-retirement-gate-failure");
#elif defined(PS5_BSP_TEXTURED)
    const int textured_valid =
        input_continuity_valid &&
        first_bright != 0u && second_bright != 0u &&
        renderer.bsp_plan.descriptor_table_dwords ==
            renderer.bsp_bundle.texture_count * BSP_TEXTURE_TABLE_DWORDS;
    if (!textured_valid)
        park("textured-render-or-input-continuity-gate-failure");
#else
    const int noclip_valid =
        input_continuity_valid &&
        renderer.noclip.moving_frames >= BSP_NOCLIP_MIN_MOVING_FRAMES &&
        renderer.noclip.looking_frames >= BSP_NOCLIP_MIN_LOOKING_FRAMES &&
        renderer.noclip.distance_travelled >= 100.0f;
    if (!noclip_valid)
        park("noclip-input-or-movement-gate-failure");
#endif
#ifdef PS5_RESOURCE_FOUNDATION
    (void)ps5log_printf(PS5LOG_MARK,
        "BSP_RESOURCE_SOAK_COMPLETE frames=%llu connected_frames=%llu "
        "read_errors=%llu textures=%u descriptor_dwords=%u "
        "constants=per-frame overlay=transient pipelines=2 "
        "tokens=exact guards=intact errors=%llu",
        (unsigned long long)run.frames_completed,
        (unsigned long long)renderer.noclip.connected_frames,
        (unsigned long long)renderer.pad_read_errors,
        renderer.bsp_bundle.texture_count,
        renderer.bsp_plan.descriptor_table_dwords,
        (unsigned long long)run.telemetry.errors);
    const uint64_t retire_token = renderer.last_completed_token;
    uint32_t reclaimed = 0u;
    if (ps5_resource_pool_release_deferred(
            &resources.resource_pool, &resources.bsp_allocation,
            retire_token) != PS5_RESOURCE_POOL_OK ||
        ps5_resource_pool_release_deferred(
            &resources.resource_pool, &resources.shader_allocation,
            retire_token) != PS5_RESOURCE_POOL_OK ||
        ps5_resource_pool_release_deferred(
            &resources.resource_pool, &resources.depth_allocation,
            retire_token) != PS5_RESOURCE_POOL_OK ||
        ps5_resource_pool_release_deferred(
            &resources.resource_pool, &resources.transient_allocation,
            retire_token) != PS5_RESOURCE_POOL_OK ||
        ps5_resource_pool_reclaim(
            &resources.resource_pool, retire_token, 1,
            &reclaimed) != PS5_RESOURCE_POOL_OK || reclaimed != 4u)
        park("resource-pool-retirement-gate-failure");
    (void)ps5log_printf(PS5LOG_MARK,
        "RESOURCE_POOL_RETIRED token=%llu reclaimed=%u "
        "completion=fence+videoout",
        (unsigned long long)retire_token, reclaimed);
#elif defined(PS5_BSP_TEXTURED)
    (void)ps5log_printf(PS5LOG_MARK,
        "BSP_TEXTURED_SOAK_COMPLETE frames=%llu connected_frames=%llu "
        "moving_frames=%llu looking_frames=%llu distance_milli=%llu "
        "input_changes=%llu input_hash=%016llx read_errors=%llu "
        "textures=%u descriptor_dwords=%u composition=base_x_lightmap "
        "tokens=exact guards=intact errors=%llu",
        (unsigned long long)run.frames_completed,
        (unsigned long long)renderer.noclip.connected_frames,
        (unsigned long long)renderer.noclip.moving_frames,
        (unsigned long long)renderer.noclip.looking_frames,
        (unsigned long long)(renderer.noclip.distance_travelled * 1000.0f),
        (unsigned long long)renderer.noclip.input_changes,
        (unsigned long long)renderer.noclip.input_hash,
        (unsigned long long)renderer.pad_read_errors,
        renderer.bsp_bundle.texture_count,
        renderer.bsp_plan.descriptor_table_dwords,
        (unsigned long long)run.telemetry.errors);
#else
    (void)ps5log_printf(PS5LOG_MARK,
        "BSP_NOCLIP_SOAK_COMPLETE frames=%llu connected_frames=%llu "
        "moving_frames=%llu looking_frames=%llu distance_milli=%llu "
        "input_changes=%llu input_hash=%016llx read_errors=%llu "
        "tokens=exact guards=intact errors=%llu",
        (unsigned long long)run.frames_completed,
        (unsigned long long)renderer.noclip.connected_frames,
        (unsigned long long)renderer.noclip.moving_frames,
        (unsigned long long)renderer.noclip.looking_frames,
        (unsigned long long)(renderer.noclip.distance_travelled * 1000.0f),
        (unsigned long long)renderer.noclip.input_changes,
        (unsigned long long)renderer.noclip.input_hash,
        (unsigned long long)renderer.pad_read_errors,
        (unsigned long long)run.telemetry.errors);
#endif
    park_complete();
#else
    const int readback_valid = first_hash == second_hash &&
        first_bright != 0u && first_bright == second_bright;
    (void)ps5log_printf(readback_valid ? PS5LOG_MARK : PS5LOG_ERR,
        "BSP_READBACK_FNV64 buffer0=%016llx buffer1=%016llx bytes=%llu "
        "match=%s bright_pixels0=%llu bright_pixels1=%llu "
        "geometry_visible=%s guards=intact frames=%llu errors=%llu",
        (unsigned long long)first_hash, (unsigned long long)second_hash,
        (unsigned long long)readback_bytes,
        first_hash == second_hash ? "true" : "false",
        (unsigned long long)first_bright,
        (unsigned long long)second_bright,
        first_bright != 0u && first_bright == second_bright ? "true" : "false",
        (unsigned long long)run.frames_completed,
        (unsigned long long)run.telemetry.errors);
    if (!readback_valid)
        park("fixed-camera-readback-or-visibility-mismatch");
    (void)ps5log_line(PS5LOG_MARK,
        "BSP_GATE1_COMPLETE fixed_camera=true readback_exact=true "
        "geometry_visible=true tokens=exact guards=intact");
    park_complete();
#endif
#else
    uint64_t last_guard_check = 0u;
    uint64_t last_heartbeat = 0u;
    for (;;) {
        GearsFrameRunnerResult run = {0};
        result = gears_frame_loop_step(&loop);
        if (result != 0 || gears_frame_loop_result(&loop, &run) != 0 ||
            run.state != GEARS_RUN_ACTIVE)
            park("frame-loop-or-ownership-failure");
        if (run.frames_completed >= last_guard_check + 60u) {
            if (!guards_intact())
                park("guard-corruption");
            last_guard_check = run.frames_completed;
        }
        if (run.frames_completed >= last_heartbeat + 3600u) {
            (void)ps5log_printf(PS5LOG_MARK,
                "GEARS_HEARTBEAT frames=%llu max_in_flight=%u "
                "retired_fences=zero tokens=exact guards=intact errors=%llu",
                (unsigned long long)run.frames_completed,
                run.max_frames_in_flight,
                (unsigned long long)run.telemetry.errors);
            last_heartbeat = run.frames_completed;
        }
    }
#endif
}
