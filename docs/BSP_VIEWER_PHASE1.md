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
| Lightmap atlas | Missing | First-style RGB light samples packed into a deterministic guttered RGBA8 atlas with normalized per-vertex UVs | Synthetic pixel/range regressions, C bundle validation and deterministic private-map bake |
| Base textures | Missing | Embedded BSP palettes decoded into separately pitched RGBA8 images plus one checked GFX1013 base/lightmap descriptor table per texture | Palette/fallback regressions, exact descriptor words, corrupt-view rejection and deterministic private-map bake |

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
remain mandatory. No present interval may exceed the telemetry budget.
`BSP_NOCLIP_SOAK_COMPLETE` is emitted only after all of those conditions pass.

Private evidence can be checked independently without adding the transcript or
map to the publication boundary:

```sh
python3 tools/validate_bsp_noclip_evidence.py /private/run.json \
  --bundle-sha256 "$PRIVATE_BUNDLE_SHA256" --bundle-bytes "$PRIVATE_BUNDLE_BYTES"
```

The validator fails closed on identity, protocol, transcript hash and size,
sequence continuity, heartbeat cadence, exact token bookends, controller
continuity, movement thresholds, frame budget, readback, guards and clean BYE.

## Gate 2 hardware evidence

The independently archived run
`20260905T202655848Z_PPSA99997_ps5-agc-gears_0x289eef5f0458` passed on GFX1013
firmware 12.02 from source commit
`6f19f8eee94ebca8cee2ccaebd66b3df6844c874`:

- signed `eboot.bin` SHA-256:
  `697940e2f8d6d0612309f9a8f873335581dfeb13ce9e57409b9b2b8c12243bee`;
- the same private Gate 1 bundle identity, with 10,000 completed frames and
  10,000 connected DualSense samples;
- 2,018 translating frames, 678 looking frames, 6,735.199 accumulated units
  and 4,011 input changes, with zero pad read errors;
- exact VideoOut token bookends, zero over-budget present intervals, intact
  guards and zero renderer errors;
- identical full-buffer readback `0e064ce046c6ebbb`, with 2,073,600 bright
  pixels in each buffer;
- clean `BSP_NOCLIP_SOAK_COMPLETE` BYE after 197 contiguous `ps5log/1`
  records; transcript SHA-256
  `2c8d0307d5de3d0be5350c6798e7f1376c954959b72ae2ff3c74250e1b749ba4`.

The accompanying 1280x720 Remote Play capture has SHA-256
`7549bd0f83d6d19c03b808f73045a206ca01f0736c4de7f7b01cd87534c9de2a`
and shows the camera at a visibly different map position. It reused the
console's existing registered Chiaki entry; no pairing or reinitialization was
performed. The title was closed by exact identity and all development services
remained healthy.

## Gate 3 lightmap atlas contract

The baker derives each face's lightmap extents from its texture axes using the
GoldSrc 16-unit luxel grid. It validates every referenced style span before
reading the lighting lump, consumes the first style, and packs lit faces in
face order into deterministic shelves. Every rectangle receives a duplicated
one-texel gutter so later bilinear filtering cannot bleed between faces.

`LMHD` records an RGBA8 atlas no larger than 2048x2048 with an exactly
`width * 4`, 256-byte-aligned row pitch. `LMPX` contains the pixels and each
vertex carries normalized texel-centre coordinates. Unlit faces retain the
explicit `UINT32_MAX` lightmap sentinel and point at the atlas's white fallback.
The C consumer rejects incomplete chunk pairs, invalid dimensions, formats,
row pitches, draw references and UVs outside `[0, 1]` before exposing the GPU
span.

The private reference map bakes reproducibly to 2,424,944 bytes with a 512x799
atlas containing 409,088 pixels. Two independent bakes produced SHA-256
`6a44083b22545c29c787a3e136c6fedda3ced08b7ec191db365e83ab612cd0f5`.
The map and generated bundle remain ignored private inputs. This gate
establishes atlas correctness only; native sampling is introduced after the
base-texture descriptor-table contract so the final shader can bind both
resources together.

## Gate 4 base-texture descriptor-table contract

The baker decodes mip level zero from each embedded GoldSrc `miptex` and its
256-entry RGB palette. Names beginning with `{` map palette index 255 to zero
alpha. WAD-only or absent texture payloads remain renderable through a
deterministic magenta checkerboard carrying an explicit fallback flag; the
baker never opens a WAD or adds commercial input to the public tree. Malformed
directories, overlapping mip chains, palettes, dimensions and aggregate sizes
fail before a bundle is written.

`TEXM` records one 32-byte image entry per original texture and `TEXP` stores
the corresponding RGBA8 rows. Every image begins at a 256-byte boundary and
every row pitch is a multiple of 256 bytes. Images remain separate rather than
being packed into an atlas, preserving GoldSrc's repeated base UVs. The C
consumer proves all spans, formats, pitches, flags, ordering and draw indices
before exposing them.

`bsp_texture_descriptor` then emits the exact 8-DWORD GFX10.3 linear image SRD
and 4-DWORD sampler SRD. Each 24-DWORD table contains binding 0 with repeat and
bilinear filtering for the base image, followed by binding 1 with clamp-to-last-
texel and bilinear filtering for the shared lightmap. The builder checks 48-bit
256-byte-aligned GPU addresses, GFX1013 dimensions and custom row pitch, sizes
the complete table before writing, and rejects forged bundle views. Generated
AMD headers are not published; the field positions are the narrow independently
reviewable subset of Mesa's MIT-licensed `ac_descriptors.c` and
`gfx10-rsrc.json` definitions.

The private reference map now bakes reproducibly to 7,210,496 bytes with 164
base textures occupying 4,780,032 pitched bytes and requiring 3,936 descriptor
DWORDs. Two independent bakes produced SHA-256
`33242fe611a896f8f26461f297e0f05f31c026a3ce0eab614d2e63f401a227d5`.
This gate establishes base-image decoding and descriptor-table construction;
the next gate consumes those tables in the native `base * lightmap` pipeline
and carries the changed command stream through the 60,000-frame soak.
