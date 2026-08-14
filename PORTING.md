# Double Dribble Native C Port

## Goal

Port **Double Dribble (USA) (Rev 1)** from NES 6502 assembly to native C. The finished program will reproduce the original game logic and presentation without emulating the NES CPU/PPU/APU and without distributing copyrighted game data.

The repository is source-only. A user supplies their legally obtained ROM to the importer, which produces the required asset pack. The native runtime consumes that pack and never consumes the ROM.

## Verified local reference

| Item | Value |
|---|---|
| ROM path (local only) | `F:\Games\NES\Double Dribble\Double Dribble (USA) (Rev 1).nes` |
| SHA-256 | `BF397EAE9486044FCA90A99215330203D6F85CAB63A8072F28CACC139B5388CF` |
| File size | 131,088 bytes |
| iNES header | 8 x 16 KiB PRG, 0 x 8 KiB CHR, mapper 2, vertical mirroring |
| Consequence | Graphics are loaded into CHR RAM from PRG data; there is no CHR-ROM asset bank to copy. |

The path above is a workstation convenience, not a runtime default or a file to commit.

## Architectural boundary

```text
user ROM -> validated importer -> versioned .assetpack -> native C runtime
                ^                              |
                |                              +-> native renderer/audio/game state
        Ghidra + FCEUX evidence
```

The importer may understand iNES and UxROM layout. The runtime may not expose mapper banks, CPU registers, PPU registers, or NES address-space reads as its game architecture.

## UxROM source mapping

The ROM has eight 16 KiB PRG banks after the 16-byte iNES header.

- CPU `$8000-$BFFF`: selected PRG bank 0-6 during normal operation.
- CPU `$C000-$FFFF`: fixed PRG bank 7.
- File offset for fixed-bank CPU address `a`: `16 + 7 * 0x4000 + (a - 0xC000)`.
- File offset for switch-bank CPU address `a` in bank `b`: `16 + b * 0x4000 + (a - 0x8000)`.

Every reverse-engineering note for `$8000-$BFFF` must name the observed bank. A bare CPU address in that range is ambiguous.

## Reverse-engineering workflow

### FCEUX

FCEUX provides the behavioral oracle. Checked-in Lua scripts under `tools/fceux/` will make captures repeatable and may:

- advance from reset to a precise frame;
- capture screenshots;
- dump diagnostic PPU/RAM/APU state into ignored capture files;
- log mapper writes and relevant instruction entry points;
- record APU writes needed to identify the spoken title cue.

Emulator dumps are research evidence only. They are not runtime assets and are never committed.

### Ghidra and Ghidra MCP

Ghidra is the source of record for control-flow recovery, symbol naming, cross-references, and 6502-to-C conversion. The intended loop is:

1. Import the fixed PRG bank at `$C000` and each switchable bank as a named overlay at `$8000`.
2. Use FCEUX observations to identify the active bank and entry address.
3. Query/decompile through Ghidra MCP when its live server is connected.
4. Use the headless export scripts for reproducible CI/local reports and as a fallback when MCP is unavailable.
5. Rename functions by behavior only after evidence supports the name.
6. Translate one bounded routine or state transition at a time and record unresolved hardware side effects explicitly.

The repository will contain bridge/client and headless-analysis scripts, but no Ghidra project database or imported program bytes.

Current workstation status (2026-08-14): Ghidra 11.3 headless analysis is working and produced the evidence above. A connection probe to the local Ghidra MCP HTTP bridge at `127.0.0.1:13370` was refused because no live server is listening. This milestone therefore used Ghidra's reproducible headless analyzer, not a live MCP session. Connect and verify the external MCP server before treating subsequent interactive symbol/decompile requests as MCP-backed evidence.

## Asset-pack policy

The first asset-pack version will use a small directory-based binary container with:

- magic and format version;
- approved source-ROM identity;
- typed entry directory;
- per-entry length and checksum;
- sanitized source provenance (bank/offset/transform), not copied source bytes beyond the entry payload itself.

Asset-pack v5 entries:

- decoded title pattern data required by the native renderer;
- title nametable/attribute data;
- title palettes and render metadata;
- spoken-title DMC bytes plus decoded playback metadata, or PCM produced deterministically by the importer;
- provenance for the title loader and audio routine.
- decoded road-intro pattern/nametable state and palette;
- normalized road-update commands with ROM pointers removed;
- procedural intro OAM metadata and a native three-channel note score.
- decoded game-configuration pattern/nametable state and palette;
- importer-expanded configuration metasprites and configuration metadata.
- normalized title-confirm music events and recovered configuration value/physics tables.

The asset pack is always local and ignored by Git.

## Milestone 1: title plus spoken cue

Status: **title milestone implemented and visually verified**.

### Recovered title path

FCEUX execution evidence and the Ghidra 6502 decompiler agree on the following path:

