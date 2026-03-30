# Technical Debt Audit — RetroArch Orbis/PS4 Port

**Branch:** `orbis-build-cleanup-pass1`
**Last updated:** 2026-03-30 (pass 5 — debt audit quick wins + pad cache + refresh rate)
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
| 7 | `argv` contract undocumented | `platform_orbis.c:88-96` — block comment documents `argv[0]`/`[1]`/`[2]` contract and nulling behaviour | **FIXED** |
| 9 | `orbisAudioInit` error code not captured | `orbis_audio.c:106` — `RARCH_ERR("...failed: 0x%08X\n", ret)` captures and logs the return code | **FIXED** |
| 10 | Audio write doesn't accumulate across small calls | `orbis_audio.c:155-183` — proper accumulation loop buffers partial writes in `float_buf`, outputs only when a full 512-sample block is ready | **FIXED** |
| 11 | Zero-padding doesn't clear stale frame data | No zero-pad path exists in current code. The new accumulation approach converts `float_buf` → `pcm_buf` as a complete block, eliminating the stale data issue | **FIXED** |
| swap_interval | `orbis_ctx_set_swap_interval` was ignoring its argument | `orbis_ctx.c:277` — now passes `swap_interval` directly to `egl_set_swap_interval` (commit `166ebb1`) | **FIXED** |
| audio stop | Audio buffers not zeroed on stop, causing resume pops | `orbis_audio.c:193-196` — `orbis_audio_stop` now `memset`s both `float_buf` and `pcm_buf` (commit `166ebb1`) | **FIXED** |
| 18 | No annotation on required vs. optional libs in `PS4_LIBS` | `Makefile.orbis:279-282` — base libs have a comment block; all conditional libs have inline comments | **FIXED** |

### Fixed in pass 4 (2026-03-30)

| # | Issue | Fix |
| --- | ------- | ----- |
| 4 | Resolution hardcoded to 1920×1080 | `orbis_ctx.c` — `orbis_ctx_init` now calls `sceVideoOutGetResolutionStatus` (gated on `ORBIS_HAS_VIDEOOUT` via `__has_include`) to populate `ctx_orbis->width`/`height` before `set_video_mode` is called. Requires on-device validation. |
| 6 | `confPad` dead field undocumented | `platform_orbis.c` — ABI comment added explaining the field must not be removed (external loader ABI) even though RetroArch never reads it. |
| 13 | `/usb0`/`/usb1` appended unconditionally | `platform_orbis.c` — `orbis_path_accessible()` helper added; both paths now guarded with `stat()` before appending. Requires on-device validation of `stat()` behaviour on mount points. |
| 16 | `get_mem_used` returns 0 silently on retail | `platform_orbis.c` — `RARCH_WARN` emitted once when `/proc/self/statm` is unavailable, so developers see the fallback in logs. Still returns 0; full fix needs `sceKernelGetProcessMemoryUsage` on-device. |

### Fixed in pass 5 (2026-03-30)

These items were identified by a full technical debt audit and fixed immediately:

| # | Issue | Fix |
| --- | ------- | ----- |
| 19 | `orbis_ctx_get_proc_address` missing return — UB when `HAVE_EGL` is off | Added `(void)symbol; return NULL;` after `#endif` (`orbis_ctx.c:339-340`) |
| 20 | Dead `extern bool platform_orbis_has_focus` — copy-paste from Switch, never defined | Removed the extern declaration (`orbis_ctx.c:44`) |
| 21 | `ORBIS_ENABLE_CHEEVOS ?= 1` misleading — silently disabled by `HAVE_NETWORKING=0` nesting in `Makefile.common` | Changed default to `?= 0`; added `$(warning ...)` when explicitly set to 1 without networking (`Makefile.orbis:11,242-248`) |
| 22 | Build artifacts tracked in git (`param.sfo`, `.gp4`) | `git rm --cached`; added `dist/pkg_orbis/sce_sys/param.sfo` to `.gitignore` |
| 23 | `NUL` Windows artifact in repo root | Deleted; added `NUL` to `.gitignore` |
| 24 | `ps4_joypad_axis` calls `scePadReadState` a second time per pad per frame | Added `pad_data_cache[]` array; `ps4_joypad_poll` stores `OrbisPadData`; `ps4_joypad_axis` reads from cache (`ps4_joypad.c:67,271,196`) |
| 25 | `refresh_rate` hardcoded to 60 — PAL displays will drift | `orbis_ctx_init` now reads `res.refreshRate` from `sceVideoOutGetResolutionStatus`; `set_video_mode` preserves the queried value with 60 Hz fallback (`orbis_ctx.c:170-176,239-241`) |

---

## OPEN — MEDIUM

### 12. Opaque PGL config fields and magic memory constants

**File:** `gfx/common/orbis_common.h`

Named constants with comments have been added (`ORBISGL_PGL_SYSTEM_SHARED_MEM`, etc.).
Fields `unk_0x5C = 2` and `dbgPosCmd_0x40`/`0x44`/`0x48`/`0x4C` remain unexplained.
There is no adaptive logic for GPUs with different available memory.

**Status:** partially addressed.

---

## OPEN — LOW

### 17. Total available RAM uses a hardcoded fallback

**File:** `frontend/drivers/platform_orbis.c`

`sceKernelGetDirectMemorySize()` is tried first (correct for OpenOrbis). The fallback is
`5 GB` which differs between PS4 models. Acceptable for now.

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
| CRITICAL | 0 | 3 |
| HIGH | 0 | 6 |
| MEDIUM | 1 | 18 |
| LOW | 1 | 3 |

---

## Remaining open items

| Priority | File | Issue |
| ---------- | ------ | ------- |
| 1 | `gfx/common/orbis_common.h` | Investigate `unk_0x5C` and `dbgPosCmd_0x4x` fields in `ScePglConfig` (on-device) |
| 2 | `frontend/drivers/platform_orbis.c` | `sceKernelGetProcessMemoryUsage` fallback for retail memory OSD (on-device) |
