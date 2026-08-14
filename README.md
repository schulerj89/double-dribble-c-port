# Double Dribble Native C Port

Source-only native C port of **Double Dribble (USA) (Rev 1)** for NES. The runtime does not emulate a 6502/PPU/APU and never opens the ROM. A validated importer converts the user-owned ROM into an ignored, versioned asset pack.

## Current milestone

The native Win32 program renders the title screen, plays the spoken "Double Dribble" cue and 1P-confirm music, plays the complete 1P city/road introduction, implements the original shooting-driven configuration screen with its looping four-channel music, and follows END through tip-off, the opening CPU shot/rebound sequence, dead-ball formation, inbound pass, and resumed possession. B can contest the tip; native CPU carriers now choose a pass or shot and all players continue updating on the original alternating 30 Hz team cadence. The post-inbound receiver takes the observed shot-decision branch instead of repeating the opening basket run, and the recovered off-ball states remain active. DDAP v11 carries the camera-selected court CHR streams and the observed 18-frame live dribble/gameplay APU sequence. Stable title, intro, configuration, and pre-jump tip-off checkpoints match local FCEUX references exactly; later gameplay remains a bounded port rather than a complete match. Every build runs deterministic gameplay regressions against the generated asset pack.

## Build

```powershell
.\build.ps1 -RomPath 'F:\Games\NES\Double Dribble\Double Dribble (USA) (Rev 1).nes'
```

Every build requires the supported ROM, produces `build\double-dribble.assetpack`, runs gameplay regressions, and prints the validated Ghidra-to-C gameplay coverage. No ROM-derived outputs are tracked by Git.

Run the native port:

```powershell
.\build\double_dribble_game.exe .\build\double-dribble.assetpack
```

Use Up/Down to choose a title option. Press Enter, Space, or X on **1P** to start the intro; the selected words continue flashing at the original eight-frame cadence until the scene changes. On the configuration screen, Up/Down moves between TIME, TEAM, LEVEL, and END. X (NES A) or Z (NES B) takes the selected basket shot. TIME cycles 5, 10, 20, and 30 minutes; TEAM cycles New York, Chicago, and Los Angeles; LEVEL cycles 1–3. END performs the original shot, plays its acceptance cue, and advances through the opening tip. Press Z (NES B) to jump for the tip. Once live, the flashing 1UP player moves with the arrow keys; Z shoots and X (NES A) passes while that player carries the ball. Escape exits.

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
.\build\double_dribble_port.exe --dump-gameplay-wav .\build\double-dribble.assetpack .\build\gameplay-dribble.wav
# Render the same live frame while holding Right (input mask 2).
.\tools\Capture-NativeGameplay.ps1 -TransitionFrames 399 -HeldInputMask 2
```

See `PORTING.md` for source addresses, Ghidra evidence, asset-pack boundaries, and fidelity status.
See `GAMEPLAY_COVERAGE.md` for the reproducible dispatcher counts, weighted translation percentage, missing states, and next coverage targets.
