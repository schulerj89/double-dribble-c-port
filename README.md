# Double Dribble Native C Port

Source-only native C port of **Double Dribble (USA) (Rev 1)** for NES. The runtime does not emulate a 6502/PPU/APU and never opens the ROM. A validated importer converts the user-owned ROM into an ignored, versioned asset pack.

## Current milestone

The native Win32 program renders the title screen, plays the spoken "Double Dribble" cue and 1P-confirm music, plays the complete 1P city/road introduction, implements the original shooting-driven configuration screen with its looping four-channel music, and follows END through tip-off, the opening CPU shot/rebound sequence, dead-ball formation, inbound pass, and resumed possession. B can contest the tip; native CPU carriers choose a pass or shot, paired CPU defenders recognize the native user-shot state and can block the owned airborne ball, and all players continue updating on the original alternating 30 Hz team cadence. On defense, B switches the role-zero user while preserving the original reciprocal opponent links, so A can block with the newly selected player. User and CPU shots use the original facing-specific shooting poses. User state `$03` preserves takeoff momentum while airborne, the ball releases through the recovered fixed-point arc, and shots can naturally make or miss through the original rim classifier. A make drives the recovered three-phase net tiles and complete `$18->$1F/$22` scoring cue before the automatic CPU inbound; make and miss pickups attach the ball with the exact `$B035` hand-offset axes and initialize every CPU pass on the original `$9018->$B0B8` schedule. Both outcomes install their traced formations: a make uses the ROM-backed `$8503/$8507` targets, while a miss reaches `$AF46->$9635` reason `$16` and the wrapped baseline target. The inbound flow also includes the extended packed boundary target, role/pair swaps, 5-tick inbound timeout, 10/24-tick violations, return-to-backcourt rule, sloped out-of-bounds geometry, exceptional foul reasons `$17/$1A`, and the exact `$2C` rule whistle. DDAP v18 carries the make/rebound formation tables, eight shot-pose indices, six four-tile net frames, and normalized score cue alongside the camera-selected court CHR streams, CPU route tables, live dribble audio, rule whistle, and `$09/$25` three-point cues. Stable title, intro, configuration, and pre-jump tip-off checkpoints match local FCEUX references exactly; later gameplay remains a bounded port rather than a complete match. Every build runs deterministic gameplay regressions against the generated asset pack.

## Build

```powershell
.\build.ps1 -RomPath 'F:\Games\NES\Double Dribble\Double Dribble (USA) (Rev 1).nes'
```

Every build requires the supported ROM, produces `build\double-dribble.assetpack`, runs gameplay regressions, and prints the validated Ghidra-to-C gameplay coverage. No ROM-derived outputs are tracked by Git.

Run the native port:

```powershell
.\build\double_dribble_game.exe .\build\double-dribble.assetpack
```

Use Up/Down to choose a title option. Press Enter, Space, or X on **1P** to start the intro; the selected words continue flashing at the original eight-frame cadence until the scene changes. On the configuration screen, Up/Down moves between TIME, TEAM, LEVEL, and END. X (NES A) or Z (NES B) takes the selected basket shot. TIME cycles 5, 10, 20, and 30 minutes; TEAM cycles New York, Chicago, and Los Angeles; LEVEL cycles 1–3. END performs the original shot, plays its acceptance cue, and advances through the opening tip. Press Z (NES B) to jump for the tip. Once live, the flashing 1UP player moves with the arrow keys; hold Z to gather a shot and release Z to launch it, or press X (NES A) to pass while that player carries the ball. Escape exits.

The original no-input gameplay trace does not run a continuous background song. Its live audio driver repeatedly queues pulse/triangle/noise streams while the ball is in dribble state `$01`, then falls silent during dead-ball and other non-dribble intervals. The native port follows that state-driven behavior.

Reproduce the reference and native captures:

```powershell
.\tools\fceux\Capture-OriginalTitle.ps1 -FinalFrame 30
.\tools\Capture-NativeTitle.ps1
.\tools\Compare-TitleCaptures.ps1
.\tools\fceux\Capture-Original1PIntro.ps1 -StartFrame 75 -FinalFrame 2084 -TraceStart 1260 -TraceEnd 2084
.\tools\Capture-NativeIntro.ps1 -OriginalFrame 2040
.\tools\Compare-IntroCaptures.ps1 -OriginalFrame 2040
.\tools\Capture-NativeConfig.ps1 -OriginalFrame 2100
.\tools\Compare-ConfigCaptures.ps1 -OriginalFrame 2100
.\tools\fceux\Capture-Original1PIntro.ps1 -FinalFrame 2500 -TraceStart 2328 -TraceEnd 2490 -InputScript '2105:down,2107:down,2109:down,2112:a,2113:a' -CaptureRangeStart 2328 -CaptureRangeEnd 2490
.\tools\Capture-NativeTipoff.ps1
.\tools\Compare-TipoffCaptures.ps1
.\tools\fceux\Capture-TipoffGameplay.ps1
.\tools\fceux\Capture-TipoffGameplay.ps1 -CaptureName original-tipoff-jump-b-2502 -JumpStart 2502 -JumpEnd 2503 -JumpButton B -FinalFrame 2580
.\tools\ghidra\Run-GameplayLoopAnalysis.ps1
.\tools\ghidra\Run-GameplayAudioAnalysis.ps1
.\tools\Capture-NativeGameplay.ps1
.\tools\Compare-GameplayCaptures.ps1
.\tools\fceux\Capture-TipoffGameplay.ps1 -TraceStart 2580 -FinalFrame 2660 -CaptureName original-user-shot-block -JumpStart 2502 -JumpEnd 2502 -JumpButton B -PassFrame 2600 -PassEnd 2600 -PassButton B -BlockFrame 2606 -DisablePcCounts
.\tools\Capture-NativeDefenseBlock.ps1
.\tools\Compare-DefenseBlockCaptures.ps1
.\tools\fceux\Capture-TipoffGameplay.ps1 -TraceStart 2598 -FinalFrame 2815 -CaptureName original-user-shot-make-final -JumpStart 2502 -JumpEnd 2515 -JumpButton B -PassFrame 2600 -PassEnd 2600 -PassButton B -UserShotDepth 0x61 -UserShotX 0xF7 -DisablePcCounts
.\tools\fceux\Capture-TipoffGameplay.ps1 -TraceStart 2598 -FinalFrame 2820 -CaptureName original-user-shot-miss-final -JumpStart 2502 -JumpEnd 2515 -JumpButton B -PassFrame 2600 -PassEnd 2600 -PassButton B -DisablePcCounts
.\tools\fceux\Capture-TipoffGameplay.ps1 -TraceStart 2588 -FinalFrame 2640 -CaptureName original-user-shot-held -JumpStart 2502 -JumpEnd 2515 -JumpButton B -PassFrame 2600 -PassEnd 2612 -PassButton B -DisablePcCounts
.\tools\Capture-NativeShooting.ps1
.\build\double_dribble_port.exe --render-gameplay-moving-shot .\build\double-dribble.assetpack held .\captures\native-shooting\held-shot.bmp
.\build\double_dribble_port.exe --dump-gameplay-wav .\build\double-dribble.assetpack .\build\gameplay-dribble.wav
# Render the same live frame while holding Right (input mask 2).
.\tools\Capture-NativeGameplay.ps1 -TransitionFrames 399 -HeldInputMask 2
```

See `PORTING.md` for source addresses, Ghidra evidence, asset-pack boundaries, and fidelity status.
See `GAMEPLAY_COVERAGE.md` for the reproducible dispatcher counts, weighted translation percentage, missing states, and next coverage targets.
