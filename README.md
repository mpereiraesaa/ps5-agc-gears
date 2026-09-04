# PS5 AGC Gears

Clean-room, source-reproducible graphics demo for native PlayStation 5
homebrew. The project is being prepared to demonstrate a small hardware-
accelerated scene through AGC using independently authored `gfx1013` shaders.

> **Early development status:** this is an independent local repository, not
> yet a buildable release. The hardware proof exists in the private
> laboratory; reusable code is being extracted only after provenance and
> dependency audits.

## Demonstrated milestone

On a PS5 running firmware 12.02, the laboratory build presented a centered
green triangle over a purple background:

| Property | Confirmed result |
| --- | --- |
| GPU target | `gfx1013` |
| Pipeline | 84 CX / 12 SH / 3 UC register pairs |
| Command buffer | 122 DWORD |
| Draw | `DrawIndexAuto(3)` |
| Completion | Submit success, GPU fence zero, exact VideoOut event |
| CPU verification | Exactly 285,120 render-target words changed |
| Lifecycle | Guards intact and complete teardown |
| Visual verification | Centered green triangle confirmed by the operator |

This is evidence for one tested console and firmware, not a universal
compatibility claim.

## Intended contents

- Minimal native application derived from the public PS5 native app boilerplate.
- Independently authored GLSL pipeline compiled for `gfx1013` with public tools.
- Small AGC renderer covering initialization, buffers, pipeline, draw, flip,
  synchronization and teardown.
- Host tests for command composition and shader metadata translation.
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

## Planned quick start

The final repository should converge on a workflow similar to:

```sh
make doctor
make test
make
```

It will produce a complete title directory under `dist/<TITLE_ID>/`; an
`eboot.bin` alone is not a deployable title. Exact prerequisites and deployment
steps will be documented only after the standalone extraction passes CI.

## Safety and publication status

Run the publication audit before creating a commit or archive:

```sh
python3 tools/audit_publication.py
```

The audit uses `PUBLICATION_ALLOWLIST.txt` and rejects unexpected files,
symlinks, unapproved binaries, private-laboratory terms, absolute workstation
paths and hard-coded local network addresses. The two original project icons
are accepted only by their pinned SHA-256 digests.

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
application shell will be derived. Dependency licenses and required notices
remain subject to the release audit.

The intended future GitHub home is under
[`mpereiraesaa`](https://github.com/mpereiraesaa). No remote repository has
been created or published yet.
