[CmdletBinding()]
param(
    [int]$OriginalFrame = 2359,
    [string]$OriginalPath = '',
    [string]$NativePath = ''
)

$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if ($OriginalPath -eq '') { $OriginalPath = Join-Path $projectRoot ('captures\original-1p\frame-{0:D4}.png' -f $OriginalFrame) }
if ($NativePath -eq '') { $NativePath = Join-Path $projectRoot 'captures\native\tipoff-2359.png' }
$diffRoot = Join-Path $projectRoot 'captures\diff'
$diffPath = Join-Path $diffRoot ('tipoff-{0:D4}-diff.png' -f $OriginalFrame)
New-Item -ItemType Directory -Force -Path $diffRoot | Out-Null

Add-Type -AssemblyName System.Drawing
$original = [System.Drawing.Bitmap]::FromFile($OriginalPath)
$native = [System.Drawing.Bitmap]::FromFile($NativePath)
$diff = New-Object System.Drawing.Bitmap($original.Width, $original.Height)
try {
    if ($original.Width -ne 256 -or $original.Height -ne 224 -or
        $native.Width -ne 256 -or $native.Height -ne 240) {
        throw 'Expected a 256x224 FCEUX capture and a 256x240 native capture.'
    }
    $different = 0
    for ($y = 0; $y -lt $original.Height; $y++) {
        for ($x = 0; $x -lt $original.Width; $x++) {
            if ($original.GetPixel($x, $y).ToArgb() -ne $native.GetPixel($x, $y + 8).ToArgb()) {
                $different++
                $diff.SetPixel($x, $y, [System.Drawing.Color]::White)
            } else {
                $diff.SetPixel($x, $y, [System.Drawing.Color]::Black)
            }
        }
    }
    $diff.Save($diffPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $total = $original.Width * $original.Height
    Write-Host ('Pixel differences: {0}/{1} ({2:N4}%)' -f $different, $total, (100.0 * $different / $total))
    Write-Host "Diff image: $diffPath"
    if ($different -ne 0) { exit 1 }
} finally {
    $original.Dispose()
    $native.Dispose()
    $diff.Dispose()
}
