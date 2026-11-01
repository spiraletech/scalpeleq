param(
    [Parameter(Mandatory=$true)][string]$Pluginval,
    [Parameter(Mandatory=$true)][string]$Bundle,
    [Parameter(Mandatory=$true)][string]$OutputDir,
    [int]$Cycles=3,
    [int]$TimeoutSeconds=60
)
$ErrorActionPreference='Stop'
$validatorPath=(Resolve-Path -LiteralPath $Pluginval).Path
$bundlePath=(Resolve-Path -LiteralPath $Bundle).Path
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$evidencePath=(Resolve-Path -LiteralPath $OutputDir).Path
$results=@()
for($cycle=0;$cycle -lt $Cycles;$cycle++){
    $cyclePath=Join-Path $evidencePath ('cycle-'+$cycle)
    New-Item -ItemType Directory -Force -Path $cyclePath | Out-Null
    $stdout=Join-Path $cyclePath 'stdout.log'
    $stderr=Join-Path $cyclePath 'stderr.log'
    $arguments=@('--strictness-level','8','--random-seed',([string](12345+$cycle)),
                 '--output-dir',('"'+$cyclePath+'"'),'--validate',('"'+$bundlePath+'"'))
    $started=[DateTime]::UtcNow
    $process=Start-Process -FilePath $validatorPath -ArgumentList $arguments -PassThru -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    if(-not $process.WaitForExit($TimeoutSeconds*1000)){
        Stop-Process -Id $process.Id -ErrorAction SilentlyContinue
        throw "VST3 host cycle $cycle did not exit within $TimeoutSeconds seconds. SUCCESS text alone is not a pass."
    }
    $process.WaitForExit()
    $reports=Get-ChildItem -LiteralPath $cyclePath -Filter '*.txt'
    if($process.ExitCode -ne 0 -or -not $reports -or -not (Select-String -Path $reports.FullName -Pattern '^SUCCESS$')){
        throw "VST3 host cycle $cycle failed; exit $($process.ExitCode)."
    }
    $results+=[pscustomobject]@{cycle=$cycle;exitCode=$process.ExitCode;seconds=([DateTime]::UtcNow-$started).TotalSeconds;guiTests=$true;strictness=8;seed=12345+$cycle}
    Write-Output "VST3 host cycle ${cycle}: SUCCESS and clean process exit 0"
}
$results | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $evidencePath 'host-unload-results.json') -Encoding utf8
