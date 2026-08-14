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
$confirmationFrames = @(1, 10, 18, 26, 34, 42, 50, 58, 66, 74)
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
foreach ($frame in $confirmationFrames) {
    $confirmBmpPath = Join-Path $captureRoot ('title-confirm-{0:D4}.bmp' -f $frame)
    $confirmPngPath = Join-Path $captureRoot ('title-confirm-{0:D4}.png' -f $frame)
    & $CliPath --render-title-confirm $AssetPackPath $frame $confirmBmpPath
    if ($LASTEXITCODE -ne 0) { throw "Native title confirmation rendering failed at frame $frame." }
    $confirmation = [System.Drawing.Bitmap]::FromFile($confirmBmpPath)
    try {
        $confirmation.Save($confirmPngPath, [System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $confirmation.Dispose()
    }
}
Write-Host "Native title: $pngPath"
Write-Host "Native audio: $wavPath"
Write-Host "Native repeating confirmation frames: $captureRoot\title-confirm-*.png"
