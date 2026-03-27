# Orbis validation review

**Last updated:** 2026-03-26
**Branch:** `orbis-build-cleanup-pass1`

---

## What was reviewed

- `Makefile.orbis` — build system, toolchain detection, PKG pipeline
- `frontend/drivers/platform_orbis.c` — init, dirs, mem stats, exec
- `gfx/drivers_context/orbis_ctx.c` — EGL/Piglet context driver
- `gfx/common/orbis_common.h` — PGL config, memory constants
- `input/drivers_joypad/ps4_joypad.c` — DualShock 4 driver
- `audio/drivers/orbis_audio.c` — orbisAudio driver
- `managers/state_manager.c` — SSE2 guard
- `libretro-common/vfs/vfs_implementation.c` — ORBIS VFS layer
- `cores/dynamic_dummy.c` — static core stub
- `griffin/griffin.c` — single-TU build includes

---

## Build results (2026-03-26)

First successful compile pass on OpenOrbis v0.5.4 / LLVM clang 21 / Windows host.

| Profile | Command | Result |
| ------- | ------- | ------ |
| `lite` | `make -f Makefile.orbis lite` | **Clean** — 2.97 MB ELF |
| `full` (no audio) | `make -f Makefile.orbis full ORBIS_ENABLE_AUDIO=0` | **Clean** — 3.0 MB ELF |
| `full` (with audio) | `make -f Makefile.orbis full` | **Blocked** — `liborbisAudio` not installed |

---

## Findings

### 1. Profile model is coherent

The `dev` / `full` / `lite` split is meaningful and is working as intended. Each profile
produces a different binary size and link footprint.

### 2. Toolchain detection is correct

The makefile correctly handles all three env var names (`ORBISDEV`, `PS4SDK`,
`OO_PS4_TOOLCHAIN`) and auto-detects modern vs legacy SDK layouts, C++ include root,
linker script, and CRT file location.

### 3. Audio requires an external library

`ORBIS_ENABLE_AUDIO=1` depends on `liborbisAudio`, a separate homebrew library not
bundled in the OpenOrbis SDK. The `full` and `dev` profiles default to audio-on and will
fail to link until `liborbisAudio` is installed. The `lite` profile disables audio and
builds cleanly with no external dependencies.

### 4. PKG pipeline is wired but untested end-to-end

The makefile has `fself`, `sfo`, `pkg`, `pkg-full`, `pkg-lite` targets. The `dist/pkg_orbis/`
directory contains a GP4 project file and a pre-built `param.sfo`. The pipeline has not
been run end-to-end in this validation pass — `create-fself` and `create-pkg` were not
exercised. PKG packaging needs a separate test pass with the full toolchain.

### 5. Several bugs fixed during this pass

The following items from the earlier technical debt audit were resolved before or during
this compile pass. See [TECHNICAL_DEBT.md](TECHNICAL_DEBT.md) for the full fix table.

**From earlier audit (resolved in prior commits):**

- Inverted Piglet return check — EGL context was never initializing
- `num_players` race condition — mutex now guards all reads/writes
- Array OOB on `ds_joypad_states` — bounds check added
- `nx_ctx_ptr` copy-paste name — renamed to `orbis_ctx_ptr`
- `rumble` struct uninitialized — zeroed after `scePadOpen`

**Found during first compile pass:**

- `cheevos-new/` → `cheevos/` rename not propagated to 12 source files
- `-lorbisAudio` linker flag missing from `PS4_LIBS`
- Wrong `sys/fcntl.h` / `sys/dirent.h` includes in VFS layer
- SSE2 guard missing `__has_include` — clang rejected on PS4 target
- `retro_get_system_av_info` called itself recursively in dynamic_dummy
- `griffin.c` include paths pointed to defunct `core/` subdirectory
- `xmb.c` called `sleep()` without `<unistd.h>` under `#ifdef ORBIS`
- `<7zip/7z.h>` not found — `-Ideps` missing from include path
- `core/` subdirectory move broke all relative `../header.h` includes — reverted

### 6. Remaining open issues

See [TECHNICAL_DEBT.md](TECHNICAL_DEBT.md) for the current open-item list. The highest
priority remaining items are:

- `check-pkg-tools` in `Makefile.orbis` does not hard-fail when tools are absent
- `audio/drivers/orbis_audio.c` discards `orbisAudioInit` error code
- Audio write path has no accumulation buffer for sub-block-size calls

---

## Recommended next steps

1. **Install `liborbisAudio`** and run a clean `full` build to confirm the audio path links.
2. **Run the PKG pipeline end-to-end:** `make pkg-lite` with `create-fself` and `create-pkg`
   available, then verify the resulting `.pkg` installs and boots on hardware.
3. **Add a pkg tool guard:** change `check-pkg-tools` to emit `$(error ...)` rather than
   a warning so missing tools fail at configure time.
4. **Test `dev` profile:** no build test has been done for the dev profile yet.
5. **Test keyboard/mouse toggles:** treat these as experimental until a real build pass
   with the required SDK stubs confirms they link correctly.
6. **Hardware boot test:** the ELF has never been booted on real hardware. Any runtime
   issues (EGL surface, audio, file access) will only surface there.

---

## SDK / toolchain used in this pass

- **SDK:** OpenOrbis v0.5.4 (`OO_PS4_TOOLCHAIN=D:/OpenOrbis_v0.5.4/OpenOrbis/PS4Toolchain`)
- **SDK layout:** legacy (`include/`, `lib/`)
- **Compiler:** LLVM clang 21.1.0 (system install, `x86_64-scei-ps4-elf` target)
- **Linker:** `ld.lld` (bundled with LLVM)
- **Host:** Windows 11, bash via Git for Windows
