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
- `ORBIS_MENU_DRIVER=xmb`

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
- `ORBIS_MENU_DRIVER=rgui`

Build it with:

```bash
make -f Makefile.orbis lite
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
- `ORBIS_MENU_DRIVER`

## Environment

The makefile accepts either:

- `ORBISDEV`
- `PS4SDK`

If `ORBISDEV` is not set, it falls back to `PS4SDK`.

## Quick sanity check

To print the resolved settings before building:

```bash
make -f Makefile.orbis info ORBIS_BUILD_PROFILE=lite
```

## Why this exists

This repo previously treated PS4 builds like a single bucket, which meant dev transport/debug libraries and feature-heavy menu behavior were too easy to carry into builds that should have been leaner.

These profiles do **not** magically optimize every libretro core. They do give you a cleaner way to separate:

- development builds
- normal release builds
- leaner frontend-focused release builds
