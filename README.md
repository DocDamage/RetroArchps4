# RetroArch — PS4 / Orbis fork

This is a PS4-focused fork of RetroArch targeting jailbroken PS4 consoles via the
[OpenOrbis toolchain](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain).

## Quick start (PS4 build)

### Requirements

- OpenOrbis PS4 Toolchain (`OO_PS4_TOOLCHAIN`, `ORBISDEV`, or `PS4SDK` in your environment)
- LLVM `clang` with `ld.lld` on `PATH`
- GNU `make` (or `mingw32-make` on Windows)
- Python 3 (for `param.sfo` generation)
- OpenOrbis `create-fself` and `create-pkg` (for PKG output only)

### Build

```bash
# Minimal build — no external dependencies
make -f Makefile.orbis lite

# Full release build (requires liborbisAudio for audio support)
make -f Makefile.orbis full ORBIS_ENABLE_AUDIO=0

# Package as installable PS4 PKG
make -f Makefile.orbis pkg-lite
```

See [docs/ps4_orbis_build_profiles.md](docs/ps4_orbis_build_profiles.md) for all profiles,
knobs, and the PKG packaging pipeline.

## Repository layout

```text
Makefile.orbis               PS4 build system (only Makefile needed for PS4)
audio/drivers/orbis_audio.c  orbisAudio driver (48 kHz stereo s16)
frontend/drivers/platform_orbis.c  PS4 frontend (init, dirs, mem stats)
gfx/drivers_context/orbis_ctx.c    EGL/Piglet context driver
gfx/common/orbis_common.h          PGL config and type definitions
input/drivers_joypad/ps4_joypad.c  DualShock 4 driver (scePad)
input/drivers/ps4_input.c          PS4 input driver wrapper
core/orbisFile.h                   PS4-specific file I/O header
dist/makefiles/Makefile.common     Shared OBJ wiring for Orbis drivers
dist/pkg_orbis/                    PKG project template (GP4 + param.sfo)
dist/pkg/make_sfo.py               param.sfo generator script
docs/ps4_orbis_build_profiles.md   Build profiles and knobs reference
docs/orbis_validation_review.md    Build validation log
docs/orbis_validation_toolchain.md Validation toolchain usage
docs/TECHNICAL_DEBT.md             Known issues and fix status
```

## Build status

| Profile | Status |
| ------- | ------ |
| `lite` | Clean (2.97 MB ELF) |
| `full` with `ORBIS_ENABLE_AUDIO=0` | Clean (3.0 MB ELF) |
| `full` with audio | Requires `liborbisAudio` — see [audio note](docs/ps4_orbis_build_profiles.md#audio-note) |

Tested on: OpenOrbis v0.5.4 · clang 21 · Windows 11 host.

## Known issues

See [docs/TECHNICAL_DEBT.md](docs/TECHNICAL_DEBT.md) for the full audit. Critical bugs have
been fixed; the main open items are in the audio accumulation buffer and PKG tool error handling.

## PS4 driver overview

### Graphics

`orbis_ctx` initialises Piglet (PS4's OpenGL ES 2 implementation) via `scePigletSetConfigurationVSH`
and creates an EGL window surface at 1920×1080. The context is registered as the `orbis` graphics
context driver in RetroArch's driver table.

### Audio

`audio_orbis` wraps the `orbisAudio` homebrew library. It opens a port at 48 kHz stereo s16,
converts RetroArch's float samples on the fly, and pushes 512-sample blocks. Disabled in the
`lite` profile.

### Input

`ps4_joypad` enumerates logged-in users via `sceUserService`, opens a pad handle per user via
`scePad`, and maps DualShock 4 buttons to RetroArch's joypad abstraction. Rumble is supported
via `scePadSetVibration`. Thread-safe with a `slock_t` mutex.

### Frontend

`platform_orbis` provides directory paths rooted at `host0:app/data/retroarch/`, memory
statistics via `sceKernelGetDirectMemorySize` and `/proc/self/statm` (jailbroken), and an
optional core-exec path via `sceAppMgrLoadExec` when `ORBIS_ENABLE_APPEXEC=1`.

---

## Original RetroArch

RetroArch is the reference frontend for the [libretro](http://libretro.com) API.
For upstream documentation, general configuration, and platform support beyond PS4 see:

- [Libretro Documentation Center](https://docs.libretro.com/)
- [Upstream RetroArch repository](https://github.com/libretro/RetroArch)
