# Technical Debt Audit — RetroArch Orbis/PS4 Port

**Branch:** `orbis-build-cleanup-pass1`
**Last updated:** 2026-03-30
**Scope:** Orbis-specific files only

---

## Fixed since initial audit

The following items from the original audit have been resolved:

| # | Issue | Fix | Commit |
| --- | ------- | ----- | -------- |
| 1 | Inverted Piglet return check — context always failed | `if (!ret)` → `if (ret != 0)` | `29d4e58` |
| 2 | Race condition on `num_players` | `slock_t` mutex guards all reads/writes | `cd4edcb` |
| 3 | Array OOB — write to `ds_joypad_states` without bounds check | `if (num_players >= PS4_MAX_ORBISPADS) break` added before increment | `cd4edcb` |
| 8 | Poll condition always-true (`if (index == num_players)`) | Rewritten with `user_already_registered` boolean flag | `cd4edcb` |
| 14 | Global context pointer named `nx_ctx_ptr` (copy-paste from NX) | Renamed to `orbis_ctx_ptr` | `29d4e58` |
| 15 | `rumble` struct not zero-initialized | `memset(...rumble, 0, ...)` added after `scePadOpen` | `cd4edcb` |

Additionally, the following issues were found and fixed during the first compile pass (2026-03-26):

| Issue | File | Fix | Commit |
| ------- | ------ | ----- | -------- |
| `cheevos-new/` references not updated after dir rename | 12 source files | s/cheevos-new/cheevos/ | `54b12ca` |
| Missing `-lorbisAudio` linker flag | `Makefile.orbis` | Added to `PS4_LIBS` when `ORBIS_ENABLE_AUDIO=1` | `3387d2b` |
| Wrong `sys/fcntl.h`, `sys/dirent.h` paths on ORBIS | `libretro-common/vfs/vfs_implementation.c` | Changed to bare `<fcntl.h>`, `<dirent.h>` | `3387d2b` |
| SSE2 guard used `__SSE2__` directly — clang/PS4 rejected | `managers/state_manager.c` | Wrapped with `__has_include(<emmintrin.h>)` | `3387d2b` |
| `retro_get_system_av_info` recursively called itself | `cores/dynamic_dummy.c` | Fixed to call `libretro_dummy_retro_get_system_av_info` | `3387d2b` |
| `griffin.c` include paths pointed to defunct `../core/` | `griffin/griffin.c` | Restored to `../` root-relative paths | `54b12ca` |
| `sleep()` used without `<unistd.h>` under `#ifdef ORBIS` | `menu/drivers/xmb.c` | Added `#include <unistd.h>` guarded by `#ifdef ORBIS` | `54b12ca` |
| `<7zip/7z.h>` not found — `-Ideps` missing | `Makefile.orbis` | Added `-Ideps` to `INCDIRS` | `54b12ca` |
| `core/` subdir move broke `../header.h` relative includes | All of `input/`, `menu/`, `gfx/`, `ui/` | Reverted: root-level headers stay at root | `2b0a8b1` |

### Fixed during second audit (2026-03-30)

These items were listed as OPEN but verified as already resolved in the current codebase:

| # | Issue | Evidence | Status |
| --- | ------- | -------- | ------ |
| 5 | Silent failure if PKG tools missing | `Makefile.orbis:443-448` — `check-pkg-tools` now uses `$(error ...)` for all tool checks | **FIXED** |
| 9 | `orbisAudioInit` error code not captured | `orbis_audio.c:106` — `RARCH_ERR("...failed: 0x%08X\n", ret)` captures and logs the return code | **FIXED** |
| 10 | Audio write doesn't accumulate across small calls | `orbis_audio.c:155-183` — proper accumulation loop buffers partial writes in `float_buf`, outputs only when a full 512-sample block is ready | **FIXED** |
| 11 | Zero-padding doesn't clear stale frame data | No zero-pad path exists in current code. The new accumulation approach converts `float_buf` → `pcm_buf` as a complete block, eliminating the stale data issue | **FIXED** |

---

## OPEN — HIGH

### 4. Resolution always 1920×1080 when `width`/`height` fields are 0

**File:** `gfx/drivers_context/orbis_ctx.c` — `orbis_ctx_get_video_size`

`orbis_ctx_get_video_size` now correctly checks `ctx_orbis->width`/`height` before falling back to
the compile-time constants. However, `orbis_ctx_set_video_mode` only sets these fields to the
`width`/`height` arguments passed by the video driver, which may themselves be 0 on first
init. A proper dynamic-resolution path would need to query the display hardware via
`sceVideoOutGetResolutionStatus` or similar.

