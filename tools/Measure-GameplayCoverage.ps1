[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$routineManifestPath = Join-Path $projectRoot 'GAMEPLAY_ROUTINES.json'
if (Test-Path -LiteralPath $routineManifestPath -PathType Leaf) {
    $routineManifest = Get-Content -Raw -LiteralPath $routineManifestPath | ConvertFrom-Json
    if ($routineManifest.schema -ne 1) { throw 'Unsupported GAMEPLAY_ROUTINES.json schema.' }
    $routineKeys = @($routineManifest.routines | ForEach-Object { "$($_.bank):$($_.address)" })
    if (($routineKeys | Sort-Object -Unique).Count -ne $routineKeys.Count) {
        throw 'GAMEPLAY_ROUTINES.json contains duplicate bank/address routines.'
    }
    $unclassified = @($routineManifest.routines | Where-Object {
        $_.classification -eq 'unclassified' -or $_.status -eq 'inventory_pending'
    }).Count
    if ($routineManifest.coverage.routine_count -ne $routineKeys.Count -or
        $routineManifest.coverage.unclassified_count -ne $unclassified) {
        throw 'GAMEPLAY_ROUTINES.json coverage counts do not match its routine records.'
    }
    $portable = @($routineManifest.routines | Where-Object {
        $_.classification -ne 'excluded_nes_mechanism'
    })
    $verifiedRoutineCount = @($portable | Where-Object { $_.status -eq 'verified' }).Count
    $partialRoutineCount = @($portable | Where-Object { $_.status -eq 'partial' }).Count
    $missingRoutineCount = @($portable | Where-Object { $_.status -eq 'missing' }).Count
    $computedComprehensive = [Math]::Round(
        100.0 * ($verifiedRoutineCount + 0.5 * $partialRoutineCount) / $portable.Count, 1)
    if ($routineManifest.coverage.portable_count -ne $portable.Count -or
        $routineManifest.coverage.verified_count -ne $verifiedRoutineCount -or
        $routineManifest.coverage.partial_count -ne $partialRoutineCount -or
        $routineManifest.coverage.missing_count -ne $missingRoutineCount -or
        [Math]::Abs($routineManifest.coverage.comprehensive_percent - $computedComprehensive) -gt 0.001) {
        throw 'GAMEPLAY_ROUTINES.json comprehensive coverage does not match its routine statuses.'
    }
    Write-Host ('  Routine graph    {0} nodes ({1} awaiting classification)' -f
        $routineKeys.Count, $unclassified)
    Write-Host ('  Comprehensive   {0,5:N1}%  (V {1}, P {2}, M {3}, excluded {4})' -f
        $computedComprehensive, $verifiedRoutineCount, $partialRoutineCount,
        $missingRoutineCount, $routineManifest.coverage.excluded_count)
}

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
        Verified = @('objects','scheduler','targets','dribble-audio','user-control')
        Partial = @('movement-physics','general-collision')
        Missing = @()
    },
    [pscustomobject]@{
        Name = 'Match rules'; Weight = 0.20; Expected = 13
        Verified = @('tipoff','made-shot','score-hud','misses','periods','user-control','inbound','cpu-choice')
        Partial = @('rebound','possession-transfer',
                    'clock','steals-blocks','fouls-out-of-bounds')
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
