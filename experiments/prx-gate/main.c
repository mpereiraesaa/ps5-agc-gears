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
                        PRX_GATE_RUNTIME_LOAD ? "runtime" : "dependency", MODULE_PATH);

    int ok = 1;
#if PRX_GATE_RUNTIME_LOAD
    int load_result = 0;
    const int32_t handle = sceKernelLoadStartModule(MODULE_PATH, 0, 0, 0, 0, &load_result);
    (void)ps5log_printf(handle > 0 ? PS5LOG_INFO : PS5LOG_ERR,
                        "load_start_module handle=0x%08x result=0x%08x", (uint32_t)handle,
                        (uint32_t)load_result);
    if (handle <= 0)
    {
        (void)ps5log_line(PS5LOG_ERR, "PRX_GATE_FAILED step=load_start_module");
        finish("load-failed", 1);
    }
    void *add_address = 0;
    void *sleep_address = 0;
    void *version_address = 0;
    void *started_address = 0;
    const int r_add = sceKernelDlsym(handle, "hello_add", &add_address);
    const int r_sleep = sceKernelDlsym(handle, "hello_sleep_and_count", &sleep_address);
    const int r_version = sceKernelDlsym(handle, "hello_version", &version_address);
    const int r_started = sceKernelDlsym(handle, "hello_started", &started_address);
    (void)ps5log_printf(PS5LOG_INFO, "dlsym hello_add rc=0x%08x address=0x%016llx", (uint32_t)r_add,
                        (unsigned long long)(uintptr_t)add_address);
    (void)ps5log_printf(PS5LOG_INFO, "dlsym hello_sleep_and_count rc=0x%08x address=0x%016llx",
                        (uint32_t)r_sleep, (unsigned long long)(uintptr_t)sleep_address);
    (void)ps5log_printf(PS5LOG_INFO, "dlsym hello_version rc=0x%08x address=0x%016llx",
                        (uint32_t)r_version, (unsigned long long)(uintptr_t)version_address);
    (void)ps5log_printf(PS5LOG_INFO, "dlsym hello_started rc=0x%08x address=0x%016llx",
                        (uint32_t)r_started, (unsigned long long)(uintptr_t)started_address);
    if (r_add != 0 || r_sleep != 0 || r_version != 0 || r_started != 0 || !add_address ||
        !sleep_address || !version_address || !started_address)
    {
        (void)ps5log_line(PS5LOG_ERR, "PRX_GATE_FAILED step=dlsym");
        finish("dlsym-failed", 1);
    }
    const hello_add_fn add = (hello_add_fn)add_address;
    const hello_sleep_and_count_fn sleep_and_count = (hello_sleep_and_count_fn)sleep_address;
    const uint32_t version = *(const uint32_t *)version_address;
    const uint32_t started = *(const uint32_t *)started_address;
#else
    const uint32_t version = hello_version;
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
