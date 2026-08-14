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

Planned title milestone entries:

- decoded title pattern data required by the native renderer;
- title nametable/attribute data;
- title palettes and render metadata;
- spoken-title DMC bytes plus decoded playback metadata, or PCM produced deterministically by the importer;
- provenance for the title loader and audio routine.

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

## Open research questions

- Identify the higher-level title/attract-mode dispatcher names around the recovered low-level routines.
- Match the native PCM against an FCEUX WAV capture including the NES nonlinear mixer response.
- Replace the current fixed title OAM construction with the complete named native title scene state machine as later animation states are ported.
