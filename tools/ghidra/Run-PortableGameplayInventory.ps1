[CmdletBinding()]
param(
    [string]$RomPath = 'F:\Games\NES\Double Dribble\Double Dribble (USA) (Rev 1).nes',
    [string]$GhidraHome = 'C:\Users\joshs\Downloads\ghidra_11.3_PUBLIC_20250205\ghidra_11.3_PUBLIC',
    [string]$JdkHome = 'C:\Users\joshs\Downloads\jdk-21\jdk-21.0.6+7'
)

$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$bankRoot = Join-Path $projectRoot 'build\ghidra-input'
$projectDirectory = Join-Path $projectRoot 'ghidra-projects'
$reportRoot = Join-Path $projectRoot 'build\ghidra-reports'
$headless = Join-Path $GhidraHome 'support\analyzeHeadless.bat'
$bank0Json = Join-Path $reportRoot 'portable-gameplay-bank-00.json'
$fixedJson = Join-Path $reportRoot 'portable-gameplay-fixed-07.json'
$manifestPath = Join-Path $projectRoot 'GAMEPLAY_ROUTINES.json'
$annotationPath = Join-Path $projectRoot 'GAMEPLAY_ROUTINE_ANNOTATIONS.json'

& (Join-Path $PSScriptRoot 'Prepare-UxRomBanks.ps1') -RomPath $RomPath
New-Item -ItemType Directory -Force -Path $projectDirectory | Out-Null
New-Item -ItemType Directory -Force -Path $reportRoot | Out-Null
$env:JAVA_HOME = $JdkHome

& $headless $projectDirectory 'DoubleDribblePortableInventoryBank0' `
    -import (Join-Path $bankRoot 'prg-bank-00-switch.bin') -overwrite `
    -loader BinaryLoader -loader-baseAddr 0x8000 -processor '6502:LE:16:default' `
    -scriptPath $PSScriptRoot -postScript 'ExportPortableGameplayInventory.java' bank0 $bank0Json
if ($LASTEXITCODE -ne 0) { throw "Ghidra bank-0 inventory exited with code $LASTEXITCODE" }

& $headless $projectDirectory 'DoubleDribblePortableInventoryFixed7' `
    -import (Join-Path $bankRoot 'prg-bank-07-fixed.bin') -overwrite `
    -loader BinaryLoader -loader-baseAddr 0xC000 -processor '6502:LE:16:default' `
    -scriptPath $PSScriptRoot -postScript 'ExportPortableGameplayInventory.java' fixed7 $fixedJson
if ($LASTEXITCODE -ne 0) { throw "Ghidra fixed-bank inventory exited with code $LASTEXITCODE" }

$documents = @((Get-Content -Raw -LiteralPath $bank0Json | ConvertFrom-Json),
               (Get-Content -Raw -LiteralPath $fixedJson | ConvertFrom-Json))
$excludedNes = @('fixed7:C15E','fixed7:C16A','fixed7:C17B',
                 'fixed7:CC1A','fixed7:CC99','fixed7:CCC1','fixed7:CD9B')
$portablePresentation = @('fixed7:C141','fixed7:C477','fixed7:C4B7',
    'fixed7:C68B','fixed7:C694','fixed7:C6A5','fixed7:C6AD','fixed7:C6CD',
    'fixed7:C6EE','fixed7:C724','fixed7:C783','fixed7:C785',
    'fixed7:D368','fixed7:D3C4','fixed7:D3D5')
