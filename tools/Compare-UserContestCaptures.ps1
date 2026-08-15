[CmdletBinding()]
param(
    [string]$OriginalCaptureName = 'original-user-contest-natural'
)

$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$originalPath = Join-Path $projectRoot "captures\$OriginalCaptureName\frame-2749.png"
$nativePath = Join-Path $projectRoot 'captures\native-user-contest\frame-0548.png'
$diffRoot = Join-Path $projectRoot 'captures\diff\user-contest'
$diffPath = Join-Path $diffRoot 'frame-2749-vs-0548.png'
New-Item -ItemType Directory -Force -Path $diffRoot | Out-Null
Add-Type -AssemblyName System.Drawing

$original = [System.Drawing.Bitmap]::FromFile($originalPath)
$native = [System.Drawing.Bitmap]::FromFile($nativePath)
$diff = New-Object System.Drawing.Bitmap(256, 224)
try {
    if ($original.Width -ne 256 -or $original.Height -ne 224 -or
        $native.Width -ne 256 -or $native.Height -ne 240) {
        throw 'Expected a 256x224 FCEUX capture and a 256x240 native capture.'
    }
    $different = 0
    for ($y = 0; $y -lt 224; ++$y) {
        for ($x = 0; $x -lt 256; ++$x) {
            if ($original.GetPixel($x, $y).ToArgb() -ne
                $native.GetPixel($x, $y + 8).ToArgb()) {
                ++$different
                $diff.SetPixel($x, $y, [System.Drawing.Color]::White)
            } else {
                $diff.SetPixel($x, $y, [System.Drawing.Color]::Black)
            }
        }
    }
    $diff.Save($diffPath, [System.Drawing.Imaging.ImageFormat]::Png)
    Write-Host ('user contest: {0}/57344 pixels differ ({1:N4}%)' -f
        $different, (100.0 * $different / 57344.0))
    Write-Host "Diff image: $diffPath"
} finally {
    $original.Dispose()
    $native.Dispose()
    $diff.Dispose()
}
