# Architecture target

The standalone demo has five explicit layers:

1. **Native application shell:** title lifecycle, logging and exact teardown.
2. **Platform services:** direct memory, VideoOut and event handling.
3. **AGC renderer:** initialization, resource ownership, command composition,
   submission, fence and double-buffer presentation.
4. **Shader toolchain:** public `gfx1013` compilation and metadata translation.
5. **Observability:** sequenced TCP checkpoints, HELLO/BYE identity, per-run
   artifact/log hashes, immutable PC archives and a fail-closed guard. The
   renderer never writes log files in the console sandbox.

## Geometry path

The first Gears renderer uses expanded triangle lists, not an index buffer.
`src/gears_mesh.c` emits a 24-byte interleaved position/normal vertex compatible
with the hardware-proven Cube SRD. It adapts Mesa's MIT-licensed `es2gears.c`
topology: seven strips become 20 explicit triangles/60 vertices per tooth, so
a 20-tooth gear contains 1,200 vertices. This avoids strip-restart degenerates
and an unproven indexed-draw contract while preserving the reference profile.

The module compiles with the Prospero toolchain with only `sincosf` unresolved;
that symbol is present in the public SDK's `libSceLibcInternal` link stub. Mesh
generation remains CPU-side startup work and is never performed per frame.

`src/gears_scene.c` materializes the shader's exact 25-DWORD direct-user-data
contract for three draws: column-major MVP, unit quaternion, RGBA material and
the 32-bit SRD-table pointer. It uses a perspective camera, a common view tilt,
three distinct placements/colors and mechanically opposed angular rates. The
per-frame CPU work updates only these small inline blocks; mesh/SRD storage is
immutable until its terminal fence.

The three parameter sets, placements and phase offsets follow `es2gears.c`,
uniformly scaled to this camera: 20:10:10 teeth, `angle`, `-2*angle-9°` and
`-2*angle-25°`. Only units and colors differ from the reference demo.

## Frame ownership

`gears_frame_tracker` owns two independent slots and monotonically allocates
positive 48-bit flip tokens. A slot moves `EMPTY -> PREPARED -> SUBMITTED`;
only the prepared state may be cancelled. After submission, the slot becomes
reusable only when both the GPU fence and the exact VideoOut token complete.
Mismatched tokens, duplicate begin attempts and any attempt to cancel submitted
work fail closed. The animation runtime and hardware soaks exercise this state
machine directly.

`gears_draw_compose` is the single command-composition path for the three
objects. For each draw it requires the measured FW 12.02 cursor advances:
27 DWORD for 25 inline SH values and three DWORD for `DrawIndexAuto`. It accepts
exactly three pairs and verifies the aggregate 90 DWORD before returning
success. Capacity exhaustion or builder ABI drift therefore stops composition
before the caller can submit.

`gears_rt_clear` replaces the color-buffer DMA fill with an oversized triangle
at the far clip plane. It reuses the independently authored `gfx1013` pipeline,
emits one exact 27-DWORD parameter update plus one 3-DWORD non-indexed draw and
leaves depth clearing independent. The host test rejects insufficient command
capacity; the runtime marker explicitly records `color_dma=false`.

`gears_renderer` is now the public frame-command core. It composes the clear
and three gears as four ordered non-indexed draws and requires exactly 120
DWORD from the two builder callbacks. The native firmware adapter supplies only the
AGC SH-update and draw builders; scene policy and command ordering no longer
live in the application integration.

`gears_animation` joins the scene builder and ownership tracker without knowing
about AGC or VideoOut APIs. Given a monotonic nanosecond timestamp it selects
the alternating buffer, allocates the next exact token and produces all three
draw parameter blocks. Submission and the two completions are explicit calls,
so the platform backend cannot accidentally infer ownership from elapsed time.
The host soak covers 10,000 sequential frames, crossed-token rejection and
pre-submit cancellation.

The standalone FW 12.02 title has exercised this contract through a
10,000-frame hardware soak with intact guards and clean teardown. It keeps two
slots in flight using independent command buffers and fences, and retires each
slot only after its fence and exact VideoOut token complete before reuse.

