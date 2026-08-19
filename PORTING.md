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

The confirmation flash affects the selected title text, not the cursor sprite. A full frame-by-frame FCEUX capture shows original frame 75 visible, frames 76-84 blank, frames 85-92 visible, and then alternating eight-frame blank/visible bands through frame 156; frame 157 begins the scene handoff. The fixed-bank state handler at `$C220-$C248` decrements the `$0025` timer from `$50`, tests bit 3, shifts that bit into the sign flag of VRAM command `$02/$82`, and tail-calls the `$C724` command builder. `$C036-$C085` consumes the previous VRAM buffer before running the state handler, accounting for the displayed one-frame pipeline. Ten native checkpoints from confirmation frames 1 through 74 compare at **0 differing pixels out of 57,344 each** against the corresponding original blank/visible frames 76 through 149.

The configuration song is selected while the screen is black and begins on original frame 2093, four frames before the visible configuration frame. A focused FCEUX trace records pulse-1 `$8A03`, pulse-2 `$89C5`, and triangle `$8A4A` channel pointers initialized at frame 2092; the complete channel state repeats at frame 2989, establishing an 896-frame loop. Ghidra anchors `$808A`, `$80ED`, `$813D`, `$8241`, `$82C3`, `$82D5`, `$83AA`, and `$847D` cover the bank-1 sequencer, envelope path, timer writes, and pointer progression. The v7 importer emits 213 normalized pulse, triangle, and noise events as `config.music`. The native mixer synthesizes one 14.933-second loop and Win32 repeats it until END acceptance replaces it with `end.music`.

`tools/fceux/Capture-ConfigMusic.ps1` reproduces the channel-state, APU-write, music-RAM, screenshot, and executed-PC evidence under ignored `captures/original-config-music/`. `tools/ghidra/Run-ConfigMusicAnalysis.ps1` regenerates the ignored bank-1 driver/data report. `--dump-config-wav` exports the native loop for audio inspection without reading the ROM at runtime.

The v7 pack retains `config.assets`, adds `config.music`, and carries the bounded tip-off PPU/OAM, END-score, and DPCM entries. The runtime still has no ROM access, bank switching, instruction interpretation, or frame-log playback.

Original and native frames 2097/2100 report **0 differing pixels out of 57,344**. The TEAM shot launch, jump, basket, and settled Chicago checkpoints also compare exactly; original frame 2180 differs only in the 31 overlapping ball pixels (0.0541%). `Capture-NativeConfig.ps1` and `Compare-ConfigCaptures.ps1` reproduce the configuration screenshot check without storing any game assets in the repository.

## Milestone 4: END audio and opening tip-off formation

Status: **pre-jump formation complete**. FCEUX presses Down on original frames 2105, 2107, and 2109, then A on 2112/2113. The basket acceptance reaches the END handler at frame 2201. The configuration remains visible for 126 more frames, frame 2327 contains the original partial clear, frame 2328 is black, frame 2336 switches to blue, and frame 2345 first displays the assembled court. This milestone intentionally freezes before the ball-launch state begins at frame 2471.

| Behavior | Source evidence | Native counterpart |
|---|---|---|
| END acceptance tone | bank 1 APU driver `$80ED-$83AA`, original frames 2185-2221 | ten normalized pulse events in `end.music` |
| END handoff | bank 1 `$A631-$A63C`, `$048F=$80` at frame 2201 | 127/135/144-frame black, blue, and visible boundaries |
| Court CHR streams | bank 3 `$8D1B`/`$8001`, 3,510/3,354 encoded bytes | decoded `tipoff.ppu` pattern tables |
| HUD clears | fixed bank `$C65F`/`$C674`, 42/21 encoded bytes | decoded into both nametables |
| Court nametables | bank 2 `$A9CF`/`$ABC6`, 503/417 encoded bytes | decoded court/HUD state plus score-label patches |
| Formation/gameplay roots | bank 0 `$8491`, `$9395`, `$94D9`, `$9CA0`, `$9CF6`, `$A84C`, `$AA20`, `$B11F`, `$B501` | named Ghidra evidence for object initialization, fixed-point helpers, and per-frame clears |
| Metasprite expansion | bank 2 `$8000-$81C2` and pointer table `$828D` | native 8×16 OAM renderer and recovered formation ordering |
| Split scroll | RAM `$0043=$7F`, fixed HUD through scanline 63 | scroll-zero HUD/crowd and `$7F` court camera |
| Sprite overflow | alternating formation OAM observed on frames 2358-2363 | native eight-sprites-per-scanline evaluation |
| Tip-off DPCM | fixed `$CD70-$CD9A`; frame 2341 writes `$4010=$0F`, `$4011=$00`, `$4012=$DE`, `$4013=$84` | 2,113 ROM-derived DPCM bytes, decoded natively at frame 140 of the handoff |

`capture_original_title.lua` now records aggregate executed-PC counts for every active switch bank in `pc-counts.csv`. The 2328-2490 formation window showed bank 2 dominating the shared metasprite work and bank 0 owning the gameplay/object loops. `Run-TipoffAnalysis.ps1` reproduces the bank-0 Ghidra report; the fixed-bank report includes the audio reset and DPCM setup anchors. Headless Ghidra was sufficient for this slice, so no live MCP dependency is required to rebuild the evidence.

`Capture-NativeTipoff.ps1` exports the formation plus both native audio cues. `Compare-TipoffCaptures.ps1` compares original frame 2359 to native rows 8-231 and reports **0 differing pixels out of 57,344**. The generated `.wav`, screenshot, trace, Ghidra project/report, and asset pack all remain ignored local outputs.

## Milestone 5 research: jump ball, possession, and gameplay loop

Status: **bounded native slice implemented through first live control**. `Capture-TipoffGameplay.ps1` reproduces the END selection and records every original frame from 2320 through 2760 as a 2 KiB RAM snapshot. Its focused write log covers gameplay zero-page state, all object arrays at `$0340-$06BF`, and `$07E0-$07FF`; it also records executed bank-0 PCs and stable screenshots. `Run-TipoffAnalysis.ps1` regenerates the expanded Ghidra report from those dynamic anchors.

### Observed transition

| Original frame | Observed state | Bank-0 evidence |
|---|---|---|
| 2345 | Court, HUD, ten-player formation, and `1ST PERIOD START` are visible. | Existing formation initialization and renderer roots. |
| 2470 | Ball slot 0 changes to launch-preparation state `$0A`. | Dynamic write to `$0340`. |
| 2471 | Ball enters airborne state `$05`; elapsed flight is zero, gravity scale is `$0C`, and initial vertical term is `$0305`. | `$B017-$B034` initializes `$004A`, `$004C`, `$0430/$0440`, and `$0340`. |
| 2505-2509 | The ball reaches its apex at height `$4D.xx`; jumper/contact arbitration becomes active. | `$9B84` height results and `$005E` writes through `$8DFE`. |
| 2531 | The no-input run awards the ball to player slot 7. Ball state becomes `$00`, `$005B` changes from no-owner sentinel `$0E` to `$07`, and sound ID `$20` is queued. | `$8DFE-$8E34`. |
| 2532-2556 | The ball is attached above the winning jumper while that player lands. | `$ACC7` sets ball height to owner height + `$18`; `$B035` supplies facing-dependent owner offsets. |
| 2557 | The jump phase ends. Slot 7 becomes the active carrier (`$0048=$07`), possession/direction becomes `$0050=$08`, ball state becomes held/dribble `$01`, and all ten player states are reassigned for live play. | `$8E35-$9380`, `$AD0E-$AD40`. |
| 2558 onward | Regular player AI, movement, camera following, held-ball animation, projection, and rendering run without a separate tip-off scene loop. | `$89B2`, `$994C`, `$9E90-$A014`, `$A84C-$AA74`, `$B035-$B400`. |

The no-input trace establishes the deterministic CPU-possession branch in original slot 7. The later A/B sweep documented below establishes B as jump and proves the alternate original-slot-2 win branch at the frame-2502 timing.

### Recovered object/game loop

The gameplay objects use structure-of-arrays storage. Slot 0 is the ball, slot 1 follows the ball as its secondary visual, and slots 2-11 are the ten players. `$004B` is the current-object index.

Each observed live frame performs these bounded passes:

1. `$9E90-$9EA4` iterates player slots 2-6 and calls `$9EBD` for state, human/AI, movement, and animation work.
2. `$994C-$9965` iterates player slots 7-11 and calls the corresponding `$89B2` state dispatcher.
3. The ball dispatcher runs with `$004B=0`; its `$0340` state selects free-flight, attached, dribble, pass, shot, or rebound behavior.
4. `$9CA0` and `$9CF6` integrate world position within court bounds. `$A85A-$A895` projects world coordinates through camera `$0043/$0044` to screen arrays `$0320/$0330` and rejects offscreen objects through `$0460`.
5. `$A896-$A8E5` converts state, facing, and animation phase into a metasprite ID in `$0300`; the bank-2 metasprite builder then emits OAM.

The current player state in `$0340 + slot` is a direct dispatcher index, not a bank number. Live initialization in the captured branch assigns ball state `$01`, slot 2 state `$0F`, slots 3-6 state `$20`, carrier slot 7 state `$25`, slots 8-9 state `$40`, slot 10 state `$3C`, and slot 11 state `$3E`. The native port should replace these numeric dispatch slots with a named enum while retaining the observed transitions.

### Ball and movement fields

| Original RAM | Recovered meaning | Confidence |
|---|---|---|
| `$0340 + slot` | Object action/state dispatcher index | high |
| `$0350 + slot` | Eight-way facing/animation direction | high |
| `$0360/$0370/$0380 + slot` | 24-bit fixed-point longitudinal world coordinate | high |
| `$0390/$03A0 + slot` | Signed longitudinal velocity | high |
| `$03B0/$03C0/$03D0 + slot` | Fixed-point court-depth coordinate | high |
| `$03E0/$03F0 + slot` | Signed court-depth velocity | high |
| `$0410/$0420 + slot` | 8.8 height above the court | high |
| `$0430/$0440 + slot` | Base vertical term used by ballistic states | high |
| `$0450 + slot` | Metasprite animation phase | high |
| `$0460 + slot` | Offscreen/out-of-bounds result | high |
| `$0480 + slot` | Quantized movement/target direction | medium-high |
| `$04F0 + slot` | Direction/elapsed phase for height scripts; flight counter for ball slot 0 | high |
| `$0500/$0510 + slot` | Pointer into signed height-delta animation data | high |
| `$0580 + slot` | Logical roster/object mapping used by AI and control swaps | medium-high |
| `$0670/$0680 + slot` | Current and edge controller/AI input masks | high |
| `$0690 + slot` | Human/AI or team-control classification used by the player dispatchers | medium |
| `$005B` | Ball owner slot; `$0E` is the observed no-owner sentinel | high |
| `$0048` | Active ball carrier/camera-follow player | high |
| `$0050` | Possession side and attack-direction bitfield (`$08`/`$40` tested independently) | high |

### Ball physics and ownership

There is deliberately no NES physics emulator to reproduce. The native model can express the recovered behavior directly:

- **Airborne/free ball:** `$9B84` computes a 16-bit vertical delta from base vertical term `$0430/$0440` minus `elapsed * gravity_scale`, then adds it to height `$0410/$0420`. The tip-off launch uses base `$0305`, scale `$0C`, and increments elapsed `$004A` once per frame. The observed height rises from `$18.00` to approximately `$4D.40`, then falls to `$34.C0` before ownership is awarded.
- **World motion:** `$9CA0` integrates the 24-bit longitudinal coordinate and zeros velocity at the recovered court limits; `$9CF6` does the same for court depth. Ball flight states call these explicitly, while ordinary object movement uses the shared `$A84C` integrator/projection path.
- **Jump-ball contact:** `$A6C3` tests a 4-by-4 volume around the ball against a player point shifted six world units toward center and eight height units upward. `$9B42` is the shared rectangle/volume comparator. `$8DFE` combines that result with the jumper's signed height-delta script to select the owner.
- **Held ball:** `$B035` indexes a facing-dependent offset table, then copies the owner's longitudinal and depth coordinates plus those offsets into ball slot 0. This is attachment, not integration.
- **Landing/award:** `$ACC7` keeps the ball at `owner.height + 0x18` until the winner's jump script terminates. `$AD0E` then initializes held/dribble state `$01`, resets the ball height to `$10`, and attaches/projects it through `$B035/$B400`.
- **Dribble height:** `$9ABD` consumes signed byte deltas through `$0500/$0510`; `$80` ends at height `$10` and `$81` reverses the read direction. This produces the bounce without a ballistic solver.
- **Basket/free-ball collision:** `$B473` sweeps seven small collision samples near the hoop. A hit clears the owner, marks the collision in `$0490`, queues the rim sound, and negates longitudinal velocity. This routine was not reached during the opening-tip branch but is part of the same recovered ball framework.

The native implementation should use explicit `Player`, `Ball`, `Possession`, and `Camera` structures, a named action enum, and fixed-point helpers with the recovered limits. Asset-pack entries may contain only the bounded facing-offset and height-delta tables plus their source provenance; runtime state and physics remain native C.

### Native implementation boundary

DDAP v10 carries the bounded, checksummed `tipoff.assets` entry containing the 42 bank-2 gameplay metasprites, the 48-byte held-ball offset table at bank 0 `$B07B`, the 32-byte height-script region at bank 0 `$9B29`, the 20-byte role target table at fixed bank 7 `$D745`, the 14-byte spacing table at bank 0 `$8452`, and two 672-byte court CHR streams. The left stream begins at bank 0 `$B59E`; the right stream begins at `$B83E`. The runtime receives normalized graphics, offsets, signed deltas, and packed court targets only; it does not receive ROM banks, 6502 state, or an instruction stream.

`dd_gameplay.c` expresses the recovered sequence as native `Player`, `Ball`, `Camera`, possession, and inbound state. It reproduces the opening ball parabola, both jumpers' table-driven height, CPU and well-timed user tip results, owner-relative ball attachment, the first CPU decision, shot states `$04-$07`, pass state `$02`, the observed dead-ball/inbound sequence, and resumed CPU possession. Arrow keys update the native 1UP player. Z (NES B) jumps during the tip and shoots while carrying; X (NES A) passes while carrying. The controlled player's palette follows the observed two-frames-on/two-frames-off flash without hiding its sprite.

`Capture-NativeGameplay.ps1` renders any gameplay checkpoint without launching the Win32 window. `Compare-GameplayCaptures.ps1` compares native and FCEUX frames after cropping native overscan. After the post-tip raster correction, frame 2531/native 330 differs by 1,437 pixels out of 57,344 (2.5059%) and the frame-2557 handoff differs by 2,334 pixels (4.0702%); complete clock logic, collision/steal branches, exact dynamic OAM ordering, and the rest of the match remain future slices.

### Opening CPU decision slice

The live player scheduler is split across bank 0 `$993A-$9976` and `$9E70-$9EA4`. The two five-player teams alternate on the low bit of `$001A`, so each team is evaluated at 30 Hz. On even phases, `$9E76-$9E83` advances persistent priority cursor `$004D` through original slots `$07-$0B`, wrapping to `$07`. On odd phases, `$9E86-$9E8D` saves that cursor, `$9E90-$9EA4` subtracts five while dispatching original slots `$02-$06`, and then restores the persistent value. Thus both teams receive the same rotating within-team role. The native scheduler preserves this stateful cadence, initializes phase `$001A=$DC` and priority original slot `$0A` at original frame 2557, and produces the native scheduled-player cycle `8,3,9,4,5,0,6,1,7,2,8`. A fresh FCEUX trace records `$004D` beside `$001A`; the native regression asserts every transition across the ten-frame cycle.

The CPU uses packed court coordinates rather than independent hard-coded X/Y destinations. Bank 0 `$AB72` packs the longitudinal and depth axes into `$05B0/$05C0`; `$ABAB-$ABCC` expands `$05D0/$05E0` targets back to world coordinates. `$AC2A-$AC57` divides that packed value into seven court regions. The native helpers implement those transformations directly with named `Player` target fields.

The first native CPU slice translates these observed handlers:

| Original routine | Action | Native behavior |
| --- | --- | --- |
| fixed bank 7 `$D6FD-$D742` | state `$40` | role/team/half-court target selection from `$D745`, periodic and arrival-driven retargeting |
| bank 0 `$829E-$836E` | state `$3C` | opening cutter route with an occupancy-safe transition to `$3D` |
| bank 0 `$83C5-$842C` | state `$3E` | seven-region spacing choice using `$8452`, rejecting an occupied first choice |
| bank 0 `$8B5A-$8BC5` plus fixed bank 7 `$D772-$DA39` | state `$25` | opening carrier route, arrival gates, and the observed `$25->$32->$26->$27` decision handoff; the full `$D99A` obstacle search remains follow-up work |

### Tip input, possession, and ball dispatcher trace

`Capture-TipoffGameplay.ps1` now accepts an explicit A/B press interval and the Lua trace covers gameplay RAM `$0700-$07DF`, PPU snapshots, and `$2006/$2007` writes. The A/B sweep established that B is the jump button. Pressing B on original frame 2502 (native transition frame 301) changes original player slot 2 from state `$10` to `$11` at bank 0 `$A61F`. The CPU is provisionally selected at frame 2531, then the successful user-contact branch changes `$005B` from original slot 7 to slot 2 at `$A68A` on frame 2532. Native player indices remove the two non-player object slots, so original slots 2 and 7 are native players 0 and 5.

The initial native slice reduced that result to the single provisional condition
`jump_start == 301`, which made a live win practically impossible and omitted
the later valid contact.  The translated path now advances `$9ABD`'s `$9B34`
height-stream pointer on the alternating dispatcher cadence, including the
two-byte descending turn after sentinel `$81`; `$A662` gates contact on the
resulting zero byte and `dd_jump_ball_contact` performs `$A6C3`'s 4-by-4 boxes.
The ten-point original B sweep is now a regression: original starts
2486/2490/2494/2498/2506/2518 lose, while 2502/2510/2514/2522 win.  The late
2522 case is not special-cased: its zero plateau and the descending CPU-owned
ball overlap at original frame 2552, exactly where the FCEUX trace records
`$005A=03`, `$005B=02`, and SFX `$20` through `$A681/$A68A/$A68C`.

The player dispatcher at bank 0 `$89B2` passes the action index to the fixed-bank indirect jump helper at `$C41C`. Its relevant opening chain is state `$25` at `$8B5A`, state `$26` at `$8D1F`, and state `$27` at `$8D57`. `$8D1F` changes the player to `$27`, selects ball state `$04`, installs the signed height/action data, and clears the flight velocities. `$8D57` continues the decision/shot path and calls the shared movement, collision, and decision helpers.

The ball dispatcher begins at bank 0 `$AC83` and uses the same `$C41C` helper. Its table resolves states `$00-$0A` to `$ACB6`, `$ACD6`, `$AD41`, `$ADF2`, `$AE0C`, `$AE25`, `$AEDE`, `$AF46`, `$AF72`, `$AFDD`, and `$B017`; states `$0B/$0C` take the dead-ball return at `$ACAB`. In the observed opening shot, `$AE0C` is the gather/owner-attachment handler and `$AE25` advances flight and calls the rim, score, world-motion, and rebound helpers including `$B377`, `$B473`, `$9CA0`, `$9CF6`, and `$9B84`.

The no-input FCEUX trace gives the following deterministic checkpoints. “Live” is counted from original frame 2557, the first regular-play frame.

| Original frame | Live | Observed state |
| ---: | ---: | --- |
| 2721 | 164 | CPU carrier enters player state `$26`. |
| 2723 | 166 | Carrier enters `$27`; ball enters shot-gather state `$04`. |
| 2749 | 192 | Ball enters airborne state `$05`. |
| 2770 | 213 | Ball enters score/rim state `$06`. |
| 2783 | 226 | Ball enters rebound state `$07`. |
| 2944 | 387 | Ball returns to held/dribble `$01` with original slot 2. |
| 3004 | 447 | Ball becomes released/awarded `$00`. |
| 3324 | 767 | `$9583->$9645` starts the dead-ball formation: ball `$0B`, players `$36`, inbounder `$41`. |
| 3501 | 944 | Original slot 7 holds the inbound in player state `$30`, ball `$01`. |
| 3545 | 988 | Inbounder advances to state `$31`, ball `$00`. |
| 3553 | 996 | Inbound pass begins in ball state `$02`. |
| 3572 | 1015 | Original slot 8 receives in state `$25`; ball resumes `$01`. |

This first CPU decision was initially bounded to the opening trace: the shooting
region chose `$04`, while other positions selected the most advanced teammate.
That historical approximation has now been replaced by the complete
`$D759-$DA39` policy documented in **Complete CPU pass/shot decision policy**.

### Camera CHR stream and moving-goal fix

The moving goal was not a geometry or scroll error. This mapper-2 game uses CHR RAM, and the pre-tip PPU snapshot contained the wrong pattern bytes once the camera approached a basket. FCEUX recorded the original camera stream beginning during original frames 2600-2620: fixed-bank `$CBB7/$CBBE` wrote 32 bytes per frame through `$2007` while bank 0 `$B51D-$B59D` queued the source data.

The routine reads camera byte `$0043`. Values below `$78` select the pointer at `$B516` (`$B59E`, left court); values at or above `$88` select the pointer at `$B518` (`$B83E`, right court); `$78-$87` is a deadband that preserves the current side. It streams 21 chunks of 32 bytes, exactly 672 bytes, into PPU `$1B00-$1D9F`. A byte comparison confirmed that bank-0 `$B59E..$B83D` exactly matches that destination in the original frame-2700 PPU dump. DDAP v10 packages the two bounded streams, and the native renderer applies one when the recovered camera thresholds select it. Native transition frame 439 now renders the complete left backboard and rim where the prior build displayed corrupt tiles.

