param(
    [datetime]$Timestamp = (Get-Date)
)

$repoRoot = git rev-parse --show-toplevel 2>$null
if (-not $?) {
    throw "Run this script inside a Git working tree."
}

$repoRoot = $repoRoot.Trim()
Set-Location $repoRoot

$files = git ls-files
$count = 0

foreach ($file in $files) {
    if ([string]::IsNullOrWhiteSpace($file)) {
        continue
    }

    if (Test-Path -LiteralPath $file) {
        (Get-Item -LiteralPath $file).LastWriteTime = $Timestamp
        $count++
    }
}

Write-Host "Touched $count tracked files to $Timestamp"
Write-Host "Note: GitHub does not track filesystem mtimes. This only changes local file timestamps."
