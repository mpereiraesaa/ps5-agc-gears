#!/usr/bin/env bash
# Builds the PRX gate title (PPSA99999) against the native-foundation fork
# checkout that carries the dynamic-module converter and a built hello module.
#
#   PRX_FOUNDATION=/path/to/ps5-native-app-boilerplate-exp-prx-module \
#   PS5LOG_DEV_CONF=/absolute/private/dev.conf \
#   PRX_GATE_RUNTIME_LOAD=0|1 \
#   bash experiments/prx-gate/build.sh
#
# Gate 1 (PRX_GATE_RUNTIME_LOAD=0, default) links the executable against the
# module's import stub, so the loader binds hello.prx as a dependency.
# Gate 2 (PRX_GATE_RUNTIME_LOAD=1) links nothing from the module and loads it
# at runtime through sceKernelLoadStartModule/sceKernelDlsym.
#
# This is an experiment build: it deliberately uses the foundation checkout as
# given, without the release pin check of tools/build_native.sh, because the
# module converter is not part of the pinned release foundation yet.
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
gate="$root/experiments/prx-gate"
foundation=${PRX_FOUNDATION:-$root/../../third_party/ps5-native-app-boilerplate-exp-prx-module}
foundation=$(cd -- "$foundation" && pwd)
runtime_load=${PRX_GATE_RUNTIME_LOAD:-0}
[[ $runtime_load == 0 || $runtime_load == 1 ]] || {
    echo "PRX_GATE_RUNTIME_LOAD must be 0 or 1" >&2; exit 2;
}
mode=dependency
[[ $runtime_load == 0 ]] || mode=runtime

sdk="$foundation/.deps/native/ps5-payload-sdk"
native="$foundation/tooling/native"
tool="$foundation/build/host/ps5-native-tool"
module="$foundation/.local/runtime/hello.prx"
stub="$foundation/.local/stubs/hello.so"
libc="$foundation/runtime/libc.prx"
[[ -d $sdk && -f $native/app_crt.cpp ]] || {
    echo "foundation checkout lacks the payload SDK or native tooling: $foundation" >&2; exit 2;
}
# The tool prints its usage and exits non-zero without arguments.
tool_usage=$([[ -x $tool ]] && "$tool" 2>&1 || true)
[[ $tool_usage == *"--module"* ]] || {
    echo "ps5-native-tool with --module support is required; run tools/build-module.sh hello in the foundation" >&2
    exit 2
}
[[ -f $module && -f $stub ]] || {
    echo "hello module is not built; run tools/build-module.sh hello in the foundation" >&2; exit 2;
}
[[ -f $libc ]] || bash "$foundation/tools/rebuild-libc.sh"

build="$root/build/prx-gate-$mode"
dist="$root/dist/PPSA99999"
rm -rf -- "$build" "$dist"
mkdir -p "$build/obj" "$dist/sce_sys" "$dist/sce_module"
cc=(env PS5_PAYLOAD_SDK="$sdk" sh "$foundation/tooling/prospero-clang18")
common=(-O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections
        -I"$root/native/ps5log")

"${cc[@]}" -std=c11 "${common[@]}" -DPRX_GATE_RUNTIME_LOAD="$runtime_load" \
    -c "$gate/main.c" -o "$build/obj/main.o"
"${cc[@]}" -std=c11 "${common[@]}" -include "$root/native/ps5log/ps5log_ps5_net.h" \
    -c "$root/native/ps5log/ps5log.c" -o "$build/obj/ps5log.o"
"${cc[@]}" -std=c11 "${common[@]}" \
    -c "$root/native/ps5log/ps5log_ps5_net.c" -o "$build/obj/ps5log_ps5_net.o"
"${cc[@]}" -std=c++20 -O2 -Wall -Wextra -Werror -fno-exceptions -fno-rtti \
    -ffunction-sections -fdata-sections -c "$native/app_crt.cpp" -o "$build/obj/app_crt.o"

link_extra=()
stub_extra=()
if [[ $runtime_load == 0 ]]; then
    link_extra+=("$stub")
    stub_extra+=(--stub "$stub")
fi
"$sdk/bin/prospero-lld" -T "$native/ps5-pie.ld" --eh-frame-hdr \
    --version-script "$native/app-symbols.map" -e _start \
    -o "$build/llvm-pie.elf" "$build/obj/app_crt.o" "$build/obj/main.o" \
    "$build/obj/ps5log.o" "$build/obj/ps5log_ps5_net.o" \
    --as-needed "$sdk"/target/lib/*.so "${link_extra[@]}"
"$tool" link --in "$build/llvm-pie.elf" --out "$build/eboot.elf" \
    --stub-dir "$sdk/target/lib" "${stub_extra[@]}" --module-sdk 0x02000009 \
    --companion-sdk 0x08050001 --file-name eboot.elf
"$tool" self --sign --in "$build/eboot.elf" --out "$dist/eboot.bin" --magic 0x1D3D154F

cp "$gate/param.json" "$dist/sce_sys/param.json"
cp "$root/sce_sys/icon0.png" "$dist/sce_sys/icon0.png"
cp "$libc" "$dist/sce_module/libc.prx"
cp "$module" "$dist/sce_module/hello.prx"
dev_conf=${PS5LOG_DEV_CONF:-$root/dev.conf}
if [[ -f $dev_conf ]]; then
    cp "$dev_conf" "$dist/dev.conf"
else
    echo "warning: no dev.conf packaged; the run will not be observable over ps5log" >&2
fi

"$tool" self --inspect --file "$dist/eboot.bin"
"$tool" self --inspect --file "$dist/sce_module/hello.prx"
sha256sum "$build/eboot.elf" "$dist/eboot.bin" "$dist/sce_module/hello.prx" > "$build/SHA256SUMS"
cat "$build/SHA256SUMS"
printf 'PRX gate build complete: mode=%s\nTitle folder: %s\n' "$mode" "$dist"
