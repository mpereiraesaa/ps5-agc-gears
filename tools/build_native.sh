#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
pin=37dd53602bdead63936f718004555ba10154be48
url=https://github.com/mpereiraesaa/ps5-native-app-boilerplate.git
foundation=${PS5_NATIVE_FOUNDATION:-$root/.deps/ps5-native-app-boilerplate}
amdllpc=${AMDLLPC:-}
readelf=${LLVM_READELF:-$(command -v llvm-readelf-18 || true)}

[[ -n $amdllpc && -x $amdllpc ]] || {
    echo "AMDLLPC must name the public LLPC executable" >&2; exit 2;
}
[[ -n $readelf && -x $readelf ]] || {
    echo "LLVM_READELF must name llvm-readelf" >&2; exit 2;
}
if [[ ! -d $foundation/.git ]]; then
    mkdir -p -- "$(dirname -- "$foundation")"
    git clone --filter=blob:none "$url" "$foundation"
fi
actual=$(git -C "$foundation" rev-parse HEAD)
if [[ $actual != "$pin" ]]; then
    git -C "$foundation" fetch origin "$pin"
    git -C "$foundation" checkout --detach "$pin"
fi
[[ $(git -C "$foundation" rev-parse HEAD) == "$pin" ]] || {
    echo "native foundation pin verification failed" >&2; exit 2;
}

make -C "$root" shaders AMDLLPC="$amdllpc" LLVM_READELF="$readelf"
make -C "$foundation" deps libc >/dev/null

bsp_bundle=${BSP_BUNDLE:-}
bsp_noclip=${BSP_NOCLIP:-0}
bsp_textured=${BSP_TEXTURED:-0}
dev_conf=${PS5LOG_DEV_CONF:-$root/dev.conf}
bsp_flags=()
[[ $bsp_noclip == 0 || $bsp_noclip == 1 ]] || {
    echo "BSP_NOCLIP must be 0 or 1" >&2; exit 2;
}
[[ $bsp_textured == 0 || $bsp_textured == 1 ]] || {
    echo "BSP_TEXTURED must be 0 or 1" >&2; exit 2;
}
if [[ $bsp_noclip == 1 && -z $bsp_bundle ]]; then
    echo "BSP_NOCLIP requires BSP_BUNDLE" >&2
    exit 2
fi
if [[ $bsp_textured == 1 && $bsp_noclip != 1 ]]; then
    echo "BSP_TEXTURED requires BSP_NOCLIP=1" >&2
    exit 2
fi
if [[ -n $bsp_bundle ]]; then
    bsp_bundle=$(realpath -- "$bsp_bundle")
    [[ -f $bsp_bundle ]] || {
        echo "BSP_BUNDLE must name a regular bundle file" >&2; exit 2;
    }
    [[ -f $dev_conf ]] || {
        echo "BSP viewer requires PS5LOG_DEV_CONF for hardware evidence" >&2
        exit 2
    }
    make -C "$root" bsp-inspect BSP_BUNDLE="$bsp_bundle"
    python3 "$root/tools/generate_bsp_build_metadata.py" \
        --bundle "$bsp_bundle" \
        --output "$root/build/generated/bsp_build_metadata.h"
    bsp_flags=(-DPS5_BSP_VIEWER=1)
    if [[ $bsp_noclip == 1 ]]; then
        bsp_flags+=(-DPS5_BSP_NOCLIP=1)
    fi
    if [[ $bsp_textured == 1 ]]; then
        bsp_flags+=(-DPS5_BSP_TEXTURED=1)
    fi
fi

sdk="$foundation/.deps/native/ps5-payload-sdk"
native="$foundation/tooling/native"
tool="$foundation/build/host/ps5-native-tool"
if [[ ! -x $tool ]]; then
    zlib_root="$foundation/.deps/native/zlib/root"
    zlib_archive=$(find "$zlib_root" -type f -name libz.a -print -quit)
    cxx=$(command -v clang++-18 || command -v clang++ || true)
    [[ -n $cxx && -n $zlib_archive ]] || {
        echo "native foundation host-tool dependencies are unavailable" >&2
        exit 2
    }
    mkdir -p "$foundation/build/host"
    "$cxx" -std=c++20 -O2 -Wall -Wextra -Werror \
        -I "$zlib_root/usr/include" \
        "$native/native_app_builder.cpp" "$native/self_container.cpp" \
        "$native/elf_object.cpp" "$native/sce_module_writer.cpp" \
        "$zlib_archive" -o "$tool"
fi
[[ -x $tool && -d $sdk && -f $foundation/runtime/libc.prx ]] || {
    echo "native foundation did not produce its SDK, tool and runtime" >&2
    exit 2
}

build="$root/build/native"
dist="$root/dist/PPSA99997"
rm -rf -- "$build" "$dist"
mkdir -p "$build/obj" "$build/import-stubs" "$dist/sce_sys" \
         "$dist/sce_module"
