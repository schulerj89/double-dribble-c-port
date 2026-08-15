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

& (Join-Path $PSScriptRoot 'Prepare-UxRomBanks.ps1') -RomPath $RomPath
New-Item -ItemType Directory -Force -Path $projectDirectory, $reportRoot | Out-Null
$env:JAVA_HOME = $JdkHome

& $headless $projectDirectory 'DoubleDribbleDunkFixed7' `
    -import (Join-Path $bankRoot 'prg-bank-07-fixed.bin') -overwrite `
    -loader BinaryLoader -loader-baseAddr 0xC000 -processor '6502:LE:16:default' `
    -scriptPath $PSScriptRoot -postScript 'ExportDunkEvidence.java' 'fixed' `
    (Join-Path $reportRoot 'dunk-fixed-bank-07.txt')
if ($LASTEXITCODE -ne 0) { throw "Ghidra fixed-bank dunk analysis exited with code $LASTEXITCODE" }

& $headless $projectDirectory 'DoubleDribbleDunkBank2' `
    -import (Join-Path $bankRoot 'prg-bank-02-switch.bin') -overwrite `
    -loader BinaryLoader -loader-baseAddr 0x8000 -processor '6502:LE:16:default' `
    -scriptPath $PSScriptRoot -postScript 'ExportDunkEvidence.java' 'bank2' `
    (Join-Path $reportRoot 'dunk-bank-02.txt')
if ($LASTEXITCODE -ne 0) { throw "Ghidra bank-2 dunk analysis exited with code $LASTEXITCODE" }

Write-Host "Ghidra dunk reports: $reportRoot"
