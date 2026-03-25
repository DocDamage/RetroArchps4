# Orbis validation review

Date: 2026-03-25
Scope: validation pass on the post-PR Orbis cleanup branch after the initial cleanup PR had already been merged.

## What was reviewed

- `Makefile.orbis`
- `frontend/drivers/platform_orbis.c`
- `docs/ps4_orbis_build_profiles.md`
- cumulative branch behavior across:
  - `dev`, `full`, `lite`
  - modern vs legacy SDK layout detection
  - optional PS4Link / DebugNet toggles
  - optional keyboard / mouse toggles
  - stripped release targets
  - upstream-adapted browser roots

## Findings

### 1. Profile model is coherent

The branch now has a clear split between:

- `dev` for console-side debugging and old workflow behavior
- `full` for a normal release-style build
- `lite` for a leaner frontend-focused build

That split is meaningful and worth keeping.

### 2. Toolchain detection is directionally good

The branch now handles both:

- legacy-style `PS4SDK`
- newer-style `ORBISDEV`

and detects:

- include root
- platform include root
- C++ include root when present
- linker script when present
- crt file location

This is a real improvement over the original hard-coded assumptions.

### 3. Runtime `lite` defaults are coherent

The Orbis frontend changes that make `lite` default to `rgui`, reduce overlay behavior, and stop forcing verbose startup are internally consistent with the goal of a leaner build.

### 4. Upstream-adapted browser roots are safe enough

Keeping the original `host0:` roots while also adding `/`, `/data`, and `/usb0` is a reasonable additive change.

### 5. One real bug was found during validation

The guarded keyboard/mouse support added in a later pass had a library-ordering bug:

- the makefile appended keyboard/mouse stub libs to `PS4_LIBS`
- then later overwrote `PS4_LIBS` with the base library set

That meant the toggles would define the macros but fail to preserve the extra libs.

## Fix applied in this validation pass

The `PS4_LIBS` base assignment was moved ahead of the optional keyboard/mouse additions so these toggles now behave consistently:

- `ORBIS_ENABLE_KEYBOARD=1`
- `ORBIS_ENABLE_MOUSE=1`

## Deliberately not validated as supported defaults

The following were intentionally *not* promoted to default behavior because this older fork does not show enough compatible plumbing yet:

- newer upstream memory reporting hooks
- newer frontend driver API fields
- newer Orbis init/runtime code paths that assume a more modern SDK environment

## Branch state note

The initial cleanup PR had already been merged into `master`, while later work continued on the branch. This validation pass was used to sanity-check the cumulative branch state after that point.

## Recommended next steps

1. Open a fresh PR from the current branch tip instead of relying on the already-merged original PR.
2. Build-test at least these combinations:
   - legacy SDK + `full`
   - legacy SDK + `lite`
   - modern SDK + `full`
   - modern SDK + `lite`
3. Treat keyboard/mouse toggles as experimental until a real build/test pass confirms the required stubs exist in the target SDK.
4. Do not import more upstream Orbis runtime code until there is a clear compatibility target for the SDK/toolchain in use.
