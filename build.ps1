[CmdletBinding()]
param(
    [string]$RomPath = 'F:\Games\NES\Double Dribble\Double Dribble (USA) (Rev 1).nes'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildDir = Join-Path $root 'build'
$objDir = Join-Path $buildDir 'obj'
$vsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
New-Item -ItemType Directory -Force -Path $buildDir, $objDir | Out-Null

if (-not (Test-Path -LiteralPath $RomPath -PathType Leaf)) {
    throw "A supported ROM is required because every build produces an asset pack: $RomPath"
}
if (-not (Test-Path -LiteralPath $vsWhere -PathType Leaf)) {
    throw 'Visual Studio Build Tools were not found.'
}
$vsPath = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcVars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path -LiteralPath $vcVars -PathType Leaf)) {
    throw 'MSVC x64 tools were not found.'
}

$cli = Join-Path $buildDir 'double_dribble_port.exe'
$game = Join-Path $buildDir 'double_dribble_game.exe'
$cpuTests = Join-Path $buildDir 'double_dribble_cpu_tests.exe'
$resource = Join-Path $objDir 'double_dribble.res'
$assetPack = Join-Path $buildDir 'double-dribble.assetpack'
$include = Join-Path $root 'include'
$commonSources = @(
    (Join-Path $root 'src\dd_asset_pack.c'),
    (Join-Path $root 'src\dd_gameplay.c'),
    (Join-Path $root 'src\dd_renderer.c'),
    (Join-Path $root 'src\dd_audio.c')
)
$batch = Join-Path $buildDir 'compile.bat'
$common = ($commonSources | ForEach-Object { '"{0}"' -f $_ }) -join ' '
$batchText = @"
@echo off
call "$vcVars" > nul
rc.exe /nologo /fo "$resource" "$root\src\double_dribble.rc"
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%
cl.exe /nologo /std:c17 /W4 /WX /O2 /MD /D_CRT_SECURE_NO_WARNINGS /I "$include" /Fo"$objDir\\" /Fe"$cli" "$root\src\main.c" $common bcrypt.lib
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%
cl.exe /nologo /std:c17 /W4 /WX /O2 /MD /D_CRT_SECURE_NO_WARNINGS /I "$include" /Fo"$objDir\\" /Fe"$game" "$root\src\win32_game_main.c" $common "$resource" bcrypt.lib user32.lib gdi32.lib shell32.lib winmm.lib /link /SUBSYSTEM:WINDOWS
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%
cl.exe /nologo /std:c17 /W4 /WX /O2 /MD /D_CRT_SECURE_NO_WARNINGS /I "$include" /Fo"$objDir\\" /Fe"$cpuTests" "$root\tests\dd_gameplay_cpu_test.c" "$root\src\dd_asset_pack.c" "$root\src\dd_gameplay.c" bcrypt.lib
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%
"@
[System.IO.File]::WriteAllText($batch, $batchText, [System.Text.Encoding]::ASCII)
& cmd.exe /c $batch
if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE"
}

& $cli --build-assetpack $RomPath $assetPack
if ($LASTEXITCODE -ne 0) {
    throw 'Asset-pack build failed.'
}
& $cli --inspect-assetpack $assetPack
if ($LASTEXITCODE -ne 0) {
    throw 'Asset-pack validation failed.'
}
& $cpuTests $assetPack
if ($LASTEXITCODE -ne 0) {
    throw 'CPU gameplay regression failed.'
}

& (Join-Path $root 'tools\Measure-GameplayCoverage.ps1')
if ($LASTEXITCODE -ne 0) {
    throw 'Gameplay coverage validation failed.'
}

Write-Host 'Build complete:' -ForegroundColor Green
Write-Host "  CLI:       $cli"
Write-Host "  Game:      $game"
Write-Host "  CPU tests: $cpuTests"
Write-Host "  Assetpack: $assetPack"
