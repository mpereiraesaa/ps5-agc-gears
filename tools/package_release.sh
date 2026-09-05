#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
version=${1:-v0.1.0-rc1}
[[ $version =~ ^v[0-9]+\.[0-9]+\.[0-9]+(-rc[1-9][0-9]*)?$ ]] || {
    echo "version must look like v0.1.0 or v0.1.0-rc1" >&2; exit 2;
}
title="$root/dist/PPSA99997"
for file in eboot.bin sce_sys/param.json sce_sys/icon0.png sce_module/libc.prx; do
    [[ -f $title/$file ]] || { echo "missing release input: $file" >&2; exit 2; }
done

out="$root/release"
name="ps5-agc-gears-$version"
stage="$out/$name"
rm -rf -- "$stage"
mkdir -p "$stage/PPSA99997/sce_sys" "$stage/PPSA99997/sce_module"
cp "$title/eboot.bin" "$stage/PPSA99997/"
cp "$title/sce_sys/param.json" "$title/sce_sys/icon0.png" \
   "$stage/PPSA99997/sce_sys/"
cp "$title/sce_module/libc.prx" "$stage/PPSA99997/sce_module/"
cp "$root/LICENSE" "$root/NOTICE.md" "$stage/"

(cd "$stage" && find . -type f ! -name SHA256SUMS -print0 | sort -z | \
    xargs -0 sha256sum > SHA256SUMS)
tar --sort=name --mtime='UTC 2026-01-01' --owner=0 --group=0 --numeric-owner \
    -C "$out" -czf "$out/$name.tar.gz" "$name"
sha256sum "$out/$name.tar.gz" > "$out/$name.tar.gz.sha256"
printf '%s\n' "$out/$name.tar.gz"