### Reproducible Ghidra provenance

`tools/ghidra/Run-GameplayLoopAnalysis.ps1` imports bank 0 at `$8000` and fixed bank 7 at `$C000`, then runs `ExportGameplayLoopEvidence.java` to force-disassemble the recorded anchors and emit instruction/decompiler reports under ignored `build/ghidra-reports/`. The most useful mappings for this slice are:

| Behavior | CPU address | PRG bank | ROM file offset |
| --- | ---: | ---: | ---: |
| User jumper enters state `$11` | `$A61F` | 0 | `$262F` |
| User contact overrides tip winner | `$A68A` | 0 | `$269A` |
| Player action dispatcher | `$89B2` | 0 | `$09C2` |
| State `$26` starts shot gather | `$8D1F` | 0 | `$0D2F` |
| State `$27` decision/continuation | `$8D57` | 0 | `$0D67` |
| Ball action dispatcher | `$AC83` | 0 | `$2C93` |
| Ball `$04` gather | `$AE0C` | 0 | `$2E1C` |
| Ball `$05` flight/rim logic | `$AE25` | 0 | `$2E35` |
| Inbound/out-of-bounds root | `$9583` | 0 | `$1593` |
| Inbound formation initializer | `$9645` | 0 | `$1655` |
| Camera CHR streamer | `$B51D` | 0 | `$352D` |
| Left/right court CHR sources | `$B59E` / `$B83E` | 0 | `$35AE` / `$384E` |
| Shared indirect dispatcher | `$C41C` | fixed 7 | `$1C42C` |
| CPU decision root | `$D759` | fixed 7 | `$1D769` |
| CPU obstacle/search helper | `$D99A` | fixed 7 | `$1D9AA` |

The live Ghidra MCP bridge was not listening during this pass, so the evidence was produced with Ghidra 11.3 headless using the checked-in script rather than by guessing from a static hex dump. FCEUX supplied the runtime branch and frame/PC correlation; Ghidra supplied the dispatcher and call-flow interpretation.

`tests/dd_gameplay_cpu_test.c` loads the generated asset pack and verifies the original target data, alternating 30 Hz team updates, `$001A` phase progression, user B-jump and next-frame winner override, carrier targets `$70/$6C/$85`, shot states `$04-$07`, recovery, dead-ball/inbound states, pass reception, a synthetic CPU pass decision, and distinct left/right court CHR streams. `build.ps1` compiles and executes these checks on every asset-pack build.

### Post-inbound scheduler, HUD split, and live audio correction

The longer no-input capture through original frame 4200 disproved the previous native post-inbound route. At original frame 3572, immediately after pass state `$02` completes, player object slots `$02-$0B` contain actions `$0F,$20,$20,$22,$20,$40,$25,$37,$3C,$3E` and packed targets `$E8,$48,$CC,$B5,$79,$21,$A6,$D7,$A9,$8C`. At frame 3600 the receiver (original slot `$08`, native player 6) has advanced to action `$27` with ball action `$04`. It holds near court coordinate `$006A/$58` during those 28 frames; it does not replay the opening `$70->$6C->$85` carrier route and pass again.

The remaining formation corruption had three concrete native causes. First, the made-basket return skipped the original `$2D` rebound chase plus the other nine `$36->$37` walkers, so the later inbound began from unrelated live-play coordinates. Second, three ordinary-inbound targets were transcribed incorrectly, and the inbounder's 16-bit `$05D7/$05E7=$21/$01` target was truncated to `$0021`; that put it at depth `$18` instead of the baseline depth `$98`. Third, `dd_step_inbound` moved all ten objects every rendered frame behind fixed ages 177/229 and then teleported every object to a captured frame-3572 table on reception.

The repaired path removes those age gates and the reception teleport. Made-basket return now runs `$2D->$2E->$2F->$30->$0D` alongside the first `$36->$37` formation. The ordinary inbound runs the real alternating scheduler through `$36/$41->$37/$30->$31/$40`, lets ball `$AD41->$B138` contact complete the 19-frame pass, and translates `$AD6D` by assigning carrier `$25`, role-three `$3C`, role-four `$3E`, and the `$842F` route target without changing any player coordinates. A normal live-pass receiver remains in `$25` for fourteen 30 Hz evaluations. The opening made-basket receiver is different: the controlled frame-2666-to-2679 trace gives it seven alternating object turns (13 rendered frames) before `$25->$32`, after which fixed `$D759` runs on frame 2681. Native code now preserves both cadences while off-ball states continue normally.

| ASM/Ghidra evidence | Recovered behavior | Native C |
| --- | --- | --- |
| action table `$89C0`; state `$22` -> `$8A98` | zeroes both velocity axes, follows its paired object's facing/animation, and may return to `$20` | `DD_PLAYER_LIVE_PAIRED_DEFENDER` |
| state `$36` -> `$904D` -> fixed `$D978` | approaches packed target; on zero/arrival stores action `$37` | `DD_PLAYER_INBOUND_FORMATION` target mover |
| state `$37` -> `$9094` -> fixed `$D990` | stationary target/render continuation | `DD_PLAYER_LIVE_SET` |
| state `$25` -> `$8B5A`; later `$26/$27` -> `$8D1F/$8D57` | carrier-specific route/decision and shot-gather transition | separate opening and post-inbound route steps |

ROM provenance for these selected-bank-0 addresses is `$89C0->$09D0`, `$8A98->$0AA8`, `$904D->$105D`, and `$9094->$10A4`. `ExportGameplayLoopEvidence.java` forces these anchors and the generated `gameplay-bank-00.txt` report shows `$904D` calling `$D978`, storing `$37` on arrival, and `$9094` jumping to `$D990`.

`1ST PERIOD START` is not erased from the nametable at tip completion. FCEUX PPU snapshots before/after the boundary remain effectively unchanged. Instead, fixed-bank `$D350-$D3D5` resets `$2005` at `$D368`, waits for the sprite-zero raster condition, then writes camera `$0043` and vertical scroll zero at `$D3C1-$D3C9`. The native renderer changes the fixed-HUD boundary from 64 to 48 at the award frame and exposes the blank raster band in place of the third HUD row. Fixed-bank ROM offsets include `$D350->$1D360`, `$D368->$1D378`, and `$D3C4->$1D3D4`.

The extended APU trace also establishes that ordinary gameplay has no continuously running background song in this branch. At original frame 2565 fixed `$CD24` installs bank-1 stream pointers `$8653/$8664/$866B`; frames 2566-2579 write a pulse/triangle/noise gesture, and a new cycle begins around frame 2584. Square 2 remains silent. During long dead-ball/non-dribble intervals the channels go silent instead of maintaining a score. The fixed driver loads stream pointer bytes into `$88/$8D`, initializes channel state, and clears the corresponding `$4000` register before the bank-1 sequencer services it.

DDAP v11 introduced `gameplay.audio`: twenty normalized events over eighteen frames, with provenance bank 1 `$8653` (ROM file offset `$4663`, bounded source span `$40`). Win32 loops the synthesized gesture only while gameplay is LIVE and ball action is dribble `$01`; pass, shot, rebound, and dead-ball states stop it. `--dump-gameplay-wav` exports the generated pack-only WAV. This is the audio actually observed in FCEUX, rather than an invented gameplay BGM. DDAP v12 retains it and adds the seven route targets extracted from `$AC78`.

The regression executable verifies all ten frame-3572 actions/targets, the `$0121` extended inbounder destination, movement in off-ball `$20/$3C` states, the 28-frame `$25->$27` shot-gather transition, the post-tip HUD split, and the gameplay audio event count. Native and original visual evidence is kept in ignored `captures/native-gameplay/` and `captures/original-post-inbound/` respectively. The rebuilt original/native inbound checkpoints differ by 4.9613% at formation start (3324/1123), 4.7886% at hold (3501/1300), 5.2211% at release setup (3545/1344), 5.3432% at pass launch (3553/1352), and 5.7338% at reception (3572/1371).

### Complete dispatcher inventory slice

The headless exporter now reads the dispatch words themselves rather than relying on a hand-maintained subset. Bank 0 `$89C0` contains 34 player targets for states `$20-$41`; `$AC91` contains 13 ball targets for `$00-$0C`. Native enums and switches now contain an explicit case for every one of those entries. This closes the prior **missing-handler inventory**, but it does not make every state verified: handlers without an observed FCEUX runtime branch remain partial until their important helper branches can be compared dynamically.

The previously missing player entries resolve as follows in the regenerated `build/ghidra-reports/gameplay-bank-00.txt`:

| State(s) | Ghidra target | Recovered control flow expressed in native C |
| --- | ---: | --- |
| `$21` | `$8A3A` | `$D978` target movement; arrival returns to `$20` |
| `$23->$24` | `$8AF4/$8B12` | initialize the signed height script, test loose-ball contact through `$A6C3`, then choose carrier or recovery on landing |
| `$2C,$33,$34` | `$8BC5` | shared `$D98A` movement/animation continuation |
| `$2D->$2E->$2F` | `$8E71/$8E88/$8EBF` | reach rebound point, exclude airborne/score ball `$05/$06`, claim possession, install return target `$BD/$A1`, then enter hold `$30` |
| `$38->$39` | `$8195/$81A2` | select a spacing target, move, and branch into `$32/$3C/$3E/$38` by role and court region |
| `$3A` | `$8266` | companion regional route feeding `$32/$3C/$38` |
| `$3B` | `$8297` | literal `RTS`; explicit stable no-op |
| `$3F` | `$8460` | `$B503` plus the common render/animation continuation |

Selected-bank ROM offsets are `$8A3A->$0A4A`, `$8AF4->$0B04`, `$8B12->$0B22`, `$8BC5->$0BD5`, `$8E71->$0E81`, `$8E88->$0E98`, `$8EBF->$0ECF`, `$8195->$01A5`, `$81A2->$01B2`, `$8266->$0276`, `$8297->$02A7`, and `$8460->$0470`.

The `$23/$24` jump is now instruction-derived rather than ballistic. In selected
bank 0, `$8AF4` reads the little-endian pointer at `$9B26/$9B27` (`$34,$9B`),
calls `$B503` to clear all three fixed-point motion vectors, clears direction
byte `$04F0+slot`, and advances the action. `$8B12` calls `$A6C3` for contact,
then `$9ABD`. The latter treats `$80` as landing, `$81` as reverse-direction,
and every other byte as a signed delta applied only to integer height
`$0410+slot`; fractional height `$0420+slot` is untouched. The importer already
stores `$9B29-$9B48` in `height_scripts`, so `$9B34` is stable asset index 11
and its backward `$80` sentinel is index 10. CPU-to-ROM mappings are
`$9B26->$1B36`, `$9B34->$1B44`, `$9ABD->$1ACD`, and `$B503->$3513`.

Opt-in FCEUX probe `DD_INJECT_PLAYER_JUMP_CASE=2` records slot 7 at original
frames 2601-2657. Frame 2601 has action `$24`, height `$1055`, pointer `$9B34`,
and all motion zero. The integer height reaches `$26` and direction becomes one
at frame 2629; it returns to `$1055`/pointer `$9B33` at frame 2653; frame 2655
enters `$28` with countdown `$10`, which becomes `$0F` at frame 2657. The
fractional `$55` never changes. The native test executes the same 27 scheduled
script updates and separately exercises the contact/possession branch.

Tip CPU states `$2A/$2B` use that same interpreter. `$8DD2` compares shared
object phase `$004A` with `$20`; on/after the threshold it clears direction,
loads `$9B34`, and writes `$2B`. `$8DF7` calls `$9ABD`, checks contact only when
the next script byte is zero, and on landing selects winner state `$25` or
resets the losing five-player side to `$20`. Natural frames 2471-2503 record
`$004A=$00,$02,...,$20` followed by `$2A->$2B`; frames 2505-2555 traverse
height `$15->$26->$10`, and frame 2557 records owner/carrier `$07/$07`, ball
state `$01`, and player `$25`. Native `object_phase` is the portable form of
`$004A`; its isolated checks cover below-threshold stability, winner landing,
and loser reset. Mappings are `$8DD2->$0DE2` and `$8DF7->$0E07` in bank 0.

The observed inbound release now follows `$8EE2->$9018->$8FE0`. Natural slot 7
frames 3501-3543 remain in state `$30` while timer `$20->$0B` and the last
formation object remains `$36`; after that object reaches `$37`, frame 3545
enters `$31` with release timer `$08` and ball state `$00`. `$8FE0` decrements
`$04E0+slot` on the alternating player schedule. At frame 3553 timer `$04`
queues SFX `$0F`, writes ball state `$02`, and clears active carrier `$0048`;
below `$06`, nonzero metasprite `$0300+slot` is reduced by eight. Frame 3563
underflows `$00->$FF` and jumps through `$9014/$8F9A` into player state `$40`.
Controlled `DD_INJECT_PLAYER_INBOUND_CASE=1/2` probes isolate the launch and
underflow paths. Selected-bank-0 mappings are `$8EE2->$0EF2`, `$8FE0->$0FF0`,
and `$9018->$1028`; fixed-bank `$D990` is ROM offset `$1D9A0`.

Native inbound formation objects now change `$36->$37` on packed target arrival,
state `$30` retains the observed `$20` hold countdown/readiness gate, and state
`$31` performs the eight-tick release rather than relying on a fixed pass event.
The native checkpoints remain release setup at frame 1344/live 988, pass launch
at frame 1352/live 996, and reception at frame 1371/live 1015.

The preceding made-basket handoff is now part of the same dispatcher flow.
Original frame 2783 supplies packed targets
`$A4,$CA,$4E,$35,$D6,$8B,$56,$B3,$CB,$47`: object `$02` enters `$2D` while
the other nine enter `$36`, then `$2E/$2F/$30` naturally returns the recovered
ball through the alternate `$0D` branch. At frame 3324 `$9583/$9645` installs
the ordinary targets `$E8,$48,$CC,$B5,$79,$0121,$A6,$D7,$A9,$44`. The high
byte on `$0121` is retained as native depth `$98`; it is not mapper state.
Reception follows `$AD41->$B138->$AD6D` and mutates actions/ownership/targets
only, so there is no native coordinate restoration table.

Controlled state `$30` probes also cover `$8EE2`'s two nonstandard branches.
With mode bit `$40`, frame 2601 writes ball `$00`, selects the same-side role-zero
object, and changes it to action `$0D`. With `$002C=1`, it additionally changes
the opposite-side role-zero object to `$0F`. The standard probe produces
`$0F,$20,$20,$20,$20,$31,$37,$37,$37,$37`, matching the natural inbound
arrangement, while the two alternate probes preserve the other `$37` objects.
The native `inbound_variant` makes these portable choices explicit without
emulating zero-page flags.

The user-entered made-basket inbound is now translated rather than falling
through to the generic CPU mover.  Headless Ghidra at `$A780` proves that the
handler preserves the current object as carrier, calls `$A129` to score the
five same-side receivers against the held direction, and accepts the pass only
when `$0680 & $80` is set.  `$A21F` changes the inbounder to `$05`, stores the
selected receiver through `$B0AB`, and `$A482` restores the live role layout.
The no-input branch compares coarse timer `$06B1` with five; `$93AE` advances
that byte every 64 frames, so `$A795` queues SFX `$2C`, reason `$12`, and jumps
to the common `$9651` turnover after exactly 320 frames.

Natural FCEUX frames 3004-3324 prove both outcomes.  With no input, frame 3004
holds ball `$00`, owner/carrier `$02/$02`, and user action `$0D`; frame 3324 is
exactly 320 frames later and installs ball `$0B`, carrier `$00`, nine `$36`
walkers, and object `$07` in `$41`.  The focused
`original-user-inbound-pass` capture instead presses right+A on frame 3010:
`$A129/$A1C9` select object `$04`, `$A7C6->$A21F->$A230->$A241` queue the pass,
and `$A482` produces `$05,$40,$40,$3C,$3E` on that side with five opposite
`$20` objects.  Frame 3012 changes ball `$00->$02`, carrier `$02->$00`, and
receiver `$04->$0C`; `$AD41->$AD58` completes reception on frame 3051 with
ball `$01` and owner/carrier `$04/$04`.

The timeout now reaches `$9651` through rule state rather than the former
`live_frame == 767` shortcut.  The regenerated Ghidra report includes the
full `$9698-$9760` placement tail: pointer `$9761->$9763` contributes offset
one, the inbound lane component is clamped to `$08..$18`, role zero enters
`$36`, and role one is placed at signed `$62/$BE` before entering `$36`.
Selected-bank ROM mappings are `$9645->$1655`, `$9698->$16A8`,
`$9761->$1771`, `$A129->$2139`, `$A21F->$222F`, `$A482->$2492`, and
`$A780->$2790`.

The common inbound tail is now translated for both possession directions.
Headless Ghidra `$9651-$9760` first calls `$9395`, flips direction with
`EOR #$48`, calls fixed `$D6BD`, and uses `$9097` to find role zero. `$D6BD`
loops object slots `$02-$0B`, indexes the 20-byte role/team/direction table at
`$D745`, assigns state `$36`, and installs its movement cadence. The exporter
now prints the complete 256-byte `$9763` adjustment table. Native C expresses
that byte-exact band transform arithmetically, retains `$05E0`'s ninth packed
bit, derives the opposite role-zero target with signed `$40/$C0`, clamps its
lane to `$08..$18`, and derives receiving role one with `$62/$BE`. `$ABCD` then
refreshes native target axes before `$D978/$D98D` performs the existing packed
arrival walk.

The award direction is also pinned through the final handoff. A CPU last touch
selects user role zero at `$9097`; native inbound variant three must therefore
take the same user `$A780` handoff used by the nonzero setup mode. It previously
fell through the CPU `$9018` release path, changed `controlled_player` to the
opponent role zero, and made an opponent award look like the user's inbound.
The symmetric regression now advances both last-touch directions through
formation, pickup, and ownership, not merely through the first `$41` assignment.

The long natural FCEUX trace supplies the opposite-direction proof. Frame 5673
writes reason `$16` from `$9635`, flips direction `$08->$40`, selects object
`$02` as inbounder `$41`, and produces final targets
`$0124,$A6,$69,$58,$B6,$A8,$D5,$5A,$45,$E8`. The preceding `$95E0-$9635`
handler accepts ball states `$01/$07/$0C`, rejects depth outside `$16-$8B`, and
applies the two sloped longitudinal boundaries. Frames 6610 and 9990 repeat the
same natural reason. The native boundary helper converts those comparisons
directly rather than using a rectangular court approximation.

Ghidra `$A1CC` supplies two more callers of the same setup: coarse timer
`$06B1 >= $18` queues reason `$14`, while a user carrier still in its original
half at tick `$0A` queues `$13`. `$9583` latches a front-court crossing in
`$06B3` and queues `$15` if that owner returns. Controlled FCEUX probes at
frame 2601 dynamically reach `$A202->$9651` (reason `$13`), `$A1DE->$9651`
(reason `$14`), and `$95C9->$9651` (reason `$15`); all ten players are written
through `$D6BD` on the following dispatcher frame. Native tests independently
seed the three thresholds/latch and verify reason, direction, timer reset, and
role-zero inbounder.

Inbound also reaches the role-maintenance helpers that run during formation.
Natural frame 3545 records `$993A->$99D9` swapping object `$07/$08` roles
`0/1 -> 1/0` and their `$0580` pair links before `$9018` restores the final
`$31/$37` actions. `$9A31` swaps the reciprocal linked pair entries. Native
players now carry mutable pair links, reproduce that persistent role/link
result at release, and verify reciprocal pairs after frame-3572 reception.
User-side reception follows `$AD58`; CPU-side reception alone runs `$AD6D`'s
role-3/role-4 route setup.

The bounded inbound inventory is **100%**: all 32 portable branches/helpers are
Verified. Mapper switching and PPU/OAM presentation are excluded. Rebuilt original/native inbound screenshot
differences are **4.8671%**, **4.7189%**, **5.1252%**, **5.2734%**, and
**5.6536%** at setup, hold, release-ready, pass, and reception respectively.

DDAP v13 adds the exact rule-whistle request `$2C` as `whistle.audio`. Fixed
`$C141->$CC99` selects bank-1 streams `$86F7/$8702/$870D`; controlled FCEUX
APU frames 2601-2612 show the two-frame request latency, pulse pairs alternating
`37/52` and `43/40` for eight audible frames, noise period `6` at volume `3`,
and explicit stops. The native gameplay event serial plays that bounded pack
sequence once for reasons `$0F/$12-$16` and resumes state-driven audio afterward.

The two exceptional reasons follow `$9651` only through its reset/direction
prefix. `$965A` compares `$0059` with `$17/$1A` and jumps to `$98A3`, which
writes `$0065=$FF`, ball state `$0B`, carrier `$0048=0`, and `$0056=$FF`
without calling `$D6BD`. `$A347` queues `$1A`; `$A1CC->$A37D` queues `$17`
when a state-`$22` opponent has the same packed target and the opposite facing
from table `$A375 = 04 05 06 07 00 01 02 03`. Controlled original frame 2602
records reason `$17` plus every `$98A3` write, and frame 2822 awards the foul
shot to object `$07`. Native C shares one exceptional-dead-ball helper for both
callers, preserves shooter/offender ownership, and deliberately avoids the
ordinary inbound formation tail.

