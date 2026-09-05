# Notices

## Native application foundation

The native application shell derives from
`blackbearreloaded/ps5-native-app-boilerplate`. The currently validated tree is
Manuel Pereira's public fork at commit
`37dd53602bdead63936f718004555ba10154be48`; it contains upstream commit
`722f2227a8bb6fa2229120546995b6562552c752` plus the separately documented
RELRO load-segment congruence fix. Original boilerplate code is Copyright (C)
2026 BlackBearReloaded and GPL-3.0-or-later. The foundation is fetched into an
ignored dependency directory and verified at the pinned revision; it is not
vendored into this repository.

## Project icon

The PS5 AGC Gears icon was generated specifically for this project with
OpenAI's built-in image-generation tool on 2026-09-05. The prompt requested
three original interlocking 3D gears, emerald/cyan illumination, a deep violet
background, strong small-icon readability, no text and no trademarks. It does
not derive from the boilerplate presentation assets.

- Master SHA-256:
  `50accc91e38822a8b11cb6eed916d968184edb8306a1099fc3d2aa0a72b402b0`
- PS5 512×512 RGB derivative SHA-256:
  `cc40f50deb429e8bcf07eb43be5a3176c4f8445a88e045e830b066202b66efb8`

## Mesa es2gears geometry

`src/gears_mesh.c` adapts the gear construction from Mesa demos
`src/egl/opengles2/es2gears.c` at revision
`649baedafcb90313ade69909fdef1ee156ab5f8d`. The original is Copyright
(C) 1999-2001 Brian Paul, with the GLES2 port/refactor credited to Kristian
Høgsberg and Alexandros Frantzis, and is licensed under the MIT License. This
project expands its seven triangle strips per tooth into non-indexed triangle
lists suitable for the AGC backend; it does not include EGL or OpenGL code.

## Gears shader

`shaders/gears_lit.pipe` is independently authored for this project. It is
distributed as source only; generated PAL ELF files and extracted ISA binaries
are build artifacts and are intentionally excluded from publication.

## ps5log client

`native/ps5log` is the project-owned client extracted from the local
`logging_server` project. The four source files are retained byte-for-byte and
covered by their original host test, with only public test addresses substituted
in the copied test fixture. They are distributed under this repository's
GPL-3.0-or-later license.

## Research boundary

No proprietary Sony SDK file, game asset, shader, module, dump or command
buffer is included. Hardware observations are reported only as sanitized facts.
