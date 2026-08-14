[CmdletBinding()]
param(
    [string]$RomPath = 'F:\Games\NES\Double Dribble\Double Dribble (USA) (Rev 1).nes',
    [string]$FceuxPath = 'C:\Users\joshs\Games\fceux-2.6.6-win32\fceux.exe',
    [int]$StartFrame = 75,
    [int]$FinalFrame = 900
)

$ErrorActionPreference = 'Stop'

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$captureRoot = Join-Path $projectRoot 'captures\original-1p'
$scriptPath = Join-Path $PSScriptRoot 'capture_original_title.lua'

if (-not (Test-Path -LiteralPath $RomPath -PathType Leaf)) {
    throw "ROM not found: $RomPath"
}
if (-not (Test-Path -LiteralPath $FceuxPath -PathType Leaf)) {
    throw "FCEUX not found: $FceuxPath"
}

New-Item -ItemType Directory -Force -Path $captureRoot | Out-Null

$env:DD_CAPTURE_ROOT = $captureRoot
$env:DD_ROM_PATH = [System.IO.Path]::GetFullPath($RomPath)
$env:DD_CAPTURE_FINAL_FRAME = $FinalFrame.ToString([System.Globalization.CultureInfo]::InvariantCulture)
$env:DD_START_FRAME = $StartFrame.ToString([System.Globalization.CultureInfo]::InvariantCulture)
$env:DD_INTRO_TRACE = '1'

Write-Host "Capturing the original 1P path (Start at frame $StartFrame) through frame $FinalFrame..."
$process = Start-Process -FilePath $FceuxPath `
    -ArgumentList @('-nothrottle', '1', '-turbo', '1', '-srendline', '0', '-erendline', '239', '-lua', ('"{0}"' -f $scriptPath), ('"{0}"' -f $RomPath)) `
    -WindowStyle Hidden `
    -PassThru `
    -Wait
if ($process.ExitCode -ne 0) {
    throw "FCEUX exited with code $($process.ExitCode)"
}

$screenshots = Get-ChildItem -LiteralPath $captureRoot -Filter 'frame-*.png'
if ($screenshots.Count -eq 0) {
    throw "FCEUX completed without producing screenshots in $captureRoot"
}

Write-Host "Captured $($screenshots.Count) 1P reference screenshots in $captureRoot"
