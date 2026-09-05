# BSP viewer Phase 1

This branch starts the first `Xash3D on PS5` gate without importing game data.
The canonical Gears renderer remains intact while the BSP path is brought up as
tested, reusable interfaces.

## Audit at branch start

| Gate 1 dependency | Baseline | Change in this branch | Host evidence |
|---|---|---|---|
| BSP v30 parsing | Missing | Deterministic host baker for entities, vertices, edges, surfedges, faces, texinfo and miptex dimensions | Synthetic four-edge face and malformed-input regressions |
| Reproducible upload format | Missing | Versioned, checksummed `VERT`/`INDX`/`DRAW` bundle with fixed spawn camera | Pinned SHA-256 plus C consumer validation |
| Z-up conversion | Missing | Baker emits right-handed Y-up positions and a forward vector | Exact spawn and direction assertions |
| Static GPU allocation | Hard-coded offsets | Bounds-checked aligned bump allocator | Alignment, exhaustion and no-advance-on-failure tests |
| Indexed AGC draw | ABI documented, unused | Six-DWORD checked writer with a full GPU-span guard | Synthetic builder cursor and visibility tests |
| Map command sizing | Fixed 120 DWORD | Preflighted `draw_count * 29` flat-map composition | Exact draw/DWORD and insufficient-capacity tests |
| Flat map shader | Missing | Position + MVP + per-face color pipeline source | Source contract in the host gate |
| Native fixed-camera frame | Missing | Next hardware-bearing change | Fresh native artifact, `ps5log/1`, exact readback hash |

## Gate 1 execution

1. Run `make all`; no console operation is allowed until it is green.
2. Bake a private map with `make bsp-bundle BSP_INPUT=/private/path/map.bsp`.
   `.bsp`, `.wad` and `.ps5bsp` files are ignored globally and are never release
   inputs.
3. Upload the validated bundle into one direct-memory allocation, use the
   bundle spawn/forward fields to build the fixed view matrix, and compose the
   flat draws only after the exact command capacity has been reserved.
4. A native run must identify the artifact and bundle hashes in `ps5log/1`,
   report bundle vertex/index/draw counts, draw DWORDs, fence completion, exact
   VideoOut token, guards and teardown.
5. Record an exact raw render-target readback hash for the fixed camera. Remote
   Play supplies the accompanying visual capture, not completion evidence.

Changes to the native command stream, direct-memory layout or indexed builder
require a soak before Gate 1 can be marked complete. No expected hardware hash
is recorded until the first independently archived run establishes it.
