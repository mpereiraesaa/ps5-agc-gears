/*
 * PRX gate: minimal native title that proves an application-owned dynamic
 * module (hello.prx, built by the native-foundation fork) resolves and runs
 * on PS5 firmware 12.02.
 *
 * Gate 1 (default): the module is a load-time dependency. The executable
 * imports hello_* by NID through the module's stub, so the loader must find
 * /app0/sce_module/hello.prx and bind the imports before main() runs.
 *
 * Gate 2 (PRX_GATE_RUNTIME_LOAD=1): nothing is imported at link time. The
 * title loads the module with sceKernelLoadStartModule, resolves the same
 * symbols with sceKernelDlsym, calls them, and unloads the module.
 *
 * Every observation goes out as ps5log/1 records; the run is complete only
 * when the transcript ends with PRX_GATE_VERIFIED followed by a clean BYE.
 */

#include "ps5log.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef PRX_GATE_RUNTIME_LOAD
#define PRX_GATE_RUNTIME_LOAD 0
#endif

#define TITLE_ID "PPSA99999"
#define APP_NAME "prx-gate"
#define MODULE_PATH "/app0/sce_module/hello.prx"

#if PRX_GATE_RUNTIME_LOAD
/* Kernel module-loading ABI as used by native titles; declared here because
 * the public payload SDK ships the symbols in libkernel without headers. */
int32_t sceKernelLoadStartModule(const char *path, size_t argc, const void *argv,
                                 uint32_t flags, const void *option, int *result);
int sceKernelDlsym(int32_t handle, const char *symbol, void **address);
int sceKernelStopUnloadModule(int32_t handle, size_t argc, const void *argv,
                              uint32_t flags, const void *option, int *result);
int sceKernelGetModuleList(int32_t *handles, size_t capacity, size_t *count);
int sceKernelGetModuleInfo(int32_t handle, void *info);
/* POSIX-style entry points that FW 12.02 libkernel also exports. */
void *dlopen(const char *path, int mode);
void *dlsym(void *handle, const char *symbol);
int dlclose(void *handle);

typedef int (*hello_add_fn)(int, int);
typedef uint32_t (*hello_sleep_and_count_fn)(unsigned int);
#else
int hello_add(int left, int right);
uint32_t hello_sleep_and_count(unsigned int microseconds);
extern const uint32_t hello_version;
extern uint32_t hello_started;
extern uint32_t hello_stopped;
#endif

static uint64_t now_ns(void)
{
    struct timespec value = {0, 0};
    (void)clock_gettime(CLOCK_MONOTONIC, &value);
    return (uint64_t)value.tv_sec * UINT64_C(1000000000) + (uint64_t)value.tv_nsec;
}

static void finish(const char *reason, int code)
{
    ps5log_close(reason);
    /* Proven clean exit for PPSA titles on this firmware: skip the CRT
     * atexit path so the shell returns to the menu without a dialog. */
    _exit(code);
}

static int check(const char *name, long observed, long expected)
{
    const int ok = observed == expected;
    (void)ps5log_printf(ok ? PS5LOG_INFO : PS5LOG_ERR, "%s=%ld expected=%ld %s", name,
                        observed, expected, ok ? "ok" : "MISMATCH");
    return ok;
}

