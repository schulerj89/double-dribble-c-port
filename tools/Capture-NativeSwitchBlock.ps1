[CmdletBinding()]
param(
    [string]$AssetPackPath = '',
    [string]$CliPath = ''
)

$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if ($AssetPackPath -eq '') { $AssetPackPath = Join-Path $projectRoot 'build\double-dribble.assetpack' }
if ($CliPath -eq '') { $CliPath = Join-Path $projectRoot 'build\double_dribble_port.exe' }
$captureRoot = Join-Path $projectRoot 'captures\native-switch-block'
$bmpPath = Join-Path $captureRoot 'switch-block.bmp'
$pngPath = Join-Path $captureRoot 'switch-block.png'
New-Item -ItemType Directory -Force -Path $captureRoot | Out-Null
Add-Type -AssemblyName System.Drawing

& $CliPath --render-gameplay-switch-block $AssetPackPath $bmpPath
if ($LASTEXITCODE -ne 0) { throw 'Native switched-defender block rendering failed.' }
$bitmap = [System.Drawing.Bitmap]::FromFile($bmpPath)
try {
    $bitmap.Save($pngPath, [System.Drawing.Imaging.ImageFormat]::Png)
} finally {
    $bitmap.Dispose()
}
Write-Host "Native switched-defender block frame: $pngPath"
