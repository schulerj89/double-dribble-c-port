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

The no-input trace therefore establishes one deterministic branch, CPU possession in slot 7. It does not yet establish the player-controlled jump timing or the alternate slot-2 win branch; that needs a second FCEUX input sweep before the native result logic is finalized.

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

DDAP v8 adds `tipoff.assets`, a bounded, checksummed entry containing the 42 bank-2 gameplay metasprites, the 48-byte held-ball offset table at bank 0 `$B07B`, and the 32-byte height-script region at bank 0 `$9B29`. The runtime receives normalized offsets and signed deltas only; it does not receive ROM banks, 6502 state, or an instruction stream.

`dd_gameplay.c` expresses the recovered sequence as native `Player`, `Ball`, `Camera`, and possession state. It reproduces the opening ball parabola, the CPU jumper's table-driven rise/landing, the deterministic no-input award to original slot 7, owner-relative ball attachment, the first dribble cycle, camera follow, and the initial live action assignments. Arrow keys update the native 1UP player after the possession handoff. Its palette attribute follows the observed two-frames-on/two-frames-off pattern, so the player flashes without disappearing.

`Capture-NativeGameplay.ps1` renders toss, award, handoff, and adjacent flash frames without launching the Win32 window. `Compare-GameplayCaptures.ps1` compares transition frame 356 with original frame 2557 after cropping the native overscan rows. The current result is 2,002 differing pixels out of 57,344 (3.4912%); the pre-jump formation regression remains pixel exact. The remaining handoff difference is primarily dynamic metasprite/OAM ordering plus the not-yet-ported game clock.

### Next implementation slice

1. Capture an FCEUX A/B timing sweep to determine the controllable slot-2 win path before locking jump controls.
2. Translate the live offensive and defensive action dispatchers closely enough to replace the provisional formation targets.
3. Port game-clock/HUD updates, then name and translate pass, shot, rebound, steal, and out-of-bounds ball states.

## Open research questions

- Identify the higher-level title/attract-mode dispatcher names around the recovered low-level routines.
- Match the native PCM against an FCEUX WAV capture including the NES nonlinear mixer response.
- Replace the current fixed title OAM construction with the complete named native title scene state machine as later animation states are ported.
- Determine the exact A/B timing window and alternate possession result for the human jump-ball branch.
- Name and translate the pass, shot, rebound, steal, and out-of-bounds ball states after the initial live-control slice.