Following the timeout exposed an older rebound-return approximation. Ghidra
shows `$8E71` and `$8EBF` both using `$D978` packed equality followed by
`$D98D`'s double fixed-point integration. The installation itself begins at
`$8491`: gate `$0056`, role `$0690`, signed phase table `$8503`, and the 20
target/action pairs at `$8507` select one `$2D` rebound chaser and nine `$36`
formation routes. Natural and controlled traces show the low-entropy object
phase changing individual targets by `$FE/$FF/$01/$02`; native preserves the
observed per-slot phases while still batching the NES object cadence. DDAP v20
contains both ROM tables. `$2D` keeps the `$FF01` longitudinal and signed
`$000C` depth terms before `$2E->$2F->$30`. The subsequent `$8EE2` branch uses
`$A0DA` to select an on-screen non-role-zero teammate for automatic state `$31`
inbound after a user make, while mode bit `$40` selects user state `$0D` in the
opposite arrangement.

State `$20`'s missing paired-player path is `$8A16->$9102`. `$9102` feeds
current and paired projected coordinates to the shared `$9B42` two-axis box
test with half extents two. Controlled case 1 records `$20->$22`, latch
`$0480=$10`, and preserved vectors `$0123/$FEDC`; case 2 moves the projection
away and records `$22->$20` with both vectors zero. `$9139` supplies the third
branch: paired action `$03` changes the current object `$20->$23`. Native uses
the same two-unit fixed-point boxes and explicit paired latch. Mappings are
selected-bank `$8A16->$0A26`, `$9102->$1112`, `$9139->$1149`, and
`$9B42->$1B52`.

Dispatcher states `$2C/$33/$34/$35` contain no state-specific decision at all: each
table entry targets `$8BC5`, which jumps to fixed-bank `$D98A`. On the observed
gate-zero path `$D98A->$A84C->$A896->$A85A`, `$A84C` calls `$9CF6` twice for
the 24-bit court-x position and `$9CA0` twice for the 16-bit court-depth
position. Controlled state `$2C` frames 2601-2607 and state `$33/$34/$35` frame
2601 seed x/depth velocities `$0123/$FEDC`; every scheduled update preserves
those vectors and adds `$0246/$FDB8` to position. Both `$33` and `$34` produce
position `$00F358/$39D8` on their injected frame. The `$35` result corrects an
earlier native target-seeking approximation. Native C now integrates the
existing vectors twice rather than synthesizing a new target speed. Mappings
are selected-bank `$8BC5->$0BD5`, `$A84C->$285C`, `$A896->$28A6`, and
fixed-bank `$D98A->$1D99A`.

The four previously missing ball entries are likewise explicit:

| State | Ghidra target | Recovered control flow expressed in native C |
| --- | ---: | --- |
| `$03` | `$ADF2` | hoop/contact check followed by longitudinal, depth, and common ball-physics integration |
| `$08` | `$AF72` | clear ownership, seed height/velocity, queue original SFX `$14`, and advance to `$09` |
| `$09` | `$AFDD` | integrate both court axes and height, then enter rebound `$07` at the recovered threshold |
| `$0C` | `$ACAB` | share state `$0B`'s zero-height projection handler |

Their ROM offsets are `$ADF2->$2E02`, `$AF72->$2F82`, `$AFDD->$2FED`, and `$ACAB->$2CBB`. State `$0A` at `$B017` was also made explicit: the routine seeds its launch fields and writes ball state `$05` before returning.

`tests/dd_gameplay_cpu_test.c` now isolates the added target-arrival, jump/landing, rebound-claim, route, no-op, inbounder, bounce-pass, loose-ball, launch, and hidden-ball paths. These checks prove that the native dispatcher is total and that its bounded transitions are deterministic. Coverage therefore promotes the 14 missing player entries and four missing ball entries from **M** to **P**, not **V**: player coverage rises from 45.6% to 66.2%, ball coverage from 57.7% to 73.1%, and the weighted gameplay-loop headline from 51.8% to 61.8%.

### Next implementation slice (historical)

1. Trace the next possession beyond the first inbound, including the full `$D99A` obstacle/search decisions and pass-lane rejection. **Completed below.**
2. Name and translate steal, block, missed-shot, and non-scripted out-of-bounds branches.
3. Port game-clock/HUD updates and the remaining defensive action states.

### Clock, score, period, collision, and missed-shot core slice

The long no-input FCEUX capture now runs through the first period boundary and writes a compact `gameplay-dispatch.csv` plus exact `$9431` entry records in `gameplay-clock-calls.csv`. The original clock stores packed-BCD seconds in `$0057` and minutes in `$0058`. Bank 0 `$9431` tests `$001A & $1F`, calls `$9490` to subtract one (or seven at a decimal rollover), and uses fixed-bank `$C694` to convert each nibble to HUD tile `$D9 + digit`. The native clock uses the same BCD representation and produces the verified early checkpoints 04:59 at native frame 296 and 04:26 at native frame 1371/original frame 3572.

The complete trace reaches 00:00 at original frame 12412, then resets the second-period formation at 12626-12628 after the active loose-ball sequence finishes. Fixed-bank reset writes at `$CEF3`, `$CF33/$CF37`, `$CF88/$CF8C`, and `$CFD5-$D258` restore the owner, 05:00 clock, ball, and ten formation objects. The HUD period tile changes from `$DA` to `$DB`, and the banner changes from `1ST PERIOD START` to `2ND PERIOD START`, while the 0-16 period score persists. Native state now carries the score, packed clock, period, and delayed period reset and patches those HUD tiles directly. This is still partial: the original calls `$9431` on 9,624 of the 9,942 traced frames, with four large presentation gaps (104, 89, 62, and 55 frames), so a fixed absolute 32-frame cadence will run early late in a period until those presentation gates are translated.

The pass and jump collision paths now use the shared Ghidra primitive rather than broad native distance checks:

| Original helper | Recovered behavior | Native counterpart |
| --- | --- | --- |
| `$9B42` | two-axis signed box overlap; carry clear means contact and the upper edge is exclusive | `dd_axis_boxes_overlap` |
| `$B138` | pass receiver: player half extent 8, ball half extent 6 in both court axes | `dd_pass_receiver_contact` |
| `$A6C3` | jump contact: 4+4 extents in height/longitudinal axes, player shifted six units by team and eight units upward | `dd_jump_ball_contact` |

The frame-12430 miss supplies a dynamic branch for `$B377/$AF72/$AFDD`. `$B377` only classifies the ball at integer heights `$34-$37`; it expands the hoop-contact box from result 1 through 4. Result 1 enters made-basket state `$06`, while results 2-4 enter loose launch `$08`. The observed ball is result 4 at `(x=$4B.A0, depth=$5D.4D, height=$37.D8)`. On the next frame `$AF72` changes `$08->$09`, raises the ball to `$38.D8`, reverses and halves longitudinal velocity `$FF.05->$00.7D`, reverses and halves depth velocity `$00.31->$FF.E7`, and seeds vertical velocity `$01.00`. `$AFDD` subtracts `$10` gravity per frame; the trace peaks near `$40.58` and enters rebound `$07` after 61 frames at height `$00.A8`, with a new vertical term near `$02.90`.

Native shot flight now aims at the original hoop centers `$0048/$01B8`, evaluates the same result-1-through-4 boxes, sends misses through `$08/$09`, and reproduces the reverse/half and `$10`-gravity behavior. `$B473` is also translated as seven diagonal samples: left-side X runs `$45->$3F`, right-side X runs `$01BB->$01C1`, and the combined depth/height sample runs `$9E->$92`. A controlled, opt-in FCEUX probe at original frame 2601 placed ball state `$03` on the first left sample; the original wrote `$0490=1` at `$B4D4`, cleared owner `$005B` at `$B4D8`, retained camera-follow `$0048`, and negated injected velocity `+$0100 -> -$0100` at `$B4FB`. The equivalent native test proves the same latch, ownership, camera-follow, and reflection behavior, while the unmodified made-shot trace proves that `$B473` is called every flight frame without a false latch.

The final `$B473` hit branch is now instruction-matched as well. `$B4DB` reads
special-finish state `$003B`: when it is nonzero, `$B4E1-$B4E5` writes the
rim-contact latch `$003F=1`; otherwise `$B4E7` requests audio event `$16`.
Both branches retain camera carrier `$0048`, clear ball owner `$005B`, and
reflect longitudinal velocity `$0390/$03A0`. Native regressions exercise both
outcomes and require that the special-finish path does not emit the ordinary
rim sound.

Deterministic checks now cover a clean opening make, a synthetic result-four miss, exact miss velocity reflection, the 61-frame loose arc, pass collision, collision-boundary exclusion, jump-ball contact, both `$B473` rim-contact outcomes, and state `$03`'s zero-vertical-term branch to hidden state `$0C`. The reusable `$9B42`, `$B435`, and `$B473` collision helpers are therefore **Verified**. CPU shot-block and higher-level possession/contact arbitration are covered below; only the remaining contested-rebound branches in this collision area stay **Partial**.

### Defensive contact and shooter recovery slice

Headless Ghidra ties player state `$28` to bank-0 `$8D9C`: it decrements
`$04F0+slot`, branches while the signed result is nonnegative, and writes state
`$29` only after the counter underflows. The unmodified FCEUX window at original
frames 6486-6552 confirms that `$04FA` begins at `$20`, reaches `$00` at frame
6550, and becomes `$FF` with the `$28->$29` transition at frame 6552. Because
that team is dispatched on alternating frames, the native handler performs 33
scheduled updates, or 66 rendered frames. State `$29` at `$8DAB` copies ball
target bytes `$05B0/$05C0` to the current player's `$05D0/$05E0` only when the
rotating priority object `$004D` matches, then calls `$91FB`. The trace records
the resulting `$B435` probes for slot `$0A` from frame 6554 onward while the
player follows the loose ball.

The defensive path begins at `$91A6` and `$9FA3`. Both honor `$001D`'s `$20`
post-possession contact lock, use `$B435`, increment
the per-object contact counter `$06A0+slot` against limit `$0068`, clear the
counter when contact or eligibility fails, and call `$A347` when sustained
contact qualifies. `$91A6` accepts only the defender paired with controlled
owner `$0046` while direction `$40` is active; `$9FA3` accepts a non-role-zero
teammate paired with ball owner `$005B` while direction `$08` is active.
`$B435` uses exclusive player/ball half extents 4+6 on court
X and depth, then accepts height when unsigned
`player_height + $11 - ball_height < $22`. A controlled, opt-in FCEUX probe at
frames 2602/2606/2608 holds opposing slots `$03/$07` in stable state `$3B` at
the same coordinates with limit 3. The original calls `$A347` at frame 2608;
because the entropy-seeded countdown `$0025` is nonzero, it returns and `$9FA3` jumps to
`$A44B`. That routine changes owner/carrier `$07->$03`, installs carrier state `$02`, assigns the
winning team `$40,$40,$3C,$3E`, and resets the other team to `$20`. The native
translation swaps a nonzero winning role with role zero through the
`$99D9/$9A31` relationship update, applies those actions by role rather than
object order, resets all ten heights to `$10`, and preserves ownership, court
direction, control, and contact-counter side effects without emulating
instructions.

The contact/tracking LEVEL dependency is now translated rather than merely
stored. Bank-1
`$A593-$A5B7` keeps the selected setting, `$A631` shifts it left twice before
the gameplay handoff, and the configuration installs
`$0068={14,0C,06}` plus `$006C={40,28,1A}`. The native state keeps the immutable
menu selection separately from mutable gameplay LEVEL values 0/4/8. Final-match
`$9408-$9418` advances that mutable byte and loads the exact twelve-entry
progression tables at `$9419/$9425`:

| Mutable LEVEL | `$0068` contact | `$006C` tracking |
| ---: | ---: | ---: |
| 0, 4, 8 (menu 0, 1, 2) | `$14`, `$0C`, `$06` | `$40`, `$28`, `$1A` |
| 1-11 progression | `$10,$0E,$0C,$0C,$0A,$08,$07,$07,$06,$05,$03` | `$31,$2D,$27,$25,$23,$1F,$1B,$17,$13,$10,$0D` |

`$91A6` and mirrored `$9FA3` return without touching `$06A0` while `$001D` or
score-return gate `$0056` is nonzero; direction, pair, action, or `$B435`
failures take the clear tail. Below LEVEL 6 the defender must match the paired
owner. LEVEL 6/7 waive that requirement only while signed phase `$001A` is
negative, and LEVEL 8+ waive it unconditionally. LEVEL 6+ also resolves ball
state `$02` immediately after collision rather than incrementing `$06A0`.
Counters retain byte-wrap `INC` behavior. `$91FB` remains a separate immediate
collision path with none of those gates. Successful user `$A44B` and CPU
`$9208` transfers clear `$0056` at their distinct `$A460/$927C` stores; shared
`$9395` does not own that side effect.
The made-basket route deliberately retains `$AE25`'s gate through rebound
return: natural original writes show `$0056=02` at frame 2770 and the later
`$8F50` clear at frame 3004, corresponding to native scene frames 569 and 803.

The adjacent `$006C` consumer is player state `$22` at `$8A57-$8A97`. It finds
the offensive role-zero object, requires the current pair to reference it,
caches low packed coordinate `$05B0` in `$05F0`, and advances byte age `$0600`.
A changed packed cell resets age through `$FF->$00`; a stable cell reaching
`$006C` copies both packed bytes to `$05D0/$05E0`, calls the route installer,
and enters `$21`. Native fields `tracked_zone` and `tracking_age` model those
bytes independently of the unrelated `$0480` paired latch.

Fresh controlled original traces under ignored capture directories prove the
contact counts. `original-level-0-contact`, `original-level-4-contact`, and
`original-level-8-contact` reach `$A009` only after counters `$13`, `$0B`, and
`$05`, respectively, with `$07E8` equal to 0/4/8 and `$0068/$006C` equal to
`14/40`, `0C/28`, and `06/1A`.
`original-level-6-pair-bypass` seeds negative `$001A`, a wrong pair, and ball
state `$02`; the original reaches `$A009` immediately with `$06A0=00`. The Lua
probe logs PCs `$91A6/$91F3/$9200/$9FA3/$9FFC/$A009/$8A57/$8A7D/$8A90` and all
relevant RAM gates to `gameplay-level-contact.csv`. Native regressions cover
both mirrors, preserve/clear exits, the 5/6/7/8 matrix, immediate state `$02`,
byte wrap, `$91FB` independence, all three `$006C` thresholds, changed-cell
reset, wrong-pair rejection, ninth packed bit, and GAME SET progression.
The non-injected `original-level-1-shipping-live` capture closes the menu
handoff itself: `$A593->$A5B7` changes `$07E8 00->01` and installs `0C/28` at
frame 2248; END reaches `$A631` with 01 and `$A637` with 04 at frame 2544.
The first live `$91A6` calls at frame 2815 and `$9FA3` calls at frame 2816 still
read `04/0C/28`. The evidence CSV includes the active bank and uses separate
bank-0 and bank-1 hook allowlists so same-address routines cannot be conflated.
Rev-1 file provenance (including the 16-byte iNES header) is bank 1
`$A593->$65A3`, `$A631->$6641`, `$A637->$6647`, `$A5B8->$65C8`,
`$A5BB->$65CB`; bank 0
`$8A57->$0A67`, `$91A6->$11B6`, `$927C->$128C`, `$9FA3->$1FB3`,
`$A460->$2470`, `$9408->$1418`,
`$9419->$1429`, and `$9425->$1435`.

This bounded claim does not cover every gameplay consumer of `$07E8`.
Bank-0 `$883A-$884F` still controls CPU free-throw release with LEVEL `<9`
versus `>=9`, signed `$001A`, and aim `$033C==$60`; that native policy remains
Partial and is the next LEVEL audit root.

`$91FB` is the immediate-contact companion used by shooter recovery. Its
ordinary path requests `$10` and falls into `$9208`, as does a successful CPU
block landing at `$8B44`. This is not `$A44B`: `$9208` swaps the winner into
role zero, installs carrier `$25`, gives offensive roles 1/2 state `$40`, gives
roles 3/4 route initialization `$38`, and selects the carrier lane `$05` or
`$0C` from the low five bits of its packed court position. Natural FCEUX block
evidence records the next-frame vector
`$0F,$20,$20,$20,$20/$40,$40,$25,$39,$39`; `$38` has advanced to `$39` through
its normal dispatcher by then. Native tests preserve the earlier exact `$38`
installation and both lane branches.

Repeated natural transitions additionally verify the rebound chain: `$2D->$2E`
at frames 2942, 3693, 4675, 7606, 8337, 9249, and 10870; `$2E->$2F` two frames
later after `$8E88` excludes ball states `$05/$06` and assigns owner/carrier;
and `$2F->$30` after the direction-selected `$BD/$A1` return target is reached.
The isolated native regression checks exercise the arrival, exclusion, claim,
target, and hold branches. Selected-bank ROM offsets are `$8D9C->$0DAC`,
`$8DAB->$0DBB`, `$91A6->$11B6`, `$9FA3->$1FB3`, `$A347->$2357`,
`$A44B->$245B`, and
`$B435->$3445`.

These additions promote player states `$28,$29,$2D,$2E,$2F` and the
`$91A6/$91FB/$9208/$A347/$A44B` possession family to Verified. The CPU
shot-block branch is translated below, and the later user-contest section
closes `$A3E2->$A607->$A638`; remaining collision work is confined to the
separately inventoried contested-rebound paths.

### Foul/free-throw rule and complete basket-result slice

The `$A347` call is not itself the ordinary steal handoff. Ghidra shows its
exceptional branch requires clock countdown `$0025==0`, ball state `$01`, and
equal owner/current-player facing. It stores the fouled owner in `$006A`, writes
animation `$29`, requests SFX `$30`, selects match mode `$1A`, removes its return
address, and jumps directly to `$9645`. Otherwise it returns to `$9FA3`, which
plays SFX `$10` and jumps to the separately exported `$A44B` possession reset.

The opt-in zero-tick FCEUX probe proves the exceptional path. At frame 2608 it
executes `$A347` but never `$A44B`, writes `$006A=$07` and `$0059=$1A`, changes
the ball to dead state `$0B`, and clears carrier `$0048`. Frames 2609-2610 put
the shooter in `$42->$4A` and the other players in formation `$43`; formation
objects independently reach `$44`. The shooter receives ball `$01` at frame
2802, advances through ball `$00`/player `$45` at 2822, player `$46` at 2860,
ball `$04`/player `$47` at 2956, ball `$05` at 2982, and player `$48` at 3010.
The old host-frame checkpoints have now been removed. Headless Ghidra exports
the complete instruction range `$852F-$89AF` plus the 40-byte `$85C7`
target/action table and 20-byte `$86AF` facing table. DDAP v20 stores those
bounded tables. Native `dd_step_free_throw` swaps the shooter into role zero,
walks all ten objects on the alternating 30 Hz dispatcher cadence, preserves
the intermediate `$42/$4A->$43->$44/$45` states, and waits until all nine lane
objects are ready before entering `$46`.

The `$87C0` translation oscillates the aim value by two through `$50-$60`.
User slots take controller bit `$40` (native B) exactly as the original does;
CPU slots consume the `$0067=$30` timer and entropy/aim gates. `$884F/$8887`
then use the original `$9B26` height stream, release at its `$81` apex, install
shot kind two as native one-point `shot_value`, and enter `$48`. `$88DE/$894C`
count the result, wait `$50` scheduled ticks, reattach at `$20`, and run the
second attempt without reforming the lane. The five-coarse-tick exit publishes
reason `$12` and the exact `$2C` rule whistle. Deterministic regressions cover
formation, CPU timing, indefinite user wait, B launch, one-point score, and
the pack bytes; `Capture-NativeFreeThrow.ps1` renders ready/gather/airborne
checkpoints. The four recursively inventoried entries `$8594/$85BE/$860A/$8682`
are now Verified; remaining foul eligibility and final rebound variants remain
part of the broader match-rules audit.

The rim result is not an analog trajectory approximation. `$B377` first checks
the ordinary `$34-$37` rim-height window, then detects shooter state `$42+`.
Aim byte `$033C==$60` leaves result one; every other value increments it to
result two and bypasses the expanding hoop boxes. Controlled original launch
at aim `$56` reaches ball `$09` (miss), while launch at `$60` reaches ball
`$06` (make). Native `dd_basket_contact_result` now follows that exact branch:
`$B189` retains its normal hoop vector, and the aim byte alone selects the
free-throw make/miss result. Regression checks cover both values.

Controlled `$AE25->$B377` probes now isolate all four basket results: result
`$01` enters score state `$06`, while `$02/$03/$04` enter loose initializer `$08`
and then airborne `$09`. A separate `$04F0=$FF` probe shows `$AE25` wrapping the
counter to zero, `$B377` rearming it to one, and returning without contact. The
native classifier now has the same wrap guard; tests cover all `$AF72` vector
branches and `$AFDD`'s 61-frame arc. This promotes missed-shot outcomes to
Verified without promoting the still-partial shot-block collision family.

### Made-basket counter and score/HUD timing

Headless Ghidra at `$AEDE` shows that score state `$06` is driven by byte
counter `$004A`, initialized to `$0C` by `$AE25`. Every dispatch decrements it.
The `$09->$08` dispatch selects two points, changes it to three for shot kind
`$01` or one for free-throw kind `$02`, calls fixed-bank `$C477/$C6AD`, clears
shot-kind `$005F`, and queues the score display through `$98B5`. Only counter
values `$05-$00` lower the ball's integer height; `$00->$FF` clears the display
mode and enters rebound state `$07`.

