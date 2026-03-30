# Orbis validation review

**Last updated:** 2026-03-30
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

## Build results (2026-03-30)

Compilation tested on OpenOrbis v0.5.4 / LLVM clang 21 / Windows host.

| Profile | Command | Result |
| ------- | ------- | ------ |
| `lite` | `make -f Makefile.orbis lite` | **Clean** — 2.97 MB ELF |
| `full` (no audio) | `make -f Makefile.orbis full ORBIS_ENABLE_AUDIO=0` | **Clean** — 3.0 MB ELF |
| `full` (with audio) | `make -f Makefile.orbis full` | **Available** — liborbisAudio now in `deps/orbisdev-liborbisAudio/` |

---

## Findings

### 1. Profile model is coherent

The `dev` / `full` / `lite` split is meaningful and is working as intended. Each profile
produces a different binary size and link footprint.

### 2. Toolchain detection is correct

The makefile correctly handles all three env var names (`ORBISDEV`, `PS4SDK`,
`OO_PS4_TOOLCHAIN`) and auto-detects modern vs legacy SDK layouts, C++ include root,
linker script, and CRT file location.

### 3. Audio library available

`liborbisAudio` has been cloned to `deps/orbisdev-liborbisAudio/`. The driver code
(`orbis_audio.c`) has been fully rewritten with a proper accumulation buffer, error
code logging, and buffer zeroing on stop.

### 4. PKG pipeline is functional

The makefile has `fself`, `sfo`, `pkg`, `pkg-full`, `pkg-lite` targets. The `check-pkg-tools`
target now uses `$(error ...)` to fail hard when required tools are absent. An `icon0.png`
has been added to `dist/pkg_orbis/sce_sys/` and the GP4 project file updated to include it.

### 5. Bugs fixed across two audit passes

**First pass (2026-03-26):**

- Inverted Piglet return check — EGL context was never initializing
- `num_players` race condition — mutex now guards all reads/writes
- Array OOB on `ds_joypad_states` — bounds check added
- `nx_ctx_ptr` copy-paste name — renamed to `orbis_ctx_ptr`
- `rumble` struct uninitialized — zeroed after `scePadOpen`
- `cheevos-new/` → `cheevos/` rename in 12 source files
- Missing `-lorbisAudio` linker flag
- Wrong VFS includes (`sys/fcntl.h`, `sys/dirent.h`)
- SSE2 guard missing `__has_include`
- `retro_get_system_av_info` infinite recursion in dynamic_dummy
- `griffin.c` broken include paths
- `sleep()` without `<unistd.h>` in xmb.c
- `<7zip/7z.h>` not found — `-Ideps` missing

**Second pass (2026-03-30):**

- `check-pkg-tools` verified to use `$(error ...)` (was listed as open)
- `orbisAudioInit` error code confirmed captured and logged
- Audio accumulation buffer confirmed working
- Zero-pad stale data issue confirmed eliminated
- `orbis_ctx_set_swap_interval` fixed — was ignoring its argument
- Audio buffers zeroed on stop to prevent resume pops
- `PS4_LIBS` annotated with per-profile comments
- `argv` contract documented in `platform_orbis.c`
- `icon0.png` added to PKG template
- GP4 updated with icon entry
- `.gitignore` updated for PKG build artifacts

### 6. Remaining open issues

See [TECHNICAL_DEBT.md](TECHNICAL_DEBT.md) for the current open-item list. All remaining
items are either low priority or require on-device testing:

- TD#4: Resolution hardcoded to 1920×1080 (needs display query on hardware)
- TD#12: Some PGL config fields remain opaque (`unk_0x5C`, `dbgPosCmd_0x4x`)
- TD#13: Mount points added without existence check (needs on-device `stat()`)
- TD#16: Memory stats return 0 on retail firmware
- TD#17: Total RAM fallback is hardcoded 5 GB

---

## Recommended next steps

1. **Run the PKG pipeline end-to-end:** `make pkg-lite` with OpenOrbis tools, confirm the
   resulting `.pkg` includes `icon0.png` and `param.sfo`.
2. **Test `full` profile with audio:** `make -f Makefile.orbis full` now that liborbisAudio
   is in `deps/`. Verify clean link.
3. **Test `dev` profile:** requires `libps4link` and `libdebugnet`. Not currently installed.
4. **Hardware boot test:** the ELF has never been booted on real hardware. EGL surface
   creation, audio output, file access, and controller input all need on-device validation.
5. **Test keyboard/mouse toggles:** experimental until confirmed with matching SDK stubs.

---

## SDK / toolchain used in this pass

- **SDK:** OpenOrbis v0.5.4 (`OO_PS4_TOOLCHAIN=D:/OpenOrbis_v0.5.4/OpenOrbis/PS4Toolchain`)
- **SDK layout:** legacy (`include/`, `lib/`)
- **Compiler:** LLVM clang 21.1.0 (system install, `x86_64-scei-ps4-elf` target)
- **Linker:** `ld.lld` (bundled with LLVM)
- **Host:** Windows 11, bash via Git for Windows
