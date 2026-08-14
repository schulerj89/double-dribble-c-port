[CmdletBinding()]
param(
    [string]$AssetPackPath = '',
    [string]$CliPath = ''
)

$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if ($AssetPackPath -eq '') { $AssetPackPath = Join-Path $projectRoot 'build\double-dribble.assetpack' }
if ($CliPath -eq '') { $CliPath = Join-Path $projectRoot 'build\double_dribble_port.exe' }
$captureRoot = Join-Path $projectRoot 'captures\native'
$bmpPath = Join-Path $captureRoot 'tipoff-2359.bmp'
$pngPath = Join-Path $captureRoot 'tipoff-2359.png'
$endWavPath = Join-Path $captureRoot 'end-select.wav'
$tipoffWavPath = Join-Path $captureRoot 'tipoff-dmc.wav'
New-Item -ItemType Directory -Force -Path $captureRoot | Out-Null

& $CliPath --render-tipoff $AssetPackPath $bmpPath
if ($LASTEXITCODE -ne 0) { throw 'Native tip-off rendering failed.' }
& $CliPath --dump-end-wav $AssetPackPath $endWavPath
if ($LASTEXITCODE -ne 0) { throw 'Native END audio export failed.' }
& $CliPath --dump-tipoff-wav $AssetPackPath $tipoffWavPath
if ($LASTEXITCODE -ne 0) { throw 'Native tip-off DMC export failed.' }

Add-Type -AssemblyName System.Drawing
$bitmap = [System.Drawing.Bitmap]::FromFile($bmpPath)
try {
    $bitmap.Save($pngPath, [System.Drawing.Imaging.ImageFormat]::Png)
} finally {
    $bitmap.Dispose()
}
Write-Host "Native tip-off frame: $pngPath"
Write-Host "Native END audio: $endWavPath"
Write-Host "Native tip-off DMC: $tipoffWavPath"