cc=(env PS5_PAYLOAD_SDK="$sdk" sh "$foundation/tooling/prospero-clang18")
common=(-O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections \
        -I"$root/include" -I"$root/src" -I"$root/native" \
        -I"$root/native/ps5log" -I"$root/build/generated" "${bsp_flags[@]}")

sources=(
    native/main.c native/ps5_agc_native.c
    src/bsp_bundle.c src/bsp_command_plan.c src/bsp_flat_draw.c
    src/bsp_noclip.c src/bsp_textured_draw.c
    src/bsp_flat_scene.c src/bsp_runtime_plan.c src/bsp_texture_descriptor.c
    src/gears_animation.c src/gears_draw_compose.c src/gears_frame_runner.c
    src/gears_frame_tracker.c src/gears_mesh.c src/gears_renderer.c
    src/gears_rt_clear.c src/gears_scene.c src/gears_telemetry.c
    src/ps5_agc_submit.c src/ps5_agc_writer.c src/ps5_color_target.c
    src/ps5_depth_target.c src/ps5_event_adapter.c
    src/ps5_frame_completion.c src/ps5_gpu_span.c src/ps5_pipeline.c
    src/ps5_present.c src/ps5_shader_header.c src/ps5_submission.c
    src/ps5_surface.c src/ps5_videoout.c src/ps5_cache_contract.c
    src/ps5_gfx1013_descriptor.c src/ps5_resource_pool.c
    src/ps5_transient_ring.c src/ps5_transient_table.c
)
objects=()
for source in "${sources[@]}"; do
    object="$build/obj/${source//\//_}.o"
    "${cc[@]}" -std=c11 "${common[@]}" -c "$root/$source" -o "$object"
    objects+=("$object")
done
"${cc[@]}" -std=c11 "${common[@]}" \
    -include "$root/native/ps5log/ps5log_ps5_net.h" \
    -c "$root/native/ps5log/ps5log.c" -o "$build/obj/ps5log.o"
"${cc[@]}" -std=c11 "${common[@]}" \
    -c "$root/native/ps5log/ps5log_ps5_net.c" \
    -o "$build/obj/ps5log_ps5_net.o"
"${cc[@]}" -c "$root/native/shader_assets.S" \
    -o "$build/obj/shader_assets.o"
"${cc[@]}" -std=c++20 -O2 -Wall -Wextra -Werror -fno-exceptions -fno-rtti \
    -ffunction-sections -fdata-sections -c "$native/app_crt.cpp" \
    -o "$build/obj/app_crt.o"

"${cc[@]}" -std=c11 -O2 -fPIC -I"$root/include" \
    -c "$root/native/stubs/libSceAgc.c" -o "$build/obj/agc-stub.o"
"$sdk/bin/prospero-lld" --shared -soname libSceAgc.prx \
    -o "$build/import-stubs/libSceAgc.so" "$build/obj/agc-stub.o"
"${cc[@]}" -std=c11 -O2 -fPIC -I"$root/include" \
    -c "$root/native/stubs/libSceAgcDriver.c" \
    -o "$build/obj/agc-driver-stub.o"
"$sdk/bin/prospero-lld" --shared -soname libSceAgcDriver.prx \
    -o "$build/import-stubs/libSceAgcDriver.so" \
    "$build/obj/agc-driver-stub.o"

objects+=("$build/obj/ps5log.o" "$build/obj/ps5log_ps5_net.o" \
          "$build/obj/shader_assets.o")
"$sdk/bin/prospero-lld" -T "$native/ps5-pie.ld" --eh-frame-hdr \
    --version-script "$native/app-symbols.map" -e _start \
    -o "$build/llvm-pie.elf" "$build/obj/app_crt.o" "${objects[@]}" \
    --as-needed "$sdk"/target/lib/*.so \
    "$build/import-stubs/libSceAgc.so" \
    "$build/import-stubs/libSceAgcDriver.so"
"$tool" link --in "$build/llvm-pie.elf" --out "$build/eboot.elf" \
    --stub-dir "$sdk/target/lib" --module-sdk 0x02000009 \
    --stub "$build/import-stubs/libSceAgc.so" \
    --stub "$build/import-stubs/libSceAgcDriver.so" \
    --companion-sdk 0x08050001 --file-name eboot.elf
"$tool" self --sign --in "$build/eboot.elf" --out "$dist/eboot.bin" \
    --magic 0x1D3D154F
cp "$root/sce_sys/param.json" "$root/sce_sys/icon0.png" "$dist/sce_sys/"
cp "$foundation/runtime/libc.prx" "$dist/sce_module/libc.prx"
if [[ -f $dev_conf ]]; then
    cp "$dev_conf" "$dist/dev.conf"
fi
if [[ -n $bsp_bundle ]]; then
    cp "$bsp_bundle" "$dist/map.ps5bsp"
fi
sha256sum "$build/eboot.elf" "$dist/eboot.bin" > "$build/SHA256SUMS"
"$tool" self --inspect --file "$dist/eboot.bin"
cat "$build/SHA256SUMS"
