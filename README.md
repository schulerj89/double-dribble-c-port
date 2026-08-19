# Double Dribble Native C Port

A source-only native C port of **Double Dribble (USA) (Rev 1)** for the NES.

This project translates the original game's behavior into portable C rather than embedding an emulator. The game runs from a generated asset pack and never reads or executes the NES ROM during normal play.

## Project status

The port currently includes:

- the title screen, voice cue, and 1P selection;
- the complete city and road introduction with music;
- time, team, and difficulty settings;
- tip-off and live player control;
- passing, shooting, rebounding, steals, blocks, and dunks;
- scoring, animated nets, sound effects, and free throws;
- inbounding, out-of-bounds calls, backcourt violations, and period progression;
- CPU offense, defense, routing, and possession decisions;
- native rendering and audio driven entirely by the asset pack.

Gameplay is actively being reconciled against the original game. The title, introduction, configuration, and opening gameplay sequences have received the most extensive fidelity testing.

## Requirements

- Windows
- Visual Studio 2022 Build Tools with the C++ workload
- PowerShell
- A user-owned copy of **Double Dribble (USA) (Rev 1)**

No game ROM or extracted game assets are included in this repository.

## Build and run

From a Visual Studio developer shell, run:

```powershell
.\build.ps1 -RomPath "<your Double Dribble Rev 1 ROM>"
```

The build validates the supported ROM, generates the local asset pack, compiles the native executables, and runs the regression tests.

Start the game with:

```powershell
.\build\double_dribble_game.exe .\build\double-dribble.assetpack
```

Generated assets, executables, captures, and analysis outputs are ignored by Git.

## Controls

| Action | Key |
| --- | --- |
| Move or navigate | Arrow keys |
| Start or confirm 1P | Enter, Space, or X |
| NES A / pass | X |
| NES B / jump or shoot | Z |
| Exit | Escape |

On the settings screen, use Up and Down to select a row and X or Z to change it. During gameplay, hold Z to gather a shot and release it to shoot. Press X to pass while carrying the ball. Defensive controls allow player switching, jumping, stealing, and blocking according to the current play state.

## Asset-pack design

The importer validates the supported game revision before creating a versioned asset pack. The shipping game loads only this generated pack; it has no runtime dependency on the ROM and contains no emulator or bank-switching layer.

Repository tests use synthetic fixtures where possible. ROM-derived assets, recordings, screenshots, and analysis outputs remain local and are not committed.

## Documentation

- [PORTING.md](PORTING.md) contains the reverse-engineering evidence and translation notes.
- [GAMEPLAY_COVERAGE.md](GAMEPLAY_COVERAGE.md) tracks gameplay implementation and verification coverage.
- [AGENTS.md](AGENTS.md) defines contribution, fidelity, and asset-handling requirements.

## Legal

This is an independent preservation and porting project. You must provide your own legally obtained copy of the supported game. Double Dribble and its original assets are the property of their respective rights holders.
