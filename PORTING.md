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

The live player scheduler is split across bank 0 `$993A-$9976` and `$9E70-$9EA4`. The two five-player teams alternate on the low bit of `$001A`, so each team is evaluated at 30 Hz. `$004D` rotates through the opposing five-player range and identifies the one slot that receives the more expensive target/vector refresh. The native scheduler preserves this alternating cadence and initializes its phase to the observed `$001A=$DC` at original frame 2557.

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

The native CPU decision is intentionally bounded to what this trace proves. A carrier in the recovered shooting region chooses the `$04` shot chain; a carrier outside that region selects the most advanced same-team receiver and enters pass state `$02`. It does not yet claim to reproduce every branch of fixed-bank `$D759-$D8B0`, `$D978`, or `$D99A`.

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

The repaired path removes those age gates and the reception teleport. Made-basket return now runs `$2D->$2E->$2F->$30->$0D` alongside the first `$36->$37` formation. The ordinary inbound runs the real alternating scheduler through `$36/$41->$37/$30->$31/$40`, lets ball `$AD41->$B138` contact complete the 19-frame pass, and translates `$AD6D` by assigning carrier `$25`, role-three `$3C`, role-four `$3E`, and the `$842F` route target without changing any player coordinates. State `$25` then holds for fourteen 30 Hz CPU evaluations before the captured shooting decision, while off-ball states continue on their normal cadence.

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

### Next implementation slice

1. Trace the next possession beyond the first inbound, including the full `$D99A` obstacle/search decisions and pass-lane rejection.
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

Deterministic checks now cover a clean opening make, a synthetic result-four miss, exact miss velocity reflection, the 61-frame loose arc, pass collision, collision-boundary exclusion, jump-ball contact, the `$B473` rim probe, and state `$03`'s zero-vertical-term branch to hidden state `$0C`. CPU shot-block arbitration is now covered below; all remaining state `$03` vertical and contested-rebound branches keep general collision **Partial**.

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

The defensive path begins at `$91A6` and `$9FA3`. Both use `$B435`, increment
the per-object contact counter `$06A0+slot` against limit `$0068`, clear the
counter when contact or eligibility fails, and call `$A347` when sustained
contact qualifies. `$B435` uses exclusive player/ball half extents 4+6 on court
X and depth, then accepts height when unsigned
`player_height + $11 - ball_height < $22`. A controlled, opt-in FCEUX probe at
frames 2602/2606/2608 holds opposing slots `$03/$07` in stable state `$3B` at
the same coordinates with limit 3. The original calls `$A347` at frame 2608;
because clock countdown `$0025` is nonzero, it returns and `$9FA3` jumps to
`$A44B`. That routine changes owner/carrier `$07->$03`, installs carrier state `$02`, assigns the
winning team `$40,$40,$3C,$3E`, and resets the other team to `$20`. The native
translation applies those team roles, ownership, court direction, control, and
contact-counter resets without emulating instructions.

Repeated natural transitions additionally verify the rebound chain: `$2D->$2E`
at frames 2942, 3693, 4675, 7606, 8337, 9249, and 10870; `$2E->$2F` two frames
later after `$8E88` excludes ball states `$05/$06` and assigns owner/carrier;
and `$2F->$30` after the direction-selected `$BD/$A1` return target is reached.
The isolated native regression checks exercise the arrival, exclusion, claim,
target, and hold branches. Selected-bank ROM offsets are `$8D9C->$0DAC`,
`$8DAB->$0DBB`, `$91A6->$11B6`, `$9FA3->$1FB3`, `$A347->$2357`,
`$A44B->$245B`, and
`$B435->$3445`.

These additions promote player states `$28,$29,$2D,$2E,$2F` to Verified and
defensive steals/blocks from Missing to Partial. The CPU shot-block branch is
now translated below; user-triggered contests and the remaining eligibility
rules are still outstanding.

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
The native match phase reproduces this dispatcher spine and scores its result-one
basket as one point rather than the ordinary two. Formation movement and the
remaining post-attempt branches are intentionally still Partial.

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
The `$09->$08` dispatch selects an ordinary two-point increment (or the
free-throw-adjusted one-point increment), calls fixed-bank `$C477/$C6AD`, clears
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
assert no points on state entry, two points on the fourth dispatch, and one
point on the corresponding free-throw dispatch. Made-shot sequence and
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
`$32`; the surrounding region/timer policy in `$D759-$D8B0` remains Partial.

Ball launch state `$0A` is a one-frame initializer at `$B017`, not a wait state.
The unmodified trace changes `$0A->$05` at frames 2470-2471 and shows vertical
term `$0305` plus curve byte `$D8`. The routine does not write owner or camera
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

## Open research questions

The current implementation percentage and its reproducible scoring rules are maintained in `GAMEPLAY_COVERAGE.md`. The current result is **91% portable Ghidra-to-C gameplay-loop coverage** and **73.1% match-rules completeness**; these are intentionally separate measures. The portable denominator explicitly excludes mapper/bank switching and NES-only PPU/CHR/OAM presentation mechanisms.

- Identify the higher-level title/attract-mode dispatcher names around the recovered low-level routines.
- Match the native PCM against an FCEUX WAV capture including the NES nonlinear mixer response.
- Replace the current fixed title OAM construction with the complete named native title scene state machine as later animation states are ported.
- Determine the full successful B timing window around the proven original-frame-2502 user jump.
- Translate the user-defender `$A3E2->$A607` contest trigger, contested-rebound, remaining rim-contact, and out-of-bounds branches.
