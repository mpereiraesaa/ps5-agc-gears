# PS5 AGC Gears

Clean-room, source-reproducible, hardware-accelerated graphics demo for native
PlayStation 5 homebrew. It renders a continuous three-gear scene through AGC
using independently authored `gfx1013` shaders.

> **Development status:** this repository builds a complete native title
> from independently authored source plus a pinned public foundation. The exact
> standalone artifact has rendered on FW 12.02 and passed short strict,
> 10,000-frame and 60,000-frame hardware soaks. Host CI, deterministic release
> packaging and a fail-closed publication audit are included.

## Demonstrated milestone

On a PS5 running firmware 12.02, an artifact built entirely by this standalone
repository rendered the animated three-gear scene for 10,000 frames:

| Property | Confirmed result |
| --- | --- |
| GPU target | `gfx1013` |
| Pipeline | 84 CX / 12 SH / 3 UC register pairs |
| Scheduling | Two frames genuinely in flight |
| Draws | Fullscreen RT clear plus three gears per frame |
| Color clear | Pipeline draw; color DMA compiled out |
| Completion | Submit success, GPU fence zero, exact VideoOut event |
| Soak | 10,000/10,000, zero errors, 59.9407 fps |
| Lifecycle | Color/depth guards intact and complete teardown |

This is evidence for one tested console and firmware, not a universal
compatibility claim.

The strict finite reference is the uninterrupted 60,000-frame run
`20260905T144120445Z_PPSA99997_ps5-agc-gears_0x15c32a4befaa`: its opening
identity, ownership markers, zero renderer errors and gap-free teardown all
passed the documented `ps5log/1` contract. It predates the continuous runtime.
The immediately preceding continuous build has separate hardware evidence up
to 25,560 completed frames with healthy heartbeats and operator closure. The
exact current commit is host/native-build validated and awaits its hardware
revalidation; see `docs/HARDWARE_VALIDATION.md`.

## Intended contents

- Minimal native application derived from the public PS5 native app boilerplate.
- Independently authored GLSL pipeline compiled for `gfx1013` with public tools.
- Small AGC renderer covering initialization, buffers, pipeline, draw, flip,
  synchronization and teardown.
- Deterministic non-indexed gear meshes with interleaved position/normal
  vertices, avoiding an indexed-draw dependency in the first release.
- A fullscreen-triangle render-target clear, avoiding color-buffer DMA in the
  current animated path.
- Pure backend contracts for two display surfaces, SetFlip/fence composition
  and exact GPU/VideoOut completion ownership.
- Host tests for command composition and shader metadata translation.
- Structured TCP run logs, immutable manifests and fail-closed cleanup.
- Reproducible build, artifact inspection and release hashes.
- Documentation for deployment through a compatible homebrew loader.

The public capability path is intentionally incremental:

```text
Triangle -> Plasma -> Cube -> Gears
```

## Non-goals

- No proprietary Sony SDK files, runtime modules or headers.
- No game dumps, shaders, command buffers or reverse-engineered assets.
- No jailbreak, kernel exploit or payload delivery implementation.
- No promise of compatibility beyond firmware actually tested.

## Host quick start

The current host validation workflow is:

```sh
make test
make audit
```

## Continuous runtime

`make native` and its alias `make native-release` build the same production
loop. It has no frame limit, chunk boundary or automatic exit and remains open
until the user selects **Close Game**. Soak duration is a supervisor policy over
the same binary and its telemetry; it never changes renderer code or compile
definitions. See `docs/RELEASING.md` for lifecycle and packaging details.

Compile the independently authored pipeline with a public LLPC build that
contains GFX1013 support:

```sh
make shaders AMDLLPC=/path/to/amdllpc LLVM_READELF=/path/to/llvm-readelf
```

The command always passes `-gfxip=10.1.3`, rejects relocation sections,
extracts GS/PS code by PAL symbol extent and writes a SHA-256 manifest under
`build/shaders/`. It then derives `build/generated/gears_shader_metadata.h`
directly from the PAL notes. Generated ELF, ISA and headers are disposable
artifacts and are not part of the publication allowlist.

Observability is part of the renderer contract: every hardware run must carry
a fresh application boot token, artifact hash, ordered GPU/VideoOut completion
markers and an archived manifest. Silence and submit return values alone never
authorize resource reuse or process cleanup.

The complete runtime telemetry contract is documented in
`docs/TELEMETRY.md`. Local network configuration belongs only in ignored
`dev.conf`, created from `dev.conf.example` during development.

Build a complete ignored title directory under `dist/PPSA99997/` with:

```sh
PS5LOG_DEV_CONF=/absolute/private/dev.conf make native \
  AMDLLPC=/path/to/amdllpc LLVM_READELF=/path/to/llvm-readelf
```

The builder verifies the pinned native-foundation revision, compiles only for
`gfx1013`, generates shader metadata, links/signs the native executable and
packages the title. An `eboot.bin` alone is not a deployable title. Deployment
remains loader-specific and is deliberately outside this repository.

See `docs/BACKEND_PROVENANCE.md` for the clean-room boundary and
`docs/HARDWARE_VALIDATION.md` for exact hardware evidence and limitations.
Future renderer work is isolated with Git worktrees instead of copied numbered
stages; see `docs/DEVELOPMENT.md`.

## Safety and publication status

Run the publication audit before creating a commit or archive:

```sh
python3 tools/audit_publication.py
```

The audit uses `PUBLICATION_ALLOWLIST.txt` and rejects unexpected files,
symlinks, unapproved binaries, private-laboratory terms, absolute workstation
paths and hard-coded local network addresses. Generated `.deps`, `build`,
`dist` and `release` trees are omitted only when Git confirms they are ignored. The two
original project icons are accepted only by their pinned SHA-256 digests.

## Application identity

The local development identity is `PPSA99997`, concept `99997`, with the title
**AGC Gears**. It is a project-local development identifier, not an official
Sony assignment; it must be reviewed before public distribution.

## Credits

The native application packaging and repository quality target are inspired by
[BlackBearReloaded's PS5 Native App Boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate),
whose author is a pioneer of practical native PS5 homebrew. AGC presentation
research in ProsperoTV also informed the public API lifecycle. All demo shaders
and runtime integration intended for this repository will remain independently
authored and source-reproducible.

## License

GPL-3.0-or-later. This is compatible with the boilerplate from which the native
application shell is derived. Dependency licenses and required notices
remain subject to the release audit.

The public repository is
[`mpereiraesaa/ps5-agc-gears`](https://github.com/mpereiraesaa/ps5-agc-gears).
Its `main` branch accepts changes only through pull requests.
