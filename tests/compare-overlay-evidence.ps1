[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)] [string]$BaselineDirectory,
    [Parameter(Mandatory = $true)] [string]$CurrentDirectory
)

$ErrorActionPreference = 'Stop'
$baselineRoot = (Resolve-Path -LiteralPath $BaselineDirectory).Path
$currentRoot = (Resolve-Path -LiteralPath $CurrentDirectory).Path
# Metrics intentionally differ. Compare the complete model, ordered geometry,
# endpoint/midpoint hit probes, snap edges and both deterministic PNG renders.
$pattern = '\.(model\.json|geometry\.json|canvas\.png|overlays\.png)$'
$baselineFiles = @(Get-ChildItem -LiteralPath $baselineRoot -File |
    Where-Object Name -Match $pattern | Sort-Object Name)
$currentFiles = @(Get-ChildItem -LiteralPath $currentRoot -File |
    Where-Object Name -Match $pattern | Sort-Object Name)
if ($baselineFiles.Count -eq 0 -or $currentFiles.Count -eq 0) {
    throw 'Both runs must emit non-empty overlay evidence.'
}
$nameDifferences = @(Compare-Object @($baselineFiles.Name) @($currentFiles.Name))
if ($nameDifferences.Count -ne 0) {
    throw ('Evidence file sets differ: ' + ($nameDifferences | ConvertTo-Json -Compress))
}
$comparisons = @($baselineFiles | ForEach-Object {
    $baselineHash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
    $currentHash = (Get-FileHash -LiteralPath (Join-Path $currentRoot $_.Name) -Algorithm SHA256).Hash
    [ordered]@{file=$_.Name; equal=($baselineHash -eq $currentHash);
        baselineSha256=$baselineHash; currentSha256=$currentHash}
})
$differences = @($comparisons | Where-Object { -not $_.equal })
[ordered]@{comparedFiles=$comparisons.Count; equal=($differences.Count -eq 0);
    comparisons=$comparisons} | ConvertTo-Json -Depth 4
if ($differences.Count -ne 0) {
    throw ('Full overlay results differ: ' + (($differences | ForEach-Object { $_.file }) -join ', '))
}
