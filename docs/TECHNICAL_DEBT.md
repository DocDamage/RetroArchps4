# Technical Debt Audit — RetroArch Orbis/PS4 Port

**Branch:** `orbis-build-cleanup-pass1`
**Date:** 2026-03-26
**Scope:** Orbis-specific files only

---

## CRITICAL

### 1. Inverted return check — context init always fails
**File:** `gfx/drivers_context/orbis_ctx.c:115`

```c
if (!ret)  // BUG: 0 = success on PS4 SDK, so this fires on SUCCESS
{
    printf("[ORBISGL] scePigletSetConfigurationVSH failed 0x%08X.\n", ret);
    goto error;
}
```

PS4 SDK returns 0 on success. `!ret` is true when ret == 0 (success), so every successful call is
treated as failure. The EGL/Piglet context can never initialize.
**Fix:** Change to `if (ret != 0)`.

---

### 2. Race condition on `num_players`
**File:** `input/drivers_joypad/ps4_joypad.c:72`

`static int16_t num_players = 0;` is read by the poll thread and written by init/destroy with no
lock. Can cause out-of-bounds array access into `ds_joypad_states`.
**Fix:** Protect with a mutex or use `_Atomic int16_t`.

---

## HIGH

### 3. Array access without bounds check
**File:** `input/drivers_joypad/ps4_joypad.c:103–109`

Loop writes to `ds_joypad_states[index]` while iterating up to `num_players`, with no check that
`num_players < PS4_MAX_ORBISPADS (16)`.
**Fix:** Guard with `if (num_players >= PS4_MAX_ORBISPADS) return false;` before the loop.

---

### 4. Resolution always 1920×1080 — dynamic fields unused
**File:** `gfx/drivers_context/orbis_ctx.c:58–59`

`orbis_ctx_get_video_size` always returns the compile-time constants `ATTR_ORBISGL_WIDTH` /
`ATTR_ORBISGL_HEIGHT` even though `ctx_orbis->width` and `ctx_orbis->height` fields exist in the
struct. Runtime resolution changes are silently ignored.

---

### 5. Silent failure if PKG tools are missing
**File:** `Makefile.orbis:389–390`

```makefile
CREATE_FSELF ?= $(or $(shell command -v create-fself 2>/dev/null),$(ORBISDEV)/bin/create-fself)
CREATE_PKG   ?= $(or $(shell command -v create-pkg   2>/dev/null),$(ORBISDEV)/bin/create-pkg)
```

No error is raised if neither path resolves. `pkg` targets fail silently at link time rather than
at configure time.
**Fix:** Add a `$(error ...)` guard in the `pkg` target that verifies the tools exist before use.

---

## MEDIUM

### 6. Incomplete pad init with immediate close
**File:** `frontend/drivers/platform_orbis.c:145–149`

`orbisPadInitWithConf` is called and then `scePadClose` is called immediately on the same handle.
Looks like unfinished scaffolding. There is no corresponding cleanup call in `frontend_orbis_deinit`.

---

### 7. `argv` stripped in two places
**Files:** `retroarch.c:20944`, `frontend/drivers/platform_orbis.c:141`

Both sites independently strip the first two argv entries under `#ifdef ORBIS` with no comment
explaining what those arguments are or why both sites need to do it. Risk of double-stripping if
execution order changes.

---

### 8. Poll condition always-true after loop
**File:** `input/drivers_joypad/ps4_joypad.c:105–115`

A `while (index < num_players)` loop is immediately followed by `if (index == num_players)`. The
`if` is always true after the loop terminates. Original intent appears to be "check if userId is
already registered" but the logic does not implement that correctly.

---

### 9. orbisAudioInit error code not captured
**File:** `audio/drivers/orbis_audio.c:101`

```c
if (orbisAudioInit() < 0) { RARCH_ERR("failed"); return NULL; }
```

The specific error code is discarded. Debugging audio init failures requires a debugger attach
rather than reading a log.

---

### 10. Audio write doesn't buffer across calls
**File:** `audio/drivers/orbis_audio.c:149–154`

