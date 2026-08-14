[CmdletBinding()]
param(
    [int]$Selection = 0,
    [int]$OriginalFrame = 2100,
    [string]$AssetPackPath = '',
    [string]$CliPath = ''
)

$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if ($AssetPackPath -eq '') { $AssetPackPath = Join-Path $projectRoot 'build\double-dribble.assetpack' }
if ($CliPath -eq '') { $CliPath = Join-Path $projectRoot 'build\double_dribble_port.exe' }
if ($Selection -lt 0 -or $Selection -gt 3) { throw 'Selection must be 0 through 3.' }
$captureRoot = Join-Path $projectRoot 'captures\native'
$stem = 'config-{0:D4}' -f $OriginalFrame
$bmpPath = Join-Path $captureRoot ($stem + '.bmp')
$pngPath = Join-Path $captureRoot ($stem + '.png')
New-Item -ItemType Directory -Force -Path $captureRoot | Out-Null

& $CliPath --render-config $AssetPackPath $Selection $bmpPath
if ($LASTEXITCODE -ne 0) { throw 'Native configuration rendering failed.' }
Add-Type -AssemblyName System.Drawing
$bitmap = [System.Drawing.Bitmap]::FromFile($bmpPath)
try {
    $bitmap.Save($pngPath, [System.Drawing.Imaging.ImageFormat]::Png)
} finally {
    $bitmap.Dispose()
}
Write-Host "Native configuration frame: $pngPath"
