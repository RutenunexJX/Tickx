param(
    [Parameter(Mandatory=$true)][string]$TickxDirectory,
    [string]$OutputDirectory = (Join-Path (Get-Location) ("tickx-session-" + (Get-Date -Format 'yyyyMMdd-HHmmss')))
)
$ErrorActionPreference = 'Stop'
$cli = Join-Path $TickxDirectory 'wave-cli.exe'
$gui = Join-Path $TickxDirectory 'Tickx.exe'
if (-not (Test-Path -LiteralPath $cli) -or -not (Test-Path -LiteralPath $gui)) {
    throw 'TickxDirectory must contain this build of wave-cli.exe and Tickx.exe.'
}
[IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null
$OutputDirectory = (Resolve-Path -LiteralPath $OutputDirectory).Path
$project = Join-Path $OutputDirectory '示例 waveform.wave.json'
if (Test-Path -LiteralPath $project) { throw "Example project already exists: $project" }
function Write-Json([string]$Path, $Value) {
    [IO.File]::WriteAllText($Path, ($Value | ConvertTo-Json -Depth 30), [Text.UTF8Encoding]::new($false))
}
function Invoke-Json([string[]]$CommandArgs) {
    $text = & $cli @CommandArgs
    if ($LASTEXITCODE -ne 0) { throw "wave-cli exit $LASTEXITCODE : $text" }
    return ($text | ConvertFrom-Json)
}
$runtime = 'suiteapp.tickx.example.' + [Guid]::NewGuid().ToString()
$created = Invoke-Json -CommandArgs @('new', $project, '--project-id=cli-example', '--scenario-id=main',
    '--name=CLI session example', '--duration=100 ns', ("--operations=" + (Join-Path $PSScriptRoot 'create.json')))
$profile = Join-Path $OutputDirectory 'settings'
$launched = Start-Process -FilePath $gui -ArgumentList @(
    ('"' + $project + '"'), ('"--settings-dir=' + $profile + '"'), ('--suite-runtime=' + $runtime)
) -PassThru
$deadline = [DateTime]::UtcNow.AddSeconds(30)
$target = $null
do {
    $text = & $cli session list "--runtime=$runtime" 2>$null
    if ($LASTEXITCODE -eq 0) {
        $rows = ($text | ConvertFrom-Json).result.sessions
        $target = @($rows | Where-Object { $_.reachable -and $_.state.processId -eq $launched.Id })
        if ($target.Count -eq 1) { break }
    }
    Start-Sleep -Milliseconds 200
} while ([DateTime]::UtcNow -lt $deadline)
if (@($target).Count -ne 1) { throw 'The example window did not register. Check the bundled SuiteApp Runtime 1.0.2+.' }
$id = $target[0].sessionId
$common = @("--runtime=$runtime", "--session=$id")
$viewPath = Join-Path $OutputDirectory 'view.json'
Write-Json $viewPath @{
    scenarioId='main'; cursor='25 ns'; start='10 ns'; end='50 ns'
    laneIds=@('group-core'); range=@{start='20 ns';end='30 ns'}
}
$view = Invoke-Json (@('session','view',"--args=$viewPath") + $common)
$operations = Join-Path $OutputDirectory 'online.operations.json'
Write-Json $operations @{
    schema='wave-workbench.operations/v1'
    operations=@(@{op='rename-lane';laneId='lane-data';name='data_online'})
}
$state = (Invoke-Json (@('session','status') + $common)).result.state
$preview = Invoke-Json (@('session','apply','--scenario=main',"--ops=$operations",'--dry-run') + $common)
$edit = Invoke-Json (@('session','apply','--scenario=main',"--ops=$operations",
    "--expect-model-sha256=$($state.modelSha256)") + $common)
$state = $edit.result.state
$save = Invoke-Json (@('session','save',"--expect-model-sha256=$($state.modelSha256)",
    "--expect-sha256=$($state.loadedSha256)") + $common)

$external = Join-Path $OutputDirectory 'external.operations.json'
Write-Json $external @{
    schema='wave-workbench.operations/v1'
    operations=@(@{op='rename-lane';laneId='lane-request';name='request_from_file'})
}
$written = Invoke-Json -CommandArgs @('apply',$project,$external,'--in-place','--scenario=main',
    "--expect-sha256=$($save.result.state.loadedSha256)")
$adopted = Invoke-Json (@('session','refresh',"--expect-sha256=$($written.resultSha256)",'--wait-ms=3000') + $common)
if ($adopted.result.state.loadedSha256 -ne $written.resultSha256) { throw 'The window did not adopt the same-path update.' }

$reopened = Invoke-Json (@('session','open',"--project=$project",
    "--expect-sha256=$($written.resultSha256)") + $common)
$view = Invoke-Json (@('session','view',"--args=$viewPath") + $common)
$export = Join-Path $OutputDirectory 'export'
& $cli generate $project $export --scenario=main | Set-Content -LiteralPath (Join-Path $OutputDirectory 'export.log') -Encoding utf8
if ($LASTEXITCODE -ne 0) { throw 'Export failed.' }
Write-Json (Join-Path $OutputDirectory 'result.json') @{
    sessionId=$id; processId=$launched.Id; runtime=$runtime; project=$project
    created=$created.resultSha256; adopted=$adopted.result.state.loadedSha256
    view=$view.result.state.view; export=$export
}
Write-Output "Completed. Result: $(Join-Path $OutputDirectory 'result.json')"
Write-Output "The example window remains open. Session: $id ; Runtime: $runtime"
