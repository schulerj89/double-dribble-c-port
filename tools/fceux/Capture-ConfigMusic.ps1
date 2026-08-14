[CmdletBinding()]
param(
    [string]$RomPath = 'F:\Games\NES\Double Dribble\Double Dribble (USA) (Rev 1).nes',
    [string]$FceuxPath = 'C:\Users\joshs\Games\fceux-2.6.6-win32\fceux.exe',
    [int]$FinalFrame = 4200
)

$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$captureRoot = Join-Path $projectRoot 'captures\original-config-music'
$scriptPath = Join-Path $PSScriptRoot 'capture_config_music.lua'

if (-not (Test-Path -LiteralPath $RomPath -PathType Leaf)) { throw "ROM not found: $RomPath" }
if (-not (Test-Path -LiteralPath $FceuxPath -PathType Leaf)) { throw "FCEUX not found: $FceuxPath" }
New-Item -ItemType Directory -Force -Path $captureRoot | Out-Null

$env:DD_CAPTURE_ROOT = $captureRoot
$env:DD_ROM_PATH = [System.IO.Path]::GetFullPath($RomPath)
$env:DD_START_FRAME = '75'
$env:DD_TRACE_START = '2080'
$env:DD_CAPTURE_FINAL_FRAME = $FinalFrame.ToString([System.Globalization.CultureInfo]::InvariantCulture)

$process = Start-Process -FilePath $FceuxPath `
    -ArgumentList @('-nothrottle', '1', '-turbo', '1', '-srendline', '0', '-erendline', '239', '-lua', ('"{0}"' -f $scriptPath), ('"{0}"' -f $RomPath)) `
    -WindowStyle Hidden -PassThru -Wait
if ($process.ExitCode -ne 0) { throw "FCEUX exited with code $($process.ExitCode)" }

$statePath = Join-Path $captureRoot 'config-music-state.csv'
if (-not (Test-Path -LiteralPath $statePath -PathType Leaf)) { throw 'FCEUX did not produce the channel-state trace.' }
Write-Host "Configuration music trace: $statePath"
