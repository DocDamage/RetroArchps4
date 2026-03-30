# PS4 / Orbis build profiles

This fork supports three convenience build profiles in `Makefile.orbis` and a full PKG packaging
pipeline so you can ship a real PS4 homebrew package without manual steps.

## Profiles

### `dev`

Use when you want console-side debugging hooks and the PS4Link / DebugNet workflow.

- `DEBUG=1`
- `ORBIS_ENABLE_PS4LINK=1`
- `ORBIS_ENABLE_DEBUGNET=1`
- `ORBIS_ENABLE_MENU_WIDGETS=1`
- `ORBIS_ENABLE_7ZIP=1`
- `ORBIS_ENABLE_CHEEVOS=1`
- `ORBIS_ENABLE_OVERLAY=1`
- `ORBIS_ENABLE_AUDIO=1` — requires `liborbisAudio` (see Audio note below)
- `ORBIS_ENABLE_KEYBOARD=0`
- `ORBIS_ENABLE_MOUSE=0`
- `ORBIS_MENU_DRIVER=xmb`

```bash
make -f Makefile.orbis dev
```

### `full`

Use when you want a release-style build without dev transport libraries in the final link.

- `DEBUG=0`
- `ORBIS_ENABLE_PS4LINK=0`
- `ORBIS_ENABLE_DEBUGNET=0`
- `ORBIS_ENABLE_MENU_WIDGETS=1`
- `ORBIS_ENABLE_7ZIP=1`
- `ORBIS_ENABLE_CHEEVOS=1`
- `ORBIS_ENABLE_OVERLAY=1`
- `ORBIS_ENABLE_AUDIO=1` — requires `liborbisAudio` (see Audio note below)
- `ORBIS_ENABLE_KEYBOARD=0`
- `ORBIS_ENABLE_MOUSE=0`
- `ORBIS_MENU_DRIVER=xmb`

```bash
make -f Makefile.orbis full
```

### `lite`

Use when you want the leanest build path: no audio, no cheevos, no overlay, rgui menu.
This profile builds cleanly with a stock OpenOrbis toolchain and no external libraries.

- `DEBUG=0`
- `ORBIS_ENABLE_PS4LINK=0`
- `ORBIS_ENABLE_DEBUGNET=0`
- `ORBIS_ENABLE_MENU_WIDGETS=0`
- `ORBIS_ENABLE_7ZIP=0`
- `ORBIS_ENABLE_CHEEVOS=0`
- `ORBIS_ENABLE_OVERLAY=0`
- `ORBIS_ENABLE_AUDIO=0`
- `ORBIS_ENABLE_KEYBOARD=0`
- `ORBIS_ENABLE_MOUSE=0`
- `ORBIS_MENU_DRIVER=rgui`

```bash
make -f Makefile.orbis lite
```

## Audio note

`ORBIS_ENABLE_AUDIO=1` adds `-DHAVE_ORBIS_AUDIO` and links `-lorbisAudio`. The `orbisAudio`
library is a separate homebrew library — it is **not** included in the OpenOrbis SDK. You must
build and install it separately before `full` or `dev` profiles will link:

1. Clone `orbisdev/orbisAudio` and follow its build instructions.
2. Copy `liborbisAudio.a` to `$(ORBISDEV)/lib/`.
3. Copy `orbisAudio.h` to `$(ORBISDEV)/include/`.

Until then, build with `ORBIS_ENABLE_AUDIO=0` to get a fully functional binary without audio.

## Stripped release artifacts

```bash
make -f Makefile.orbis full-stripped
make -f Makefile.orbis lite-stripped
```

Produces `retroarch_orbis.stripped.elf` alongside the main ELF. Also available:

```bash
make -f Makefile.orbis stripped   # strip whatever was just built
make -f Makefile.orbis size       # print section sizes
```

## PKG packaging

The makefile includes a complete PKG pipeline using the OpenOrbis packaging tools.