$routines = @()
foreach ($document in $documents) {
    foreach ($routine in $document.routines) {
        $key = "$($document.bank):$($routine.address)"
        $classification = if ($excludedNes -contains $key) { 'excluded_nes_mechanism' }
            elseif ($portablePresentation -contains $key) { 'portable_presentation' }
            else { 'portable_logic' }
        $status = if ($classification -eq 'excluded_nes_mechanism') { 'excluded' }
            else { 'partial' }
        $routines += [ordered]@{
            bank = $document.bank
            address = $routine.address
            instruction_count = $routine.instruction_count
            truncated = $routine.truncated
            calls = @($routine.calls)
            tail_calls = @($routine.tail_calls)
            classification = $classification
            status = $status
            evidence = @("Ghidra recursive node $key")
            native_symbols = @()
            regression_tests = @()
        }
    }
}
$routineByKey = @{}
foreach ($routine in $routines) { $routineByKey["$($routine.bank):$($routine.address)"] = $routine }
if (-not (Test-Path -LiteralPath $annotationPath -PathType Leaf)) {
    throw 'Missing GAMEPLAY_ROUTINE_ANNOTATIONS.json.'
}
$annotations = Get-Content -Raw -LiteralPath $annotationPath | ConvertFrom-Json
if ($annotations.schema -ne 1) { throw 'Unsupported GAMEPLAY_ROUTINE_ANNOTATIONS.json schema.' }
$annotatedKeys = @{}
foreach ($group in $annotations.groups) {
    foreach ($key in $group.routines) {
        if (-not $routineByKey.ContainsKey($key)) { throw "Annotation '$($group.name)' names unknown routine $key." }
        if ($annotatedKeys.ContainsKey($key)) { throw "Routine $key appears in multiple annotation groups." }
        $annotatedKeys[$key] = $group.name
        $routine = $routineByKey[$key]
        if ($routine.classification -eq 'excluded_nes_mechanism') {
            throw "Annotation '$($group.name)' attempts to include excluded routine $key."
        }
        $routine.status = $group.status
        $routine.evidence = @($routine.evidence) + @($group.evidence)
        $routine.native_symbols = @($group.native_symbols)
        $routine.regression_tests = @($group.regression_tests)
    }
}
$portableRoutines = @($routines | Where-Object { $_.classification -ne 'excluded_nes_mechanism' })
$verifiedCount = @($portableRoutines | Where-Object { $_.status -eq 'verified' }).Count
$partialCount = @($portableRoutines | Where-Object { $_.status -eq 'partial' }).Count
$missingCount = @($portableRoutines | Where-Object { $_.status -eq 'missing' }).Count
$comprehensivePercent = if ($portableRoutines.Count -eq 0) { 0.0 } else {
    [Math]::Round(100.0 * ($verifiedCount + 0.5 * $partialCount) / $portableRoutines.Count, 1)
}
$manifest = [ordered]@{
    schema = 1
    scope = 'All recursively reachable bank-0 and fixed-bank gameplay routines from the player, user, ball, match, audio-request, rule, and CPU dispatcher roots. Classification is required before comprehensive coverage can be calculated.'
    source = [ordered]@{
        rom = 'Double Dribble (USA) (Rev 1)'
        sha256 = 'BF397EAE9486044FCA90A99215330203D6F85CAB63A8072F28CACCC139B5388CF'
        generator = 'tools/ghidra/ExportPortableGameplayInventory.java'
    }
    banks = @($documents | ForEach-Object {
        [ordered]@{ bank = $_.bank; roots = @($_.roots); routine_count = @($_.routines).Count }
    })
    coverage = [ordered]@{
        comprehensive_percent = $comprehensivePercent
        reason = 'All included nodes begin Partial until routine-level native symbols, dynamic/static evidence, and regression annotations justify promotion.'
        routine_count = $routines.Count
        portable_count = $portableRoutines.Count
        excluded_count = @($routines | Where-Object { $_.classification -eq 'excluded_nes_mechanism' }).Count
        verified_count = $verifiedCount
        partial_count = $partialCount
        missing_count = $missingCount
        unclassified_count = @($routines | Where-Object { $_.classification -eq 'unclassified' }).Count
    }
    routines = $routines
}
$json = $manifest | ConvertTo-Json -Depth 12
[System.IO.File]::WriteAllText($manifestPath, $json + [Environment]::NewLine,
    [System.Text.UTF8Encoding]::new($false))
Write-Host "Portable gameplay manifest: $manifestPath ($($routines.Count) routines, $comprehensivePercent% initial comprehensive coverage)"