The dedicated FCEUX `gameplay-score-calls.csv` trace proves the timing. Original
frame 2770 installs state `$06/$0C`; frames 2771-2773 reach `$0B/$0A/$09` with
both score copies `$07F0/$07F8` still zero. Frame 2774 writes both copies to
`$02` as the counter reaches `$08`; frames 2777-2782 lower height `$32->$2C`;
frame 2783 underflows and installs `$07`. The queued HUD transfer later writes
the right score digits, while the native renderer reads the same score state
directly because NES PPU buffering is excluded. Native regression checks now
assert no points on state entry, two or three points on the fourth dispatch,
and one point on the corresponding free-throw dispatch. Made-shot sequence and
score/HUD updates are therefore Verified.

### CPU packed avoidance and terminal ball states

The fixed-bank `$D99A-$DA39` helper is a short lookahead rather than a general
pathfinder. Bank-0 `$8C36` adds signed packed-coordinate deltas
`+1,-33,-32,-31,-1,+31,+32,+33`; `$D99A` projects one step in the player's
current facing and reacts only if that byte equals the ball or linked opponent.
It then reads four direction candidates from `$8BC8`, projects each two steps,
and uses `$8CF3` plus the seven court-band bounds at `$8D0F` to install the first
valid target. Original frame 2559 supplies the successful dynamic branch:
object `$07`, facing `$04`, position `$B0`, linked object `$02` at `$AF`, global
phase `$DE`, and direction `$08` change target `$D4->$70` at return `$DA36`.
The same trace records 83 no-change returns at `$DA38`. Native isolated checks
reproduce both outcomes. This helper now runs in carrier `$25` and CPU setup
`$32`; the surrounding region/timer policy in `$D759-$D8B0` and receiver gate
through `$D94E` are now translated and Verified in the later CPU-policy slice.

Ball launch state `$0A` is a one-frame initializer at `$B017`, not a wait state.
The unmodified trace changes `$0A->$05` at frames 2470-2471 and shows vertical
term `$0305` plus curve byte `$0C`. The routine does not write owner or camera
carrier, and the native version now preserves both. Dispatcher states `$0B` and
`$0C` share `$ACAB`, which clears only integer height `$0410` before projection.
The long natural capture contains 1,542 `$0B` frames. An opt-in state `$0C`
probe converts injected height `$467B->$007B` at frame 2601 while retaining
state `$0C`; Ghidra proves the shared target and absence of velocity writes.
Native tests cover `$0A`, `$0B`, and `$0C`, promoting all three to Verified.
Selected-bank offsets are `$8C36->$0C46`, `$8CF3->$0D03`, `$B017->$3027`, and
`$ACAB->$2CBB`; fixed-bank `$D99A` is offset `$199A` in the fixed 16 KiB bank.

Awarded ball `$00` at `$ACB6` has two portable height branches after attachment:
tip modes `$01/$03` add `$18` to the owner's integer height, while ordinary mode
`$00` adds `$08`. Natural frames 2532 and 3004 prove `$26+$18=$3E` and
`$10+$08=$18` respectively. The native ball carries this mode explicitly so a
later inbound cannot accidentally retain the tip offset. Loose flight `$09` at
`$AFDD` checks the unsigned integer height after integration, not elapsed time;
`>= $E0` changes to rebound `$07`, writes vertical term `$02E0`, and clears the
rim latch. The observed result-four arc reaches that condition on its 61st
frame, and the native regression now asserts both the threshold and writes.
States `$00` and `$09` therefore move to Verified.

Bounce-pass state `$03` at `$ADF2` calls the rim sweep and then `$B167`, whose
byte operations update the integer height using velocity high minus gravity
phase. A negative trial decrements the high velocity byte and resets phase only
when that decremented velocity remains nonnegative. Controlled original frames
2601-2603 prove height `$0B00`, velocity `$0100->$0000`, phase `$3D->$00`, then
the next dispatch `$03->$0C`; native checks reproduce both dispatches.

Loose launch `$08` at `$AF72` clears owner `$005B` but not camera carrier `$0048`,
sets integer height `$38`, vertical term `$0100`, and advances to `$09`. Three
controlled FCEUX probes with angle `$20` distinguish its outcome branches:
`$02` produces vector `$005A/$005A`, `$03` leaves `$0000/$0000`, and `$04`
produces `$FFA5/$FFA5`. The routine also retains the rim latch until `$AFDD`.
The native handler reproduces those branches and ownership/latch side effects,
promoting `$03/$08` and completing all 13 ball dispatcher states. Broader block,
foul, and general rim-contact eligibility remains incomplete at the rule level.

Player route initializer `$38` at `$8195` stores `$39`, calls `$8468`, and then
uses the common movement tail. `$8468` calls `$AC2A`; nonzero regions index the
seven bytes at `$AC78` through `$AC58`, while region zero substitutes
`($001A + 1) & 3`. Natural frames 9110-9112 show slots `$07/$0A` in region one
selecting packed target `$EC` and then changing `$38->$39`. DDAP v12 extracts
the complete `$AC78` table (`96 EC 8C 2C E6 85 25`) so the native route contains
no embedded asset bytes.

Two controlled frame-2601 probes seed motion vectors `$0123/$FEDC/$0345`.
State `$3B` reaches the literal `$8297 RTS` and preserves all three through
frame 2604. State `$3F` reaches `$8460->$B503` and clears all three on frame
2601. The native dispatcher now makes the same distinction and tests it;
`$38/$3B/$3F` move to Verified.

State `$39` at `$81A2` first handles role zero and packed target arrival. Its
moving branch uses `$9097(A=0,Y=8)` to find the possession-side role-zero
object, compares `$AC2A` regions, then tries signed `$8262` offsets `-65/+95`
through `$8CF3`; both failures fall back to `$AC78[2]`. A controlled probe sets
current/reference packed positions `$EC/$EB` in the same region and target
`$8C`. On frame 2601 the original rejects overflowing `+$5F`, accepts `-$41`,
and records `$39->$3A` with target `$AB`. The native isolated test produces the
same result. Natural frames 9114 and 9126 exercise the separate arrival branch:
`$842F` uses bit two of `$001A` to select `$8C` and `$E6` before state `$3E`.
State `$3A` at `$8266` shares the role-zero and packed-arrival decisions without
the search. Both handlers now include their significant Ghidra branches and
move to Verified.

State `$21` at `$8A3A` compares packed position/target through `$D978`; equality
writes `$20` and clears per-object `$0600`. A controlled `$EC==$EC` trace seeds
`$0607=$55` and observes `$21->$20`, `$55->$00` on frame 2601. Inbounder `$41`
at `$8C6B` uses the same packed equality, installs owner/carrier, copies its
packed and court coordinates to the ball, and enters `$30`. Natural frames
3498-3501 show slot `$07` approach `$0122->$0121`, followed by player/target/ball
all `$0121`, owner/carrier `$07`, and `$41->$30`. Native tests cover both flows,
promoting `$21/$41` to Verified.

The final-match rule is rooted at bank 0 `$93AE`. After its ordinary per-frame
bookkeeping, `$93D1` rejects player-slot-two actions `$41` and above, and
`$93D8-$93EA` requires a zero `$0057/$0058` clock plus ball action `$07` or
`$01`. The accepted path calls `$CBE0`, requests sound `$28` through `$C141`,
clears `$0025`, writes mode `$09` to `$002A`, and clears match state `$0059` at
`$93FE`. `$9408-$9416` then advances `$07E8` and selects the next `$0068/$006C`
presentation pair from the tables at `$9419/$9425`.

The long FCEUX run proves that control flow rather than inferring it from the
screen alone. Original frame 45337 renders `PERIOD 4` at 00:00 with ball state
`$07`; frame 45338 records `$0059=00` at PC `$9400`, `$07E8` advancing to `$04`,
and `$0068=0C/$006C=25` at PCs `$9413/$9418`, while the screen says `GAME SET`.
Frames 45596-45597 are the solid NES blue transition and frame 45620 is back at
the title. Native C preserves the final 00:00 frame, applies the same `$01/$07`
and player-action eligibility gates, holds `GAME SET` for 258 frames, switches
to blue, and signals the Win32 shell to return to its native title at age 282.
The native frame-39634 capture retains the live court and match score, and the
stable earlier gameplay comparison remains 2,560/57,344 differing pixels
(4.4643%, below the 5% regression limit). The tile/sprite mechanism that draws
the original message is NES presentation and remains outside portable coverage;
the match termination and timing rules themselves are native state transitions.

### User pass ownership and defensive switching

Bank-0 `$A129` (bank offset `$2129`, ROM file offset `$2139`) is the user-pass
receiver selector. It scans the five original team objects `$02-$06`, rejects
the role-zero current object through `$0690` and offscreen objects through
`$0460`, and scores each candidate against
the held direction byte `$0670`: left/right compare projected X `$0320`, while
up/down compare projected Y `$0330`. Every matching half-plane adds one point;
the greatest nonzero score wins and an equal score selects the later object.
The native `dd_user_pass_receiver` translates that scoring directly, mapping
original slots `$02-$06` to native players `0-4`. A without a direction therefore
does not start a pass, matching the original.

The catch path is bank-0 `$AD41` (bank offset `$2D41`, ROM offset `$2D51`). Once
`$B138` accepts receiver contact, `$AD58-$AD67` copies receiver `$0052` to both
owner `$005B` and camera/control `$0048`, then writes action `$02` to that one
receiver. The CPU-side branch at `$AD6D-$ADBA` instead installs action `$25`
without changing the user's defending-team control. Native reception now makes
the same distinction, changes `controlled_player` only for the user's receiver,
and makes the alternating team scheduler skip that new dynamic slot. The former
passer remains in pass recovery and later returns to off-ball action `$40`.

Focused FCEUX traces make the handoff observable. With the opening tip won on
frame 2502, direction+A on frame 2600 selected original receiver `$04` for left,
`$06` for right/up, and `$05` for down. In the left trace, ball state `$02` and
receiver action `$0C` appear on frame 2602; `$AD58` runs on frame 2608 and owner,
camera/control, and action become `$04/$04/$02`. The former `$02` passer reaches
action `$40` on frame 2634. The added native regression starts the same
directional pass, forces the `$B138` contact boundary, proves that only the
receiver becomes controlled, and then moves that receiver with user input. A
separate CPU-reception check proves that user control does not cross teams.

Defensive switching comes from bank-0 `$A29D` (bank offset `$229D`, ROM offset
`$22AD`). It runs only for the role-zero action `$0F` player. `$A2A4-$A30E`
retains two candidates using projected X distance from the ball, adds projected
Y distance to each, and selects the smaller wrapped total. Eligibility is the
`$AA20` screen gate (bank offset `$2A20`, ROM offset `$2A30`): after subtracting
12, the projected X must remain on page zero and below `$E8`. Pressed input bit
`$40` (NES B, native Z) then runs the transfer helpers, writes `$20` to the old
player, `$0F` to the selected player, and updates `$0046` before clearing `$0061`.
On original frame 2684, the focused trace records `$0680=$40`, old slot `$02`
changing `$0F->$20`, and selected slot `$04` changing `$20->$0F`. The native
selector preserves the same one's-complement distance, two-candidate shortlist,
byte-wrapped totals, eligibility bounds, and action/control writes; its regression
proves B selects native player 2 in the corresponding controlled layout.

### Paired CPU shot contest and block ownership

The user-shot initializer is bank-0 `$AA75` (bank offset `$2A75`, ROM file
offset `$2A85`). It installs height pointer `$9B34`, clears the direction byte,
writes user action `$03`, and writes ball action `$04`. The user dispatcher
entry `$03->$A504` (bank offset `$2504`, ROM offset `$2514`) runs movement and
the shared `$9ABD` height interpreter. When the shot is released it calls
`$B189/$A7EA`; the natural trace records player `$02=$03` and ball `$04` on
frame 2600, then ball `$05` with owner still `$02` and camera object `$00` on
frame 2602. Native user shooting now exposes that distinct action instead of
reusing CPU carrier-decision state `$27`, and retains the shooter as owner
during flight just as the original does.

Paired CPU defense enters through bank-0 `$8A16/$8A98` (ROM offsets
`$0A26/$0AA8`) and shared helper `$9139` (ROM offset `$1149`). The already
latched original defender `$07` reads its paired player’s action `$03` on frame
2601 and changes `$22->$23`. `$8AF4` (ROM offset `$0B04`) clears motion,
installs `$9B34`, and advances `$23->$24`; the natural unmodified attempt then
calls `$8B12->$A6C3` every other frame. `$A6C3` (bank offset `$26C3`, ROM offset
`$26D3`) compares 4+4 integer units in height and longitudinal X after shifting
original user slots `$02-$06` by +6 and CPU slots `$07-$0B` by -6.

The natural frame-2606 attempt proves the no-contact branch: shifted defender X
is `$00` but ball height `$2A` is above defender-height-plus-eight `$18`. The
opt-in `-BlockFrame 2606` probe changes only the ball X/height to enter those
same boxes. On that frame `$8B27` (ROM offset `$0B37`) changes owner `$02->$07`,
`$8B2B` changes ball `$05->$00`, and the original issues sound request `$10`
through fixed-bank audio entry `$C141` (fixed bank 7, ROM offset `$1C151`);
camera/current carrier remains `$00`. The state/ownership path is native in this
slice, but dynamic SFX `$10` still needs its own pack-backed Win32 playback path.
When the defender lands, `$8B44->$9208` (ROM offsets
`$0B54/$1218`) performs the delayed reset. Original frame 2644 records ball
`$00->$01`, camera `$00->$07`, defender `$24->$25`, the user player `$03->$0F`,
and the remaining team actions restored for live possession.

`dd_jump_ball_contact` and the `$9ABD` interpreter already express the geometry
and jump data natively. The completed branch now permits contact with an owned
airborne shot, changes only ball ownership/action at contact, and calls the
native possession/team reset after the blocker lands. Deterministic regression
checks cover `$22->$23`, the owned-shot contact, the pre-landing camera value,
and the final CPU possession. Reproduce the ignored visual/trace evidence with:

```powershell
.\tools\fceux\Capture-TipoffGameplay.ps1 -TraceStart 2580 -FinalFrame 2660 -CaptureName original-user-shot-block -JumpStart 2502 -JumpEnd 2502 -JumpButton B -PassFrame 2600 -PassEnd 2600 -PassButton B -BlockFrame 2606 -DisablePcCounts
.\tools\Capture-NativeDefenseBlock.ps1
.\tools\Compare-DefenseBlockCaptures.ps1
```

Original frames 2606/2644 and native `contact`/`landing` are screenshots of the
same two state boundaries. The native probe enters through `dd_gameplay_step`
and renders current C state; it does not replay RAM or the original screenshot.
Exact player/camera positions still differ because the broader live route and
object-swap logic remain incomplete, so this pair is state/visual evidence, not
a pixel-equality claim. The logical 256x224 crop reports 11,441/57,344 differing
pixels (19.9515%) at contact and 6,919/57,344 (12.0658%) after landing; the
repeatable diff makes that remaining visual gap explicit rather than hiding it.

## Fixed-point height and movement physics

Status: **Partial**. The earlier native code mixed direct integer approaches,
clamps, and per-state gravity approximations. The portable primitives now
follow the ROM's shared fixed-point helpers and their actual call graph, but
the native 30 Hz route scheduler still adapts the installed unit vectors to its
already-verified per-state arrival cadence. That remaining integration adapter,
not the vector/pass/shot initializers themselves, keeps the subsystem Partial.

### Ghidra control/data flow followed

| Ghidra/ASM | Recovered behavior | Native C |
| --- | --- | --- |
| bank 0 `$9CA0-$9CF5` | sign-extend 8.8 X velocity `$0390/$03A0`, add it to 16.8 X `$0360/$0370/$0380`, accept integer `$0010-$01F1`; on failure retain position and zero X velocity | `dd_integrate_longitudinal` |
| bank 0 `$9CF6-$9D2C` | add 8.8 depth `$03E0/$03F0` to `$03B0/$03C0/$03D0`, accept rows `$05-$98`; on failure retain position and zero depth velocity | `dd_integrate_depth` |
| bank 0 `$A84C-$A859` | call `$9CF6` twice, then `$9CA0` twice | shared installed-vector player tail |
| fixed `$D98A/$D98D` | enter the shared `$A84C` tail after state-specific setup/arrival logic | states `$2C/$2D/$2F/$33/$34/$35` and other installed-vector paths |
| bank 0 `$9B84-$9BAF` plus fixed `$C3C5` | divide `(elapsed << 8)` by curve, subtract quotient from base vertical, then add the wrapped 8.8 delta to height | `dd_integrate_height` |
| bank 0 `$B412-$B434` | reset elapsed and integer height, preserve the height fraction, subtract `$0050` from base vertical, saturate a signed-negative base to zero | `dd_restart_height_bounce` |
| bank 0 `$9D2D/$9BB0` | classify target angle using `$9DEB`, then expand/rotate signed motion using `$9C1C/$9C5E` | `dd_target_motion_vector` |
| bank 0 `$9F70->$AA07->$9E2D/$9E4C` | map the input nibble to facing and copy one of eight exact cardinal/diagonal signed 8.8 vectors | `dd_user_motion_vector` |
| bank 0 `$ABCD->$9D2D->$AA98->$9BB0` | unpack a packed route target, derive its facing, and install the signed unit vector | `route_facing` and `route_velocity_x/depth` in `dd_move_cpu_player` |
| bank 0 `$B035` | attach the ball using table-selected facing offsets; first byte is longitudinal X (`$0370`), second is depth (`$03C0`) | `dd_attach_ball` |
| bank 0 `$B0AB-$B11E` | attach with table zero, target receiver `$0052`, set integer height `$18`, and multiply each signed unit-vector axis by five with 16-bit wrap | `dd_begin_pass` |
| bank 0 `$B189-$B376` | initialize shot state `$05`, aim at the active hoop, halve major distance/vector before division, derive duration/curve/base vertical, and apply the cross-court `$36/$0207/$D8` override | `dd_initialize_shot_flight` |
| bank 0 `$AD41/$ADF2/$AE25/$AF46/$AFDD` | pass, bounce pass, airborne shot, rebound, and loose-airborne states call the shared axis/height helpers at their individual recovered counts | native ball dispatcher states `$02/$03/$05/$07/$09` |

`$B167` remains deliberately separate: bounce-pass state `$03` updates only
integer height using its high-byte velocity/gravity algorithm. `$B017` also
distinguishes the flight divisor `$004C=$0C` from a separate object field
`$04B0=$D8`; native `flight_curve` represents `$004C`, so it is `$0C`, not the
old `$D8` approximation.

The selected-bank-0 ROM-file mappings (including the 16-byte iNES header) are
`$9B84->$1B94`, `$9BB0->$1BC0`, `$9C1C->$1C2C`, `$9C5E->$1C6E`,
`$9CA0->$1CB0`, `$9CF6->$1D06`, `$9D2D->$1D3D`, `$9E4C->$1E5C`,
`$9F70->$1F80`, `$9DEB->$1DFB`, `$A84C->$285C`, `$AA07->$2A17`,
`$AA98->$2AA8`, `$ABCD->$2BDD`, `$AD41->$2D51`, `$AE25->$2E35`,
`$AF46->$2F56`, `$AFDD->$2FED`, `$B017->$3027`, `$B035->$3045`,
`$B0AB->$30BB`, `$B167->$3177`, `$B189->$3199`, and `$B412->$3422`.
Fixed-bank `$C3C5`, `$D98A`, and `$D98D` map to
`$1C3D5`, `$1D99A`, and `$1D99D`. These addresses are evidence only; the
native runtime does not load or execute ROM instructions.

### Dynamic and native verification

The gameplay Lua trace now records every physics call with object, action,
position, velocity, height, base, elapsed, curve, packed position, and target.
The bounded input run reaches `$9CA0/$9CF6` 13,810 times each, `$A84C` 6,556
times, `$9B84` 574 times, and `$B189` four times. A no-input shot records:

| Original event | X / velocity | Depth / velocity | Height inputs/result |
| ---: | --- | --- | --- |
| frame 2749 `$B189` → frame 2750 ball entry | `$005700 / $FF43` | `$004B00 / $00AB` | `$3800`, base `$0200`, elapsed `$00`, curve `$05` |
| frame 2750 helper returns | `$005643 / $FF43` | `$004BAB / $00AB` | `$39CD`, elapsed `$01` |
| frame 2751 helper returns | `$005586 / $FF43` | `$004C56 / $00AB` | `$3B67`, elapsed `$02` |

That height is exactly
`$3800 + ($0200 - floor($0100 / 5)) = $39CD`; the next step is `$3B67`.
Native regression checks assert both values, the controlled user-make launch
`$00FD00/$006100/$2200`, vector `$00FF/$FFF4`, and
duration/curve/base `$BC/$2F/$021D`, the double-add `$A84C` path, and
upper/lower rejection for both axes. The user-control checks additionally
prove that boundary attempts preserve sub-cell coordinates and zero rejected
velocity rather than snapping to a clamp.

Four controlled FCEUX boundary captures close the remaining primitive
rejection branches. `phase4-physics-x-upper` enters `$9CA0` with
`$01F1.80/$0100` and returns `$01F1.80/$0000`; `x-lower` does the same for
`$0010.80/$FF00`. `depth-upper` enters `$9CF6` with `$98.80/$0100`, while
`depth-lower` uses `$05.80/$FF00`; both return at `$9D2C` with the coordinate
unchanged and velocity zero. The checked-in capture option injects only at the
original helper entry, so the original instructions execute every decision
and the ordinary trace hook records the result. This promotes the bounded-axis,
height-script, angle/vector, packed-target, user-direction, and motion-clear
primitive routine families to Verified. The higher-level route cadence adapter
remains Partial and is tracked separately.