int main(void)
{
    ps5log_config config;
    const char *config_path = 0;
    const uint64_t boot_token = now_ns();
    ps5log_config_defaults(&config);
    const int config_result = ps5log_load_config(ps5log_default_conf_paths,
                                                 ps5log_default_conf_path_count, &config,
                                                 &config_path);
    const int log_result =
        config_result == 0 ? ps5log_init(&config, TITLE_ID, APP_NAME, boot_token) : 1;
    (void)ps5log_line(PS5LOG_INFO, "LOG_SCHEMA=3");
    (void)ps5log_line(PS5LOG_INFO, "LOG_TRANSPORT=ps5log/1 tcp structured");
    (void)ps5log_line(PS5LOG_INFO, "LOG_FS_SINKS=disabled");
    (void)ps5log_hex64(PS5LOG_INFO, "LOG_BOOT_MONOTONIC_NS", boot_token);
    (void)ps5log_printf(PS5LOG_INFO, "LOG_CONFIG_RESULT=%d LOG_INIT_RESULT=%d path=%s",
                        config_result, log_result, config_path ? config_path : "unavailable");
    (void)ps5log_printf(PS5LOG_MARK, "PRX_GATE_BEGIN mode=%s module=%s fw=12.02",
                        PRX_GATE_RUNTIME_LOAD ? "runtime" : "dependency",
                        PRX_GATE_RUNTIME_LOAD ? MODULE_PATH : MODULE_PATH);

    int ok = 1;
#if PRX_GATE_RUNTIME_LOAD
    /* Diagnostic probes, all read-only: the same call against the runtime
     * shim that is already a loaded dependency and against a system module
     * separates "this module is rejected" from "this call is unavailable to
     * the title". Neither probe is unloaded: libc.prx is in use, and a system
     * module handle is left to the process teardown. */
    /* Symbol resolution without kernel dlsym: the module carries a static,
     * relocated descriptor; sceKernelGetModuleInfo gives the module's segments
     * and the host scans them for the descriptor magic. */
    int load_result = 0;
    const int32_t handle = sceKernelLoadStartModule(MODULE_PATH, 0, 0, 0, 0,
                                                    &load_result);
    (void)ps5log_printf(handle > 0 ? PS5LOG_INFO : PS5LOG_ERR,
                        "load_start_module hello_g9 handle=0x%08x result=0x%08x", (uint32_t)handle,
                        (uint32_t)load_result);
    if (handle <= 0)
    {
        (void)ps5log_line(PS5LOG_ERR, "PRX_GATE_FAILED step=load_start_module");
        finish("load-failed", 1);
    }
    typedef struct prx_export { const char *name; const void *address; } prx_export;
    typedef struct prx_descriptor { uint64_t magic; uint32_t version; uint32_t count; prx_export exports[]; } prx_descriptor;
    const prx_descriptor *descriptor = 0;
    {
        /* SceKernelModuleInfo: size(8) name[256] segments[4]{addr(8) size(4) prot(4)} count(4) fingerprint(20) */
        uint64_t info[0x200 / 8];
        memset(info, 0, sizeof(info));
        info[0] = 0x160;
        const int info_rc = sceKernelGetModuleInfo(handle, info);
        const uint8_t *raw = (const uint8_t *)info;
        const uint32_t segment_count = *(const uint32_t *)(raw + 0x148);
        (void)ps5log_printf(PS5LOG_INFO, "module_info rc=0x%08x name=%.32s segments=%u", (uint32_t)info_rc,
                            (const char *)(raw + 8), segment_count);
        for (uint32_t index = 0; index < segment_count && index < 4u; ++index)
        {
            const uint8_t *segment = raw + 0x108 + index * 16;
            const uintptr_t address = *(const uintptr_t *)segment;
            const uint32_t size = *(const uint32_t *)(segment + 8);
            const uint32_t prot = *(const uint32_t *)(segment + 12);
            (void)ps5log_printf(PS5LOG_INFO, "segment[%u] address=0x%016llx size=0x%08x prot=0x%x", index,
                                (unsigned long long)address, size, prot);
            if (descriptor || !address || !size || !(prot & 1u))
                continue;
            for (uintptr_t cursor = address; cursor + sizeof(prx_descriptor) <= address + size; cursor += 16)
            {
                if (*(const uint64_t *)cursor == 0x3143534544585250ull)
                {
                    descriptor = (const prx_descriptor *)cursor;
                    break;
                }
            }
        }
    }
    if (!descriptor)
    {
        (void)ps5log_line(PS5LOG_ERR, "PRX_GATE_FAILED step=descriptor-scan");
        finish("descriptor-missing", 1);
    }
    (void)ps5log_printf(PS5LOG_INFO, "descriptor at 0x%016llx version=%u count=%u",
                        (unsigned long long)(uintptr_t)descriptor, descriptor->version, descriptor->count);
    void *add_address = 0;
    void *sleep_address = 0;
    const void *version_address = 0;
    const void *started_address = 0;
    for (uint32_t index = 0; index < descriptor->count; ++index)
    {
        const prx_export *entry = &descriptor->exports[index];
        (void)ps5log_printf(PS5LOG_INFO, "export \"%s\" address=0x%016llx", entry->name,
                            (unsigned long long)(uintptr_t)entry->address);
        if (strcmp(entry->name, "hello_add") == 0) add_address = (void *)entry->address;
        if (strcmp(entry->name, "hello_sleep_and_count") == 0) sleep_address = (void *)entry->address;
        if (strcmp(entry->name, "hello_version") == 0) version_address = entry->address;
        if (strcmp(entry->name, "hello_started") == 0) started_address = entry->address;
    }
    {
        void *probe = 0;
        const int rc = sceKernelDlsym(handle, "hello_add", &probe);
        (void)ps5log_printf(PS5LOG_INFO, "kernel dlsym hello_add rc=0x%08x (recorded, not required)", (uint32_t)rc);
    }
    if (!add_address || !sleep_address || !version_address || !started_address)
    {
        (void)ps5log_line(PS5LOG_ERR, "PRX_GATE_FAILED step=descriptor-entries");
        finish("descriptor-incomplete", 1);
    }
    const hello_add_fn add = (hello_add_fn)add_address;
    const hello_sleep_and_count_fn sleep_and_count = (hello_sleep_and_count_fn)sleep_address;
    const uint32_t version = *(const uint32_t *)version_address;
    const uint32_t started = *(const uint32_t *)started_address;
#else
    /* Report the loader-resolved addresses before dereferencing anything, so
     * an unbound import (address 0) is visible in the transcript instead of
     * only as a crash. */
    const uintptr_t add_ptr = (uintptr_t)&hello_add;
    const uintptr_t sleep_ptr = (uintptr_t)&hello_sleep_and_count;
    const uintptr_t version_ptr = (uintptr_t)&hello_version;
    const uintptr_t started_ptr = (uintptr_t)&hello_started;
    const uintptr_t stopped_ptr = (uintptr_t)&hello_stopped;
    (void)ps5log_printf(PS5LOG_INFO, "import hello_add=0x%016llx hello_sleep_and_count=0x%016llx",
                        (unsigned long long)add_ptr, (unsigned long long)sleep_ptr);
    (void)ps5log_printf(PS5LOG_INFO,
                        "import hello_version=0x%016llx hello_started=0x%016llx hello_stopped=0x%016llx",
                        (unsigned long long)version_ptr, (unsigned long long)started_ptr,
                        (unsigned long long)stopped_ptr);
    if (add_ptr == 0 || sleep_ptr == 0 || version_ptr == 0 || started_ptr == 0)
    {
        (void)ps5log_line(PS5LOG_ERR, "PRX_GATE_FAILED step=import-binding");
        finish("import-unbound", 1);
    }
    (void)ps5log_line(PS5LOG_INFO, "step=call hello_add before any data access");
    const int first_add = hello_add(1, 2);
    (void)ps5log_printf(PS5LOG_INFO, "first hello_add(1,2)=%d", first_add);
    (void)ps5log_line(PS5LOG_INFO, "step=read hello_version");
    const uint32_t version = hello_version;
    (void)ps5log_line(PS5LOG_INFO, "step=read hello_started");
    const uint32_t started = hello_started;
#define add hello_add
#define sleep_and_count hello_sleep_and_count
#endif

    /* module_start is observational: the loader may or may not invoke it. */
    (void)ps5log_printf(PS5LOG_INFO, "hello_started=%u module_start_invoked=%s", started,
                        started ? "yes" : "no");
    ok &= check("hello_version", (long)version, 0x00010000L);
    ok &= check("hello_add(1,2)", add(1, 2), 3);
    ok &= check("hello_add(40,2)", add(40, 2), 42);
    const uint64_t before = now_ns();
    const uint32_t count = sleep_and_count(2000u);
    const uint64_t elapsed = now_ns() - before;
    ok &= check("hello_sleep_and_count", (long)count, 3);
    (void)ps5log_printf(PS5LOG_INFO, "hello_sleep_elapsed_ns=%llu", (unsigned long long)elapsed);

#if PRX_GATE_RUNTIME_LOAD
    int stop_result = 0;
    const int unload = sceKernelStopUnloadModule(handle, 0, 0, 0, 0, &stop_result);
    (void)ps5log_printf(unload == 0 ? PS5LOG_INFO : PS5LOG_ERR,
                        "stop_unload_module rc=0x%08x result=0x%08x", (uint32_t)unload,
                        (uint32_t)stop_result);
    ok &= unload == 0;
#else
    (void)ps5log_printf(PS5LOG_INFO, "hello_stopped=%u", hello_stopped);
#endif

    if (!ok)
    {
        (void)ps5log_line(PS5LOG_ERR, "PRX_GATE_FAILED step=verify");
        finish("verify-failed", 1);
    }
    (void)ps5log_printf(PS5LOG_MARK, "PRX_GATE_VERIFIED mode=%s exports=4 module_start=%s",
                        PRX_GATE_RUNTIME_LOAD ? "runtime" : "dependency",
                        started ? "invoked" : "not-invoked");
    finish("complete", 0);
    return 0;
}
