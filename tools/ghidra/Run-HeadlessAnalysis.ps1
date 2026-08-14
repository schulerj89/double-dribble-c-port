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
$reportPath = Join-Path $projectRoot 'build\ghidra-reports\fixed-bank-07.txt'
$fixedBankPath = Join-Path $bankRoot 'prg-bank-07-fixed.bin'
$headless = Join-Path $GhidraHome 'support\analyzeHeadless.bat'

& (Join-Path $PSScriptRoot 'Prepare-UxRomBanks.ps1') -RomPath $RomPath

if (-not (Test-Path -LiteralPath $headless -PathType Leaf)) {
    throw "Ghidra headless analyzer not found: $headless"
}
if (-not (Test-Path -LiteralPath (Join-Path $JdkHome 'bin\java.exe') -PathType Leaf)) {
    throw "JDK not found: $JdkHome"
}

New-Item -ItemType Directory -Force -Path $projectDirectory | Out-Null
$env:JAVA_HOME = $JdkHome

& $headless $projectDirectory 'DoubleDribbleRev1' `
    -import $fixedBankPath `
    -overwrite `
    -loader BinaryLoader `
    -loader-baseAddr 0xC000 `
    -processor '6502:LE:16:default' `
    -scriptPath $PSScriptRoot `
    -postScript 'ExportEvidence.java' $reportPath

if ($LASTEXITCODE -ne 0) {
    throw "Ghidra headless analysis exited with code $LASTEXITCODE"
}

Write-Host "Ghidra report: $reportPath"

