param(
    [string]$RepoRoot = "",
    [string]$MakeCommand = "make",
    [string]$OutputRoot = "",
    [switch]$IncludeDev,
    [switch]$EnableKeyboard,
    [switch]$EnableMouse,
    [switch]$SkipBuild,
    [switch]$SkipStripped
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

function Invoke-Step {
    param(
        [string]$Name,
        [string[]]$Arguments,
        [string]$WorkingDirectory,
        [string]$LogPath
    )

    $output = & $MakeCommand @Arguments 2>&1
    $exitCode = $LASTEXITCODE

    $text = ($output | Out-String)
    Set-Content -LiteralPath $LogPath -Value $text -Encoding UTF8

    return [pscustomobject]@{
        name = $Name
        arguments = $Arguments -join ' '
        exit_code = $exitCode
        succeeded = ($exitCode -eq 0)
        log = $LogPath
    }
}

$resolvedRepoRoot = Resolve-RepoRoot -Candidate $RepoRoot
Set-Location $resolvedRepoRoot

$timestamp = Get-Date -Format 'yyyyMMdd_HHmmss'
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $resolvedRepoRoot "artifacts/orbis_validation/$timestamp"
}
New-Item -ItemType Directory -Path $OutputRoot -Force | Out-Null

$envReportPath = Join-Path $OutputRoot 'env_report.json'
$envReportRaw = & (Join-Path $resolvedRepoRoot 'tools/orbis/Get-OrbisEnvReport.ps1') -RepoRoot $resolvedRepoRoot -OutputPath $envReportPath
$envReport = $envReportRaw | ConvertFrom-Json

$profiles = @('full', 'lite')
if ($IncludeDev) {
    $profiles += 'dev'
}

$matrixResults = @()
foreach ($profile in $profiles) {
    $profileDir = Join-Path $OutputRoot $profile
    New-Item -ItemType Directory -Path $profileDir -Force | Out-Null

    $commonArgs = @('-f', 'Makefile.orbis', "ORBIS_BUILD_PROFILE=$profile")
    if ($EnableKeyboard) { $commonArgs += 'ORBIS_ENABLE_KEYBOARD=1' }
    if ($EnableMouse) { $commonArgs += 'ORBIS_ENABLE_MOUSE=1' }

    $infoResult = Invoke-Step -Name "$profile-info" -Arguments (@('info') + $commonArgs) -WorkingDirectory $resolvedRepoRoot -LogPath (Join-Path $profileDir 'info.log')
    $profileResults = @($infoResult)

    if (-not $SkipBuild) {
        $buildTarget = if ($profile -eq 'dev') { 'dev' } else { $profile }
        $profileResults += Invoke-Step -Name "$profile-build" -Arguments (@($buildTarget) + $commonArgs) -WorkingDirectory $resolvedRepoRoot -LogPath (Join-Path $profileDir 'build.log')

        if (-not $SkipStripped -and $profile -in @('full', 'lite')) {
            $profileResults += Invoke-Step -Name "$profile-stripped" -Arguments (@("$profile-stripped") + $commonArgs) -WorkingDirectory $resolvedRepoRoot -LogPath (Join-Path $profileDir 'stripped.log')
        }
    }

    $matrixResults += [pscustomobject]@{
        profile = $profile
        steps = $profileResults
    }
}

$summary = [pscustomobject]@{
    generated_at = (Get-Date).ToString('o')
    repo_root = $resolvedRepoRoot
    make_command = $MakeCommand
    output_root = $OutputRoot
    toggles = [pscustomobject]@{
        include_dev = [bool]$IncludeDev
        enable_keyboard = [bool]$EnableKeyboard
        enable_mouse = [bool]$EnableMouse
        skip_build = [bool]$SkipBuild
        skip_stripped = [bool]$SkipStripped
    }
    env_report = $envReport
    profiles = $matrixResults
}

$summaryJsonPath = Join-Path $OutputRoot 'summary.json'
$summary | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $summaryJsonPath -Encoding UTF8

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add('# Orbis build matrix summary')
$lines.Add('')
$lines.Add("Generated: $($summary.generated_at)")
$lines.Add("")
$lines.Add("Make command: `$MakeCommand`")
$lines.Add("")
$lines.Add('## SDK')
$lines.Add("")
$lines.Add("- source: $($envReport.sdk.source)")
$lines.Add("- root: $($envReport.sdk.root)")
$lines.Add("- layout: $($envReport.sdk.toolchain_layout)")
$lines.Add("- include root: $($envReport.sdk.include_root)")
$lines.Add("- lib root: $($envReport.sdk.lib_root)")
$lines.Add("- crtfile: $($envReport.sdk.crtfile)")
$lines.Add("- linker script: $($envReport.sdk.linker_script)")
$lines.Add("")
$lines.Add('## Profiles')
$lines.Add('')

foreach ($profile in $matrixResults) {
    $lines.Add("### $($profile.profile)")
    $lines.Add('')
    foreach ($step in $profile.steps) {
        $status = if ($step.succeeded) { 'PASS' } else { 'FAIL' }
        $lines.Add("- [$status] $($step.name) — exit $($step.exit_code) — `$(Split-Path -Leaf $step.log)`")
    }
    $lines.Add('')
}

$summaryMdPath = Join-Path $OutputRoot 'summary.md'
$lines | Set-Content -LiteralPath $summaryMdPath -Encoding UTF8

Write-Host "Wrote validation artifacts to $OutputRoot"
Write-Host "Summary JSON: $summaryJsonPath"
Write-Host "Summary MD:   $summaryMdPath"
