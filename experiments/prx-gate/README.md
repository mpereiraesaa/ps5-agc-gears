# PRX gate

Hardware gate for application-owned dynamic modules. A minimal native title,
`PPSA99999` "AGC PRX Gate", exercises `hello.prx`, a module built from source
by the native-foundation fork's `tools/build-module.sh`. Nothing here touches
the Gears renderer or its release build.

## Questions the gate answers

| Gate | Build | Question | Result on FW 12.02 (2026-09-06) |
| --- | --- | --- | --- |
| 1 | `PRX_GATE_RUNTIME_LOAD=0` | Does the loader bind a `DT_NEEDED` application module from `sce_module/` before `main()`? | **No.** Only platform-known entries such as `libc.prx` are loaded at start; the imports stay zero. Not a tooling defect. |
| 2 | `PRX_GATE_RUNTIME_LOAD=1` | Does `sceKernelLoadStartModule` load our module, can its exports be called, does `sceKernelStopUnloadModule` work? | **Yes**, once the module satisfies the two loader contracts below. Run `20260906T085730845Z_PPSA99999_prx-gate_0x519451728697`, `PRX_GATE_VERIFIED`, gap-free BYE. |
| both | | Does the loader invoke `module_start`? | No. It calls `e_entry` as the start routine and requires it to return 0. |

Loader contracts established by bisection (about twenty launches, each
changing one variable) and now enforced by the module converter:

- `DT_PLTGOT` must be a three-entry table; the loader writes entries 1 and 2.
  A one-entry GOT with the module parameters behind it is corrupted before
  validation and rejected with `0x80020063`.
- `e_entry` is executed after mapping and must return 0; `int3` filler there
  raises SIGTRAP and a garbage return yields `0x80020016`.

Symbol resolution: `sceKernelDlsym` answers `0x80020003` for every
application module, including the boilerplate's `libc.prx`, whatever the hash
table holds. The module therefore publishes a static, relocated export
descriptor (magic `PRXDESC1`) and the host finds it by scanning the segments
returned by `sceKernelGetModuleInfo`. That is the validated resolution path and
the one a Xash3D library loader should use.

## Build

Prerequisites in the foundation checkout: `tools/build-module.sh hello` has
produced `.local/runtime/hello.prx` and `.local/stubs/hello.so`, and
`build/host/ps5-native-tool` understands `link --module`.

```sh
PRX_FOUNDATION=../../third_party/ps5-native-app-boilerplate-exp-prx-module \
PS5LOG_DEV_CONF=/absolute/private/dev.conf \
bash experiments/prx-gate/build.sh                       # gate 1

PRX_GATE_RUNTIME_LOAD=1 PS5LOG_DEV_CONF=... bash experiments/prx-gate/build.sh   # gate 2
```

Output: `dist/PPSA99999/` with `eboot.bin`, `sce_module/libc.prx`,
`sce_module/hello.prx`, `sce_sys/` and the packaged `dev.conf`. The build
prints the SHA-256 of the executable and the module; record both with the run.

## Deploy and observe

1. `make serve` in the logging server on the PC.
2. Upload the whole `dist/PPSA99999` folder to `/data/homebrew/PPSA99999`
   over FTP. `PPSA99999` is the identity the lab's launch and close helpers
   already know for the native Hello World template.
3. Reload ShadowMountPlus with no BigApp running, then launch the exact title.
4. The title exits by itself after `PRX_GATE_VERIFIED`; on failure it exits
   after `PRX_GATE_FAILED step=...` with a non-zero code.

## Acceptance

A gate passes only when the `ps5logd` manifest shows title `PPSA99999`, app
`prx-gate`, boot token equal to `LOG_BOOT_MONOTONIC_NS`, no gaps, and the
transcript contains, in order:

```text
PRX_GATE_BEGIN mode=runtime module=/app0/sce_module/hello.prx fw=12.02
load_start_module ... handle=0x000000d0 result=0x00000000
module_info rc=0x00000000 name=hello.prx segments=4
descriptor at 0x... version=1 count=6
hello_version=65536 expected=65536 ok
hello_add(1,2)=3 expected=3 ok
hello_add(40,2)=42 expected=42 ok
hello_sleep_and_count=3 expected=3 ok
stop_unload_module rc=0x00000000 result=0x00000000
PRX_GATE_VERIFIED mode=runtime exports=4 module_start=not-invoked
BYE seq=<n> reason=complete
```

The passing run is archived with artifact and module hashes in the private
laboratory under `research/gpu/captures/runtime/`.

## After the gates

The Xash3D port keeps its modular layout: engine executable plus renderer,
menu, client and server as `sce_module/*.prx`, loaded with
`sceKernelLoadStartModule` and resolved through each module's export
descriptor. Static linking is no longer needed as a fallback for the engine's
own libraries; load-time `DT_NEEDED` binding and `sceKernelDlsym` are simply
not part of the design on this firmware.
