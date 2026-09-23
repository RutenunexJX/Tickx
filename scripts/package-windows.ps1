[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDirectory,

    [string]$OutputDirectory,
    [string]$CMakeExecutable = "cmake",
    [string]$QtBinDirectory,
    [string]$Configuration = "Release",
    [switch]$SkipBuild,
    [switch]$StageOnly,
    [string]$ValidationSummary
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
if ($StageOnly -and (Test-Path -LiteralPath $stageDirectory)) {
    throw "StageOnly requires a fresh staging directory: $stageDirectory"
}

$expectedParent = [System.IO.Path]::GetFullPath($fullOutputDirectory).TrimEnd('\') + '\'
$resolvedStage = [System.IO.Path]::GetFullPath($stageDirectory)
if (-not $resolvedStage.StartsWith($expectedParent, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to stage outside the requested output directory: $resolvedStage"
}

if ($StageOnly) {
    $revision = (& git -C $sourceDirectory rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0) { throw "Cannot record the staging source revision." }
    $branch = (& git -C $sourceDirectory branch --show-current).Trim()
    $sourceChanges = & git -C $sourceDirectory status --porcelain --untracked-files=no
    if ($LASTEXITCODE -ne 0 -or $sourceChanges) {
        throw "StageOnly requires a committed, clean source tree."
    }
}

if (-not $SkipBuild) {
    & $CMakeExecutable --build $resolvedBuildDirectory --config $Configuration `
        --target wave-workbench wave-cli wave-generate wave-compare wave-bridge
    if ($LASTEXITCODE -ne 0) {
        throw "CMake build failed with exit code $LASTEXITCODE."
    }
}

if (-not $StageOnly) {
    foreach ($path in @($stageDirectory, $archivePath, $hashPath)) {
        if (Test-Path -LiteralPath $path) {
            Remove-Item -LiteralPath $path -Recurse -Force
        }
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

if ($StageOnly) {
    $metadata = [ordered]@{
        application = "WaveWorkbench"
        version = $version
        revision = $revision
        branch = $branch
        configuration = $Configuration
        platform = "windows-x64"
        qtVersion = "6.10.2"
        privateRuntime = "WaveWorkbenchEla.dll"
        upstreamElaRevision = "454cac2d57a47d3cc28577dc817793aec1881ca7"
        capabilityReference = "75180fad5e5f5142684cf092649deffe5720994d"
        listViewLifetimeFix = "xIPs source patch 28; private ABI unchanged"
        overlayOriginLifetimeFix = "guarded origin/area, replacement teardown regression; Wave vendor patch 13"
        comboPopupPaddingFix = "ZeroSlack/RegMap patch 30; Wave vendor patch 14"
        comboPopupSourceSha256 = "e69b815ba035831e2a84484acb0c46f957c83f346fff230a8c2b3034d4a44046"
        generatedUtc = [DateTime]::UtcNow.ToString("o")
        validation = $ValidationSummary
        archiveCreated = $false
        formalDirectoryReplaced = $false
    }
    $metadata | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $stageDirectory "build-info.json") -Encoding utf8
    $checksums = Get-ChildItem -LiteralPath $stageDirectory -Recurse -File |
        Sort-Object FullName | ForEach-Object {
            $relative = $_.FullName.Substring($stageDirectory.Length + 1).Replace('\', '/')
            $digest = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            "$digest  $relative"
        }
    $checksums | Set-Content -LiteralPath (Join-Path $stageDirectory "SHA256SUMS.txt") -Encoding ascii
    Write-Output $stageDirectory
    Write-Output (Join-Path $stageDirectory "build-info.json")
    Write-Output (Join-Path $stageDirectory "SHA256SUMS.txt")
    return
}

Compress-Archive -LiteralPath $stageDirectory -DestinationPath $archivePath -CompressionLevel Optimal
$hash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()
Set-Content -LiteralPath $hashPath -Encoding ascii -NoNewline `
    -Value "$hash  $([System.IO.Path]::GetFileName($archivePath))"

Write-Output $archivePath
Write-Output $hashPath
