[CmdletBinding()]
param(
    [string]$AssetPackPath = '',
    [string]$CliPath = ''
)

$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if ($AssetPackPath -eq '') { $AssetPackPath = Join-Path $projectRoot 'build\double-dribble.assetpack' }
if ($CliPath -eq '') { $CliPath = Join-Path $projectRoot 'build\double_dribble_port.exe' }
$captureRoot = Join-Path $projectRoot 'captures\native-dunk-release'
New-Item -ItemType Directory -Force -Path $captureRoot | Out-Null
Add-Type -AssemblyName System.Drawing

foreach ($outcome in @('make', 'miss')) {
    foreach ($checkpoint in @('gather', 'airborne', 'result')) {
        $bmpPath = Join-Path $captureRoot ("$outcome-$checkpoint.bmp")
        $pngPath = Join-Path $captureRoot ("$outcome-$checkpoint.png")
        & $CliPath --render-gameplay-dunk $AssetPackPath $outcome $checkpoint $bmpPath
        if ($LASTEXITCODE -ne 0) {
            throw "Native running-dunk $outcome/$checkpoint capture failed."
        }
        $bitmap = [System.Drawing.Bitmap]::FromFile($bmpPath)
        try {
            $bitmap.Save($pngPath, [System.Drawing.Imaging.ImageFormat]::Png)
        } finally {
            $bitmap.Dispose()
        }
        Write-Host "Native running-dunk frame: $pngPath"
    }
}
