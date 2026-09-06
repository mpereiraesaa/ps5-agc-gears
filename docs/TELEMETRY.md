# Telemetry contract

Every hardware run uses the vendored `native/ps5log` client and structured TCP
protocol `ps5log/1`. The native title reads only `/app0/dev.conf`; the real
configuration is ignored and packaged locally from `dev.conf`. No console log
file, USB mount, `/download0` fallback or filesystem mirror is permitted.

## Required run identity

The title creates one monotonic boot token before AGC initialization. The same
token appears in the HELLO frame and `LOG_BOOT_MONOTONIC_NS`. A run is accepted
only when title, app, token and deployed artifact SHA-256 all match a new PC-side
manifest.

Required opening records:

```text
LOG_SCHEMA=3
LOG_TRANSPORT=ps5log/1 tcp structured
LOG_FS_SINKS=disabled
LOG_BOOT_MONOTONIC_NS=<same token as HELLO>
```

## GPU evidence

The final classifier must observe, for the exact requested frame count:

- render-target clear marker with `color_dma=false`;
- maximum frames in flight equal to two;
- one unique positive 48-bit flip token per frame;
- terminal GPU fence zero before slot reuse;
- matching VideoOut event token before backbuffer reuse;
- intact color/depth guard words;
- zero errors and nonzero consecutive-presentation interval;
- ordered unregister, event removal, VideoOut close, memory release and AGC
  unload;
- `BYE` with a gap-free final sequence.

No submit return value, silence, elapsed timeout or clean TCP close substitutes
for these ownership observations. If telemetry becomes incomplete after submit,
the process parks and retains resources.

The Phase 3 dynamic-lightmap gate additionally emits
`DYNAMIC_LIGHTMAP_READY`, four `DYNAMIC_LIGHTMAP_FRAME` samples,
`DYNAMIC_LIGHTMAP_READBACK` and `BSP_TEXTURE_PATH_LIGHTMAP_COMPLETE`. The
samples separate resident bytes, transient bytes, actual lightmap upload bytes,
the wider aligned acquire span and the cumulative upload total. Acceptance
requires deterministic A/B patch hashes, distinct final GPU buffer hashes,
stable bytes outside the patch, intact guards and six-allocation exact-token
reclamation.

The mip/sampler gate adds `MIP_CHAINS_READY`, four `MIP_SAMPLER_FRAME`
samples, `MIP_SAMPLER_READBACK` and `BSP_TEXTURE_PATH_MIP_COMPLETE`. The chain
record fixes layout order, pitch alignment, level range and aggregate bytes.
The frame samples expose the exact four S# DWORDs. Final trilinear and
anisotropic frames must use the same lightmap pattern and produce different
GPU-visible framebuffer hashes; both dynamic-lightmap slot hashes must remain
equal. The accepted run must also retain every Gate 1 ownership, guard, upload
and exact-token invariant.

The alpha-test gate adds `ALPHA_TEST_READY`, three `ALPHA_TEST_FRAME` samples,
`ALPHA_TEST_READBACK` and `BSP_TEXTURE_PATH_ALPHA_COMPLETE`. The ready record
fixes the `{` texture/draw partition, the selected camera target and the raw
opaque/alpha `DB_SHADER_CONTROL` values. Final control and alpha-test frames
must share the same camera, anisotropic sampler and dynamic-lightmap pattern,
while producing distinct GPU-visible framebuffer hashes. The accepted run must
also preserve all prior ownership, guard, upload and exact-token invariants.

The accounting gate replaces the ad-hoc aggregate with a platform-neutral,
checked-`uint64_t` ledger. `TEXTURE_RESIDENCY_READY` partitions all six
fence-retired allocations and separately identifies the base mip payload,
source lightmap and two live dynamic-lightmap images. The allocation partition
must equal the reported pool-resident total; the three texture payload classes
must equal the texture-resident subtotal without being double-counted into the
pool total.

Every frame is recorded in order before submit. The ledger rejects a skipped or
repeated frame, a changed transient footprint, a first/full lightmap upload
outside the first use of each slot, a changed bounded-patch size or any integer
overflow. Four `TEXTURE_UPLOAD_FRAME` bookends expose the exact component and
cumulative values. `TEXTURE_UPLOAD_SUMMARY` proves the complete 10,000-frame
sequence through component totals, min/max bytes, 2 full plus 9,998 bounded
updates and an order-sensitive FNV-64 digest. The strict validator recomputes
the closed-form totals and requires the final sampled digest to match both the
summary and `BSP_TEXTURE_PATH_ACCOUNTING_COMPLETE`.

This texture-only gate declares `input_dependency=none` and
`input_gate=not-repeated`. Controller connection and movement remain observed
telemetry, but are not success criteria because the input path did not change
after its earlier hardware gate. This keeps the accounting proof from silently
becoming another DualSense movement proof.

## Continuous-runtime closure

The production runtime uses one persistent frame state machine and emits a
heartbeat every 3,600 completed frames. It has no frame chunks or automatic
exit. When the user invokes PS5 **Close Game**, the system terminates the title
and the stream may end without BYE; the PC records that as an operator-closed
runtime session. Automated soaks use the exact same binary and let the
supervisor close the exact title after observing the requested continuous frame
target and healthy invariants. Historical finite runs remain reproducible
evidence, not production architecture.

Current frame records report average GPU/VideoOut waits plus average and maximum
intervals between consecutive completed presentations. The 17 ms budget count
is evaluated on that interval, not on prepare-to-retire latency. Older captures'
`deadline_misses` field used the latter and must not be interpreted as dropped
frames.

## Development workflow

Run the companion `ps5logd` server on the PC, copy `dev.conf.example` to the
ignored `dev.conf`, fill in the development PC IPv4 address and package it as
`/app0/dev.conf`. Never add that file to the repository or bake its address into
the executable.

Alternatively, keep the configuration elsewhere and pass it only to the
ignored native artifact:

```sh
PS5LOG_DEV_CONF=/absolute/private/dev.conf make native
```

The builder copies that file to `dist/PPSA99997/dev.conf`; it never copies it
into the source tree.
