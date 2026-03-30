param(
    [string]$RepoRoot = "",
    [string]$OutputPath = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Resolve-RepoRoot {
    param([string]$Candidate)

    if ($Candidate -and (Test-Path -LiteralPath $Candidate)) {
        return (Resolve-Path -LiteralPath $Candidate).Path
    }

    $root = git rev-parse --show-toplevel 2>$null
    if (-not $?) {
        throw "Run this script inside a Git working tree or pass -RepoRoot."
    }

    return $root.Trim()
}

function Find-CommandPath {
    param([string]$Name)

    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if ($null -eq $cmd) {
        return $null
    }

    return $cmd.Source
}

function Resolve-OrbisLayout {
    param([string]$SdkRoot)

    if ([string]::IsNullOrWhiteSpace($SdkRoot)) {
        return [pscustomobject]@{
            sdk_root = $null
            sdk_source = $null
            toolchain_layout = 'missing'
            include_root = $null
            platform_include_root = $null
            cxx_include_root = $null
            lib_root = $null
            crtfile = $null
            linker_script = $null
        }
    }

    $sdkRoot = (Resolve-Path -LiteralPath $SdkRoot).Path

    $includeRoot = if (Test-Path -LiteralPath (Join-Path $sdkRoot 'usr/include')) {
        Join-Path $sdkRoot 'usr/include'
    } else {
        Join-Path $sdkRoot 'include'
    }

    $toolchainLayout = if ($includeRoot -like '*usr/include') { 'modern' } else { 'legacy' }

    $libRoot = if (Test-Path -LiteralPath (Join-Path $sdkRoot 'usr/lib')) {
        Join-Path $sdkRoot 'usr/lib'
    } else {
        Join-Path $sdkRoot 'lib'
    }

    $platformIncludeRoot = if (Test-Path -LiteralPath (Join-Path $includeRoot 'sce')) {
        Join-Path $includeRoot 'sce'
    } elseif (Test-Path -LiteralPath (Join-Path $includeRoot 'orbis')) {
        Join-Path $includeRoot 'orbis'
    } else {
        $includeRoot
    }

    $cxxIncludeRoot = $null
    foreach ($candidate in @(
        (Join-Path $includeRoot 'c++/v1'),
        (Join-Path $sdkRoot 'usr/include/c++/v1')
    )) {
        if (Test-Path -LiteralPath $candidate) {
            $cxxIncludeRoot = $candidate
            break
        }
    }

    $crtfile = $null
    foreach ($candidate in @(
        (Join-Path $libRoot 'crt0.o'),
        (Join-Path $sdkRoot 'crt0.s')
    )) {
        if (Test-Path -LiteralPath $candidate) {
            $crtfile = $candidate
            break
        }
    }

    $linkerScript = $null
    $linkerCandidate = Join-Path $libRoot 'linker.x'
    if (Test-Path -LiteralPath $linkerCandidate) {
        $linkerScript = $linkerCandidate
    }

    return [pscustomobject]@{
        sdk_root = $sdkRoot
        sdk_source = if ($env:ORBISDEV) { 'ORBISDEV' } elseif ($env:PS4SDK) { 'PS4SDK' } else { 'unknown' }
        toolchain_layout = $toolchainLayout
        include_root = $includeRoot
        platform_include_root = $platformIncludeRoot
        cxx_include_root = $cxxIncludeRoot
        lib_root = $libRoot
        crtfile = $crtfile
        linker_script = $linkerScript
    }
}

function Test-LibraryPresence {
    param(
        [string]$LibRoot,
        [string[]]$Names
    )

    $results = @{}
    if ([string]::IsNullOrWhiteSpace($LibRoot) -or -not (Test-Path -LiteralPath $LibRoot)) {
        foreach ($name in $Names) {
            $results[$name] = $false
        }
        return $results
    }

    $files = Get-ChildItem -LiteralPath $LibRoot -File -Recurse -ErrorAction SilentlyContinue
    foreach ($name in $Names) {
        $results[$name] = [bool]($files | Where-Object { $_.Name -like "lib$name*" -or $_.Name -like "$name*" } | Select-Object -First 1)
    }

    return $results
}

$resolvedRepoRoot = Resolve-RepoRoot -Candidate $RepoRoot
$sdkRoot = if ($env:ORBISDEV) { $env:ORBISDEV } elseif ($env:PS4SDK) { $env:PS4SDK } else { $null }
$layout = Resolve-OrbisLayout -SdkRoot $sdkRoot

$requiredLibs = @(
    'orbisFile',
    'elfloader',
    'orbisKeyboard',
    'orbis2d',
    'orbisGl',
    'orbisPad',
    'orbisAudio',
    'mod',
    'orbisFileBrowser',
    'orbisXbmFont',
    'ps4link',
    'debugnet',
    'SceDbgKeyboard_stub',
    'SceMouse_stub'
)

$report = [pscustomobject]@{
    generated_at = (Get-Date).ToString('o')
    repo_root = $resolvedRepoRoot
    git = [pscustomobject]@{
        executable = Find-CommandPath -Name 'git'
        branch = ((git rev-parse --abbrev-ref HEAD) 2>$null | Out-String).Trim()
        commit = ((git rev-parse HEAD) 2>$null | Out-String).Trim()
    }
    build_tools = [pscustomobject]@{
        make = Find-CommandPath -Name 'make'
        mingw32_make = Find-CommandPath -Name 'mingw32-make'
        clang = Find-CommandPath -Name 'clang'
        pwsh = Find-CommandPath -Name 'pwsh'
        powershell = Find-CommandPath -Name 'powershell'
    }
    sdk = [pscustomobject]@{
        root = $layout.sdk_root
        source = $layout.sdk_source
        toolchain_layout = $layout.toolchain_layout
        include_root = $layout.include_root
        platform_include_root = $layout.platform_include_root
        cxx_include_root = $layout.cxx_include_root
        lib_root = $layout.lib_root
        crtfile = $layout.crtfile
        linker_script = $layout.linker_script
        libraries = (Test-LibraryPresence -LibRoot $layout.lib_root -Names $requiredLibs)
    }
}

if ($OutputPath) {
    $parent = Split-Path -Parent $OutputPath
    if ($parent) {
        New-Item -ItemType Directory -Path $parent -Force | Out-Null
    }

    $report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $OutputPath -Encoding UTF8
}

$report | ConvertTo-Json -Depth 8
