# PRX gate

Hardware gate for application-owned dynamic modules. A minimal native title,
`PPSA99999` "AGC PRX Gate", exercises `hello.prx`, a module built from source
by the native-foundation fork's `tools/build-module.sh`. Nothing here touches
the Gears renderer or its release build.

## Questions the gate answers

| Gate | Build | Question |
| --- | --- | --- |
| 1 | `PRX_GATE_RUNTIME_LOAD=0` | Does the loader accept our module as a `DT_NEEDED` dependency from `sce_module/` and bind NID imports to it before `main()`? |
| 2 | `PRX_GATE_RUNTIME_LOAD=1` | Do `sceKernelLoadStartModule`, `sceKernelDlsym` and `sceKernelStopUnloadModule` work on that module from a native title? |
| both | | Does the loader invoke `module_start`? The module sets `hello_started`; the title reports it without assuming either answer. |

One variable per iteration: run gate 1 first. Gate 2 changes only the load
mechanism; the module, title identity and telemetry stay the same.

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
PRX_GATE_BEGIN mode=<dependency|runtime>
hello_version=65536 expected=65536 ok
hello_add(1,2)=3 expected=3 ok
hello_add(40,2)=42 expected=42 ok
hello_sleep_and_count=3 expected=3 ok
PRX_GATE_VERIFIED mode=<dependency|runtime> exports=4 module_start=<invoked|not-invoked>
BYE seq=<n> reason=complete
```

Gate 2 additionally requires `load_start_module handle=` with a positive
handle, four `dlsym ... rc=0x00000000` lines with non-zero addresses, and
`stop_unload_module rc=0x00000000`.

A launch return code, a shell without an error dialog, or a transcript without
BYE is not a pass. A pre-entry rejection in gate 1 points at the module's
loader-visible shape; a `dlsym` failure in gate 2 points at the export
encoding or the hash table.

## After the gates

If both pass, the Xash3D port keeps its modular layout: engine executable
plus renderer, menu, client and server as `sce_module/*.prx`, with the
engine's library loader mapped to the two kernel calls used here. If either
fails, static linking remains the fallback and the failure record goes into
the plan's risk table.