| Behavior | Source evidence | Native counterpart |
|---|---|---|
| Reset | fixed bank `$C001` | native process initialization |
| PPU stream decoder | fixed bank `$C566-$C5C6` | `dd_decode_stream` |
| Pattern stream 0 | bank 5 `$AFB2`, 484 encoded bytes | decoded into asset-pack PPU entry at `$0000` |
| Pattern stream 1 | bank 6 `$B0A0`, 2,991 encoded bytes | decoded into asset-pack PPU entry at `$1000` |
| Title nametable stream | bank 2 `$A7C2`, 525 encoded bytes | decoded into asset-pack PPU entry at `$2000` |
| Title palette | fixed bank table `$C946`, written through `$CB63-$CBAF` | copied into asset-pack PPU palette state |
| DMC setup | fixed bank `$CD70-$CD9A` | native DMC-to-PCM conversion |
| Spoken sample | fixed bank `$EAC0`, 3,073 bytes | `title.dmc` asset-pack entry |

At frame 10 the original writes `$4010=$0F`, `$4011=$00`, `$4012=$AB`, `$4013=$C0`, then enables DMC with `$4015=$1F`. The native cue uses rate index 15, initial DAC 0, and the resulting 3,073-byte sample.

The title is a background/sprite composition. The initial 20 8x16 OAM records are rebuilt procedurally by the importer from the recovered title layout; the runtime renders them with sprite palette, flip, priority, and transparency rules. It does not store or replay the FCEUX screenshot.

Acceptance checklist:

- [x] Original stable title frame captured at frame 30.
- [x] Spoken cue trigger recorded at frame 10.
- [x] Title loader entry, active banks, and stream transform identified in Ghidra/FCEUX.
- [x] DMC setup routine, source span, playback rate, and length identified.
- [x] `--build-assetpack` validates Rev 1 and writes title/audio entries.
- [x] Native executable refuses to launch without a valid asset pack.
- [x] Native renderer reproduces the reference title frame.
- [x] Native audio plays the recovered spoken cue at frame-10 timing.
- [x] Original and native screenshots are compared with a repeatable exact diff.

## Fidelity comparison

Reference and native captures remain local:

```text
captures/original/title.png
captures/native/title.png
captures/diff/title-diff.png
```

The native framebuffer is 256 x 240. This workstation's FCEUX configuration captures visible scanlines 8 through 231 as a 256 x 224 PNG, so `Compare-TitleCaptures.ps1` compares it to native rows 8 through 231 without scaling or tolerance. Current result: **0 differing pixels out of 57,344**. Audio is exported as mono 44.1 kHz, 16-bit PCM from the recovered DMC bitstream.

## Milestone 2: complete 1P introduction and music

Status: **complete**. Selectable 1P now runs from the original transition through the road approach, two-pass balloon object sequence, rising/waving U.S. flag, and the end of the music cue. Original frame 2085 begins loading the separate game-configuration screen and is the next scene boundary.

### Original input and timing

`Capture-Original1PIntro.ps1` presses Start for controller 1 on frames 75 and 76. The title remains active during the original transition, the new scene streams begin at frame 158, its palette is installed at frame 166, music begins at frame 166, and the road scene first becomes visible at frame 170. The native state machine preserves the corresponding 95-frame delay from selection to the first visible intro frame.

### Recovered road scene

| Behavior | Source evidence | Native counterpart |
|---|---|---|
| Intro pattern stream 0 | bank 5 `$B196`, 3,294 encoded bytes | decoded `intro.ppu` data at `$0000` |
| Intro pattern stream 1 | bank 2 `$AD67`, 2,647 encoded bytes | decoded `intro.ppu` data at `$1000` |
| Road nametable stream | bank 1 `$A030`, 555 encoded bytes | decoded `intro.ppu` data at `$2000` |
| Intro palette stream | fixed bank `$C996`, palette bytes begin at `$C998` | asset-pack intro palette |
| Road updater | bank 1 `$9348-$93BD` | normalized phase/update iterator in `dd_render_intro` |
| Phase counts | bank 1 `$9402-$9416` | importer-generated update records |
| Phase pointer table | bank 1 `$9504-$952D` | resolved by importer; no ROM pointers at runtime |
| NMI update consumer | fixed bank `$CB63-$CBAF`, RAM buffer `$0700` | native PPU-state command application |
| Blimp OAM | frame-170 RAM/OAM observation, 16 8x16 sprites | procedural OAM plus one-pixel-per-eight-frame motion |
| Balloon/flag controller | bank 1 `$9329`, `$942F-$9501` | native eight-object timing, re-route, completion, flag-rise, and wave state |
| Balloon tables | bank 1 `$9417-$942E` | asset-pack delays, animation IDs, and X routes |
| Metasprite builder | bank 2 `$8154-$81C2` | native variable-record metasprite expansion to OAM |
| Balloon/flag metasprites | bank 2 pointer table `$828D`, IDs `$6F`, `$74`, `$7A` | bounded asset-pack records resolved by the importer |

Ghidra recovered `$0471` as the 21-phase index, `$0472` as the update index inside a phase, and `$0473` as the countdown. Each phase's first update waits 48 frames; its remaining row updates execute on consecutive frames. The importer resolves the source pointer tables into 170 bounded records containing only delay, size, PPU address, and tile values. The native runtime advances those records as a scene state machine; it does not execute 6502 code or replay an emulator log.

