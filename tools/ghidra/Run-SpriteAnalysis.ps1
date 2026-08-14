[CmdletBinding()]
param(
    [string]$RomPath = 'F:\Games\NES\Double Dribble\Double Dribble (USA) (Rev 1).nes',
    [string]$GhidraHome = 'C:\Users\joshs\Downloads\ghidra_11.3_PUBLIC_20250205\ghidra_11.3_PUBLIC',
    [string]$JdkHome = 'C:\Users\joshs\Downloads\jdk-21\jdk-21.0.6+7'
)

$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$bankPath = Join-Path $projectRoot 'build\ghidra-input\prg-bank-02-switch.bin'
$projectDirectory = Join-Path $projectRoot 'ghidra-projects'
$reportPath = Join-Path $projectRoot 'build\ghidra-reports\switch-bank-02-sprites.txt'
$headless = Join-Path $GhidraHome 'support\analyzeHeadless.bat'

& (Join-Path $PSScriptRoot 'Prepare-UxRomBanks.ps1') -RomPath $RomPath
New-Item -ItemType Directory -Force -Path $projectDirectory | Out-Null
$env:JAVA_HOME = $JdkHome
& $headless $projectDirectory 'DoubleDribbleIntroBank2' -import $bankPath -overwrite `
    -loader BinaryLoader -loader-baseAddr 0x8000 -processor '6502:LE:16:default' `
    -scriptPath $PSScriptRoot -postScript 'ExportSpriteEvidence.java' $reportPath
if ($LASTEXITCODE -ne 0) { throw "Ghidra headless sprite analysis exited with code $LASTEXITCODE" }
Write-Host "Ghidra sprite report: $reportPath"