If `size` is smaller than one full block, `frames_in` becomes 0 and the function returns 0 with no
output. Repeated undersize writes produce no audio output at all. Needs an accumulation buffer.

---

### 11. Zero-padding doesn't clear stale frame data
**File:** `audio/drivers/orbis_audio.c:160–166`

Only the tail portion of `pcm_buf` is zeroed when a partial block arrives. Data from a prior
incomplete write can bleed into the next output block.

---

### 12. Opaque PGL config fields and magic memory constants
**Files:** `gfx/common/orbis_common.h:20–24, 106–110`

Fields such as `unk_0x5C = 2` and `dbgPosCmd_0x40` are unexplained. Memory budgets
(2 MB / 36 MB / 170 MB / 768 KB) have no source references and no adaptive logic for GPUs with
different available memory.

---

### 13. Hardcoded mount points with no existence check
**File:** `frontend/drivers/platform_orbis.c:444–483`

`host0:app`, `/usb0`, `/usb1` are unconditionally added to the drive list with no check that they
exist. Should use `readdir` on root to discover actual mounts, or at minimum check before appending.

---

### 14. Global context pointer named `nx_ctx_ptr`
**File:** `gfx/drivers_context/orbis_ctx.c:32`

```c
orbis_ctx_data_t *nx_ctx_ptr = NULL;
```

Copy-paste artifact from the Nintendo Switch driver. Misleading name in an Orbis context.
**Fix:** Rename to `orbis_ctx_ptr`.

---

## LOW

### 15. `rumble` struct not zero-initialized
**File:** `input/drivers_joypad/ps4_joypad.c:66`

`ScePadVibrationParam rumble` is declared but never zeroed. The first rumble call may submit
garbage motor values until both fields are explicitly set.
**Fix:** `memset(&ds_joypad_states[i].rumble, 0, sizeof(...))` after `scePadOpen`.

---

### 16. `frontend_orbis_get_mem_used` returns 0 silently on retail firmware
**File:** `frontend/drivers/platform_orbis.c:293–312`

`/proc/self/statm` does not exist on retail PS4 firmware. The function silently returns 0, causing
the OSD to show "0 MB used".

---

### 17. Total available RAM is a hardcoded constant
**File:** `frontend/drivers/platform_orbis.c:289`

```c
return (uint64_t)5 * 1024 * 1024 * 1024;  /* ~5 GB accessible */
```

Differs across PS4 models (CUH-1000 vs. CUH-7000). Should query the SDK if an API exists.

---

### 18. No documentation on required vs. optional libs in `PS4_LIBS`
**File:** `Makefile.orbis:238–239`

The ~25-entry `PS4_LIBS` list has no annotation indicating which libraries are required for each
build profile (dev / full / lite). Adding a new profile risks linking unnecessary or missing libs.

---

## Summary

| Severity | Count | Key Items |
|----------|-------|-----------|
| CRITICAL | 2 | Inverted Piglet return check; `num_players` race condition |
| HIGH     | 3 | Array bounds, hardcoded resolution, silent PKG tool failure |
| MEDIUM   | 9 | Error handling gaps, stale audio data, arg-stripping duplication, opaque config |
| LOW      | 4 | Uninitialized rumble, silent mem fallbacks, misleading pointer name, undocumented libs |

---

## Recommended Fix Order

| Priority | File | Issue | Fix |
|----------|------|-------|-----|
| 1 | `gfx/drivers_context/orbis_ctx.c:115` | Inverted return check | `if (!ret)` → `if (ret != 0)` |
| 2 | `input/drivers_joypad/ps4_joypad.c:72` | Race on `num_players` | Add mutex or `_Atomic` |
| 3 | `input/drivers_joypad/ps4_joypad.c:103` | Array OOB risk | Bounds-check before loop |
| 4 | `Makefile.orbis:389` | Silent tool failure | Add `$(error ...)` guard in `pkg` target |
| 5 | `frontend/drivers/platform_orbis.c:145` | Incomplete pad init | Finish or remove immediate-close pattern |
| 6 | `gfx/drivers_context/orbis_ctx.c:32` | Misleading pointer name | Rename `nx_ctx_ptr` → `orbis_ctx_ptr` |
