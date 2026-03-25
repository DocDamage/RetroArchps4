# Orbis validation toolchain

This toolchain is for validating the PS4/Orbis build environment and running a repeatable build matrix on a real machine that has access to the target SDK/toolchain.

## What it solves

The repo changes can be reviewed in GitHub, but real validation still has to happen on a machine that actually has:

- a usable `ORBISDEV` or `PS4SDK`
- `make` or `mingw32-make`
- `clang`
- the expected Orbis stub libraries and headers

This toolchain gives you a standard way to collect that information and produce logs that can be reviewed later.

## Files

- `tools/orbis/Get-OrbisEnvReport.ps1`
- `tools/orbis/Invoke-OrbisBuildMatrix.ps1`

## 1. Environment probe

This script inspects:

- repo root
- current branch and commit
- `git`, `make`, `mingw32-make`, `clang`, `pwsh`, `powershell`
- SDK root source (`ORBISDEV` vs `PS4SDK`)
- modern vs legacy SDK layout
- include root
- platform include root
- C++ include root when present
- lib root
- crt file
- linker script
- presence of important libraries/stubs

### Usage

```powershell
pwsh ./tools/orbis/Get-OrbisEnvReport.ps1
```

Write JSON to a file:

```powershell
pwsh ./tools/orbis/Get-OrbisEnvReport.ps1 -OutputPath ./artifacts/orbis_env_report.json
```

## 2. Build matrix runner

This script runs a repeatable build matrix and writes logs + summary files.

By default it runs:

- `full`
- `lite`

You can optionally include:

- `dev`

You can also optionally test:

- `ORBIS_ENABLE_KEYBOARD=1`
- `ORBIS_ENABLE_MOUSE=1`

### Usage

Default matrix:

```powershell
pwsh ./tools/orbis/Invoke-OrbisBuildMatrix.ps1
```

Use `mingw32-make` explicitly:

```powershell
pwsh ./tools/orbis/Invoke-OrbisBuildMatrix.ps1 -MakeCommand mingw32-make
```

Include the `dev` profile too:

```powershell
pwsh ./tools/orbis/Invoke-OrbisBuildMatrix.ps1 -IncludeDev
```

Probe keyboard/mouse toggles:

```powershell
pwsh ./tools/orbis/Invoke-OrbisBuildMatrix.ps1 -EnableKeyboard -EnableMouse
```

Collect info only without building:

```powershell
pwsh ./tools/orbis/Invoke-OrbisBuildMatrix.ps1 -SkipBuild
```

## Output

By default artifacts are written under:

```text
artifacts/orbis_validation/<timestamp>/
```

Typical output includes:

- `env_report.json`
- `summary.json`
- `summary.md`
- per-profile log files such as:
  - `full/info.log`
  - `full/build.log`
  - `full/stripped.log`
  - `lite/info.log`
  - `lite/build.log`
  - `lite/stripped.log`

## Recommended validation sequence

### Legacy SDK path

```powershell
$env:PS4SDK = 'C:\path\to\ps4sdk'
pwsh ./tools/orbis/Invoke-OrbisBuildMatrix.ps1 -MakeCommand mingw32-make
```

### Modern ORBISDEV path

```powershell
$env:ORBISDEV = 'C:\path\to\orbisdev'
pwsh ./tools/orbis/Invoke-OrbisBuildMatrix.ps1 -MakeCommand mingw32-make
```

### Extra pass for optional toggles

```powershell
pwsh ./tools/orbis/Invoke-OrbisBuildMatrix.ps1 -MakeCommand mingw32-make -EnableKeyboard -EnableMouse
```

## What to review after running it

- whether `full` builds cleanly
- whether `lite` builds cleanly
- whether `full-stripped` and `lite-stripped` work
- whether the SDK layout was detected correctly
- whether keyboard/mouse stubs actually exist before enabling those toggles
- whether the linker script and C++ include root were found when expected

## Reality check

This toolchain does not magically give this environment an Orbis SDK.
It is a local validation harness for a real machine that has the necessary build tools.