Focused traces close the four former initializer gaps:

- `$9F70->$AA07->$9E2D` maps all eight user directions to `$0130/$0000`,
  `$00C0/$FF40`, `$0000/$FF00`, `$FF40/$FF40`, `$FEC0/$0000`,
  `$FF40/$00C0`, `$0000/$0100`, and `$00C0/$00C0`. The original user side
  updates every other rendered frame and calls `$A84C` twice, equivalent to
  one native integration per host frame.
- The controlled frame-2600 pass enters `$B0AB` at ball `$010700/$005900`
  and returns from `$B11E` with `$0424/$02C6`, proving signed unit-vector
  multiplication by five rather than the removed native `/19` interpolation.
- The natural inbound enters pass state `$02` at frame 3553. Its first logged
  integration starts from `$001A3C/$00965C` with `$03D9/$FCD6`, and `$AD41`
  accepts receiver contact at frame 3572. `$B035`'s exact longitudinal-first,
  depth-second axis order is now shared by shot gather, dribble, award, pass,
  and inbound pickup; universal route installation remains Partial.
- The frame-2749 short shot enters `$B189` at `$005700/$004B00/$38C0` and
  returns from `$B376` with vector `$FF43/$00AB`, duration `$14`, curve `$05`,
  base `$0200`, and height `$3800`. The native minimum-21 normalization is gone.

Reproduce the ignored evidence with:

```powershell
.\tools\ghidra\Run-GameplayLoopAnalysis.ps1
.\tools\fceux\Capture-TipoffGameplay.ps1 -TraceStart 2450 -FinalFrame 6000 -CaptureName original-fixed-physics -JumpStart 2502 -JumpEnd 2515 -JumpButton B -DisablePcCounts
.\tools\fceux\Capture-TipoffGameplay.ps1 -TraceStart 2450 -FinalFrame 3000 -CaptureName original-fixed-physics-no-input -DisablePcCounts
.\tools\fceux\Capture-TipoffGameplay.ps1 -TraceStart 2550 -FinalFrame 2800 -CaptureName original-exact-vectors -JumpStart 2502 -JumpEnd 2502 -JumpButton B -MoveStart 2580 -MoveEnd 2587 -MoveDirection right -PassFrame 2600 -PassEnd 2600 -PassButton A -PassDirection right -DisablePcCounts
.\tools\fceux\Capture-TipoffGameplay.ps1 -TraceStart 2748 -FinalFrame 2751 -CaptureName original-exact-short-shot -DisablePcCounts
.\tools\fceux\Capture-TipoffGameplay.ps1 -TraceStart 3548 -FinalFrame 3575 -CaptureName original-exact-inbound-pass -DisablePcCounts
.\build.ps1 -RomPath 'F:\Games\NES\Double Dribble\Double Dribble (USA) (Rev 1).nes'
```

The first post-apex reference screenshot is
`captures/original-fixed-physics-no-input/frame-2749.png`; the capture remains
ignored along with its RAM/CSV evidence. Native transition frame 548 is the
corresponding release checkpoint. The fresh native capture/compare at that
boundary reports 3,007/57,344 differing pixels (5.2438%), improved
from 3,074/57,344 (5.3606%). The exact ball launch/arc is covered numerically;
the visible residual remains concentrated in player route positions and sprite
ordering. The stable live-handoff gate remains 2,300/57,344 (4.0109%), below
its 5% limit.

## Complete CPU pass/shot decision policy

Status: **Verified**. The previous native choice used only longitudinal distance
and selected the most advanced teammate. The fixed-bank routine is a
seven-region, phase-driven policy, so that approximation has been replaced by a
named native decision tree.

The Ghidra headless exporter now forces every meaningful basic-block entry in
`$D759-$D94D`, prints the tables at `$D8B9`, `$D8D5`, and `$D94E`, and includes
the bank-0 helpers `$8BF8/$AC2A/$AC5C/$AC64`. The recovered mapping is:

| Ghidra/ASM | Behavior | Native C |
| --- | --- | --- |
| fixed `$D759-$D771` | force a shot below five seconds or at coarse possession tick `$18` | `dd_cpu_decide_possession` urgent-shot gates |
| fixed `$D772-$D81F` | classify the mirrored packed region, select a phase/height route through `$D8B9/$D8D5`, decrement `$04F0`, and try a pass | `dd_cpu_policy_target`, `decision_timer`, and the at-target branch |
| fixed `$D77B-$D7C5` | compare `$D99A->$8C36`'s projected high byte with role zero and the paired player; reject through `$D857`, otherwise install the sole center target `$85`/mirrored `$9A` | `cpu_projection_high` plus the region-two branch in `dd_cpu_decide_possession` |
| fixed `$D820-$D854` | region-four obstacle response and region-six/entropy-band lane target | `dd_cpu_avoid_ball_or_defender` and `dd_cpu_set_lane_target` |
| fixed `$D862-$D8B6` | moving-carrier lookahead, phase `$80/$C0` pass gates, region-four/five shots, and priority refresh tail | moving branch of `dd_cpu_decide_possession` |
| fixed `$D8F1-$D92E` | decrement two-decision pass cooldown and reject an ineligible receiver | `cpu_pass_cooldown` and `dd_cpu_try_region_pass` |
| fixed `$D8FA-$D94D`, table `$D94E` | choose role three when phase bit seven is clear, role four when set; accept only a different nonzero region | `dd_team_role` plus `dd_cpu_try_region_pass` |
| bank 0 `$9018`, fixed `$D92F` | queue passer `$31`, receiver `$37`, attached ball `$00`, and release timer eight | `dd_queue_cpu_pass` plus the shared `$31` release handler |
| fixed `$D99A-$DA39` | predict one facing step against the ball or the player's `$0580` paired defender, then test four two-step escape directions | `dd_cpu_avoid_ball_or_defender` using `paired_player` |

Fixed `$C02B-$C033` continuously mixes global phase `$001A` into entropy byte
`$0063`; `$D834` consumes its high band. Native `cpu_entropy` advances once per
rendered frame and supplies the same bit bands without reproducing the NES busy
loop or instruction timing.

The original decision latency is dispatcher-driven as well. The opening inbound
receiver is action `$25` at FCEUX frame 2666, becomes `$32` at frame 2679, and
reaches `$D759` at frame 2681. That is seven alternating object updates, not the
fourteen-update delay the native route had accidentally shared with ordinary
live-pass reception. A deterministic regression starts at age six, dispatches
the seventh turn, and requires re-entry into the policy without an airborne
half-court shot.

The region-two follow corrected an earlier audit premise: rev-1 does **not**
cycle through `$85/$86/$87`. `$D77B-$D7AF` initializes `$85`, then loops around
the same `$D78F` comparisons three times without recomputing the candidate. A
successful path therefore installs only `$85`, or `$9A` after `$AC64/$AC5C`
mirroring. The comparisons are also deliberately unusual: `$D795` and `$D7A2`
compare the high/ninth byte left in scratch `$0031` by `$D99A->$8C36` against
the packed low bytes of the possession-selected role-zero object and its paired
player. They do not test general occupancy of candidate `$85`.

The shipping predecessor matters. State `$25` starts at bank-0 `$8B5A` and
unconditionally calls `$D99A` before `$D978` can enter fixed `$D759`. Native
route steps four and five now follow that chain. An accepted lookahead stays in
`$25`, scales the escape vector once through the `$ABCD->$8BF8` equivalent, and
moves on that dispatch. A clear lookahead leaves `cpu_projection_high` for the
region-two checks; `$D7C5` rejection enters `$D857`'s normal policy and timer
path. The move result also uses the original same-dispatch integration tail.

The fixed-bank ROM-file mappings, including the 16-byte iNES header, are
`$C02B->$1C03B`, `$D759->$1D769`, `$D77B->$1D78B`, `$D7C5->$1D7D5`,
`$D8B0->$1D8C0`, `$D8B9->$1D8C9`, `$D8D5->$1D8E5`, `$D8F1->$1D901`,
`$D92F->$1D93F`, `$D94E->$1D95E`, `$D99A->$1D9AA`, and `$DA39->$1DA49`.
Selected-bank-0 `$9018` maps to `$1028`. The native runtime does not load those
addresses or execute their bytes; the compact behavior tables and algorithms
are ordinary C policy constants and state.

The checked-in FCEUX hooks now include every decision boundary. This fresh
natural run reaches 320 `$D759` roots, six `$D8FA` receiver searches, seven
`$D92F` pass initializers, eleven `$D7CC` shot selections, and 463 `$D99A`
lookaheads (one accepted escape and 462 clear/rejected returns):

```powershell
.\tools\fceux\Capture-TipoffGameplay.ps1 -TraceStart 2500 -FinalFrame 12000 -CaptureName original-cpu-decision-complete -JumpStart 2502 -JumpEnd 2515 -JumpButton B -DisablePcCounts
.\tools\ghidra\Run-GameplayLoopAnalysis.ps1
```

`tests/dd_gameplay_cpu_test.c` proves the two receiver roles, different-region
acceptance, same-region rejection, cooldown, original delayed pass release,
mirrored route-table output, lane targeting, region-five shots, clock and
possession forced shots, decision-timer underflow, and the paired-defender
avoidance branches. Its region-two checks cover natural-trace coordinates,
role-zero and paired rejection, `$D857` fallback, unrelated occupancy, mirrored
`$9A`, signed projection overflow, and exact raw-to-scaled vectors
`$FF02/$0019 -> $FEC2/$001F`. A legal-flow regression queues a pass through
`$D8FA/$9018`, releases it, receives it through `$AD41/$B138/$AD6D` into `$25`,
then requires the fourteenth scheduled update to reach
`$D99A->$D759->$D772` without assigning the action or projection field in the
test. It also runs a complete accelerated four-period match with
no controller input. Each period uses the original one-minute BCD clock path;
the test observes CPU pass, shot, and inbound decisions, validates all ten live
court positions and every dribble owner each frame, fails if a CPU carrier
holds an identical position and action for more than 640 frames, and requires
period four to reach GAME SET and return to the title within 25,000 frames.
This long-run guard complements the isolated seed and region checks, promoting
the fixed `$D7CC-$DA38` policy and the called bank-0 route, region, and paired-
player helpers to routine Verified. Per-state installed-vector cadence remains
Partial until every action follows the recovered `$ABCD` movement path.
The existing deterministic opening and post-inbound checkpoints also
remain unchanged.

## User defender contest and recovered shot angles

Status: **implemented and verified for the portable state/ownership path**.
This slice uses the user dispatcher rather than reusing the CPU `$23/$24`
approximation.

The headless Ghidra follow is:

| Ghidra/ASM | Recovered behavior | Native C |
| --- | --- | --- |
| `$A014` table entry `$0F->$A3E2` | run user motion/render, then classify ball state | `dd_step_live`, `dd_user_contest_eligible` |
| `$A3EB-$A409` | `$07/$09` may contest without input; `$01/$04/$05` require bit 7 of pressed input `$0680` | A-edge/automatic branches in `dd_user_contest_eligible` |
| `$A40A-$A42A` | require `$001D=$0056=0`, follow mutable pair `$0580`, accept paired states `$26/$27/$03` | phase guard plus `paired_player`/action checks |
| `$A607-$A62A` | clear jump direction, install `$9B26/$9B27`, enter user state `$11`, set `$04E0=1` | `dd_begin_user_contest` |
| user table entry `$11->$A638` | face inward while unowned and advance exact `$9ABD` signed height script | `dd_step_user_contest` |
| `$A662-$A68F` | only when the next script byte is zero, call `$A6C3`; contact sets acquisition mode 3, ball state `$00`, owner=current, SFX `$20` | apex byte check, `dd_jump_ball_contact`, awarded ball, `dd_request_audio_event(0x20)` |
| `$A693-$A6AA` | on landing, only the recorded owner with gate clear runs `$92BD->$A44B` | landing-delayed `dd_transfer_contact_possession` |
| `$A6AD->$A5D0` | a miss exposes state `$10`, then returns to live user `$0F` | `DD_PLAYER_USER_CONTEST_RECOVER->$0F` |

The dynamic FCEUX run presses A/X at original frame 2748. User slot `$02` is
in `$0F`, the ball is gather state `$04`, and its mutable pair is shooter slot
`$07` in state `$27`. The same frame reaches
`$A3E2->$A402->$A40A->$A426->$A607`; frame 2750 enters `$A638` in state `$11`.
Frames 2766-2770 reach the zero-byte `$A6C3` checks. That natural shot scores,
so score gate `$0056=2` stops later contact checks, which is useful proof of the
remaining gate caveat. Earlier tip-contact traces reach `$A68A`, while native
regressions deterministically prove the successful apex contact, ball state
`$00`, SFX request `$20`, delayed landing transfer, and the missed `$10->$0F`
branch.

This trace also resolves the opening pair map. Before the first inbound,
original `$0582=$07` and `$0587=$02`, converting to native players `0<->5`;
the other affected reciprocal pair is `4<->6`. After `$99D9/$9A31` swaps
inbound roles/links, the later trace has original `$0582=$08`, or native
`0<->6`. The native initialization and inbound regression now reproduce those
two distinct stages.

### Angle result

Shots are not selected from a small set of canned 45-degree angles. `$B189`
calls `$9D2D`, which quantizes the target ratio through the 33 thresholds at
`$9DEB` into an **8-bit circular angle byte** (even steps), then `$9BB0`
expands it through the `$9C1C/$9C5E` signed unit-vector tables. `$AA98` groups
that byte into eight display facings, but flight retains the finer byte.

For the natural frame-2749 shot from `$005700/$004B00` toward the left hoop,
Ghidra/FCEUX records angle `$62`. `$9BB0` expands it to longitudinal/depth
velocity `$FF43/$00AB` (`-189/+171` in signed 8.8), with duration `$14`, curve
`$05`, and vertical base `$0200`. Native `flight_angle` now preserves `$62`
and the regression asserts the complete tuple. On a rim miss, `$AF72` keeps
that angle for result `$02`, clears horizontal motion for result `$03`, and
adds `$80` (180 degrees) for result `$04`; the selected vector is then
arithmetically halved. Thus the rim result changes the outgoing direction, but
there is no user-controlled release-angle meter in this path.

Reproduce the ignored evidence and original/native screenshot comparison with:

```powershell
.\tools\ghidra\Run-GameplayLoopAnalysis.ps1
.\tools\fceux\Capture-TipoffGameplay.ps1 -TraceStart 2738 -FinalFrame 2820 -CaptureName original-user-contest-natural -PassFrame 2748 -PassEnd 2748 -PassButton A -DisablePcCounts
.\tools\Capture-NativeUserContest.ps1
.\tools\Compare-UserContestCaptures.ps1
```

Original frame 2749 and native scene 548 differ by 3,007/57,344 pixels
(5.2438%), the same bounded residual already attributed to off-ball route
positions and dynamic sprite ordering. Both expose the user contest and the
numeric shot tuple above; ignored captures contain no repository assets.

## Curved three-point line, scoring, and `$09/$25` audio

Status: **implemented and verified**. This closes the release-to-score chain
instead of treating every non-free-throw basket as two points.

The Ghidra follow begins at both release sites. CPU shooter `$8D57` calls
`$A7EA` before `$B189`; user shooter `$A504` calls `$B189` and then `$A7EA`.
That ordering is preserved in C around the common shot initializer. The exact
classifier and score consumer are:

| Ghidra/ASM | Recovered behavior | Native C |
| --- | --- | --- |
| `$A7EA-$A7F0` | clear shot kind `$005F`, then split on current object `$004B < 7` | `dd_classify_field_goal` user/CPU side branch |
| `$A7F7-$A80A` | classify the far X page immediately as three; otherwise subtract depth `$26` | mirrored full-court X/depth checks |
| `$A80C-$A81A` | depths outside the 23-entry span are three; otherwise use `(depth-$26)>>2` | bounded table index |
| table `$A834` | `70 68 60 58 50 4C 48 46 44 42 41 40 40 41 42 43 44 45 46 47 48 54 60` | `boundary` in `dd_classify_field_goal` |
| `$A81B-$A829` | user side is two only when X low is strictly greater than the table byte; CPU side negates the byte and is three only when X low is strictly greater | asymmetric equality/edge comparisons preserved byte-for-byte |
| `$A82A-$A833` | write kind `$01` and request SFX `$09` through `$C141` | `shot_value=3`, `dd_request_audio_event(0x09)` |
| `$AEDE-$AEFC` | at score counter `$08`, start with X=2; kind 1 increments to 3 and requests `$25`; kind 2 decrements to 1; score via `$C477/$C6AD`, then clear kind | `dd_apply_made_basket_score` |

The controlled release traces pin the equality edge in both court directions.
At depth `$58`, table index `$0C` is `$40`: user X `$0141` is inside/two and
`$0140` is outside/three; CPU X `$00C0` is inside/two and `$00C1` is
outside/three. The three-point score probe keeps kind `$01` through counters
`$0C-$09`, requests `$25` at original frame 2607, then writes both score copies
`00->03` and clears the kind at counter `$08` on frame 2608. Native regressions
cover these four boundary cases, the far-page cases, release ordering, and all
one-/two-/three-point score branches.

DDAP v20 retains the audible results, not only event numbers. Matched FCEUX
APU runs isolate `$09` as a 189-frame pulse-2 cue at duty 50%/volume 6 whose
timer moves `256->162->255` before stopping. `$25` is a 42-frame two-pulse
arpeggio; comparing shot kind 1 against kind 0 proves the simultaneous
triangle/noise traffic belongs to the surrounding basket state, so those
channels are deliberately excluded. `dd_build_three_call_audio_wav` and
`dd_build_three_score_audio_wav` synthesize the normalized pack entries, and
Win32 plays them for native events `$09/$25`. This is native event synthesis;
there is no 6502/APU instruction interpreter.

Reproduce the ignored evidence with:

```powershell
.\tools\ghidra\Run-GameplayLoopAnalysis.ps1
.\tools\fceux\Capture-TipoffGameplay.ps1 -TraceStart 2598 -FinalFrame 2610 -CaptureName original-shot-kind-user-inside -JumpStart 2502 -JumpEnd 2515 -JumpButton B -PassFrame 2600 -PassEnd 2600 -PassButton B -ShotKindCase 1 -DisablePcCounts
.\tools\fceux\Capture-TipoffGameplay.ps1 -TraceStart 2598 -FinalFrame 2610 -CaptureName original-shot-kind-user-outside -JumpStart 2502 -JumpEnd 2515 -JumpButton B -PassFrame 2600 -PassEnd 2600 -PassButton B -ShotKindCase 2 -DisablePcCounts
.\tools\fceux\Capture-TipoffGameplay.ps1 -TraceStart 2738 -FinalFrame 2760 -CaptureName original-shot-kind-cpu-inside -ShotKindCase 3 -DisablePcCounts
.\tools\fceux\Capture-TipoffGameplay.ps1 -TraceStart 2738 -FinalFrame 2760 -CaptureName original-shot-kind-cpu-outside -ShotKindCase 4 -DisablePcCounts
.\tools\fceux\Capture-TipoffGameplay.ps1 -TraceStart 2598 -FinalFrame 2660 -CaptureName original-three-point-score -BasketFrame 2601 -BasketResult 1 -BasketShotKind 1 -DisablePcCounts
.\tools\fceux\Capture-TipoffGameplay.ps1 -TraceStart 2598 -FinalFrame 2900 -CaptureName original-sfx09-outside-full -JumpStart 2502 -JumpEnd 2515 -JumpButton B -PassFrame 2600 -PassEnd 2600 -PassButton B -ShotKindCase 2 -DisablePcCounts
.\build.ps1 -RomPath 'F:\Games\NES\Double Dribble\Double Dribble (USA) (Rev 1).nes'
```

`gameplay-shot-kind-calls.csv`, `gameplay-score-calls.csv`,
`gameplay-sfx-calls.csv`, `gameplay-apu-writes.csv`, and
`gameplay-audio-state.csv` hold the dynamic proof. The Ghidra report prints the
entire `$A834` table and the `$A7EA/$AEDE` instruction flows. All captures,
WAVs, Ghidra reports, the ROM, and the generated asset pack remain ignored.

## User shooting animation and natural make/miss completion

Status: **implemented and verified**. This follow starts at the real B-button
entry rather than injecting a result after release.

| Ghidra/ASM | Recovered behavior | Native C |
| --- | --- | --- |
| `$AA75` | clear `$04F0+X`, install height stream `$9B34` from `$9B26`, set player `$03`, release gate `$04E0=1`, and ball `$04` | `dd_begin_shot` |
| `$A896` plus `$A8E6/$A9DC` | state `$03` and CPU state `$27` share the facing-indexed metasprites `$22,$28,$23,$27,$21,$25,$24,$26` | DDAP v20 `shot_animation[8]` and `player.animation` |
| `$A504->$A84C` | integrate the takeoff depth vector twice and longitudinal vector twice, then update pose, projection, and `$9ABD` height; fresh input does not retarget the airborne state | user-shot gather dispatcher preserves takeoff momentum |
| `$8D1F->$AAEE->$AA98->$B503` and `$8D57` | face the CPU shooter toward the active hoop, clear its three motion vectors, use shared pose selection, and advance the shot height stream | CPU shot initializer and state `$27` dispatcher |
| `$B035` table 2 | add the first facing offset to longitudinal `$0370` and the second to depth `$03C0` | exact shot attachment path |
| `$B189->$9D2D->$9BB0` | target virtual hoop object `$0D`, derive signed angle/vector, duration, curve, and vertical base | `dd_initialize_shot_flight` |
| `$B2F8/$B318/$B32B` | keep full duration in divider `$0003` for `(target height-current height)/duration`; replace it with curve only for the half-duration term | corrected fixed-point vertical initializer |
| `$AE25->$B377` | integrate flight and classify the descending ball at heights `$34-$37`; result `$01` scores through `$06`, results `$02-$04` enter `$08->$09` miss handling | airborne, score, miss, and rebound dispatcher states |
| `$AE25/$AEDE->$98B5->$990A` and table `$9922` | select net phase 2 on the make, phase 1 at score counter `$08`, then phase 0 on underflow; patch four tiles at `$2168/$2188` or `$2576/$2596` | `net_animation_phase`, `net_basket_side`, and the renderer's pack-backed net patch |

