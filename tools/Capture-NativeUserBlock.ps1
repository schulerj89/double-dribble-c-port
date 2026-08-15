[CmdletBinding()]
param(
    [string]$AssetPackPath = '',
    [string]$CliPath = ''
)

$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if ($AssetPackPath -eq '') { $AssetPackPath = Join-Path $projectRoot 'build\double-dribble.assetpack' }
if ($CliPath -eq '') { $CliPath = Join-Path $projectRoot 'build\double_dribble_port.exe' }
$captureRoot = Join-Path $projectRoot 'captures\native-user-block'
New-Item -ItemType Directory -Force -Path $captureRoot | Out-Null
Add-Type -AssemblyName System.Drawing

foreach ($phase in @('contact', 'landing')) {
    $bmpPath = Join-Path $captureRoot "$phase.bmp"
    $pngPath = Join-Path $captureRoot "$phase.png"
    & $CliPath --render-gameplay-user-block $AssetPackPath $phase $bmpPath
    if ($LASTEXITCODE -ne 0) { throw "Native user-block $phase rendering failed." }
    $bitmap = [System.Drawing.Bitmap]::FromFile($bmpPath)
    try {
        $bitmap.Save($pngPath, [System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $bitmap.Dispose()
    }
    Write-Host "Native user-block frame: $pngPath"
}
