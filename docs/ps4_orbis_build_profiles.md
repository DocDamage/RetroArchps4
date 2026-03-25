# PS4 / Orbis build profiles

This fork now supports three convenience build profiles in `Makefile.orbis` so you can stop treating every PS4 build like a debug/dev build.

## Profiles

### `dev`
Use when you want the old-school development flow and console-side debugging hooks.

- `DEBUG=1`
- `ORBIS_ENABLE_PS4LINK=1`
- `ORBIS_ENABLE_DEBUGNET=1`
- `ORBIS_ENABLE_MENU_WIDGETS=1`
- `ORBIS_ENABLE_7ZIP=1`
- `ORBIS_ENABLE_CHEEVOS=1`
- `ORBIS_ENABLE_OVERLAY=1`
- `ORBIS_ENABLE_KEYBOARD=0`
- `ORBIS_ENABLE_MOUSE=0`
- `ORBIS_MENU_DRIVER=xmb`

Build it with:

```bash
make -f Makefile.orbis dev
```

### `full`
Use when you want a release-style build that stays close to the current feature set, but without dev hooks being dragged into the final link.

- `DEBUG=0`
- `ORBIS_ENABLE_PS4LINK=0`
- `ORBIS_ENABLE_DEBUGNET=0`
- `ORBIS_ENABLE_MENU_WIDGETS=1`
- `ORBIS_ENABLE_7ZIP=1`
- `ORBIS_ENABLE_CHEEVOS=1`
- `ORBIS_ENABLE_OVERLAY=1`
- `ORBIS_ENABLE_KEYBOARD=0`
- `ORBIS_ENABLE_MOUSE=0`
- `ORBIS_MENU_DRIVER=xmb`
- non-debug builds now add `-DNDEBUG` and `-fomit-frame-pointer`
- when available, newer Orbis linker-script settings are auto-detected and used

Build it with:

```bash
make -f Makefile.orbis full
```

### `lite`
Use when you want the leanest build path currently wired into this fork.

- `DEBUG=0`
- `ORBIS_ENABLE_PS4LINK=0`
- `ORBIS_ENABLE_DEBUGNET=0`
- `ORBIS_ENABLE_MENU_WIDGETS=0`
- `ORBIS_ENABLE_7ZIP=0`
- `ORBIS_ENABLE_CHEEVOS=0`
- `ORBIS_ENABLE_OVERLAY=0`
- `ORBIS_ENABLE_KEYBOARD=0`
- `ORBIS_ENABLE_MOUSE=0`
- `ORBIS_MENU_DRIVER=rgui`
- defaults to `rgui` at runtime on first boot
- disables overlay by default at runtime
- avoids forcing verbose startup by default
- non-debug builds now add `-DNDEBUG` and `-fomit-frame-pointer`
- when available, newer Orbis linker-script settings are auto-detected and used

Build it with:

```bash
make -f Makefile.orbis lite
```

## Stripped release artifacts

You can also build stripped release artifacts without touching the normal debug/dev flow:

```bash
make -f Makefile.orbis full-stripped
make -f Makefile.orbis lite-stripped
```

This creates a second output artifact:

- `retroarch_orbis.stripped.elf`

The makefile also includes:

```bash
make -f Makefile.orbis stripped
make -f Makefile.orbis size
```

## Custom builds

You can still override knobs directly:

```bash
make -f Makefile.orbis all ORBIS_BUILD_PROFILE=custom ORBIS_MENU_DRIVER=rgui ORBIS_ENABLE_MENU_WIDGETS=0 ORBIS_ENABLE_PS4LINK=0 ORBIS_ENABLE_DEBUGNET=0
```

Available knobs:

- `ORBIS_BUILD_PROFILE`
- `ORBIS_ENABLE_PS4LINK`
- `ORBIS_ENABLE_DEBUGNET`
- `ORBIS_ENABLE_MENU_WIDGETS`
- `ORBIS_ENABLE_7ZIP`
- `ORBIS_ENABLE_CHEEVOS`
- `ORBIS_ENABLE_OVERLAY`
- `ORBIS_ENABLE_KEYBOARD`
- `ORBIS_ENABLE_MOUSE`
- `ORBIS_MENU_DRIVER`

## Environment

The makefile accepts either:

- `ORBISDEV`
- `PS4SDK`

If `ORBISDEV` is not set, it falls back to `PS4SDK`.

It also auto-detects modern vs legacy SDK layouts:

- modern: `$(ORBISDEV)/usr/include` and `$(ORBISDEV)/usr/lib`
- legacy: `$(ORBISDEV)/include` and `$(ORBISDEV)/lib`

Additional upstream-style compatibility detection now includes:

- optional C++ include root detection (`c++/v1`) when present
- optional linker script detection (`linker.x`) when present
- upstream-style Orbis identity macros (`__ORBIS__`, `__PS4__`, `D_BSD_SOURCE`)

The makefile prints the resolved layout via:

```bash
make -f Makefile.orbis info ORBIS_BUILD_PROFILE=lite
```

## Upstream-adapted Orbis file browser roots

The Orbis frontend now keeps the older host0-based browser roots **and** adds a few newer upstream-style roots for convenience:

- `host0:app`
- `host0:app/data`
- `host0:app/data/retroarch`
- `/`
- `/data`
- `/usb0`

This is an additive change intended to make it easier to browse common PS4 storage paths without removing the older fork’s expected host0 paths.

## Optional input toggles

New upstream-inspired optional toggles are available for SDKs that provide the relevant stubs:

- `ORBIS_ENABLE_KEYBOARD=1`
- `ORBIS_ENABLE_MOUSE=1`

These remain **off by default** because this older fork has not been fully audited against every newer Orbis SDK/lib combination. They are intended as optional compatibility hooks, not guaranteed-on defaults.

## Why this exists

This repo previously treated PS4 builds like a single bucket, which meant dev transport/debug libraries and feature-heavy menu behavior were too easy to carry into builds that should have been leaner.

These profiles do **not** magically optimize every libretro core. They do give you a cleaner way to separate:

- development builds
- normal release builds
- leaner frontend-focused release builds
- stripped release artifacts for lighter distribution/testing
- upstream-adapted Orbis improvements that still fit an older fork
