[CmdletBinding()]
param(
    [string]$AssetPackPath = '',
    [string]$CliPath = ''
)

$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if ($AssetPackPath -eq '') { $AssetPackPath = Join-Path $projectRoot 'build\double-dribble.assetpack' }
if ($CliPath -eq '') { $CliPath = Join-Path $projectRoot 'build\double_dribble_port.exe' }
$captureRoot = Join-Path $projectRoot 'captures\native-user-contest'
New-Item -ItemType Directory -Force -Path $captureRoot | Out-Null
$bmpPath = Join-Path $captureRoot 'frame-0548.bmp'
$pngPath = Join-Path $captureRoot 'frame-0548.png'
Add-Type -AssemblyName System.Drawing

& $CliPath --render-gameplay-user-contest $AssetPackPath $bmpPath
if ($LASTEXITCODE -ne 0) { throw 'Native user-contest rendering failed.' }
$bitmap = [System.Drawing.Bitmap]::FromFile($bmpPath)
try {
    $bitmap.Save($pngPath, [System.Drawing.Imaging.ImageFormat]::Png)
} finally {
    $bitmap.Dispose()
}
Write-Host "Native user-contest frame: $pngPath"
