# AGC capability and import matrix

This document separates missing public imports from missing renderer contracts.
Most remaining work does not require discovering a new NID.

## Hardware-proven core

| Area | Imports already exercised on FW 12.02 |
| --- | --- |
| AGC lifecycle | `sceAgcInit`, `sceAgcGetRegisterDefaults`, `sceAgcCreateShader`, `sceAgcLinkShaders` |
| Command construction | `sceAgcDcbSetCxRegistersIndirect`, `sceAgcDcbSetUcRegistersIndirect`, `sceAgcDcbSetShRegistersIndirect`, `sceAgcDcbDrawIndexAuto`, `sceAgcDcbSetFlip` |
| Submission/ownership | `sceAgcDriverGetWaitRenderingPacketSizeInDwords`, `sceAgcDriverWaitUntilSafeForRendering`, `sceAgcDriverSubmitDcb` |
| VideoOut | open, buffer attributes/registration, flip event/data, unregister and close |
| Memory/events | Main Direct Memory, reserve, map/BatchMap, unmap/release and equeue primitives |

## Capability progression

| Stage | New import required? | Missing contract |
| --- | --- | --- |
| Animated triangle | No | Per-frame command reset, monotonic flip IDs and double-buffer ownership |
| Plasma | No | Time/resolution user data and a stable frame loop |
| Procedural cube | Probably no | Matrix user data, perspective, culling and optionally depth |
| Non-indexed gear | Probably no | Vertex/normal buffer descriptors or procedural vertex fetch |
| Indexed gear | Possibly | A validated indexed-draw builder; avoid it initially by expanding indices |
| Correct overlapping gears | No known new import | Depth allocation, layout, descriptor/register block, clear and barriers |
| Textured extensions | Useful new builder likely | Texture/sampler descriptors, resource residency and cache transitions |

## Candidate optional imports

- `sceAgcCbSetShRegisterRangeDirect`: public-reference code uses it for resource
  descriptors. It is convenient for dynamic uniforms/buffers but not required
  for Plasma because the existing SH-indirect path can update user registers.
- `sceAgcSuspendPoint`: present in a public reference after submit. It is not
  required by our proven fence/event completion path and should remain optional.
- An indexed-draw builder: useful for compact gear meshes, but not a gate; a
  first implementation can expand the index list and retain
  `sceAgcDcbDrawIndexAuto`.
- `sceVideoOutGetFlipStatus`, `sceVideoOutWaitVblank` or an equivalent pacing
  query: potentially useful for telemetry/pacing, but the existing exact flip
  event is sufficient for correctness.

## Reverse-engineering priorities

1. **No Ghidra work for Plasma.** Use the already proven imports and derive the
   time uniform from public compiler metadata.
2. Validate how LLPC maps one small constant/user-data block into SH registers;
   prefer compiler metadata and host disassembly before firmware analysis.
3. For buffered geometry, recover only the descriptor words and user-SGPR
   binding contract needed by our own shader. Cross-check public PAL/LLVM and
   `sceAgcCbSetShRegisterRangeDirect` callers.
4. Investigate depth only after Cube renders without it: defaults selector,
   `DB_Z_*`/`DB_DEPTH_*` block, layout/alignment and safe clear/barrier sequence.
5. Resolve an indexed-draw API only after non-indexed Gears works.

Firmware analysis should record symbol/NID, module hash, calling convention,
arguments, cursor advance, side effects and confidence. No proprietary bytes or
substantial decompilation enter this repository.
