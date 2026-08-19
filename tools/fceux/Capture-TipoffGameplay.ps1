[CmdletBinding()]
param(
    [string]$RomPath = 'F:\Games\NES\Double Dribble\Double Dribble (USA) (Rev 1).nes',
    [string]$FceuxPath = 'C:\Users\joshs\Games\fceux-2.6.6-win32\fceux.exe',
    [int]$TraceStart = 2320,
    [int]$FinalFrame = 2760,
    [string]$CaptureName = 'original-tipoff-gameplay',
    [int]$JumpStart = -1,
    [int]$JumpEnd = -1,
    [int]$PassFrame = -1,
    [int]$PassEnd = -1,
    [ValidateSet('A', 'B')]
    [string]$PassButton = 'A',
    [ValidateSet('none', 'left', 'right', 'up', 'down')]
    [string]$PassDirection = 'none',
    [int]$MoveStart = -1,
    [int]$MoveEnd = -1,
    [ValidateSet('none', 'left', 'right', 'up', 'down')]
    [string]$MoveDirection = 'none',
    [ValidateSet('none', 'x-upper', 'x-lower', 'depth-upper', 'depth-lower')]
    [string]$PhysicsBoundary = 'none',
    [int]$ContactFrame = -1,
    [int]$ContactClockGate = -1,
    [ValidateRange(-1, 255)]
    [int]$ContactLevel = -1,
    [ValidateRange(-1, 255)]
    [int]$ContactLimit = -1,
    [ValidateRange(-1, 255)]
    [int]$ContactPhase = -1,
    [ValidateRange(-1, 255)]
    [int]$ContactBallState = -1,
    [ValidateSet('owner', 'wrong')]
    [string]$ContactPair = 'owner',
    [int]$UserFreeThrowFrame = -1,
    [int]$CpuFreeThrowFrame = -1,
    [ValidateRange(-1, 255)]
    [int]$CpuFreeThrowLevel = -1,
    [ValidateRange(-1, 255)]
    [int]$CpuFreeThrowPhase = -1,
    [ValidateRange(-1, 255)]
    [int]$CpuFreeThrowAim = -1,
    [ValidateRange(-1, 255)]
    [int]$CpuFreeThrowTimer = -1,
    [ValidateRange(-1, 255)]
    [int]$CpuFreeThrowGate = -1,
    [int]$BasketFrame = -1,
    [ValidateRange(1, 4)]
    [int]$BasketResult = 1,
    [ValidateRange(0, 255)]
    [int]$BasketCounter = 0,
    [ValidateRange(0, 2)]
    [int]$BasketShotKind = 0,
    [int]$BlockFrame = -1,
    [int]$UserBlockFrame = -1,
    [int]$UserStealFrame = -1,
    [ValidateSet('A', 'none')]
    [string]$UserStealButton = 'A',
    [ValidateRange(-1, 255)]
    [int]$UserStealLock = -1,
    [ValidateRange(-1, 255)]
    [int]$UserStealGate = -1,
    [ValidateRange(-1, 255)]
    [int]$UserStealBallState = -1,
    [ValidateRange(-1, 255)]
    [int]$UserStealPairedAction = -1,
    [ValidateSet('hit', 'miss', 'boundary_in', 'boundary_out')]
    [string]$UserStealCollision = 'hit',
    [switch]$UserStealFoul,
    [switch]$UserStealSamePlayer,
    [int]$InboundRuleFrame = -1,
    [ValidateRange(0, 4)]
    [int]$InboundRuleCase = 0,
    [int]$ExceptionalReasonFrame = -1,
    [ValidateRange(0, 4)]
    [int]$ShotKindCase = 0,
    [ValidateRange(-1, 255)]
    [int]$UserShotDepth = -1,
    [ValidateRange(-1, 65535)]
    [int]$UserShotX = -1,
    [int]$UserPositionFrame = -1,
    [int]$ScoreAudioFreezeFrame = -1,
    [ValidateSet('none', 'role0', 'paired', 'mirror')]
    [string]$CpuRegion2Probe = 'none',
    [switch]$DisablePcCounts,
    [ValidateSet('A', 'B')]
    [string]$JumpButton = 'A'
)

