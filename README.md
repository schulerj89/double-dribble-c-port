# Double Dribble Native C Port

Source-only native C port of **Double Dribble (USA) (Rev 1)** for NES. The runtime does not emulate a 6502/PPU/APU and never opens the ROM. A validated importer converts the user-owned ROM into an ignored, versioned asset pack.

## Current milestone

The native Win32 program renders the title screen, plays the spoken "Double Dribble" cue and 1P-confirm music, plays the complete 1P city/road introduction, implements the original shooting-driven configuration screen, and follows END through the court-loading transition to the opening tip-off formation. Stable title, intro, configuration, and tip-off checkpoints match local FCEUX references exactly.

## Build

```powershell
.\build.ps1 -RomPath 'F:\Games\NES\Double Dribble\Double Dribble (USA) (Rev 1).nes'
```

Every build requires the supported ROM and produces `build\double-dribble.assetpack`. No ROM-derived outputs are tracked by Git.

Run the native port:

```powershell
.\build\double_dribble_game.exe .\build\double-dribble.assetpack
```

Use Up/Down to choose a title option. Press Enter, Space, or X on **1P** to start the intro. On the configuration screen, Up/Down moves between TIME, TEAM, LEVEL, and END. X (NES A) or Z (NES B) takes the selected basket shot. TIME cycles 5, 10, 20, and 30 minutes; TEAM cycles New York, Chicago, and Los Angeles; LEVEL cycles 1–3. END performs the original shot, plays its acceptance cue, and advances to the pre-jump formation. Escape exits.

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
```

See `PORTING.md` for source addresses, Ghidra evidence, asset-pack boundaries, and fidelity status.
