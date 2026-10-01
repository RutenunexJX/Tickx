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
    [switch]$DevelopmentPreview,
    [string]$ValidationSummary
)

$ErrorActionPreference = "Stop"

$sourceDirectory = Split-Path -Parent $PSScriptRoot
$resolvedBuildDirectory = (Resolve-Path -LiteralPath $BuildDirectory).Path
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $resolvedBuildDirectory "package"
}
$fullOutputDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)
if ($DevelopmentPreview) {
    if (-not $StageOnly) { throw "DevelopmentPreview requires -StageOnly." }
    $previewRoot = [System.IO.Path]::GetFullPath((Join-Path $sourceDirectory "build")).TrimEnd('\') + '\'
    if (-not $fullOutputDirectory.StartsWith($previewRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "DevelopmentPreview must stay under this repository's build directory."
    }
}
New-Item -ItemType Directory -Force -Path $fullOutputDirectory | Out-Null
$fullOutputDirectory = (Resolve-Path -LiteralPath $fullOutputDirectory).Path

$cmakeCache = Join-Path $resolvedBuildDirectory "CMakeCache.txt"
if (-not (Test-Path -LiteralPath $cmakeCache -PathType Leaf)) {
    throw "BuildDirectory is not a configured CMake build tree: $resolvedBuildDirectory"
}

$versionMatch = Select-String -LiteralPath (Join-Path $sourceDirectory "CMakeLists.txt") `
    -Pattern 'project\(Tickx VERSION ([0-9]+\.[0-9]+\.[0-9]+)' | Select-Object -First 1
if ($null -eq $versionMatch) {
    throw "Cannot determine the Tickx version from CMakeLists.txt."
}
$version = $versionMatch.Matches[0].Groups[1].Value
$packageName = if ($DevelopmentPreview) { "Tickx-$version-development-windows-x64" } else { "Tickx-$version-windows-x64" }
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

$revision = (& git -C $sourceDirectory rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw "Cannot record the staging source revision." }
$branch = (& git -C $sourceDirectory branch --show-current).Trim()
if ($LASTEXITCODE -ne 0) { throw "Cannot record the source branch." }
$sourceChanges = @(& git -C $sourceDirectory status --porcelain --untracked-files=normal)
if ($LASTEXITCODE -ne 0) { throw "Cannot record the source changes." }
$sourceDirty = $sourceChanges.Count -ne 0
if ($sourceDirty -and -not $DevelopmentPreview) {
    throw "Release packaging requires a committed, clean source tree. Use -StageOnly -DevelopmentPreview for local development."
}

if (-not $SkipBuild) {
    & $CMakeExecutable --build $resolvedBuildDirectory --config $Configuration `
        --target Tickx wave-cli wave-generate wave-compare wave-bridge wave-sim-runner
    if ($LASTEXITCODE -ne 0) {
        throw "CMake build failed with exit code $LASTEXITCODE."
    }
}

$buildConfigPath = Join-Path $resolvedBuildDirectory "tickx-build-config.json"
if (-not (Test-Path -LiteralPath $buildConfigPath -PathType Leaf)) {
    throw "Reconfigure this build for Tickx before packaging."
}
$buildConfig = Get-Content -LiteralPath $buildConfigPath -Raw | ConvertFrom-Json
if ($buildConfig.application -ne "Tickx" -or $buildConfig.version -ne $version) {
    throw "The configured build does not match the Tickx source version."
}

if (-not $StageOnly) {
    foreach ($path in @($stageDirectory, $archivePath, $hashPath)) {
        if (-not [System.IO.Path]::GetFullPath($path).StartsWith($expectedParent, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to remove a path outside the requested output directory: $path"
        }
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
    "Tickx.exe",
    "wave-cli.exe",
    "wave-generate.exe",
    "wave-compare.exe",
    "wave-bridge.exe",
    "wave-sim-runner.exe"
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

foreach ($obsoleteExecutable in @("wave-workbench.exe", "WaveWorkbench.exe")) {
    if (Test-Path -LiteralPath (Join-Path $stageDirectory $obsoleteExecutable)) {
        throw "An obsolete GUI executable entered the Tickx package: $obsoleteExecutable"
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

$metadata = [ordered]@{
    application = "Tickx"
    executable = "Tickx.exe"
    repository = "https://github.com/RutenunexJX/Tickx"
    version = $version
    revision = $revision
    branch = $branch
    sourceDirty = $sourceDirty
    sourceChanges = $sourceChanges
    developmentPreview = [bool]$DevelopmentPreview
    suiteAppEnabled = [bool]$buildConfig.suiteApp
    wellenEnabled = [bool]$buildConfig.wellen
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
    archiveCreated = -not $StageOnly
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

if ($StageOnly) {
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