$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$captureRoot = Join-Path $projectRoot (Join-Path 'captures' $CaptureName)
$scriptPath = Join-Path $PSScriptRoot 'capture_tipoff_gameplay.lua'
if (-not (Test-Path -LiteralPath $RomPath -PathType Leaf)) { throw "ROM not found: $RomPath" }
if (-not (Test-Path -LiteralPath $FceuxPath -PathType Leaf)) { throw "FCEUX not found: $FceuxPath" }
New-Item -ItemType Directory -Force -Path $captureRoot | Out-Null
$env:DD_CAPTURE_ROOT = $captureRoot
$env:DD_ROM_PATH = [System.IO.Path]::GetFullPath($RomPath)
$env:DD_CAPTURE_FINAL_FRAME = $FinalFrame.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_TRACE_START = $TraceStart.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_TRACE_END = $FinalFrame.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_TIP_JUMP_START = $JumpStart.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_TIP_JUMP_END = $JumpEnd.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_TIP_JUMP_BUTTON = $JumpButton
$env:DD_PASS_FRAME = $PassFrame.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_PASS_END = $(if ($PassEnd -ge 0) { $PassEnd } else { $PassFrame }).ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_PASS_BUTTON = $PassButton
$env:DD_PASS_DIRECTION = $PassDirection
$env:DD_MOVE_START = $MoveStart.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_MOVE_END = $(if ($MoveEnd -ge 0) { $MoveEnd } else { $MoveStart }).ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_MOVE_DIRECTION = $MoveDirection
$env:DD_INJECT_PHYSICS_BOUNDARY = $PhysicsBoundary
$env:DD_ENABLE_PC_COUNTS = if ($DisablePcCounts) { '0' } else { '1' }
$env:DD_INJECT_CONTACT_FRAME = $ContactFrame.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_INJECT_CONTACT_CLOCK_GATE = $ContactClockGate.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_INJECT_CONTACT_LEVEL = $ContactLevel.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_INJECT_CONTACT_LIMIT = $ContactLimit.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_INJECT_CONTACT_PHASE = $ContactPhase.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_INJECT_CONTACT_BALL_STATE = $ContactBallState.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_INJECT_CONTACT_PAIR = $ContactPair
$env:DD_INJECT_USER_FREE_THROW_FRAME = $UserFreeThrowFrame.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_INJECT_CPU_FREE_THROW_FRAME = $CpuFreeThrowFrame.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_INJECT_CPU_FREE_THROW_LEVEL = $CpuFreeThrowLevel.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_INJECT_CPU_FREE_THROW_PHASE = $CpuFreeThrowPhase.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_INJECT_CPU_FREE_THROW_AIM = $CpuFreeThrowAim.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_INJECT_CPU_FREE_THROW_TIMER = $CpuFreeThrowTimer.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_INJECT_CPU_FREE_THROW_GATE = $CpuFreeThrowGate.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_INJECT_BASKET_FRAME = $BasketFrame.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_INJECT_BASKET_RESULT = $BasketResult.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_INJECT_BASKET_COUNTER = $BasketCounter.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_INJECT_BASKET_SHOT_KIND = $BasketShotKind.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_INJECT_BLOCK_FRAME = $BlockFrame.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_INJECT_USER_BLOCK_FRAME = $UserBlockFrame.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_INJECT_USER_STEAL_FRAME = $UserStealFrame.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_INJECT_USER_STEAL_BUTTON = $UserStealButton
$env:DD_INJECT_USER_STEAL_LOCK = $UserStealLock.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_INJECT_USER_STEAL_GATE = $UserStealGate.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_INJECT_USER_STEAL_BALL_STATE = $UserStealBallState.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_INJECT_USER_STEAL_PAIRED_ACTION = $UserStealPairedAction.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_INJECT_USER_STEAL_COLLISION = $UserStealCollision
$env:DD_INJECT_USER_STEAL_FOUL = if ($UserStealFoul) { '1' } else { '0' }
$env:DD_INJECT_USER_STEAL_SAME_PLAYER = if ($UserStealSamePlayer) { '1' } else { '0' }
$env:DD_INJECT_INBOUND_RULE_FRAME = $InboundRuleFrame.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_INJECT_INBOUND_RULE_CASE = $InboundRuleCase.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_INJECT_EXCEPTIONAL_REASON_FRAME = $ExceptionalReasonFrame.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_INJECT_SHOT_KIND_CASE = $ShotKindCase.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_USER_SHOT_DEPTH = $UserShotDepth.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_USER_SHOT_X = $UserShotX.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_USER_POSITION_FRAME = $UserPositionFrame.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_SCORE_AUDIO_FREEZE_FRAME = $ScoreAudioFreezeFrame.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:DD_CPU_REGION2_PROBE = $CpuRegion2Probe

$process = Start-Process -FilePath $FceuxPath `
    -ArgumentList @('-nothrottle', '1', '-turbo', '1', '-srendline', '0', '-erendline', '239', '-lua', ('"{0}"' -f $scriptPath), ('"{0}"' -f $RomPath)) `
    -WindowStyle Hidden -PassThru -Wait
if ($process.ExitCode -ne 0) { throw "FCEUX exited with code $($process.ExitCode)" }
$statePath = Join-Path $captureRoot 'tipoff-state.bin'
if (-not (Test-Path -LiteralPath $statePath -PathType Leaf)) { throw 'FCEUX did not produce the tip-off state trace.' }
Write-Host "Tip-off/gameplay trace: $captureRoot"
