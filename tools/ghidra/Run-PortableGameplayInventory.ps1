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
$routines = @()
foreach ($document in $documents) {
    foreach ($routine in $document.routines) {
        $routines += [ordered]@{
            bank = $document.bank
            address = $routine.address
            instruction_count = $routine.instruction_count
            truncated = $routine.truncated
            calls = @($routine.calls)
            tail_calls = @($routine.tail_calls)
            classification = $routine.classification
            status = $routine.status
            evidence = @()
            native_symbols = @()
            regression_tests = @()
        }
    }
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
        comprehensive_percent = $null
        reason = 'Inventory classification and implementation/evidence annotations are incomplete.'
        routine_count = $routines.Count
        unclassified_count = @($routines | Where-Object { $_.classification -eq 'unclassified' }).Count
    }
    routines = $routines
}
$json = $manifest | ConvertTo-Json -Depth 12
[System.IO.File]::WriteAllText($manifestPath, $json + [Environment]::NewLine,
    [System.Text.UTF8Encoding]::new($false))
Write-Host "Portable gameplay manifest: $manifestPath ($($routines.Count) routines)"
