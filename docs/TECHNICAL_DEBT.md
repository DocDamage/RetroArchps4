# Technical Debt Audit — RetroArch Orbis/PS4 Port

**Branch:** `orbis-build-cleanup-pass1`
**Last updated:** 2026-03-26
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

---

## OPEN — HIGH

### 4. Resolution always 1920×1080 when `width`/`height` fields are 0

**File:** `gfx/drivers_context/orbis_ctx.c` — `orbis_ctx_get_video_size`

`orbis_ctx_get_video_size` now correctly checks `ctx_orbis->width`/`height` before falling back to
the compile-time constants. However, `orbis_ctx_set_video_mode` only sets these fields to the
`width`/`height` arguments passed by the video driver, which may themselves be 0 on first
init. A proper dynamic-resolution path would need to query the display hardware.

---

### 5. Silent failure if PKG tools are missing

**File:** `Makefile.orbis` — `pkg` target

The `check-pkg-tools` target prints an error message but does not halt make with a non-zero
exit code when `create-fself` or `create-pkg` are absent. `$(error ...)` should be used instead.

---

## OPEN — MEDIUM

### 6. Incomplete pad init scaffolding

**File:** `frontend/drivers/platform_orbis.c` — `frontend_orbis_init`

`scePadInit()` is called from `ps4_joypad_init` (the joypad driver), not the frontend. Any
residual pad-open/close scaffolding in `platform_orbis.c` that pre-dates the joypad driver
should be audited and removed if it duplicates the joypad driver's work.

---

### 7. `argv` stripped in two places

**Files:** `retroarch.c`, `frontend/drivers/platform_orbis.c`

Both sites independently strip or inspect the first two `argv` entries under `#ifdef ORBIS`.
No comment explains what those arguments represent or which site is authoritative. Risk of
double-processing if execution order changes.

---

### 9. `orbisAudioInit` error code not captured

**File:** `audio/drivers/orbis_audio.c`

```c
if (orbisAudioInit() < 0) { RARCH_ERR("failed"); return NULL; }
```

The specific error code is discarded. Debugging audio init failures requires a debugger attach
rather than reading a log. Capture the return value and log it with `%d` / `0x%08X`.

---

### 10. Audio write doesn't accumulate across small calls

**File:** `audio/drivers/orbis_audio.c`

If `size` is smaller than one full 512-sample block, `frames_in` rounds to 0 and the function
returns 0 with no output. Repeated small writes produce silence. An accumulation buffer is
needed to hold partial blocks across calls.

---

### 11. Zero-padding doesn't clear stale frame data

**File:** `audio/drivers/orbis_audio.c`

Only the tail portion of `pcm_buf` is zeroed when a partial block arrives. Data from a prior
incomplete write can bleed into the next output block if the buffer is not fully cleared first.

---

### 12. Opaque PGL config fields and magic memory constants

**File:** `gfx/common/orbis_common.h`

Fields such as `unk_0x5C = 2` and `dbgPosCmd_0x40` are unexplained. Memory budgets
(2 MB / 36 MB / 170 MB / 768 KB) have no source references. There is no adaptive logic for
GPUs with different available memory.

---

### 13. Hardcoded mount points with no existence check

**File:** `frontend/drivers/platform_orbis.c`

`host0:app`, `/usb0`, `/usb1` are unconditionally added to the drive list with no check that
they exist. Should check before appending, or discover mounts dynamically.

---

## OPEN — LOW

### 16. `frontend_orbis_get_mem_used` returns 0 silently on retail firmware

**File:** `frontend/drivers/platform_orbis.c`

`/proc/self/statm` does not exist on retail PS4 firmware. The function silently returns 0,
causing the OSD to show "0 MB used" on retail units. The jailbroken path is correct; retail
needs a fallback via `sceKernelGetProcessMemoryUsage` or similar.

---

### 17. Total available RAM uses a hardcoded fallback

**File:** `frontend/drivers/platform_orbis.c`

`sceKernelGetDirectMemorySize()` is tried first (correct for OpenOrbis). The fallback is
`5 GB` which differs between PS4 models (CUH-1000 has 5 GB accessible; CUH-7000 has
more). Acceptable for now but document which SDK versions provide the API.

---

### 18. No annotation on required vs. optional libs in `PS4_LIBS`

**File:** `Makefile.orbis`

The `PS4_LIBS` list has no comments indicating which libraries are required for each build
profile (dev / full / lite). Adding a new profile risks linking unnecessary or missing stubs.

---

## Build status (as of 2026-03-26)

| Profile | Command | Result |
| --------- | --------- | -------- |
| `lite` | `make -f Makefile.orbis lite` | **Clean** — 2.97 MB ELF |
| `full` (no audio) | `make -f Makefile.orbis full ORBIS_ENABLE_AUDIO=0` | **Clean** — 3.0 MB ELF |
| `full` (audio on) | `make -f Makefile.orbis full` | **Blocked** — requires `liborbisAudio` (external homebrew lib, not in OpenOrbis SDK) |
| `dev` | not yet tested | — |

`orbisAudio` must be built separately from its source and installed under `$(ORBISDEV)/lib/`
and `$(ORBISDEV)/include/` before the `full` / `dev` profiles can use audio.

---

## Summary

| Severity | Open | Fixed |
| ---------- | ------ | ------- |
| CRITICAL | 0 | 2 |
| HIGH     | 2 | 3 |
| MEDIUM   | 6 | 4 |
| LOW      | 3 | 1 |

---

## Recommended fix order (remaining open items)

| Priority | File | Issue |
| ---------- | ------ | ------- |
| 1 | `Makefile.orbis` | `check-pkg-tools`: use `$(error ...)` to fail hard |
| 2 | `audio/drivers/orbis_audio.c` | Log `orbisAudioInit` return code |
| 3 | `audio/drivers/orbis_audio.c` | Accumulation buffer for sub-block writes |
| 4 | `audio/drivers/orbis_audio.c` | Clear full buffer before partial-block zero-pad |
| 5 | `frontend/drivers/platform_orbis.c` | Audit/remove duplicate pad init scaffolding |
| 6 | `frontend/drivers/platform_orbis.c` | Annotate argv stripping logic |
