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
| Command allocation | Two fixed 4 KiB slots | Draw-count-sized, 64 KiB-aligned dual slots with separated fences | Overflow, offsets and capacity plan tests |
| Flat map shader | Missing | Position + MVP + per-face color pipeline compiled and embedded beside Gears | GFX1013 PAL metadata, zero-relocation ELF and native-link checks |
| Native fixed-camera frame | Missing | Private bundle load, dynamic command slots, indexed flat draws, two-buffer drain and readback | Hardware `ps5log/1` gate and Remote Play visual capture passed |

## Gate 1 execution

1. Run `make all`; no console operation is allowed until it is green.
   `make shaders` compiles both `gears_lit.pipe` and `bsp_flat.pipe`, derives
   namespaced AGC metadata for each pipeline and emits separately named native
   stage blobs. Generated shader products remain under `build/`.
2. Bake a private map with `make bsp-bundle BSP_INPUT=/private/path/map.bsp`.
   `.bsp`, `.wad` and `.ps5bsp` files are ignored globally and are never release
   inputs. The target validates the generated file through the C runtime parser.
   A hardware-bearing artifact is built with:

   ```sh
   make bsp-native-release BSP_INPUT=/private/path/map.bsp \
     PS5LOG_DEV_CONF=/private/path/dev.conf \
     AMDLLPC=/path/to/amdllpc LLVM_READELF=/path/to/llvm-readelf
   ```

   This target refuses a missing TCP configuration, embeds the bundle SHA-256
   as generated build metadata, and copies the validated bundle only into the
   ignored `dist/PPSA99997/` title directory.
3. Upload the validated bundle into one direct-memory allocation, use the
   bundle spawn/forward fields to build the fixed view matrix, and compose the
   flat draws only after the exact command capacity has been reserved.
4. A native run must identify the artifact and bundle hashes in `ps5log/1`,
   report bundle vertex/index/draw counts, draw DWORDs, fence completion, exact
   VideoOut token, guards and teardown.
5. Record an exact raw render-target readback hash for the fixed camera. Remote
   Play supplies the accompanying visual capture, not completion evidence.

The BSP build submits the same fixed camera for 600 frames, drains GPU and
VideoOut ownership, then requires equal FNV-1a hashes over both complete tiled
render-target footprints. It also requires both buffers to contain the same
nonzero count of pixels brighter than the deliberately dark clear, proving map
geometry wrote the targets. It parks only after emitting
`BSP_GATE1_COMPLETE`; any token, guard, compose, ownership, visibility or
readback mismatch parks as an error and retains all resources.

Changes to the native command stream, direct-memory layout or indexed builder
require a soak before Gate 1 can be marked complete.

## Gate 1 hardware evidence

The independently archived run
`20260905T194257169Z_PPSA99997_ps5-agc-gears_0x2638941f043d` establishes the
fixed-camera reference on GFX1013 firmware 12.02:

- signed `eboot.bin` SHA-256:
  `ab2f4040a2d271bfa1a779500410ef6bc0b9d852c3f4bd115d94c8400c80c633`;
- private bundle SHA-256:
  `9efe388d160b0f75f7861c88bdc496a75f1b85ffa42c06acbac411527981815d`;
- 600 completed frames, zero errors and zero over-budget present intervals;
- exact VideoOut token bookends at frames 0 and 599;
- identical full-buffer FNV-1a readback `bac7777b7831038e` with 1,941,430
  bright pixels in each buffer;
- intact guards and a clean `BSP_GATE1_COMPLETE` BYE after 23 contiguous
  `ps5log/1` records.

The console title was then closed by exact identity and all development
services remained healthy. The transcript, server manifest and capture
manifest live only in the repository's ignored private evidence tree. A later
launch reused the console's existing registered Chiaki entry without pairing,
reinitializing or refocusing the stream. Its 1280x720 direct window capture has
SHA-256 `e16b7fca59fad6ba3ff1aaf75364b6288aebfd8be40fe079566c5fbc335ecebd`
and visibly shows the flat-shaded map geometry. The title was again closed by
exact identity while Chiaki remained running.

## Gate 2 noclip contract

`make bsp-noclip-native-release` preserves the fixed-camera build as a
separate reproducible mode and adds the legacy ScePad ABI only to the noclip
artifact. The left stick moves forward/back and strafes, the right stick
controls yaw/pitch, and L2/R2 descend/ascend in the baker's Y-up coordinates.
Both sticks use a 12-count deadzone and movement is normalized before applying
a 320-unit-per-second speed.

The host regression runs 10,000 deterministic camera samples and checks input
continuity, finite transforms, accumulated distance and live MVP replacement
without modifying draw colors, indices or descriptors. The native gate is
stricter than merely surviving 10,000 frames: every frame must have a connected
DualSense sample with no read error, at least 600 frames must translate, at
least 120 must look, and accumulated movement must reach 100 units. GPU and
VideoOut ownership, periodic guards, final readback and zero renderer errors
remain mandatory. `BSP_NOCLIP_SOAK_COMPLETE` is emitted only after all of those
conditions pass; hardware evidence for this gate is not yet established.
