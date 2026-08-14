[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$buildDir = Join-Path $root 'build'
$game = Join-Path $buildDir 'double_dribble_game.exe'
$assetPack = Join-Path $buildDir 'double-dribble.assetpack'
$desktop = [Environment]::GetFolderPath('Desktop')
$shortcutPath = Join-Path $desktop 'Double Dribble Native Port.lnk'

if (-not (Test-Path -LiteralPath $game -PathType Leaf)) {
    throw "Build the game before installing the shortcut: $game"
}
if (-not (Test-Path -LiteralPath $assetPack -PathType Leaf)) {
    throw "Build the asset pack before installing the shortcut: $assetPack"
}

$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut($shortcutPath)
$shortcut.TargetPath = $game
$shortcut.Arguments = '"' + $assetPack + '"'
$shortcut.WorkingDirectory = $buildDir
$shortcut.IconLocation = $game + ',0'
$shortcut.Description = 'Double Dribble native C port'
$shortcut.Save()

Write-Host "Desktop shortcut installed: $shortcutPath" -ForegroundColor Green