The selected bank-0 CPU addresses map to ROM offsets `$8D1F->$0D2F`,
`$8D57->$0D67`, `$98B5->$18C5`, `$9922->$1932`, `$A504->$2514`,
`$A84C->$285C`, `$A896->$28A6`, `$A8E6->$28F6`, `$A9DC->$29EC`,
`$AA75->$2A85`, `$B035->$3045`, `$B189->$3199`, `$B2F8->$3308`,
`$B318->$3328`, `$B32B->$333B`, and `$B377->$3387`, including the iNES
header. The headless export anchors those code sites and prints the
animation-pointer, eight-pose, net-tile, held-offset, and `$9B26`
script-pointer tables.

The controlled original make changes only the carrier's pre-shot position to
X `$00F7`, depth `$61`, then runs unmodified `$AA75/$A504/$B189/$AE25/$B377`.
At frame 2602 the facing-zero player changes from metasprite `$20` to `$22`,
the ball releases at `$00FD00/$006100/$2200`, and `$B189` returns vector
`$00FF/$FFF4`, duration `$BC`, curve `$2F`, and vertical base `$021D`.
On descent it reaches the right hoop `$01B8/$58`, produces result `$01`, and
enters score state `$06` at frame 2790. The untouched neighboring start at X
`$00F6`, depth `$5A` follows the same code but misses the depth window and
enters rebound state `$07` at frame 2816. Native regressions reproduce the
make launch tuple exactly and drive both cases through the shipping input and
dispatcher loop, proving one score and one non-score/rebound outcome.

A second controlled trace starts at X `$00D0`, depth `$61` while holding
right before the shot. `$AA75` leaves `$0130=$0260` intact; scheduled `$A504`
updates move X from `$00D040` at frame 2602 to `$00D2A0`, `$00D500`, and
onward while metasprite `$22` remains selected. The native `takeoff` and
`airborne` checkpoints assert the same preserved vector and two-add cadence.
This is the original form of in-air movement: existing takeoff momentum
continues, but state `$03` does not accept a new steering vector mid-jump.

The make trace also proves the net sequence independently of ball motion.
Original frame 2792 has phase 2 applied, frame 2798 has phase 1, and frame
2807 has restored phase 0. Table `$9922` is 24 bytes: two basket sides by
three four-tile frames. DDAP v20 extracts those bytes, and the native renderer
patches the equivalent two adjacent tiles on two rows before drawing the
court. Native regression checks assert the exact table and phase transitions
`2->1->0`.

Ignored visual evidence includes original release/make/miss frames and eight
native gather/release/result/inbound captures. Whole-frame differences are 17.6322%
at release, 15.8831% at the make result, and 15.7628% at the miss result; the
known residual is dominated by clock timing, frame selection, and dynamic
sprite ordering. The post-result comparisons differ by **14.8193%** for the
automatic make inbound (original 2929/native make-inbound) and **13.3214%**
for the missed-shot boundary formation (original 2944/native miss-inbound).
Visual inspection confirms
the recovered `$22` shooting pose, hoop entry for the make, and loose ball for
the miss. Numeric/state comparisons are the acceptance gate for this slice.

Reproduce the evidence with:

```powershell
.\tools\ghidra\Run-GameplayLoopAnalysis.ps1
.\tools\fceux\Capture-TipoffGameplay.ps1 -TraceStart 2598 -FinalFrame 2815 -CaptureName original-user-shot-make-final -JumpStart 2502 -JumpEnd 2515 -JumpButton B -PassFrame 2600 -PassEnd 2600 -PassButton B -UserShotDepth 0x61 -UserShotX 0xF7 -DisablePcCounts
.\tools\fceux\Capture-TipoffGameplay.ps1 -TraceStart 2598 -FinalFrame 2820 -CaptureName original-user-shot-miss-final -JumpStart 2502 -JumpEnd 2515 -JumpButton B -PassFrame 2600 -PassEnd 2600 -PassButton B -DisablePcCounts
.\tools\fceux\Capture-TipoffGameplay.ps1 -TraceStart 2588 -FinalFrame 2810 -CaptureName original-user-shot-moving-right -MoveStart 2588 -MoveEnd 2644 -MoveDirection right -UserShotX 0xD0 -UserShotDepth 0x61 -DisablePcCounts
.\tools\Capture-NativeShooting.ps1
.\build.ps1 -RomPath 'F:\Games\NES\Double Dribble\Double Dribble (USA) (Rev 1).nes'
```

### Post-shot make/miss formation and inbound state flow

This follow fixes the scattered-player aftermath rather than repositioning
players in the renderer. The relevant state effects recovered from Ghidra and
confirmed in FCEUX are:

| State / helper | Original effect | Native translation |
| --- | --- | --- |
| `$AE25` result `$01` | flip `$0050`, set/decrement gate `$0056`, enter ball score `$06` | change possession direction and arm `rebound_formation_pending` |
| `$8491->$8503/$8507` | combine gate, role `$0690`, team side, and object phase; install one `$2D` plus nine `$36` targets | DDAP v20 tables plus the traced per-slot cadence adapter |
| player `$2D->$2E->$2F->$30` | chase/claim the scored ball, return to the boundary, then wait for all `$36->$37` routes | native rebound-chase dispatcher chain and formation readiness gate |
| `$8EBF/$8C6B->$AD0E->$B035` | claim the scored or loose ball, reset its local phase, set held height `$10`, and add the table-0 longitudinal/depth hand offsets | exact owner/carrier assignment and pickup attachment |
| `$8EE2->$A0DA->$9018->$B0B8` | find an on-screen non-role-zero teammate, enter release `$31`, and immediately install pass angle, facing, and velocity before the countdown | `dd_automatic_inbound_receiver`, `dd_start_inbound_release`, and `dd_prepare_pass_motion` |
| `$8FE0` at timer `$04` | change ball `$00->$02` and clear camera carrier `$0048`; retain inbound owner `$005B` during flight | release gate without reattaching or recomputing the pass |
| ball `$07->$AF46` | on an untouched miss, increment the rebound counter; its third dispatch may reach the boundary rule | outcome-zero guard lasts only while `action_age < 3` |
| `$9635->$9651->$D6BD` | queue reason `$16`, flip possession, reset ten formation targets, and assign receiving role zero state `$41` | `dd_begin_common_inbound` and native `$36/$41` formation |
| `$96AF-$96CA` | determine `$05E0` from the source side, then add `$9763` to `$05D0` with 8-bit wrap and no carry into `$05E0` | explicit low-byte wrap preserving the independently derived ninth bit |

The controlled make reaches `$8491` at original frame 2790. Receiving role
zero (original slot `$07`, native player 5) gets `$2D/$BB`; the other nine get
`$36`. It then reaches `$30`, `$8EE2` takes the clear-mode branch, `$A0DA`
selects a teammate, and `$9018` launches the inbound. The untouched miss enters
ball state `$07` at frames 2815-2816 and reaches `$9635` at frame 2818. At that
instant the original ball is `x=$01D328, depth=$004FE0`, packed `$009D`.
Because `$9763[$9D]=$80`, low-byte wrap produces target `$001D`, which `$ABCD`
expands to `x=$01D800, depth=$000800`. The earlier native 16-bit addition
incorrectly produced `$011D` and parked the inbounder at depth `$88`, causing
the visibly broken formation.

The pickup follow anchors `$AD0E` at ROM `$2D1E`, `$B035` at `$3045`,
`$9018` at `$1028`, and `$8FE0` at `$0FF0`. The `$B075` pointer table selects
three 16-byte facing tables at `$B07B/$B08B/$B09B`; in all three, the first
signed byte belongs to longitudinal X and the second to court depth. The old
native adapter had those axes reversed for dribble, award, pass, and inbound.
Original frames 2914 and 2942 plus native `make-pickup.png` and
`miss-pickup.png` show the corrected baseline pickup, while regression checks
pin the facing-zero table-0 attachment at X `+8`, depth `-1`, height `$10C0`.

FCEUX frame 2944 and native `miss-inbound.png` now show the same baseline-side
formation; frame 2929 and native `make-inbound.png` show the made-basket
automatic inbound. The regression drives both shots from B input through the
shipping ball/player dispatchers, asserts the make's CPU receiver, and asserts
the miss's reason `$16`, state `$41`, and exact `$001D` coordinates.

### CPU pass ownership, switched defense, and made-basket audio repair

Status: **implemented and verified within the existing coverage entries**.
This repair follows original state transitions that were already classified
Verified but had incomplete native side effects.

The orphaned ball came from ordinary CPU pass setup, not pass collision. Fixed
bank `$D8FA-$D92F` (ROM `$1D90A-$1D93F`) selects role three/four, stores the
receiver in `$0052/$0009`, writes receiver state `$37`, then calls bank-0
`$9018` (ROM `$1028`). `$9018` writes passer state `$31`, animation `$0A`, and
release timer `$08`, but it also calls `$B503->$B0B8` (ROM `$30C8`) immediately.
That tail aims ball object zero at the receiver through `$9D2D/$AA98/$9BB0`,
stores facing, and multiplies the signed unit vector by five. `$8FE0` later
reaches timer `$04`, changes ball `$00->$02`, and clears camera carrier `$0048`;
it does not calculate motion. The native ordinary CPU path had installed zero
velocity and therefore left `$02` permanently stationary after release. Both
ordinary CPU passing and CPU inbound now attach the held ball and run the same
`dd_prepare_pass_motion` side effect before the countdown. Regressions require
a nonzero vector at queue time, visible position change after release, and the
existing natural CPU inbound reception at native frame 1371.

Defensive switching follows bank-0 `$A29D` (ROM `$22AD`) beyond candidate
selection. The switch is legal only while the current `$0F` player owns role
zero. `$99D9` (ROM `$19E9`) swaps `$0690` roles and `$0580` opponent links;
`$9A31` (ROM `$1A41`) repairs the reciprocal links before the old/new actions
become `$20/$0F`. Native switching previously moved only the action and control
index, so the selected defender still referenced the wrong CPU opponent and
could not satisfy `$A3E2`'s paired-shooter gate. The C path now performs both
swaps. A deterministic B-switch selects native player 2, produces links
`2<->5` and `0<->8`, then A reaches `$A607` (ROM `$2617`) and the `$A638`
(ROM `$2648`) apex/contact block state.

The score sound follow begins in ball flight `$AE25`. A clean `$B377` result
one reaches `$AE8E` (ROM `$2E9E`) and requests `$18`; score-state `$06`
underflow reaches `$AF2F/$AF34` (ROM `$2F3F/$2F44`) and requests `$1F/$22`.
Headless Ghidra maps the bank-1 streams to `$87B6/$87CA` (ROM
`$47C6/$47DA`), `$886D` (`$487D`), and `$8922` (`$4932`). A controlled FCEUX
run at frame 2600 injects only a clean rim result and freezes gameplay after
frame 2615: `gameplay-sfx-calls.csv` contains exactly `18`, then `1F,22`, and
the isolated pulse/triangle/noise output ends at normalized frame 436. DDAP
v20 stores 104 normalized note events as `basket.score`; native code requests
event `$18` at the make and plays the complete 437-frame cue without a 6502 or
APU emulator.

Reproduce the ignored evidence and screenshots with:

```powershell
.\tools\ghidra\Run-GameplayAudioAnalysis.ps1
.\tools\fceux\Capture-TipoffGameplay.ps1 -TraceStart 2595 -FinalFrame 3800 -CaptureName original-score-audio-isolated -BasketFrame 2600 -BasketResult 1 -BasketCounter 0 -BasketShotKind 0 -ScoreAudioFreezeFrame 2616 -DisablePcCounts
.\tools\Capture-NativeGameplay.ps1 -TransitionFrames 1344,1352,1371
.\tools\Capture-NativeSwitchBlock.ps1
.\tools\Capture-NativeShooting.ps1
.\build.ps1 -RomPath 'F:\Games\NES\Double Dribble\Double Dribble (USA) (Rev 1).nes'
```

The principal native frames are `native-gameplay/frame-1352.png` (CPU inbound
pass in flight), `native-gameplay/frame-1371.png` (CPU reception),
`native-switch-block/switch-block.png`, and `native-shooting/make-result.png`.
Original score checkpoint `original-score-audio-isolated/frame-2600.png` and
the earlier `original-user-switch` / `original-user-contest-natural` captures
remain alongside their CSV proof under ignored `captures/` directories.

Coverage remains **92.6% unrounded (93% displayed)** and match rules remain
**80.8%**. The portable shot, result, score, and miss states were already
Verified; adding a NES metasprite selection does not inflate the denominator.
The corrected fixed-point math, held-ball axes, inbound initialization, and
net state strengthen those verified entries. Core movement physics remains
Partial because route interpolation still uses the documented cadence adapter
and lacks a dynamic original court-boundary rejection capture.

### Held-shot release gate and bounded inbound tracking

Status: **implemented and verified**. A fresh FCEUX input interval holds NES B
from original frame 2600 through 2612. Player object `$02` remains state `$03`,
ball object zero remains attached in state `$04`, and the player's integer
height rises `$10,$15,$19,$1C,$1F,$21`; after B clears, the next scheduled
player dispatch launches ball `$05` at frame 2614. This follows bank-0
`$A504-$A528` (ROM `$2514-$2538`): `$A516` requires release gate
`$04E0+X != 0`, then `$A51B-$A520` waits while controller byte `$0670+X` still
has bit `$40` set. Only the clear-bit branch reaches `$B189` and `$A7EA`.

Holding through the height-script landing follows the separate
`$A52B-$A540` branch: request SFX `$05`, detect ball `$04`, request whistle
`$2C`, store inbound reason `$0F`, and jump into `$9651`. Native C now retains
the ball in gather for the complete B-held interval, releases on the B-up
frame, and reproduces the reason-`$0F` turnover rather than firing an arbitrary
two ticks after takeoff or auto-launching after landing.

The inbound tracking correction follows player `$41` at `$8C6B` and formation
movement `$36` at `$904D`. Both call fixed-bank `$D978`, which compares current
packed bytes `$05B0/$05C0` with target bytes `$05D0/$05E0`; their movement tail
reaches `$D98D->$A84C->$9CA0/$9CF6`. The nominal center of packed column `$1F`
is world X `$01F8`, but `$9CA0` legally accepts only `$0010-$01F1`. The
original unit-vector walker therefore enters packed cell `$1F` and claims it
before a later integration can leave the court. The native cadence adapter now
uses the same packed edge arrival, validates the extended depth band on the
`$2F->$30` return, and passes every CPU target step through the recovered axis
bounds. Only the inbound retriever is allowed to occupy the baseline pickup
cell; the other `$36->$37` players remain on legal formation coordinates.

Deterministic regressions hold B for six native frames and require ball `$04`
to remain attached, release to `$05` only on B-up, cover the reason-`$0F`
landing branch, reject a `$2F` X-only false arrival, and prove a `$41` edge
pickup stops at X `$01F0` instead of chasing `$01F8`. Reproduce the ignored
evidence and screenshot checkpoints with:

```powershell
.\tools\ghidra\Run-GameplayLoopAnalysis.ps1
.\tools\fceux\Capture-TipoffGameplay.ps1 -TraceStart 2588 -FinalFrame 2640 -CaptureName original-user-shot-held -JumpStart 2502 -JumpEnd 2515 -JumpButton B -PassFrame 2600 -PassEnd 2612 -PassButton B -DisablePcCounts
.\build.ps1 -RomPath 'F:\Games\NES\Double Dribble\Double Dribble (USA) (Rev 1).nes'
.\build\double_dribble_port.exe --render-gameplay-moving-shot .\build\double-dribble.assetpack held .\captures\native-shooting\held-shot.bmp
.\build\double_dribble_port.exe --render-gameplay-shot .\build\double-dribble.assetpack miss inbound .\captures\native-shooting\bounded-inbound.bmp
```

Coverage remains **92.6% unrounded (93% displayed)**, with **80.8%** match-rule
completeness. The verified player/ball dispatcher entries do not gain duplicate
credit; this repair strengthens user shooting and inbound, while universal
route interpolation and the remaining contested-rebound paths remain Partial.

### Post-score chase, reception, and native sprite completeness

Status: **implemented and regression-verified**. Fresh headless Ghidra reports
confirm that `$8491->$ABCD` installs the inbound retriever's route vector,
`$8E71->$D98D` consumes that vector in state `$2D`, and `$D990->$A896` still
runs the common facing/metasprite tail. State `$2F` follows the same pattern at
`$8EBF` after `$8E88->$ABCD`. Both handlers also contain a narrower correction:
`$8E7C-$8E82` and `$8ED6-$8EDC` rerun `$ABCD` only when current player `$004B`
equals rotating priority `$004D`. The first native repair removed every refresh
and retained approximate hardcoded `$00FF/$000C` components; diagonal chasers
could therefore miss their packed target and stop at a court bound. The corrected
C path installs `$9D2D->$9BB0`'s exact vector initially, refreshes only priority
`$004D`, tests packed arrival before refreshing, and advances facing-specific run
frames through `$D990->$A896`.

An August 15 regression exposed a native-only shuffle in that same route.  A
fresh FCEUX trace (`diagnostic-inbound-shuffle-original`) records player `$02`
entering `$2D` in packed cell `$A5`, reaching target `$A4` at frame 2774,
entering `$2F` with target `$A1` at frame 2776, and reaching `$30` at frame
2834.  The C path had replaced `$D978`'s full packed equality in `$8EBF` with
a longitudinal center crossing; meanwhile the untouched axis could leave its
already-reached packed component and reverse on the next `$004D->$ABCD`
refresh.  Native now restores full `$D978` equality and latches each reached
packed axis while the other finishes.  The made-basket regression follows the
entire `$2D->$2E->$2F->$30->$0D` path and rejects any X/depth sign reversal.

The Win32 host now also retains each non-repeat gameplay key-down until one
simulation frame consumes it.  Previously `WM_PAINT` supplied only the current
held mask to every missed frame; a quick X press/release between paints could
therefore disappear before user-inbound state `$0D` evaluated its A-button
rising edge.  The first catch-up frame now receives the latched edge plus the
held mask, subsequent frames receive only the held mask, and the unchanged C
dispatcher performs `$0D->$05` pass release.  A live rebuilt-window check
reached the left-baseline user inbound and a single synthesized X tap returned
the formation to live play (`captures/native-gameplay/inbound-input-latch-live.jpg`).

The later common-inbound regression was a separate native phase bug.  Fixed
bank `$8EE2->$8F0B` installs player state `$0D`, then selected bank 0 `$A780`
(bank offset `$2780`, ROM file offset `$2790`) owns the direction+A input,
`$A129` receiver selection, and `$A21F/$A482` pass handoff.  Variant three
correctly reached `$0D` after a CPU-last-touch rule award, but the portable
scene remained `DD_GAMEPLAY_INBOUND`; that scene branch returned through
`dd_step_inbound` before the native `$A780` handler could run.  The handoff now
returns to the live dispatcher when `$0D` is installed.  The symmetric rule
regression is bounded to 400 frames, rejects route-axis reversals, presses
right+A, and follows the ball through `$AD41->$AD58` reception with ownership
and user control on the selected teammate.  Confidence is high: the state and
input chain is direct from the Ghidra export, and the pre-fix test reaches `$0D`
but cannot select a receiver while the post-fix test completes the entire pass.

The original deliberately starts dribble state `$01` through `$8E88->$AD0E`
when the inbounder claims the loose scoring ball. Native code retains that
dribble during `$2F/$30`; `$9018` changes it to attached release state `$00`,
and `$8FE0` launches pass state `$02` at countdown four. This is separate from
the fixed reception bug: `$AD4E-$AD56` selects the expanded CPU reception from
`$002C` and possession bit `$0050.3`, even though the made-basket sequence is
still using the live dispatcher. Native reception now follows that condition,
runs `$AD6D`'s role/link swap and `$38/$3C/$3E` route restoration, then assigns
the receiving role-zero player state `$25` and ball state `$01`. The opposing
user is restored by role through `$8F8D->$9097`, not by assuming physical slot
zero.

Gameplay rendering no longer emulates the NES eight-sprites-per-scanline
secondary-OAM dropout. Every bounded DDAP metasprite record is drawn during
live native play, so clustered players remain complete. That change exposed a
native-only bug in the shared intro emitter: signed off-court coordinates were
cast to OAM bytes, so individual records could wrap into the fixed scoreboard.
The gameplay emitter now decodes the same bounded DDAP record stream with signed
coordinates, culls player anchors above logical court line `$60`, and clips
remaining records at the HUD raster before writing OAM. The limit remains
on for the reference tip-off renderer because its acceptance captures explicitly
verify the original overflow pattern. Regressions both union three same-scanline
players and sweep every projected base Y while requiring the HUD/raster pixels to
remain identical to a no-object baseline.