```bash
make -f Makefile.orbis fself          # wrap ELF → dist/pkg_orbis/eboot.bin
make -f Makefile.orbis sfo            # generate dist/pkg_orbis/sce_sys/param.sfo
make -f Makefile.orbis pkg            # full package from current build
make -f Makefile.orbis pkg-full       # build full profile then package
make -f Makefile.orbis pkg-lite       # build lite profile then package
```

Required tools (must be on `PATH` or in `$(ORBISDEV)/bin/`):

- `create-fself` / `create-fself.cmd`
- `create-pkg` / `PkgTool.Core`
- `create-gp4` (for `.gp4` project generation)

The `sfo` target uses `dist/pkg/make_sfo.py` (Python 3) to generate `param.sfo` from the
title ID / title name defined in `Makefile.orbis`.

PKG assets (icon, background) go in `dist/pkg_orbis/sce_sys/`. A placeholder `param.sfo` is
committed; replace `icon0.png` (512×512 PNG) before distributing.

## Custom builds

Override individual knobs directly:

```bash
make -f Makefile.orbis ORBIS_BUILD_PROFILE=custom \
  ORBIS_MENU_DRIVER=rgui \
  ORBIS_ENABLE_MENU_WIDGETS=0 \
  ORBIS_ENABLE_PS4LINK=0 \
  ORBIS_ENABLE_DEBUGNET=0 \
  ORBIS_ENABLE_AUDIO=0
```

### All available knobs

| Knob | Default | Description |
| ---- | ------- | ----------- |
| `ORBIS_BUILD_PROFILE` | `custom` | Select preset (`dev`, `full`, `lite`, `custom`) |
| `ORBIS_ENABLE_PS4LINK` | `1` | Link ps4link transport library |
| `ORBIS_ENABLE_DEBUGNET` | `1` | Link debugnet UDP logging library |
| `ORBIS_ENABLE_MENU_WIDGETS` | `1` | Include on-screen notification widgets |
| `ORBIS_ENABLE_7ZIP` | `1` | Include 7-Zip decompression |
| `ORBIS_ENABLE_CHEEVOS` | `1` | Include RetroAchievements support |
| `ORBIS_ENABLE_OVERLAY` | `1` | Include overlay / controller overlay |
| `ORBIS_ENABLE_AUDIO` | `1` | Include orbisAudio driver (requires external lib) |
| `ORBIS_ENABLE_APPEXEC` | `0` | Enable `sceAppMgrLoadExec` for core launching |
| `ORBIS_ENABLE_KEYBOARD` | `0` | Link keyboard stub (experimental) |
| `ORBIS_ENABLE_MOUSE` | `0` | Link mouse stub (experimental) |
| `ORBIS_MENU_DRIVER` | `xmb` | Default menu driver (`xmb`, `rgui`, `materialui`) |

## Environment

The makefile accepts any of these environment variables (checked in order):

1. `ORBISDEV`
2. `PS4SDK`
3. `OO_PS4_TOOLCHAIN`

It auto-detects modern vs legacy SDK layouts:

- **modern:** `$(ORBISDEV)/usr/include` and `$(ORBISDEV)/usr/lib`
- **legacy:** `$(ORBISDEV)/include` and `$(ORBISDEV)/lib`

Additional auto-detection:

- C++ include root (`c++/v1`) when present
- Linker script (`link.x` or `linker.x`) when present
- CRT file (`crt0.o`, `crt1.o`, or `crt0.s`)

Print the resolved layout:

```bash
make -f Makefile.orbis info ORBIS_BUILD_PROFILE=lite
```

## File browser roots

The Orbis frontend exposes the following paths in the file browser:

- `host0:app` — application directory (primary content location)
- `/` — filesystem root (jailbroken only)
- `/data` — writable data partition
- `/usb0`, `/usb1` — external USB storage
- `/data/self` — self process data

## Optional input toggles

```bash
make -f Makefile.orbis ORBIS_ENABLE_KEYBOARD=1 ORBIS_ENABLE_MOUSE=1
```

These are **off by default**. They link additional SDK stubs that may not be present in every
SDK version. Enable only when you have confirmed the required stubs exist in your toolchain.
