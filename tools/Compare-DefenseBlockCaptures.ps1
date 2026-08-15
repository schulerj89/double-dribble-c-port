[CmdletBinding()]
param(
    [string]$OriginalCaptureName = 'original-user-shot-block'
)

$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$originalRoot = Join-Path $projectRoot (Join-Path 'captures' $OriginalCaptureName)
$nativeRoot = Join-Path $projectRoot 'captures\native-defense-block'
$diffRoot = Join-Path $projectRoot 'captures\diff\defense-block'
New-Item -ItemType Directory -Force -Path $diffRoot | Out-Null
Add-Type -AssemblyName System.Drawing

$checkpoints = @(
    [pscustomobject]@{ Phase = 'contact'; OriginalFrame = 2606 },
    [pscustomobject]@{ Phase = 'landing'; OriginalFrame = 2644 }
)
foreach ($checkpoint in $checkpoints) {
    $originalPath = Join-Path $originalRoot ('frame-{0:D4}.png' -f $checkpoint.OriginalFrame)
    $nativePath = Join-Path $nativeRoot ("$($checkpoint.Phase).png")
    $diffPath = Join-Path $diffRoot ("$($checkpoint.Phase)-diff.png")
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
        Write-Host ('{0}: {1}/57344 pixels differ ({2:N4}%)' -f
            $checkpoint.Phase, $different, (100.0 * $different / 57344.0))
        Write-Host "Diff image: $diffPath"
    } finally {
        $original.Dispose()
        $native.Dispose()
        $diff.Dispose()
    }
}
