[CmdletBinding()]
param(
    [int]$TransitionFrame = 356,
    [int]$OriginalFrame = 2557,
    [double]$MaximumDifferentPercent = 5.0,
    [string]$OriginalPath = '',
    [string]$NativePath = ''
)

$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if ($OriginalPath -eq '') {
    $OriginalPath = Join-Path $projectRoot ('captures\original-tipoff-gameplay\frame-{0:D4}.png' -f $OriginalFrame)
}
if ($NativePath -eq '') {
    $NativePath = Join-Path $projectRoot ('captures\native-gameplay\frame-{0:D4}.png' -f $TransitionFrame)
}
$diffRoot = Join-Path $projectRoot 'captures\diff'
$diffPath = Join-Path $diffRoot ('gameplay-{0:D4}-{1:D4}-diff.png' -f $TransitionFrame, $OriginalFrame)
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
    $percent = 100.0 * $different / $total
    Write-Host ('Pixel differences: {0}/{1} ({2:N4}%)' -f $different, $total, $percent)
    Write-Host "Diff image: $diffPath"
    if ($percent -gt $MaximumDifferentPercent) {
        throw ('Difference {0:N4}% exceeds the {1:N4}% limit.' -f $percent, $MaximumDifferentPercent)
    }
} finally {
    $original.Dispose()
    $native.Dispose()
    $diff.Dispose()
}
