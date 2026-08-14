[CmdletBinding()]
param(
    [int]$OriginalFrame = 180,
    [string]$AssetPackPath = '',
    [string]$CliPath = ''
)

$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if ($AssetPackPath -eq '') { $AssetPackPath = Join-Path $projectRoot 'build\double-dribble.assetpack' }
if ($CliPath -eq '') { $CliPath = Join-Path $projectRoot 'build\double_dribble_port.exe' }
if ($OriginalFrame -lt 165) { throw 'The road-intro baseline begins at original frame 165.' }
$captureRoot = Join-Path $projectRoot 'captures\native'
$stem = 'intro-{0:D4}' -f $OriginalFrame
$bmpPath = Join-Path $captureRoot ($stem + '.bmp')
$pngPath = Join-Path $captureRoot ($stem + '.png')
$wavPath = Join-Path $captureRoot 'intro-music.wav'
$introFrame = $OriginalFrame - 165
New-Item -ItemType Directory -Force -Path $captureRoot | Out-Null

& $CliPath --render-intro $AssetPackPath $introFrame $bmpPath
if ($LASTEXITCODE -ne 0) { throw "Native intro rendering failed at local frame $introFrame." }
& $CliPath --dump-intro-wav $AssetPackPath $wavPath
if ($LASTEXITCODE -ne 0) { throw 'Native intro music export failed.' }

Add-Type -AssemblyName System.Drawing
$bitmap = [System.Drawing.Bitmap]::FromFile($bmpPath)
try {
    $bitmap.Save($pngPath, [System.Drawing.Imaging.ImageFormat]::Png)
} finally {
    $bitmap.Dispose()
}
Write-Host "Native intro frame: $pngPath"
Write-Host "Native intro music: $wavPath"