### Music path

FCEUX observed the active music driver in switch bank 1. Ghidra anchors `$80ED-$83AA` cover channel sequencing, envelope/control updates, and timer writes to `$4000-$400B`. The importer emits a normalized native score for two pulse voices and one triangle voice, with provenance pointing to the bank-1 driver/data region. `dd_build_intro_music_wav` synthesizes those voices directly at 44.1 kHz; there is no APU emulator and no per-frame APU-write log in the asset pack.

The score contains 219 normalized note events, covers the complete 1,920-frame scene window, and stops instead of looping. Its final timer events occur at native frame 1,781, followed by the original musical tail/silence while the flag remains on screen. Envelope and nonlinear-mixer matching remain audio-fidelity work; the melody, channel periods, duty choices, and entry timing come from the traced bank-1 driver.

### Screenshot fidelity

The checked-in capture tools compare original frames against native logical frames (`original - 165`). Stable road checkpoints 170, 180, 240, 300, 360, 600, 1200, and 1260 plus balloon/flag checkpoints 1320, 1500, 1680, 1800, 1920, and 2040 report **0 differing pixels out of 57,344**. Frames captured during an NMI row update can contain a partial scanline from the old/new tile state and are intentionally not used as stable regression checkpoints.

The supplied application art is committed only as `resources/double_dribble.ico`. It is embedded as Windows branding and is never part of the generated game-content asset pack.

## Milestone 3: game-configuration screen through END

Status: **configuration sequence implemented through END**. Original frame 2084 is the last flag frame. Frames 2085-2091 show the cyan loading field, frames 2092-2096 are black, and frame 2097 is the first visible configuration frame. The native timeline preserves those boundaries.

| Behavior | Source evidence | Native counterpart |
|---|---|---|
| Configuration initializer | bank 1 `$A25B-$A2D8` | native scene entry and configuration metadata |
| Object initialization table | bank 1 `$A2DB-$A306`, 11 records | importer-expanded `config.oam` |
| Pattern stream | bank 4 `$B0B4`, 3,010 encoded bytes | decoded `config.ppu` data at `$1000` |
| Pattern/scene stream | bank 4 `$A74B`, 2,409 encoded bytes | decoded configuration PPU state |
| Nametable stream | bank 1 `$A7FD`, 496 encoded bytes | decoded `config.ppu` data at `$2000` |
| Palette | fixed bank `$C96F`, 32 bytes | asset-pack configuration palette |
| Metasprite pointer table/builder | bank 2 `$828D`, `$80E8-$8151` | bounded importer-side metasprite expansion |
| Row navigation | bank 1 `$A307-$A363`, positions `$A364-$A367` | wrapping native Up/Down selection |
| A/B shot initializer | bank 1 `$A38C-$A3EA` | native per-row fixed-point shot state |
| Shot/landing handler | bank 1 `$A40F-$A4D4`, `$A73C-$A7BF` | native player animation, ball arc, basket animation, and landing |
| TIME handler/data | bank 1 `$A4EB-$A54B`, tables `$A368-$A38B` | 5:00, 10:00, 20:00, and 30:00 tile patches |
| TEAM handler/data | bank 1 `$A54C-$A58E`, `$A609-$A630`, tables `$A63D-$A6BC` | New York, Chicago, and Los Angeles names and palettes; CPU team is skipped |
| LEVEL handler/data | bank 1 `$A593-$A5B7`, table `$A58F-$A591` | three highlight positions |
| END boundary | bank 1 `$A631-$A63C` | original 128-frame handoff retained, then native execution stops before gameplay |
| Computed dispatcher | fixed bank `$C41C-$C437`, inline table at bank 1 `$A4D5-$A4DC` | direct native calls to the four named handlers |

The title-confirm sound was traced from original frames 76-138 through the bank-1 APU driver and normalized into the v5 asset pack as `select.music`. It plays immediately when 1P is accepted and ends before the road-intro score begins.

The v5 pack adds `config.assets`, containing only importer-extracted menu tables and metasprites needed by the native state machine. The runtime still has no ROM access, bank switching, instruction interpretation, or frame-log playback.

Original and native frames 2097/2100 report **0 differing pixels out of 57,344**. The TEAM shot launch, jump, basket, and settled Chicago checkpoints also compare exactly; original frame 2180 differs only in the 31 overlapping ball pixels (0.0541%). `Capture-NativeConfig.ps1` and `Compare-ConfigCaptures.ps1` reproduce the configuration screenshot check without storing any game assets in the repository.

## Open research questions

- Identify the higher-level title/attract-mode dispatcher names around the recovered low-level routines.
- Match the native PCM against an FCEUX WAV capture including the NES nonlinear mixer response.
- Replace the current fixed title OAM construction with the complete named native title scene state machine as later animation states are ported.
- Trace and port the gameplay scene entered after the END handoff; this milestone intentionally stops before it.
