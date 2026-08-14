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
$bmpPath = Join-Path $captureRoot 'title.bmp'
$pngPath = Join-Path $captureRoot 'title.png'
$wavPath = Join-Path $captureRoot 'double-dribble.wav'
New-Item -ItemType Directory -Force -Path $captureRoot | Out-Null

& $CliPath --render-title $AssetPackPath $bmpPath
if ($LASTEXITCODE -ne 0) { throw 'Native title rendering failed.' }
& $CliPath --dump-title-wav $AssetPackPath $wavPath
if ($LASTEXITCODE -ne 0) { throw 'Native title audio export failed.' }

Add-Type -AssemblyName System.Drawing
$bitmap = [System.Drawing.Bitmap]::FromFile($bmpPath)
try {
    $bitmap.Save($pngPath, [System.Drawing.Imaging.ImageFormat]::Png)
} finally {
    $bitmap.Dispose()
}
Write-Host "Native title: $pngPath"
Write-Host "Native audio: $wavPath"