User loose-ball acquisition follows the fallthrough that was missing from the
first contest translation. At `$A3E2-$A426`, rebound/loose ball states `$07/$09`
enter `$A607` only when the mutable `$0580` opponent is in `$26/$27/$03`. If that
paired-state test fails, execution continues at `$A42D`: contact result three
calls `$B435`, then ordinary success reaches `$A44B`'s possession reset. Native
`dd_try_user_loose_ball_pickup` now performs that ground-contact fallback, so an
unpaired loose ball becomes user-owned dribble state `$01` without requiring a
button edge. The jump/apex branch remains unchanged for linked shooter contests.

Reproduce the rebuilt ignored screenshots with:

```powershell
.\tools\ghidra\Run-GameplayLoopAnalysis.ps1
.\build.ps1 -RomPath 'F:\Games\NES\Double Dribble\Double Dribble (USA) (Rev 1).nes'
.\build\double_dribble_port.exe --render-gameplay-shot .\build\double-dribble.assetpack make chase .\captures\native-post-score-fix\run-to-ball.bmp
.\build\double_dribble_port.exe --render-gameplay-shot .\build\double-dribble.assetpack make pickup .\captures\native-post-score-fix\inbound-hold.bmp
.\build\double_dribble_port.exe --render-gameplay-shot .\build\double-dribble.assetpack make receive .\captures\native-post-score-fix\post-inbound-reception.bmp
```

Coverage remains **92.6% unrounded (93% displayed)** and match rules remain
**80.8%**. These corrections close defects inside already-verified dispatcher
states rather than adding new states to the denominator.

### Rule calls, CPU shot gating, native dunk, and bounce audio

Status: **implemented and regression-verified**, including the specialized
dunk cinematic as native asset-pack data.

The out-of-bounds follow starts in switched bank 0 at `$95E0` (ROM `$15F0`).
The boundary branch reaches `$9635` (ROM `$1645`), stores reason `$16`, and
enters `$9651` (ROM `$1661`). That common setup calls `$9395`, XORs possession
bit `$0050` by `$48`, resets formation roles through fixed `$D6BD`, finds the
new receiving role zero through `$9097`, and selects the boundary target through
`$9763`. Back pass reaches the same setup from fixed `$9583/$95C4-$95CD` with
reason `$15`. Native setup now derives the receiver from the offending or
last-touch team, then writes the corresponding portable possession direction;
camera direction can no longer accidentally award the throw-in to the offender.

The message handler at bank-0 `$94A5` (ROM `$14B5`) reads `$0059`, toggles its
high bit every four frames, and clears it after `$006B` reaches `$28`. A
controlled FCEUX rule-`$16` capture writes `$0059=16` at PC `$9635`, installs
state `$41` at `$965A`, and visibly renders `OUT OF BOUNDS`; the existing
rule-`$15` capture renders `BACK PASS`. The native renderer gives gameplay the
full 64-pixel green HUD, clears the 20-tile message span, and reproduces that
160-frame flash. The prior 48-pixel split and invented period ordinal were the
reason the message box was clipped and overwritten.

The CPU repair follows fixed-bank `$D759` (ROM `$1D769`). That decision tree
forces a shot only in the last five clock seconds or when the possession tick
reaches 24; otherwise it classifies the packed court region, checks arrival and
decision timers, and may search for a pass. The earlier native post-inbound
route bypassed this policy and shot unconditionally after 14 updates, even from
half court. It now returns through the translated policy, with only a bounded
near-rim lane allowed to enter a direct finish. State `$32` also consumes the
high phase bit as a window instead of requiring host frame exactly `$80`, so a
skipped host tick cannot freeze a CPU player indefinitely. Tests start after
`$80` and prove the setup exits, and place a carrier at half court and prove it
retains dribble instead of launching a shot.

A controlled close-rim original run starts the shot at ball packed position
`$01B4/$57`, then uniquely executes fixed `$D40F-$D428` and `$D5F9-$D60C` plus
switched bank 2 `$8000-$8287`. Headless Ghidra shows `$D40F` calling `$D5F9`,
advancing subcounter `$003E` to `$0E`, advancing cinematic frame `$0039`, and
requesting `$1D` or `$1A` at frame five before clearing objects `$0D-$0F` and
returning to match state `$0E`. Bank-2 `$808E` iterates those three objects,
loads metasprite pointers from `$90C4`, calls the `$801A` decoder, and fills
unused OAM with `$F4`. DDAP v21 resolves `$C549`'s bank/pointer table while
building the mandatory asset pack, applies `$D403/$D409` plus the six `$D501`
stage streams, copies the three records per stage from the six `$D55A` variant
pointers, and expands them through native `$808E/$801A/$90C4` logic. The 36
resulting PPU/OAM frames have no runtime ROM dependency. Native `$D40F` timing
displays each of the six frames for fourteen ticks, requests `$1A/$1D` at the
stage-five boundary, suspends the ordinary player scheduler while the
cinematic owns the match dispatcher, then returns make/miss results to the
normal score/net or `$AF72` loose-ball flow.

The ball sound follow is exact at the portable dispatcher boundary. Bank 0
`$AEC3-$AEC8` (ROM `$2ED3`), `$AF66-$AF6B` (`$2F76`), and `$AFF7-$B001`
(`$3007`) each request event `$0A` on their first bounce/landing wrap;
`$AF72-$AF83` (`$2F82`) requests event `$14` for the loose launch. Native ball
states now issue those same event IDs. Event `$0A` uses the already recovered
DDAP v20 `gameplay.audio` stream from switched bank 1, and every asset-pack
build exports the exact native playback path as `build/ball-bounce-0a.wav` for
audible and waveform inspection. No ROM audio or WAV is committed.

Reproduce the ignored evidence with:

```powershell
.\tools\ghidra\Run-GameplayLoopAnalysis.ps1
.\tools\ghidra\Run-DunkAnalysis.ps1
.\tools\fceux\Capture-TipoffGameplay.ps1 -TraceStart 2588 -FinalFrame 2660 -CaptureName evidence-rule-16 -InboundRuleFrame 2600 -InboundRuleCase 4 -DisablePcCounts
.\build.ps1 -RomPath 'F:\Games\NES\Double Dribble\Double Dribble (USA) (Rev 1).nes'
.\build\double_dribble_port.exe --render-gameplay-rule .\build\double-dribble.assetpack oob .\captures\native-rules-dunk\out-of-bounds.bmp
.\build\double_dribble_port.exe --render-gameplay-rule .\build\double-dribble.assetpack backpass .\captures\native-rules-dunk\back-pass.bmp
.\build\double_dribble_port.exe --render-gameplay-dunk .\build\double-dribble.assetpack make airborne .\captures\native-rules-dunk\dunk-airborne.bmp
.\build\double_dribble_port.exe --render-gameplay-dunk .\build\double-dribble.assetpack make result .\captures\native-rules-dunk\dunk-make.bmp
.\build\double_dribble_port.exe --render-gameplay-dunk .\build\double-dribble.assetpack miss result .\captures\native-rules-dunk\dunk-miss.bmp
```

Coverage remains **92.6% unrounded (93% displayed)**: player and ball action
dispatchers are **100%**, core loop is **85.7%**, and match rules are **80.8%**.
These changes repair behavior inside already-catalogued Verified or Partial
entries; the honest score does not rise merely because the same states gained
more faithful side effects.

### Comprehensive completion plan and first visible-continuity repairs

`PORTING_PLAN.json` is the machine-readable execution contract for the final
portable-completion goal. It distinguishes the existing **92.6% catalogued**
score from comprehensive coverage, which remains intentionally unset until an
all-bank executed-PC and Ghidra call-graph inventory proves that every reachable
non-NES routine is represented. The plan defines six ordered phases, evidence
requirements for every routine, milestone commit rules, explicit NES-only
exclusions, and the zero-Partial/zero-Missing exit gate.

The first visible repair removes a native-only whole-object cull in
`dd_gameplay_emit`. Signed per-record clipping already prevents negative
metasprite coordinates from wrapping into the fixed HUD; additionally rejecting
every player whose anchor was above `$60` made otherwise visible lower records
disappear between the 64-pixel court raster and Y `$5F`. The renderer now clips
each record at the raster and keeps the visible portion. A framebuffer sweep
covers every anchor Y from -32 through 255, requires an object to remain visible
in the `$40-$5F` interval, and still proves that no record modifies the HUD.

The user tip-off pose is grounded in a fresh controlled FCEUX capture. With NES
B pressed at original frame 2502, object `$02` changes `$0342:10->11` at bank-0
PC `$A61F`. `$A896` records facing `$00`, metasprite `$1C` on the input frame,
then metasprite `$22` at frame 2504 and throughout the height-script rise.
Object `$07`, facing `$04`, independently uses `$21`. Native code had assigned
the CPU's `$21` pose to the user, which visually faced backward; it now selects
the recovered `$22` pose and a regression pins action, facing, and animation.

Finally, configuration values now become match state instead of renderer-only
UI. `dd_gameplay_configure` validates and stores TIME, TEAM, and LEVEL, reads
the pack-backed bank-1 `$A368` time table (`$05,$10,$20,$30`), and preserves the
chosen values across the match. Ghidra `$A54C->$A6BD` shows `$0482` as the 1P
team and `$0483` as the fixed CPU team. The original frame-2500 PPU bytes are
`02 20 17 0F | 02 20 36 0F | 02 29 17 0F | 02 29 36 0F`, while object
attributes `$0312-$031B` are `00 00 01 00 01 03 02 03 03 02`: user objects
select palettes 0/1 and CPU objects select 2/3. Native rendering now replaces
only `$3F11/$3F15` with the selected `$0482` jersey color and
`$3F19/$3F1D` with fixed `$0483`, preserving the gameplay skin/white entries.
The former whole-palette copy wrote the selection into CPU palettes 2/3 and
caused the exact reversed/weird-color symptom. Isolated framebuffer regressions
require a 1P player to change and a CPU player to remain identical.

Reproduce the ignored evidence and native frames with:

```powershell
.\tools\fceux\Capture-TipoffGameplay.ps1 -TraceStart 2495 -FinalFrame 2535 -CaptureName phase1-tip-facing -JumpStart 2502 -JumpEnd 2502 -JumpButton B -DisablePcCounts
.\build.ps1 -RomPath 'F:\Games\NES\Double Dribble\Double Dribble (USA) (Rev 1).nes'
.\build\double_dribble_port.exe --render-gameplay-team .\build\double-dribble.assetpack 0 .\captures\phase1-native\team-new-york.bmp
.\build\double_dribble_port.exe --render-gameplay-team .\build\double-dribble.assetpack 3 .\captures\phase1-native\team-los-angeles.bmp
.\build\double_dribble_port.exe --render-gameplay-top-edge .\build\double-dribble.assetpack .\captures\phase1-native\top-edge.bmp
.\build\double_dribble_port.exe --render-gameplay-tip-jump .\build\double-dribble.assetpack .\captures\phase1-native\tip-jump.bmp
```

This milestone deliberately leaves catalogued coverage at **92.6%**. Natural
dunk eligibility, complete blocking, exhaustive CPU audit, free throws, and the
comprehensive routine manifest remain active phases in `PORTING_PLAN.json`.

The next correction replaces the native dunk radius with the original literal
eligibility gate. Headless Ghidra at bank-0 `$B189-$B1DC` (ROM
`$3199-$31EC`) shows `$AB53` packing the current shooter into `$05B0+X`.
Original user slots `$02-$06` enter the special finish only for packed cells
`$BA/$BB/$9C/$9D`; CPU slots `$07-$0B` use `$A5/$A4/$83/$84`. On a match,
`$B1B9` stores `$003B=1`, `$B1BD` selects main state `$002A=0C`, and `$B1D3`
chooses one of the side-specific animation variants before ordinary shot-vector
initialization continues at `$B1DE`. The former C radius covered only the center
of that approach lane, so natural attempts from valid outer cells silently
became jump shots. Native code now compares the exact packed byte. Regression
tests exercise all eight accepted cells through shipping user-B or CPU-state
`$26` paths and prove adjacent `$B9/$A6` remain ordinary shots. The native dunk
capture starts from `$BA`, a cell deliberately outside the removed radius, so a
successful screenshot also proves this repair rather than the old synthetic
center case.

A later natural-play regression found one remaining ordering error in that
translation. `$AA75` (the initial B press) only installs user state `$03` and
attached ball state `$04`; `$A504->$A84C` continues the already-installed
takeoff vector while B is held. The literal cell test does not execute until
`$B189`, after B is released. The C port had sampled it at `$AA75`, so a player
starting in rejected cell `$B9` could visibly travel into `$BA` in the air but
still receive an ordinary shot. Eligibility now runs at the shared user-release
or CPU-apex boundary. The deterministic test begins in `$B9`, retains the exact
rightward takeoff vector for eight frames, releases in `$BA`, and requires the
special finish to activate; all eight literal cells and both adjacent-cell
rejections remain covered. `tools/Capture-NativeDunk.ps1` regenerates ignored
make/miss gather, airborne, and result screenshots through that same running
entry. The subsequent DDAP v21 reconciliation closes the close-up gap: the
same shipping running entry selects one of the six `$003A` variants and renders
the ROM-derived six-stage bank-2 presentation.

The defensive follow-up corrects a separate native approximation in player
states `$20/$22`. Ghidra at `$8A16` calls `$9102`, which loads the opponent
through mutable link `$0580+X`; it never assumes the arithmetic player five
slots away. When the current rotating object remains separated, `$8A28` calls
`$90B3` to derive a vector to the linked player's `$914E` projected collision
anchor, `$8BF8` scales both 8.8 components to 5/4, and `$D98A->$A84C` performs the
shared double integration. The former port used a 20-unit basket-side offset
and, outside inbound, ignored `$0580`. That target could not enter `$9102`'s
combined four-unit contact box, leaving defenders unlatched and poorly placed
for a block. Native state `$20` now follows the mutable opponent anchor at
the resulting 2.5-unit scheduled cadence; state `$22` and its release check
use the same link.

Regression coverage deliberately changes the link from player 5 to player 2,
proves the tracker moves toward player 2 rather than player 0, then proves
`$9102` latches `$20->$22` only at the linked contact. The native user-block
capture enters through shipping X/NES-A at `$A3E2->$A607`, reaches the exact
apex-only `$A638->$A6C3` collision, awards ball state `$00` to the user, and
then shows the `$A693->$92BD->$A44B` landing transfer. Its controlled collision
fixture uses the same four-unit height/X boxes as the original FCEUX block
probe; it does not enlarge the hit area.

Reproduce the native contact/landing pair with
`tools\Capture-NativeUserBlock.ps1` after a normal asset-pack build.

## Open research questions

### Exhaustive routine inventory gate

`tools/ghidra/Run-PortableGameplayInventory.ps1` now seeds all 34 player,
18 user, and 13 ball dispatch targets plus match/rule/CPU roots, recursively
walks direct calls and tail calls in Ghidra, and generates
`GAMEPLAY_ROUTINES.json`. The first graph contains **181 bank-0 routines and
42 fixed-bank routines (223 total)**. The build validates unique bank/address
keys and the manifest's summary counts.

The first boundary review classifies **216 nodes as portable** and excludes
only **7 fixed-bank bank-switch/APU register-driver nodes** whose gameplay-
visible event selection is represented by their portable callers. Portable
score/HUD, animation, timing, audio-event selection, and CPU routines remain
in scope. All 216 included records deliberately begin Partial, producing a
conservative comprehensive score of **50.0%**. Promotion now requires explicit
Ghidra/FCEUX evidence, native symbols, and regression annotations per routine.
The older 92.6% number remains visible only as the bounded, hand-catalogued
baseline. This prevents dispatcher counts from concealing uncatalogued helper
routines or treating a broad playable approximation as Verified.

The first annotation pass promotes the **31 unique handler entries** behind
player states `$20-$41` and the **12 unique handler entries** behind ball
states `$00-$0C`. `GAMEPLAY_ROUTINE_ANNOTATIONS.json` ties those 43 nodes to
their dispatcher evidence, native `dd_update_cpu_player` / `dd_step_ball`
symbols, and the state-by-state regression suite. This raises the comprehensive
routine score from its 50.0% Partial floor to **60.0% (43 Verified, 173 Partial,
7 excluded)**. Shared helpers and user/rule/free-throw handlers do not inherit
credit merely because a verified dispatcher calls them.

### Original block audio `$10/$20`

The CPU jump-contact cue follows bank-0 `$8B12`: after `$A6C3` succeeds,
`$8B27-$8B2E` records the contact and loads `$10` before `$8B30` calls the
portable sound selector `$C141`. A fresh natural FCEUX run records event `$10`
for object `$07` at frames 2606 and 2644. The corresponding switched-bank-1
streams at `$87A4/$87AD` produce four native audio frames: pulse/noise
`384/13`, silence, `336/8`, silence, with the recovered duties and volumes.

The user cue follows `$A638->$A6C3`. Only the zero byte at the top of the signed
height script reaches the collision helper; successful contact stores the
user as owner and `$A68A-$A68C` requests `$20`. The controlled FCEUX contact at
frame 2766 records event `$20` for object `$02`. Bank-1 streams `$87DD` and
`$866B` supply the thirteen-frame descending pulse and initial noise transient.
DDAP v20 carries both sequences as `gameplay.audio.cpu.block` and
`gameplay.audio.user.block`; Win32 maps events `$10/$20` to generated PCM, and
the gameplay regression compares every packed event exactly. These two fully
evidenced helpers raise comprehensive recursive coverage to **60.4% (45
Verified, 171 Partial, 7 excluded)**.

Reproduce the ignored proof with:

```powershell
.\tools\ghidra\Run-GameplayAudioAnalysis.ps1
.\tools\fceux\Capture-TipoffGameplay.ps1 -TraceStart 2580 -FinalFrame 2660 -CaptureName phase1-block-audio -JumpStart 2502 -JumpEnd 2502 -JumpButton B -PassFrame 2600 -PassEnd 2600 -PassButton B -BlockFrame 2606 -DisablePcCounts
.\tools\fceux\Capture-TipoffGameplay.ps1 -TraceStart 2738 -FinalFrame 2785 -CaptureName phase1-user-block-audio -PassFrame 2748 -PassEnd 2748 -PassButton A -UserBlockFrame 2766 -DisablePcCounts
.\build.ps1 -RomPath 'F:\Games\NES\Double Dribble\Double Dribble (USA) (Rev 1).nes'
```

The build writes ignored `build/cpu-block-10.wav` and
`build/user-block-20.wav`; generated WAVs, captures, ROM data, and Ghidra
projects remain outside version control.

## Installed route, formation, and extended-arrival closure

Status: **implemented and routine-verified**. Headless Ghidra resolves the
movement chain as `$ABCD->$9D2D/$AA98/$9BB0`, optional
`$8BF8->$8C02`, and `$D98D->$A84C->$9CF6/$9CA0`. `$8C02` performs a
wrapping arithmetic quarter shift and adds it to the original signed 8.8
component, so the selected route families use **5/4**, not the former 3/4
approximation. `$A84C` integrates depth twice and longitudinal position twice;
legal integer endpoints retain their fractional bytes through `$01F1.FF` and
`$98.FF`.

State `$20` follows `$8A16->$9102/$90B3`. `$914E` projects the paired
player toward the active hoop by 16 units for the controlled object or 32 for
the other objects and stores that collision anchor in `$0490/$04A0/$04B0`.
The native defender now targets and tests the mutable `$0580` pair's projected
anchor, installs its `$ABCD` vector, applies `$8BF8`, and consumes it through
the shared doubled integrator. Exact native regressions reproduce original
frames 2558/2559, including player positions `$010E26/$21AA`,
`$01081E/$5782`, `$0123C4/$3158`, and `$011C08/$7064`.

Inbound state `$41` follows its distinct `$8C6B-$8CE8` path: `$D978`
compares the extended packed bytes, `$ABCD` refreshes only for rotating
priority object `$004D`, and the vector remains unscaled. The FCEUX trace keeps
target `$0121`, clamps at depth `$98A0`, and claims the ball only when the
priority refresh observes the destination cell. Free-throw setup likewise
copies the ball's fixed-point and packed coordinates through the recovered
`$85A8-$85B4` target override. Tests now cover this pickup/release/reception
chain, both free-throw shooters, legal fractional bounds, and a four-period
soak that reaches GAME SET with CPU passes, shots, and inbounds. Six additional
recursive routines are Verified, raising comprehensive coverage to **89.8%
(172 Verified, 44 Partial, 0 Missing; 7 NES mechanisms excluded)**.
The new ignored original/native captures differ by 4.5061% at original
frame 3501/native 1290 (pickup) and 5.0694% at original frame 3572/native 1396
(reception); the remaining object-placement delta is tracked by the 44 Partial
routines rather than being hidden by the coverage promotion.

## Final reachable portable-routine closure

The last headless Ghidra inventory pass closes the 44 nodes that remained after
the installed-route milestone. They divide into four bounded families:

