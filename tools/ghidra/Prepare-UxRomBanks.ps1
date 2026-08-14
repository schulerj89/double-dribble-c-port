[CmdletBinding()]
param(
    [string]$RomPath = 'F:\Games\NES\Double Dribble\Double Dribble (USA) (Rev 1).nes'
)

$ErrorActionPreference = 'Stop'

$expectedSha256 = 'BF397EAE9486044FCA90A99215330203D6F85CAB63A8072F28CACC139B5388CF'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$outputRoot = Join-Path $projectRoot 'build\ghidra-input'

if (-not (Test-Path -LiteralPath $RomPath -PathType Leaf)) {
    throw "ROM not found: $RomPath"
}

$identity = (Get-FileHash -LiteralPath $RomPath -Algorithm SHA256).Hash
if ($identity -ne $expectedSha256) {
    throw "Unsupported ROM identity: $identity"
}

$rom = [System.IO.File]::ReadAllBytes($RomPath)
if ($rom.Length -ne 131088 -or $rom[0] -ne 0x4E -or $rom[1] -ne 0x45 -or
    $rom[2] -ne 0x53 -or $rom[3] -ne 0x1A -or $rom[4] -ne 8 -or
    $rom[5] -ne 0 -or (($rom[6] -shr 4) -band 0x0F) -ne 2) {
    throw 'The file is not the supported eight-bank mapper-2 Rev 1 ROM.'
}

New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null

for ($bank = 0; $bank -lt 8; $bank++) {
    $bytes = [byte[]]::new(0x4000)
    [Array]::Copy($rom, 16 + ($bank * 0x4000), $bytes, 0, $bytes.Length)
    $kind = if ($bank -eq 7) { 'fixed' } else { 'switch' }
    $path = Join-Path $outputRoot ('prg-bank-{0:D2}-{1}.bin' -f $bank, $kind)
    [System.IO.File]::WriteAllBytes($path, $bytes)
    Write-Host ('Prepared bank {0} at {1}' -f $bank, $path)
}

