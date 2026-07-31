param(
    [Parameter(Mandatory = $true)]
    [string]$WaveCli,
    [Parameter(Mandatory = $true)]
    [string]$Project,
    [Parameter(Mandatory = $true)]
    [string]$Operations
)

$output = Get-Content -LiteralPath $Operations -Raw -Encoding utf8 |
    & $WaveCli apply $Project - --dry-run
if ($LASTEXITCODE -ne 0) {
    Write-Error "PowerShell stdin apply failed with exit code $LASTEXITCODE"
    exit 1
}

$result = ($output -join "`n") | ConvertFrom-Json
if (-not $result.ok -or -not $result.dryRun -or $result.written) {
    Write-Error "PowerShell stdin apply returned an invalid JSON contract"
    exit 1
}
