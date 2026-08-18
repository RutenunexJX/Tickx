[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDirectory,

    [string]$OutputDirectory,
    [string]$CMakeExecutable = "cmake",
    [string]$QtBinDirectory,
    [string]$Configuration = "Release",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

$sourceDirectory = Split-Path -Parent $PSScriptRoot
$resolvedBuildDirectory = (Resolve-Path -LiteralPath $BuildDirectory).Path
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $resolvedBuildDirectory "package"
}
$fullOutputDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $fullOutputDirectory | Out-Null
$fullOutputDirectory = (Resolve-Path -LiteralPath $fullOutputDirectory).Path

$cmakeCache = Join-Path $resolvedBuildDirectory "CMakeCache.txt"
if (-not (Test-Path -LiteralPath $cmakeCache -PathType Leaf)) {
    throw "BuildDirectory is not a configured CMake build tree: $resolvedBuildDirectory"
}

$versionMatch = Select-String -LiteralPath (Join-Path $sourceDirectory "CMakeLists.txt") `
    -Pattern 'project\(WaveWorkbench VERSION ([0-9]+\.[0-9]+\.[0-9]+)' | Select-Object -First 1
if ($null -eq $versionMatch) {
    throw "Cannot determine the Wave Workbench version from CMakeLists.txt."
}
$version = $versionMatch.Matches[0].Groups[1].Value
$packageName = "WaveWorkbench-$version-windows-x64"
$stageDirectory = Join-Path $fullOutputDirectory $packageName
$archivePath = "$stageDirectory.zip"
$hashPath = "$archivePath.sha256"

$expectedParent = [System.IO.Path]::GetFullPath($fullOutputDirectory).TrimEnd('\') + '\'
$resolvedStage = [System.IO.Path]::GetFullPath($stageDirectory)
if (-not $resolvedStage.StartsWith($expectedParent, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to stage outside the requested output directory: $resolvedStage"
}

if (-not $SkipBuild) {
    & $CMakeExecutable --build $resolvedBuildDirectory --config $Configuration `
        --target wave-workbench wave-cli wave-generate wave-compare wave-bridge
    if ($LASTEXITCODE -ne 0) {
        throw "CMake build failed with exit code $LASTEXITCODE."
    }
}

foreach ($path in @($stageDirectory, $archivePath, $hashPath)) {
    if (Test-Path -LiteralPath $path) {
        Remove-Item -LiteralPath $path -Recurse -Force
    }
}

& $CMakeExecutable --install $resolvedBuildDirectory --config $Configuration `
    --prefix $stageDirectory --component Portable
if ($LASTEXITCODE -ne 0) {
    throw "CMake portable install failed with exit code $LASTEXITCODE."
}

if ([string]::IsNullOrWhiteSpace($QtBinDirectory)) {
    $deployCommand = Get-Command windeployqt.exe -ErrorAction SilentlyContinue
    if ($null -eq $deployCommand) {
        throw "windeployqt.exe was not found. Pass -QtBinDirectory from the selected Qt kit."
    }
    $windeployqt = $deployCommand.Source
} else {
    $windeployqt = Join-Path ([System.IO.Path]::GetFullPath($QtBinDirectory)) "windeployqt.exe"
    if (-not (Test-Path -LiteralPath $windeployqt -PathType Leaf)) {
        throw "windeployqt.exe does not exist: $windeployqt"
    }
}

foreach ($executable in @(
    "wave-workbench.exe",
    "wave-cli.exe",
    "wave-generate.exe",
    "wave-compare.exe",
    "wave-bridge.exe"
)) {
    $executablePath = Join-Path $stageDirectory $executable
    if (-not (Test-Path -LiteralPath $executablePath -PathType Leaf)) {
        throw "Portable install omitted required executable: $executable"
    }
    & $windeployqt --release --dir $stageDirectory $executablePath
    if ($LASTEXITCODE -ne 0) {
        throw "windeployqt failed for $executable with exit code $LASTEXITCODE."
    }
}

foreach ($requiredPath in @(
    "wave-cli.exe",
    "docs\automation-cli.md",
    "docs\cli-quick-start.md",
    "docs\examples\quick-start.operations.json",
    "schemas\automation\v1\capabilities.schema.json",
    "schemas\automation\v1\report.schema.json",
    "schemas\automation\v1\operation-batch.schema.json"
)) {
    if (-not (Test-Path -LiteralPath (Join-Path $stageDirectory $requiredPath))) {
        throw "Portable package contract is incomplete: $requiredPath"
    }
}

Compress-Archive -LiteralPath $stageDirectory -DestinationPath $archivePath -CompressionLevel Optimal
$hash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()
Set-Content -LiteralPath $hashPath -Encoding ascii -NoNewline `
    -Value "$hash  $([System.IO.Path]::GetFileName($archivePath))"

Write-Output $archivePath
Write-Output $hashPath
