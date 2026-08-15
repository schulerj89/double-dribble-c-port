# Gameplay Translation Coverage

This ledger tracks how much of the original gameplay loop has been translated from Ghidra/6502 behavior into native C. Update it whenever a gameplay handler or subsystem changes. The checked-in `tools/Measure-GameplayCoverage.ps1` inventory validates the denominators and recalculates the headline during every build. The headline is a weighted engineering measure, not a claim that the match is halfway feature-complete.

## Current headline

**Ghidra-to-C gameplay-loop coverage: 66%**

**End-to-end match completeness: approximately 46%**

The first number measures the currently catalogued dispatcher and loop work. The second is deliberately more conservative: clock, score, periods, and the observed result-four miss now have bounded native paths, while exact clock gating, final match end, steals, blocks, fouls, general out-of-bounds handling, and broad CPU decision coverage remain incomplete.

Coverage statuses have fixed values:

- **Verified (V) = 1.0**: handler control/data flow is substantially translated, tied to Ghidra plus a dynamic FCEUX trace, and exercised by a native check or capture.
- **Partial (P) = 0.5**: the state is playable or scripted, but important original branches/helpers remain missing.
- **Missing (M) = 0.0**: no native behavior corresponding to that original state or subsystem.

The weighted headline is:

`player actions × 30% + ball actions × 25% + core loop × 25% + match rules × 20% = 66.1%`, rounded to **66%**.

## Player action dispatcher — 66.2%

Ghidra anchor `$89B2` subtracts `$20` from player action `$0340+slot` and dispatches through the 34-entry table at `$89C0`. `ExportGameplayLoopEvidence.java` prints all entries so this denominator is reproducible.

| Status | Count | Original states |
| --- | ---: | --- |
| V | 11 | `$22,$25,$26,$27,$32,$36,$37,$3C,$3D,$3E,$40` |
| P | 23 | `$20,$21,$23,$24,$28,$29,$2A,$2B,$2C,$2D,$2E,$2F,$30,$31,$33,$34,$35,$38,$39,$3A,$3B,$3F,$41` |
| M | 0 | none |

Score: `(11 + 23 × 0.5) / 34 = 66.2%`.

Every table entry now has an explicit native handler. The partial group includes states that currently use bounded timing, movement, collision, or formation logic rather than every branch of the original handler. In particular, the full `$D99A` obstacle search and several `$D759-$DA39` CPU branches remain incomplete.

## Ball action dispatcher — 73.1%

Ghidra anchor `$AC83` dispatches ball action `$0340` through the 13-entry table at `$AC91`, covering states `$00-$0C`.

| Status | Count | Original states |
| --- | ---: | --- |
| V | 6 | `$01` dribble, `$02` pass, `$04` gather, `$05` flight, `$06` score/rim, `$07` rebound |
| P | 7 | `$00` award/attachment, `$03` bounce pass, `$08/$09` loose-ball launch/flight, `$0A` shot launch preparation, `$0B/$0C` dead/hidden ball |
| M | 0 | none |

Score: `(6 + 7 × 0.5) / 13 = 73.1%`.

Every table entry now has an explicit native handler. Although states `$04-$09` are present, their current made-shot and loose-ball paths are bounded. General rim outcomes, misses, blocks, contested rebounds, and every branch of `$B377/$B473` are not complete.

## Core per-frame loop — 75%

Ten tracked subsystems produce this score.

| Status | Subsystems |
| --- | --- |
| V (6) | native object state, alternating 30 Hz team scheduler, packed target coordinates, camera/CHR streaming, metasprite rendering, state-driven dribble audio |
| P (3) | user control/control ownership, fixed-point height and movement physics, general player/ball collision resolution |
| M (1) | exact full dynamic OAM/order behavior |

Score: `(6 + 3 × 0.5) / 10 = 75%`.

## Match rules and possession flow — 46.2%

Thirteen tracked match-level capabilities produce this score.

| Status | Capabilities |
| --- | --- |
| V (1) | opening tip-off and possession award |
| P (10) | live user control, CPU pass/shot choice, made-shot sequence, rebound sequence, inbound sequence, possession transfer, game clock, score/HUD updates, period transitions/end conditions, missed-shot outcomes |
| M (2) | steals/blocks, fouls and general out-of-bounds rules |

Score: `(1 + 10 × 0.5) / 13 = 46.2%`.

## Evidence and update rules

The generated headless reports live under ignored `build/ghidra-reports/`. The source of truth needed to regenerate them remains tracked:

- `tools/ghidra/Run-GameplayLoopAnalysis.ps1`
- `tools/ghidra/ExportGameplayLoopEvidence.java`
- `tools/fceux/capture_tipoff_gameplay.lua`
- `tests/dd_gameplay_cpu_test.c`

When updating this ledger:

1. Add the responsible Ghidra address/table entry and ROM bank provenance to `PORTING.md`.
2. Capture the runtime branch in FCEUX.
3. Translate the behavior into native C without an instruction interpreter or recorded-state playback.
4. Add a deterministic test or original/native capture.
5. Promote `M -> P` when a bounded playable path exists; promote `P -> V` only when the important observed handler branches and helpers are translated and verified.
6. Recalculate both the component and weighted headline percentages.
7. Update `tools/Measure-GameplayCoverage.ps1`; `build.ps1` must print the same component and headline values as this document.

## Highest-value next coverage work

1. Deepen the partial dispatcher handlers with dynamic FCEUX branch captures, especially `$21/$23/$24/$2D-$2F` and `$38-$3A`.
2. Translate the remaining `$D99A` CPU decision/search branches and pass-lane rejection.
3. Complete the remaining `$B473` hoop/rim sweep and contested-rebound branches behind ball `$03/$08/$09`.
4. Match the clock's presentation-state call gaps and implement fourth-period/final match end conditions.
5. Add steals, blocks, fouls, and non-scripted out-of-bounds handling.
