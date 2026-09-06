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