**Deferred:** requires on-device testing.

---

## OPEN — MEDIUM

### 6. Incomplete pad init scaffolding

**File:** `frontend/drivers/platform_orbis.c`

`scePadInit()` is called from `ps4_joypad_init` (the joypad driver), not the frontend.
`OrbisGlobalConf` still carries a `confPad` field that is never read by the joypad driver.
This dead field should be removed or documented as reserved.

**Status:** partially resolved — comment on line 137 documents pad ownership.

---

### 7. `argv` stripped in two places

**Files:** `frontend/drivers/platform_orbis.c`

`argv[1]` is consumed by `attach_runtime_conf` (parsed as OrbisGlobalConf pointer), then
nulled on line 133. `argv[2]` is consumed as a content path on line 210.
`retroarch.c` has no ORBIS-specific argv processing, so the risk is low. Adding comments
to document the `argv` contract is the remaining action.

---

### 12. Opaque PGL config fields and magic memory constants

**File:** `gfx/common/orbis_common.h`

Named constants with comments have been added (`ORBISGL_PGL_SYSTEM_SHARED_MEM`, etc.).
Fields `unk_0x5C = 2` and `dbgPosCmd_0x40`/`0x44`/`0x48`/`0x4C` remain unexplained.
There is no adaptive logic for GPUs with different available memory.

**Status:** partially addressed.

---

### 13. Hardcoded mount points with no existence check

**File:** `frontend/drivers/platform_orbis.c`

`host0:app`, `/usb0`, `/usb1` are unconditionally added to the drive list with no check that
they exist. Should check before appending, or discover mounts dynamically.

**Deferred:** `stat()` behavior on PS4 mount points needs on-device testing.

---

### NEW: `orbis_ctx_set_swap_interval` ignores the parameter

**File:** `gfx/drivers_context/orbis_ctx.c` — line 277

The function receives `swap_interval` but always passes `0` to `egl_set_swap_interval`.
This means vsync cannot be controlled at runtime until this is fixed.

---

## OPEN — LOW

### 16. `frontend_orbis_get_mem_used` returns 0 silently on retail firmware

**File:** `frontend/drivers/platform_orbis.c`

`/proc/self/statm` does not exist on retail PS4 firmware. The function silently returns 0,
causing the OSD to show "0 MB used" on retail units.

**Deferred:** needs `sceKernelGetProcessMemoryUsage` or similar; retail-only.

---

### 17. Total available RAM uses a hardcoded fallback

**File:** `frontend/drivers/platform_orbis.c`

`sceKernelGetDirectMemorySize()` is tried first (correct for OpenOrbis). The fallback is
`5 GB` which differs between PS4 models. Acceptable for now.

---

### 18. No annotation on required vs. optional libs in `PS4_LIBS`

**File:** `Makefile.orbis`

The `PS4_LIBS` list has no comments indicating which libraries are required for each build
profile (dev / full / lite).

---

## Build status (as of 2026-03-30)

| Profile | Command | Result |
| --------- | --------- | -------- |
| `lite` | `make -f Makefile.orbis lite` | **Clean** — 2.97 MB ELF |
| `full` (no audio) | `make -f Makefile.orbis full ORBIS_ENABLE_AUDIO=0` | **Clean** — 3.0 MB ELF |
| `full` (audio on) | `make -f Makefile.orbis full` | **Available** — `liborbisAudio` now in `deps/orbisdev-liborbisAudio/` |
| `dev` | not yet tested | Requires `libps4link` + `libdebugnet` |

---

## Summary

| Severity | Open | Fixed |
| ---------- | ------ | ------- |
| CRITICAL | 0 | 2 |
| HIGH     | 1 | 4 |
| MEDIUM   | 4 | 7 |
| LOW      | 3 | 1 |

---

## Recommended fix order (remaining open items)

| Priority | File | Issue |
| ---------- | ------ | ------- |
| 1 | `gfx/drivers_context/orbis_ctx.c` | `orbis_ctx_set_swap_interval`: pass actual argument |
| 2 | `audio/drivers/orbis_audio.c` | Zero buffers in `orbis_audio_stop` to prevent resume pops |
| 3 | `Makefile.orbis` | Annotate `PS4_LIBS` with per-profile comments |
| 4 | `frontend/drivers/platform_orbis.c` | Document `argv` contract in comments |
| 5 | `frontend/drivers/platform_orbis.c` | Remove or document `confPad` in `OrbisGlobalConf` |
