[CmdletBinding()]
param(
    [string]$AssetPackPath = '',
    [string]$CliPath = '',
    [int[]]$TransitionFrames = @(330, 356, 357, 358, 399),
    [int]$HeldInputMask = 0
)

$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if ($AssetPackPath -eq '') { $AssetPackPath = Join-Path $projectRoot 'build\double-dribble.assetpack' }
if ($CliPath -eq '') { $CliPath = Join-Path $projectRoot 'build\double_dribble_port.exe' }
$captureRoot = Join-Path $projectRoot 'captures\native-gameplay'
New-Item -ItemType Directory -Force -Path $captureRoot | Out-Null
Add-Type -AssemblyName System.Drawing

foreach ($frame in $TransitionFrames) {
    $suffix = if ($HeldInputMask -eq 0) { '' } else { '-input-{0:X2}' -f $HeldInputMask }
    $bmpPath = Join-Path $captureRoot ('frame-{0:D4}{1}.bmp' -f $frame, $suffix)
    $pngPath = Join-Path $captureRoot ('frame-{0:D4}{1}.png' -f $frame, $suffix)
    if ($HeldInputMask -eq 0) {
        & $CliPath --render-gameplay $AssetPackPath $frame $bmpPath
    } else {
        & $CliPath --render-gameplay-input $AssetPackPath $frame $HeldInputMask $bmpPath
    }
    if ($LASTEXITCODE -ne 0) { throw "Native gameplay rendering failed at transition frame $frame." }
    $bitmap = [System.Drawing.Bitmap]::FromFile($bmpPath)
    try {
        $bitmap.Save($pngPath, [System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $bitmap.Dispose()
    }
    Write-Host "Native gameplay frame: $pngPath"
}
