[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$components = @(
    [pscustomobject]@{
        Name = 'Player actions'; Weight = 0.30; Expected = 34
        Verified = @('20','21','22','23','24','25','26','27','28','29','2A','2B','2C','2D','2E','2F','30','31','32','33','34','35','36','37','38','39','3A','3B','3C','3D','3E','3F','40','41')
        Partial = @()
        Missing = @()
    },
    [pscustomobject]@{
        Name = 'Ball actions'; Weight = 0.25; Expected = 13
        Verified = @('00','01','02','03','04','05','06','07','08','09','0A','0B','0C')
        Partial = @()
        Missing = @()
    },
    [pscustomobject]@{
        Name = 'Core loop'; Weight = 0.25; Expected = 7
        Verified = @('objects','scheduler','targets','dribble-audio')
        Partial = @('user-control','movement-physics','general-collision')
        Missing = @()
    },
    [pscustomobject]@{
        Name = 'Match rules'; Weight = 0.20; Expected = 13
        Verified = @('tipoff','made-shot','score-hud','misses')
        Partial = @('user-control','cpu-choice','rebound','inbound','possession-transfer',
                    'clock','periods','steals-blocks','fouls-out-of-bounds')
        Missing = @()
    }
)

$weightTotal = 0.0
$weightedCoverage = 0.0
$matchCoverage = 0.0
foreach ($component in $components) {
    $all = @($component.Verified) + @($component.Partial) + @($component.Missing)
    if ($all.Count -ne $component.Expected) {
        throw "$($component.Name) coverage has $($all.Count) entries; expected $($component.Expected)."
    }
    if (($all | Sort-Object -Unique).Count -ne $all.Count) {
        throw "$($component.Name) coverage contains duplicate entries."
    }
    $score = ($component.Verified.Count + 0.5 * $component.Partial.Count) / $component.Expected
    $weightTotal += $component.Weight
    $weightedCoverage += $score * $component.Weight
    if ($component.Name -eq 'Match rules') { $matchCoverage = $score }
    Write-Host ('  {0,-15} {1,5:N1}%  (V {2}, P {3}, M {4})' -f
        $component.Name, (100.0 * $score), $component.Verified.Count,
        $component.Partial.Count, $component.Missing.Count)
}

if ([Math]::Abs($weightTotal - 1.0) -gt 0.000001) {
    throw "Gameplay coverage weights total $weightTotal instead of 1.0."
}

Write-Host ('Ghidra-to-C gameplay-loop coverage: {0:N1}%' -f (100.0 * $weightedCoverage)) -ForegroundColor Green
Write-Host ('Match-rules completeness:             {0:N1}%' -f (100.0 * $matchCoverage))