| Family | Original entries | Native proof |
| --- | --- | --- |
| Dispatcher wrappers, shared tails, and no-op entries | `$825F,$89B2,$8D99,$A014,$A0B2,$A0C3,$A0D0,$A0D9,$A280,$A500,$A555,$A556,$A624,$AC83` | exact dispatcher transitions/no-change exits exercised by the state suite |
| Live user pass, shot, contest, and inbound flow | `$A0DA,$A1CC,$A21F,$A37D,$A3E2,$A482,$A504,$A557,$A5D0,$A607,$A6F4,$A712,$A765,$A780,$A7D4,$AA75` | shipping input paths, ownership transfer, held-shot release, jump/block, landing, pickup, and inbound regressions |
| Shot/pass attachment and initializers | `$A7EA,$AD0E,$B0AB,$B0B8,$B11F,$B189,$B280,$B2A0,$B3E9` | byte-exact vectors, hand offsets, result classification, animation attachment, and make/miss tests |
| Portable fixed-bank effects | `$C02B,$C141,$D368,$D3C4,$D3D5` | entropy update, asset-pack audio request, and gameplay-visible camera/scroll state checks; hardware register writes stay excluded |

The `$C02B` regression additionally proves the exact accumulator effect
`$0063 = $0063 + $001A + 1` (`$12 + $20 + 1 = $33`). The annotation generator
rejects unknown, duplicate, excluded, or overlapping routine keys, and the
coverage script rejects inconsistent routine classifications or summary counts.

The current address inventory and its reproducible scoring rules are maintained
in `GAMEPLAY_COVERAGE.md`. The manifest currently catalogs 216 portable
routines, but independent review found omitted gameplay roots, so its discovery
denominator remains open. The within-manifest verified routine score is **99.8%
(215 Verified, 1 Partial, 0 Missing; 7 NES mechanisms excluded)**. Mapper/bank switching, PPU/CHR/OAM mechanics, APU
register driving, and hardware-only interrupt timing remain outside the
denominator.

### `$A3E2` controlled-steal coverage correction

A branch-level review prompted by the inability to steal during live defense
invalidates the earlier Verified classification for `$A3E2` (bank 0, bank
offset `$23E2`, ROM file offset `$23F2`). The original first classifies ball
states at `$A3EB-$A40A`, then applies the `$001D/$0056` gates. A paired opponent
in action `$26/$27/$03` takes `$A426->$A607`, the jump-contest path already in
C. Otherwise `$A42D` calls collision helper `$B435` with radius 3. Contact
continues through `$A347` for the exceptional foul test and `$A44B` for the
ordinary possession/role transfer.

The current C helpers only allow normal dribble `$01` to reach the paired
jump-contest gate; the `$A42D` fallback is limited to loose/rebound ball states.
There is also no shipping-input regression with the controlled defender beside
a normally dribbling CPU carrier. `$A3E2`, core user control, and the defensive
steals/blocks match capability are therefore Partial until that path and its
foul/gate variants are implemented and dynamically reconciled. With the broader
CPU-choice capability also held Partial, the resulting catalog-weighted gameplay
score is **96.7%**, and match rules are **92.3%**. These are catalog scores, not
claims of whole-game parity while the address denominator remains incomplete.

The live offense freeze reconciliation follows the state gate rather than the
ball owner alone. During `$2E->$2F`, the rebound winner legitimately owns a
dribbling ball while the other nine objects remain in formation state `$37`.
The former C input gate saw only `carrier == controlled` plus ball `$01`, so an
A/B edge could interrupt `$2F`, create a pass/shot, and strand the formation.
Native A/B dispatch now additionally requires controlled player state `$02`,
the live user-carrier state. A deterministic frame-743 regression rejects the
premature input; a frame-803 shipping inbound then proves all five CPU defenders
resume updates and move while user offense is live.

## CPU free-throw delay and LEVEL policy closure ($883A-$884F)

Status: **implemented and routine-verified**. Headless Ghidra and FCEUX dynamic traces
resolve the bank-0 CPU free-throw release decision chain at `$8832-$884F` (ROM file offsets
`0x0842-0x085F`), with shot setup through `$884F-$8884` (ROM file offsets `0x085F-0x0894`).

### Recovered 6502 Flow

| Address | Bank | ROM Offset | Disassembly | Effect |
|---|---|---|---|---|
| `$8832` | 0 | `0x0842` | `LDA $0067` | Load free-throw delay countdown |
| `$8834` | 0 | `0x0844` | `BEQ $883A` | If delay is zero, evaluate policy immediately |
| `$8836` | 0 | `0x0846` | `DEC $0067` | Decrement delay timer |
| `$8838` | 0 | `0x0848` | `BNE $884C` | If decremented timer != 0, hold in action `$46` via `$D990` |
| `$883A` | 0 | `0x084A` | `LDA $07E8` | Load gameplay LEVEL (0, 4, 8 in 1P mode; higher in progression) |
| `$883D` | 0 | `0x084D` | `CMP #$09` | Unsigned comparison with 9 |
| `$883F` | 0 | `0x084F` | `BCS $8845` | If LEVEL >= 9, bypass phase check -> require exact center aim `$60` |
| `$8841` | 0 | `0x0851` | `LDA $001A` | Load global frame / phase counter |
| `$8843` | 0 | `0x0853` | `BPL $884F` | If signed `$001A >= 0` (bit 7 == 0, 50% of frames), SHOOT immediately |
| `$8845` | 0 | `0x0855` | `LDA $033C` | Load hoop aim indicator position (`$50..$60`) |
| `$8848` | 0 | `0x0858` | `CMP #$60` | Check if aim indicator is centered at hoop (`$60`) |
| `$884A` | 0 | `0x085A` | `BEQ $884F` | If aim == `$60`, SHOOT |
| `$884C` | 0 | `0x085C` | `JMP $D990` | Hold in state `$46` (no shot this dispatch) |
| `$884F` | 0 | `0x085F` | `LDX $004B` ... | Initialize shooter state `$47`, height script `$9B34`, ball `$04` gather |
| `$887D` | 0 | `0x088D` | `JSR $B503` | Clear motion velocities |
| `$8880` | 0 | `0x0890` | `LDA #$00; STA $0056` | Clear score-return / contact gate `$0056 = 0` |
| `$8884` | 0 | `0x0894` | `JMP $D98D` | Exit object dispatcher |

### Behavioral Summary
1. **LEVEL `< 9` vs `>= 9`**:
   - For `LEVEL < 9` (Level 1 = 0, Level 2 = 4, Level 3 = 8): when global phase `$001A` has bit 7 clear (`$00`..`$7F`), the CPU releases immediately on timer expiry, causing realistic variable accuracy depending on aim position `$033C`. When bit 7 is set (`$80`..`$FF`), the CPU waits for exact aim `$60`.
   - For `LEVEL >= 9`: the CPU skips the phase check via `$883F BCS` and strictly waits until aim `$033C == $60` on every frame, guaranteeing 100% free-throw accuracy on high/expert levels.
2. **Delay Timer `$0067`**: Initialized to `0x30` on entering action `$46`. Decrements once per scheduled 30 Hz dispatch; falls through to the policy check when reaching zero.
3. **Score/Contact Gate `$0056`**: Set to `0xFF` on foul entry; cleared to `0` at `$8882` on shot initiation.

### FCEUX Dynamic Verification & Native Tests
- Controlled FCEUX traces verify all 5 matrix permutations and timing transitions:
  - `cpu-ft-level8-pos` (frame 2601): LEVEL 8, phase `$10` (`>=0`), aim `$52` (`!=$60`) -> executes `$883A->$8841->$884F->$8882`.
  - `cpu-ft-level8-neg-hold` (frame 2601): LEVEL 8, phase `$90` (`<0`), aim `$52` (`!=$60`) -> executes `$883A->$8841->$8845->$884C` (holds in `$46`).
  - `cpu-ft-level9-pos-hold` (frame 2601): LEVEL 9, phase `$10` (`>=0`), aim `$52` (`!=$60`) -> executes `$883A->$8845->$884C` (bypasses `$8841`, holds in `$46`).
  - `cpu-ft-level9-aim60-full` (frame 2631): LEVEL 9, aim reaches `$60` -> executes `$883A->$8845->$884F->$8882`.
  - `cpu-ft-delay-timer2` (frames 2601/2603): timer decrements `2->1` (holds via `$8838 BNE`), then `1->0` (shoots on reach zero).
- Native C test suite (`check_cpu_free_throw_level_policy` in `tests/dd_gameplay_cpu_test.c`) verifies the full 9-point assertion matrix, menu configuration mapping (`0->0, 1->4, 2->8`), `$0067` delay transitions, `$0056` gate clearing, and ball flight to rim collision.

Future fidelity work may compare the native PCM mix and title/attract-mode
presentation against the NES, but it does not represent missing reachable
portable gameplay behavior in this inventory.

## Ordinary User Live-Dribble Stealing and Possession Transfer ($A3E2-$A4FF)

Status: **implemented and routine-verified**. Headless Ghidra and FCEUX dynamic traces resolve the complete Bank-0 user defense live-dribble stealing and possession transfer chain at `$A3E2-$A4FF` (ROM file offsets `0x23F2-0x2513`).

### Recovered 6502 Flow

| Address | Bank | ROM Offset | Disassembly | Effect |
|---|---|---|---|---|
| `$A3E2` | 0 | `0x23F2` | `LDX $004B; LDA $0340` | Load active player object index into X and ball state into A |
| `$A3E5` | 0 | `0x23F5` | `CMP #$09; BEQ $A402` | Ball state `$09` (loose pickup) -> bypass button check |
| `$A3E9` | 0 | `0x23F9` | `CMP #$07; BEQ $A402` | Ball state `$07` (rebound) -> bypass button check |
| `$A3EB` | 0 | `0x23FB` | `CMP #$01; BEQ $A3FF` | Ball state `$01` (dribble) -> check controller button |
| `$A3F0` | 0 | `0x2400` | `CMP #$04; BEQ $A3FF` | Ball state `$04` (gather) -> check controller button |
| `$A3F5` | 0 | `0x2405` | `CMP #$05; BEQ $A3FF` | Ball state `$05` (airborne) -> check controller button |
| `$A3FA` | 0 | `0x240A` | `CMP #$06; BNE $A3DF` | Other ball states (`$00, $02, $03, $08, $0A, $0B, $0C`) reject |
| `$A3FF` | 0 | `0x240F` | `LDA $0680,X; ASL A; BCC $A3DF` | Check bit 7 of held controller input (`$0680,X` - Button A). If not held, reject |
| `$A40A` | 0 | `0x241A` | `LDA $001D; BNE $A3DF` | If contact lock `$001D != 0` (e.g. post-tipoff 32-frame freeze), reject |
| `$A40E` | 0 | `0x241E` | `LDA $0056; BNE $A3DF` | If score-return / dead-ball gate `$0056 != 0`, reject |
| `$A412` | 0 | `0x2422` | `LDA $0580,X; TAX; LDA $0300,X` | Load paired opponent action |
| `$A41A` | 0 | `0x242A` | `CMP #$26; BEQ $A426` | If paired opponent in action `$26` (carrier route) -> jump contest |
| `$A41E` | 0 | `0x242E` | `CMP #$27; BEQ $A426` | If paired opponent in action `$27` (carrier decide) -> jump contest |
| `$A422` | 0 | `0x2432` | `CMP #$03; BEQ $A426` | If paired opponent in action `$03` (takeoff) -> jump contest |
| `$A426` | 0 | `0x2436` | `JMP $A607` | Redirect to jump contest entry |
| `$A42D` | 0 | `0x243D` | `LDA #$03; JSR $B435; BCS $A3DF` | Test 10px longitudinal and depth radius collision with ball. If no overlap, reject |
| `$A436` | 0 | `0x2446` | `JSR $A347` | Test exceptional foul `$1A` (`$0025 == 0`, ball `$01`, facings match) |
| `$A439` | 0 | `0x2449` | `LDX $004B; CPX $005B; BNE $A44B` | Check if current player X == ball owner `$005B`. If equal, turnover violation |
| `$A43F` | 0 | `0x244F` | `LDA #$2C; JSR $C141` | Play whistle SFX `$2C` |
| `$A444` | 0 | `0x2454` | `LDA #$0F; STA $0065; JMP $9645` | Set inbound reason `$0F` and enter common inbound setup |
| `$A44B` | 0 | `0x245B` | `LDX $004B; STX $0048; STX $005B` | Transfer possession: set carrier `$0048` and ball owner `$005B` to winner X |
| `$A456` | 0 | `0x2466` | `LDA #$40; STA $0055` | Flip possession direction to `$40` (Team 1 heading right) |
| `$A460` | 0 | `0x2470` | `LDA #$00; STA $0056` | Clear score-return / dead-ball gate `$0056 = 0` |
| `$A468` | 0 | `0x2478` | `LDA #$02; STA $0300,X` | Set winner action to `DD_PLAYER_LIVE_USER_CARRIER` (`$02`) |
| `$A46F` | 0 | `0x247F` | `JSR $99D9; JSR $9A31` | Swap role zero and reciprocal paired links (`$0580`) if winner was not role 0 |
| `$A482-$A4B7` | 0 | `0x2492-0x24C7` | Team 1 teammate loop | Set winner to `$02`, other four teammates to `DD_PLAYER_LIVE_TEAMMATE` (`$20`) |
| `$A4B8-$A4F1` | 0 | `0x24C8-0x2501` | Team 2 defensive loop | Set CPU roles 0,1,2 to `$40` (`DD_PLAYER_LIVE_CPU`), role 3 to `$3C` (`DD_PLAYER_LIVE_CPU_CUT`), role 4 to `$3E` (`DD_PLAYER_LIVE_CPU_ROUTE`) |
| `$A4F2-$A4FF` | 0 | `0x2502-0x250F` | Height and ball reset | Reset all 10 player heights to `$10` (`0x1000`), clear ball velocities, set ball action `$01` (`DD_BALL_DRIBBLE`) |

### Behavioral Summary
1. **Held Button Input Gate (`$A404-$A408`)**: Uses `$0680,X` (held input), not single-frame edge `$0670,X`. Bit 7 (Button A) must be set for live-dribble stealing.
2. **Phase / Gate Protections (`$A40A-$A410`)**: Requires `$001D == 0` (contact lock countdown) and `$0056 == 0` (score gate).
3. **Collision Detection (`$B435`)**: Evaluates a 10-pixel longitudinal and depth boundary radius around the ball. Distance <= 10 pixels succeeds; distance >= 11 pixels rejects.
4. **Turnover & Exceptional Foul Helpers (`$A347, $A439`)**: Exceptional foul `$1A` triggers if `$0025 == 0`, ball is `$01`, and facings match. If defender is already the ball owner, whistle `$2C` and turnover inbound reason `$0F` are triggered.
5. **Team Formation & Role Reassignment (`$A44B-$A4FF`)**: Assigns stealer to `DD_PLAYER_LIVE_USER_CARRIER` (`$02`), teammates to `DD_PLAYER_LIVE_TEAMMATE` (`$20`), CPU defense to `$40/$3C/$3E`, swaps role zero / reciprocal links, and clears ball velocities.

### FCEUX Dynamic Verification & Native Tests
- Controlled FCEUX traces verify all 11 execution branches:
  - `user-steal-success` (frame 2600): Executes full `$A3E2 -> $A402 -> $A40A -> $A42D -> $B435 -> $A347 -> $A44B -> $A460 -> $A478` transfer.
  - `user-steal-no-button` (frame 2600): Button A not held -> rejects via `$A404 ASL/BCC`.
  - `user-steal-lock1d` (frame 2600): Contact lock `$001D != 0` -> rejects via `$A40A BNE`.
  - `user-steal-gate56` (frame 2600): Score gate `$0056 != 0` -> rejects via `$A40E BNE`.
  - `user-steal-wrong-ball` (frame 2600): Ball state `$00` -> rejects via `$A3EB BNE`.
  - `user-steal-collision-miss` (frame 2600): Outside collision radius -> rejects via `$A434 BCS`.
  - `user-steal-boundary-in` (frame 2600): Distance 10px -> succeeds inside `$B435`.
  - `user-steal-boundary-out` (frame 2600): Distance 11px -> rejects via `$A434 BCS`.
  - `user-steal-paired-26-contest` (frame 2600): Opponent in `$26` -> redirects via `$A41A BEQ` to jump contest `$A607`.
  - `user-steal-foul-1a` (frame 2600): `$0025 == 0`, same facing -> triggers foul `$1A` to `$9645`.
  - `user-steal-same-player-0f` (frame 2600): Same player -> triggers whistle `$2C`, reason `$0F`.
- Native C test suite (`check_user_ordinary_steal` in `tests/dd_gameplay_cpu_test.c`) verifies all 11 regression checks including natural approach and post-steal live dribble downcourt.

### Universal CPU route/movement cadence and installed-vector execution ($ABCD → $9D2D → $D98D/$D98A → $A84C)

#### Recovered 6502 Flow

| Address | Bank | ROM Offset | Name / Function | Operational Description |
| :--- | :--- | :--- | :--- | :--- |
| `$ABCD-$AC29` | 0 | `0x2BDD-0x2C39` | `gameplay_ABCD` (Vector Installation) | Unpacks target cell coordinates via `$AB96`, computes quadrant and 16-bit distance angle via `$9D2D`, derives 8-way facing (`0..7`) via `$AA98`, and expands quadrant angle into signed 8.8 velocity components using depth tables `$9C1C/$9C1D` (`$000A/$000B -> $03E0/$03F0`) and longitudinal tables `$9C5E/$9C5F` (`$000C/$000D -> $0390/$03A0`) via `$9BB0` |
| `$9D2D-$9DCF` | 0 | `0x1D3D-0x1DDF` | `gameplay_9D2D` (Angle Classification) | Computes `dx` and `dd` differences, determines quadrant (`$0014`), forms half-distance quotient `ratio = (half_dd * 256) / half_dx`, searches 33-entry threshold table at `$9DEB`, and produces angle direction with quadrant offsets `$00/$80` |
| `$9BB0-$9C1B` | 0 | `0x1BC0-0x1C2B` | `gameplay_9BB0` (Velocity Expansion) | Expands angle into signed 8.8 velocities: normalizes angle to quadrant offset `0..0x40`, indexes tables `$9C1C` and `$9C5E`, applies `$9C0B` (axis swap) and `$9BF6/$9C06` (signed negation) according to quadrant bands `< $41`, `$41..$80`, `$81..$C0`, `> $C0` |
| `$8BF8-$8C0E` | 0 | `0x0C08-0x0C1E` | `gameplay_8BF8` (5/4 Vector Scaling) | Multiplies 8.8 signed velocity components by 5/4 via arithmetic shift right 2 and addition: `v + (v >> 2)` with signed preservation |
| `$D98D` | 7 | `0x599D` | `gameplay_D98D` (Arrival & Vector Step) | Tests cell arrival via `$D978` and falls into `$D98A` |
| `$D98A` | 7 | `0x599A` | `gameplay_D98A` (Installed-Vector Integration) | Calls `$A84C` for object motion integration and continues to `$D990 -> $A896` animation tail |
| `$A84C-$A859` | 0 | `0x285C-0x2869` | `gameplay_A84C` (Double Integration Cadence) | Calls depth integrator `$9CF6` twice and longitudinal integrator `$9CA0` twice per 30 Hz object dispatch turn |
| `$9CA0-$9CF5` | 0 | `0x1CB0-0x1D05` | `gameplay_9CA0` (Longitudinal Integrator) | Adds 8.8 velocity (`$0390/$03A0`) to 16.8 position (`$0360/$0370/$0380`). Clamps to integer interval `($000F, $01F1]`. If out of bounds, retains position and zeroes velocity |
| `$9CF6-$9D2C` | 0 | `0x1D06-0x1D3C` | `gameplay_9CF6` (Depth Integrator) | Adds 8.8 velocity (`$03E0/$03F0`) to 8.8 position (`$03C0/$03D0`). Clamps to integer interval `[$05, $98]`. If out of bounds, retains depth and zeroes velocity |
| `$AC64-$AC77` | 0 | `0x2C74-0x2C87` | `gameplay_AC64` (Target Mirroring) | Inverts 5-bit column coordinate for inverted possession direction `$40`: `(packed & 0xE0) | (0x1F - (packed & 0x1F))` |

#### Behavioral Summary
1. **Installed 8.8 Vectors**: Target installation computes exact signed 8.8 velocities along both court axes via `$ABCD -> $9D2D -> $9BB0`, avoiding host-frame approximations.
2. **Double-Integration Cadence (`$A84C`)**: Each active 30 Hz object dispatch executes `$9CF6` twice (depth) and `$9CA0` twice (longitudinal), advancing positions in fractional 8.8 / 16.8 subpixels.
3. **Boundary Clamping & Collision Zeroing (`$9CA0/$9CF6`)**: Longitudinal movement outside `$0010..$01F1` and depth movement outside `$05..$98` clamps position and zeroes velocity.
4. **5/4 Speed Scaling (`$8BF8`)**: Signed quarter-step addition accelerates carrier drives, defensive pursuit, and avoidance paths.
5. **Direction Mirroring (`$AC64`)**: Seamlessly flips packed targets and route angles across direction 0 and direction 1 (`$40`).

#### Native Tests & Verification
- Unit test suite `check_cpu_movement_and_route_cadence` in `tests/dd_gameplay_cpu_test.c` validates:
  1. All 8 cardinal/diagonal vector installations and zero-distance routes.
  2. Exact 5/4 speed scaling on positive and negative components.
  3. Fractional subpixel accumulation, carry, and borrow arithmetic.
  4. Longitudinal and depth boundary clamping with collision zeroing.
  5. `$A84C` 2x double integration cadence per dispatch turn.
  6. Direction 0 and 1 coordinate mirroring via `$AC64`.
  7. CPU cut routes (`$3D`), defensive pursuit (`$22`), carrier drives, and full 4-period match progression.

