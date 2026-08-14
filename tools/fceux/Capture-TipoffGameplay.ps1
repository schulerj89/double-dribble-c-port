[CmdletBinding()]
param(
    [string]$RomPath = 'F:\Games\NES\Double Dribble\Double Dribble (USA) (Rev 1).nes',
    [string]$FceuxPath = 'C:\Users\joshs\Games\fceux-2.6.6-win32\fceux.exe',
    [int]$TraceStart = 2320,
    [int]$FinalFrame = 2760,
    [string]$CaptureName = 'original-tipoff-gameplay',
    [int]$JumpStart = -1,
    [int]$JumpEnd = -1,
    [ValidateSet('A', 'B')]
    [string]$JumpButton = 'A'
)

$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$captureRoot = Join-Path $projectRoot (Join-Path 'captures' $CaptureName)
$scriptPath = Join-Path $PSScriptRoot 'capture_tipoff_gameplay.lua'
if (-not (Test-Path -LiteralPath $RomPath -PathType Leaf)) { throw "ROM not found: $RomPath" }
if (-not (Test-Path -LiteralPath $FceuxPath -PathType Leaf)) { throw "FCEUX not found: $FceuxPath" }
New-Item -ItemType Directory -Force -Path $captureRoot | Out-Null
$env:DD_CAPTURE_ROOT = $captureRoot
$env:DD_ROM_PATH = [System.IO.Path]::GetFullPath($RomPath)
$env:DD_CAPTURE_FINAL_FRAME = $FinalFrame.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_TRACE_START = $TraceStart.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_TRACE_END = $FinalFrame.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_TIP_JUMP_START = $JumpStart.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_TIP_JUMP_END = $JumpEnd.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_TIP_JUMP_BUTTON = $JumpButton

$process = Start-Process -FilePath $FceuxPath `
    -ArgumentList @('-nothrottle', '1', '-turbo', '1', '-srendline', '0', '-erendline', '239', '-lua', ('"{0}"' -f $scriptPath), ('"{0}"' -f $RomPath)) `
    -WindowStyle Hidden -PassThru -Wait
if ($process.ExitCode -ne 0) { throw "FCEUX exited with code $($process.ExitCode)" }
$statePath = Join-Path $captureRoot 'tipoff-state.bin'
if (-not (Test-Path -LiteralPath $statePath -PathType Leaf)) { throw 'FCEUX did not produce the tip-off state trace.' }
Write-Host "Tip-off/gameplay trace: $captureRoot"
