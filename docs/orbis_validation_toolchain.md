# Orbis validation toolchain

A local validation harness for running repeatable build-matrix tests on a machine that has
the PS4 SDK and toolchain available.

## What it solves

Code review can happen in GitHub, but real validation requires a machine that has:

- `ORBISDEV`, `PS4SDK`, or `OO_PS4_TOOLCHAIN` set in the environment
- `make` or `mingw32-make`
- `clang` with `ld.lld`
- The expected Orbis stub libraries and headers

This toolchain gives you a standard way to collect that information and produce logs that
can be reviewed later.

## Files

- `tools/orbis/Get-OrbisEnvReport.ps1` — environment probe
- `tools/orbis/Invoke-OrbisBuildMatrix.ps1` — build matrix runner

## 1. Environment probe

Inspects and reports:

- Repo root, current branch and commit
- Tool availability: `git`, `make`, `mingw32-make`, `clang`, `pwsh`, `powershell`
- SDK root source (`ORBISDEV` / `PS4SDK` / `OO_PS4_TOOLCHAIN`)
- SDK layout (modern vs legacy)
- Include root, platform include root, C++ include root
- Lib root, CRT file, linker script
- Presence of key libraries and stubs

### Usage

```powershell
pwsh ./tools/orbis/Get-OrbisEnvReport.ps1
```

Write JSON to a file:

```powershell
pwsh ./tools/orbis/Get-OrbisEnvReport.ps1 -OutputPath ./artifacts/orbis_env_report.json
```

## 2. Build matrix runner

Runs a repeatable build matrix and writes per-profile logs plus a summary.

Default matrix: `full` and `lite`. Options:

- `-IncludeDev` — also build `dev`
- `-EnableKeyboard` / `-EnableMouse` — probe optional input stubs
- `-SkipBuild` — collect env info only, do not compile
- `-MakeCommand` — override the make binary (default: `make`)

### Examples

```powershell
# Default matrix (full + lite, ORBIS_ENABLE_AUDIO=0 to avoid liborbisAudio dependency)
pwsh ./tools/orbis/Invoke-OrbisBuildMatrix.ps1

# Explicit make command on Windows
pwsh ./tools/orbis/Invoke-OrbisBuildMatrix.ps1 -MakeCommand mingw32-make

# Include dev profile
pwsh ./tools/orbis/Invoke-OrbisBuildMatrix.ps1 -IncludeDev

# Probe keyboard/mouse stubs
pwsh ./tools/orbis/Invoke-OrbisBuildMatrix.ps1 -EnableKeyboard -EnableMouse

# Environment report only
pwsh ./tools/orbis/Invoke-OrbisBuildMatrix.ps1 -SkipBuild
```

## Output

Artifacts are written under:

```text
artifacts/orbis_validation/<timestamp>/
```

Typical output:

```text
env_report.json
summary.json
summary.md
full/info.log
full/build.log
full/stripped.log
lite/info.log
lite/build.log
lite/stripped.log
```

## Audio note

`full` and `dev` profiles default to `ORBIS_ENABLE_AUDIO=1`. `liborbisAudio` is now
available as a submodule at `deps/orbisdev-liborbisAudio/` — build and install it before
running the `full` or `dev` profiles. To skip audio, pass `ORBIS_ENABLE_AUDIO=0`:

```powershell
pwsh ./tools/orbis/Invoke-OrbisBuildMatrix.ps1 -MakeCommand mingw32-make `
  -ExtraArgs "ORBIS_ENABLE_AUDIO=0"
```

Or build `lite` only, which disables audio unconditionally.

## Recommended validation sequence

### OpenOrbis (Windows)

```powershell
$env:OO_PS4_TOOLCHAIN = 'D:\OpenOrbis_v0.5.4\OpenOrbis\PS4Toolchain'
pwsh ./tools/orbis/Invoke-OrbisBuildMatrix.ps1 -MakeCommand mingw32-make
```

### Legacy PS4SDK path

```powershell
$env:PS4SDK = 'C:\path\to\ps4sdk'
pwsh ./tools/orbis/Invoke-OrbisBuildMatrix.ps1 -MakeCommand mingw32-make
```

### Optional toggles pass

```powershell
pwsh ./tools/orbis/Invoke-OrbisBuildMatrix.ps1 -MakeCommand mingw32-make `
  -EnableKeyboard -EnableMouse
```

## What to review after running

- Whether `full` and `lite` build cleanly
- Whether `full-stripped` and `lite-stripped` produce stripped ELFs
- Whether the SDK layout (modern vs legacy) was detected correctly
- Whether the linker script and C++ include root were found when expected
- Whether keyboard/mouse stubs actually exist before enabling those toggles
- ELF file sizes (regression indicator between passes)

## Current known baseline (OpenOrbis v0.5.4 · clang 21 · Windows 11)

| Profile | Result | ELF size |
| ------- | ------ | -------- |
| `lite` | Clean | 2.97 MB |
| `full ORBIS_ENABLE_AUDIO=0` | Clean | 3.0 MB |
| `full` (audio on) | Clean — `liborbisAudio` available in `deps/orbisdev-liborbisAudio/` | ~3.0 MB |
| `dev` | Not yet tested — requires `libps4link` + `libdebugnet` | — |