`gears_telemetry` aggregates compose, GPU-wait, VideoOut-wait and consecutive
presentation-retirement intervals in memory, retaining averages and maxima
plus interval-over-budget and error counters. Sampling is
requested for frame zero, every 60 frames, every error and the final summary;
the backend must not format or `fsync` one line per frame. Ownership transitions
and errors remain immediate persistent checkpoints.

`gears_frame_runner` is a persistent `init / step / drain` orchestration state
machine: prepare, compose, submit, GPU fence, exact VideoOut token and
telemetry. The production runtime calls `step` indefinitely and never calls
`drain`; host tests call `drain` directly when they need a bounded assertion.
Composition failures are cancel-safe; ownership is marked submitted before
entering the submit callback, so a submit/wait/token failure returns
`POST_SUBMIT_RETAIN` with the exact token that must remain alive. Host tests
cover 10,000 stepped-and-drained frames plus a separate 10,001-step continuous
lifecycle. A fully retired slot is cleared before fallible telemetry accounting;
an accounting overflow enters an explicit telemetry-failure state without
reviving ownership already completed by GPU and VideoOut.

`ps5_surface`, `ps5_present` and `ps5_frame_completion` are the first extracted
backend units. They respectively plan the two registered display surfaces,
enforce SetFlip-before-release-fence command order, and retain submitted
resources until both exact completion signals arrive. All three are pure C,
have synthetic host regressions and contain no loader or proprietary SDK code.

`ps5_event_adapter` injects the two native event functions rather than creating
hidden link dependencies, while `ps5_submission` injects the submit and SetFlip
builders. Once either operation crosses its irreversible boundary, the public
state retains all resources on every error. `ps5_gpu_span` rejects partial,
empty or wrapping mappings before a register table or geometry address can be
submitted.

`ps5_agc_writer` is the checked boundary around AGC's mutable command-buffer
ABI. It constructs the exact 0x38-byte writer, refuses empty or overflowing
streams, commits the caller cursor only after a valid builder advance, enforces
the measured 27-DWORD direct-parameter and three-DWORD draw packets, and checks
that indirect tables and depth-fill destinations lie wholly inside a declared
GPU mapping. The wait-rendering adapter likewise commits its cursor only after
the driver reports success and advances by exactly its advertised packet size.
DMA is deliberately exposed only as `fill_depth`; MRT color clearing remains a
draw through `gears_rt_clear` and cannot regress to a DMA shortcut accidentally.
`ps5_agc_submit` then verifies the used command span against its GPU mapping,
flushes exactly that byte range and constructs the zero-flag 0x10-byte DCB
descriptor. `native/ps5_agc_native.c` is the only binding to firmware symbols;
it compiles cleanly for `x86_64-sie-ps5` and contains no renderer policy.

`native/main.c` is the final orchestration boundary. It owns two command slots,
two independent fences and two per-buffer MRT pipelines; initializes VideoOut,
direct memory and AGC; composes wait/depth clear/pipeline/four draws/present;
and releases resources only after the exact GPU and VideoOut completions. The
standalone hardware evidence therefore exercises these public units rather than
an unreleased laboratory adapter.

## Required invariants

- All GPU-referenced storage remains alive until its completion fence.
- A zero submit return is not success without observable effect and completion.
- CX, UC and SH indirect packets use firmware builders and their measured
  five-DWORD cursor contract; they are not manually synthesized.
- Shader symbol offsets and sizes come from the PAL ELF symbol table.
- Cleanup is ordered and fail-closed when ownership is ambiguous.
- Every hardware iteration produces a manifest, including launch failures; a
  fresh app boot token is required before a log is attributed to a new run.
- The application never reads or modifies another process.

## Publication boundary

The complete demo is now standalone. Its project-owned interfaces, provenance,
tests and public dependencies are recorded in `BACKEND_PROVENANCE.md`. The
private laboratory is evidence only and is not a build input. Native service
loading, direct-memory lifecycle, platform ABI, AGC symbol stubs, shader
compilation, checked command writing, VideoOut/AGC orchestration, telemetry,
entry point and title packaging all live in this repository.

`ps5_shader_header` receives a metadata object rather than including a private
generated header. It creates the self-relative input arena required by the
firmware shader constructor. `native/shader_assets.S` embeds only stages built
from the checked-in `.pipe`; no precompiled shader enters source control.
