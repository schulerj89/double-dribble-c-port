# Gameplay Translation Coverage

This ledger tracks how much of the original gameplay loop has been translated from Ghidra/6502 behavior into native C. Update it whenever a gameplay handler or subsystem changes. The checked-in `tools/Measure-GameplayCoverage.ps1` inventory validates the denominators and recalculates the headline during every build. The headline is a weighted engineering measure, not a claim that the match is halfway feature-complete.

## Current headline

**Portable Ghidra-to-C gameplay-loop coverage: 82%**

**End-to-end match completeness: approximately 50%**

The first number measures the currently catalogued dispatcher and loop work. The second is deliberately more conservative: clock, score, periods, the observed result-four miss, and sustained-contact steals now have bounded native paths, while exact clock gating, final match end, blocks, fouls, general out-of-bounds handling, and broad CPU decision coverage remain incomplete.

Coverage statuses have fixed values:

- **Verified (V) = 1.0**: handler control/data flow is substantially translated, tied to Ghidra plus a dynamic FCEUX trace, and exercised by a native check or capture.
- **Partial (P) = 0.5**: the state is playable or scripted, but important original branches/helpers remain missing.
- **Missing (M) = 0.0**: no native behavior corresponding to that original state or subsystem.

The portable denominator excludes mapper/bank switching, PPU/CHR streaming,
metasprite/OAM construction, and exact dynamic OAM ordering. Those renderer paths
can still exist in the port, but they are NES presentation mechanisms rather than
native gameplay behavior and do not count toward the user's 99% portable target.

The weighted headline is:

`player actions × 30% + ball actions × 25% + portable core loop × 25% + match rules × 20% = 82.4%`, rounded to **82%**.

## Player action dispatcher — 92.6%

Ghidra anchor `$89B2` subtracts `$20` from player action `$0340+slot` and dispatches through the 34-entry table at `$89C0`. `ExportGameplayLoopEvidence.java` prints all entries so this denominator is reproducible.

| Status | Count | Original states |
| --- | ---: | --- |
| V | 29 | `$21-$29,$2C-$2F,$31-$34,$36-$41` |
| P | 5 | `$20,$2A,$2B,$30,$35` |
| M | 0 | none |

Score: `(29 + 5 × 0.5) / 34 = 92.6%`.

Every table entry now has an explicit native handler. States `$28/$29` reproduce
the traced `$20` countdown, rotating-priority ball target, immediate `$B435`
contact, and `$A347` possession handoff. States `$2D-$2F` have repeated dynamic
traces plus isolated native checks for target arrival, ball-state exclusions,
possession claim, direction-dependent return target, and hold-state arrival. The
partial group still uses bounded timing, movement, collision, or formation logic
where important original branches remain, especially the full `$D99A` obstacle
search and several `$D759-$DA39` CPU branches.

State `$38` now follows `$8195->$8468` exactly: `$AC2A` selects the court
region, region zero uses `($001A + 1) & 3`, and `$AC58` reads the seven-byte
target table at `$AC78`. Natural frames 9110-9112 prove two objects in region
one selecting target `$EC` before `$38->$39`. Controlled probes also distinguish
the bare `$8297` state `$3B` handler (seeded vectors remain unchanged) from
`$8460->$B503` state `$3F` (all three vectors clear). State `$39` now includes
the `$9097` role-zero region comparison, signed `$8262` two-candidate search,
`$8CF3` rejection, `$AC78[2]` fallback, and `$842F` arrival target. A controlled
probe proves `$39->$3A` and `$8C->$AB`; natural frames 9114/9126 prove arrival
to `$3E` with phase targets `$8C/$E6`. `$3A` implements its shorter packed
arrival loop. States `$39/$3A` are therefore V.

State `$21` now uses exact `$D978` packed equality and clears `$0600`; a
controlled `$EC==$EC` probe records `$21->$20` and `$55->$00`. State `$41`
uses the same arrival rule before copying the inbounder's position to the ball:
natural frame 3501 records position/target/ball `$0121`, owner/carrier `$07`,
and `$41->$30`. Both states are V.

States `$23/$24` now translate `$8AF4->$8B12->$9ABD` instead of using a
ballistic approximation. `$8AF4->$B503` clears all three motion vectors and
installs ROM pointer `$9B34` (asset index 11). The native byte interpreter adds
the signed integer-height deltas, preserves the fractional byte, reverses both
pointer direction and sign at `$81`, and restores integer height `$10` at the
backward `$80` sentinel. Controlled FCEUX frames 2601-2657 record
`$23->$24`, height `$1055->$2655->$1055`, pointer `$9B34->$9B3F->$9B33`,
direction `0->1`, then `$24->$28` with timer `$10->$0F`. Native regression
checks reproduce the 27 scheduled interpreter updates and the `$A6C3` loose-ball
contact branch. Both states are therefore V.

State `$31` now follows `$8FE0`'s complete release countdown. Natural original
frames 3545-3563 record timer `$08->$FF`, ball `$00->$02` and carrier `$07->$00`
at timer `$04`, metasprite index subtraction by eight below `$06`, and the
underflow transition `$31->$40`. Controlled probes separately prove the launch
and underflow branches. The native inbound phase now enters `$31` from state
`$30` only when every remaining `$36` formation object has reached `$37`, then
uses the same alternating-frame timer; `$31` is V. State `$30` remains P because
its alternate `$002C`, mode-bit `$40`, and timeout orchestration branches are
not yet translated.

States `$2C/$33/$34` share the literal `$8BC5->$D98A->$A84C` tail. `$A84C`
calls the longitudinal fixed-point integrator twice and the depth integrator
twice; it does not calculate a new target vector. Controlled probes seed
`$0123/$FEDC` and preserve both terms while positions advance by
`$0246/$FDB8` on every scheduled dispatch. Original frame 2601 produces the
same `$00F358/$39D8` position for injected states `$33` and `$34`; state `$2C`
continues identically through frames 2601-2607. Native tests reproduce the
two-add result for all three states, promoting them to V.

## Ball action dispatcher — 100%

Ghidra anchor `$AC83` dispatches ball action `$0340` through the 13-entry table at `$AC91`, covering states `$00-$0C`.

| Status | Count | Original states |
| --- | ---: | --- |
| V | 13 | `$00-$0C` |
| P | 0 | none |
| M | 0 | none |

Score: `13 / 13 = 100%`.

Every table entry now has an explicit native handler. State `$0A` reproduces the
observed one-dispatch `$B017` initializer, including vertical term `$0305` and
curve byte `$D8`, while preserving owner/carrier fields the original does not
write. States `$0B/$0C` share `$ACAB`: only the integer height byte is cleared,
leaving the fractional byte and motion terms untouched. Natural FCEUX traces
cover `$0A` and 1,542 frames of `$0B`; an opt-in controlled trace covers `$0C`.
State `$00` now reproduces both observed `$ACB6` height branches: `$18` for
tip-award modes and `$08` for ordinary held play. State `$09` integrates until
the exact unsigned integer-height threshold `$E0`, then enters `$07` with
vertical term `$02E0` and a cleared rim latch; the natural result-four trace
crosses that threshold on frame 61. State `$03` follows `$ADF2` and byte-exact
helper `$B167`: controlled frames 2601-2603 prove vertical term `$0100->$0000`,
gravity phase `$3D->$00`, and the next dispatch `$03->$0C`. State `$08` follows
`$AF72`; controlled outcome probes prove forward half-speed (`$02`), cleared
horizontal terms (`$03`), and reverse half-speed (`$04`), while retaining the
camera carrier and rim latch exactly as the original does. The ball dispatcher
is therefore fully Verified. Higher-level block, foul, and general rim-contact
eligibility remain tracked under core/rules rather than hidden in this score.

## Portable core per-frame loop — 78.6%

Seven portable subsystems produce this score.

| Status | Subsystems |
| --- | --- |
| V (4) | native object state, alternating 30 Hz team scheduler, packed target coordinates, state-driven dribble audio |
| P (3) | user control/control ownership, fixed-point height and movement physics, general player/ball collision resolution |
| M (0) | none |

Score: `(4 + 3 × 0.5) / 7 = 78.6%`.

Excluded NES-only presentation mechanisms: camera-driven CHR streaming,
metasprite/OAM construction, and exact dynamic OAM ordering. Mapper and bank
switching are likewise excluded globally and are not represented as gameplay
coverage entries.

## Match rules and possession flow — 50.0%

Thirteen tracked match-level capabilities produce this score.

| Status | Capabilities |
| --- | --- |
| V (1) | opening tip-off and possession award |
| P (11) | live user control, CPU pass/shot choice, made-shot sequence, rebound sequence, inbound sequence, possession transfer, game clock, score/HUD updates, period transitions/end conditions, missed-shot outcomes, defensive steals/blocks |
| M (1) | fouls and general out-of-bounds rules |

Score: `(1 + 11 × 0.5) / 13 = 50.0%`.

The defensive entry is Partial: the original sustained-contact steal path and
immediate loose-ball possession arbitration are now native, but shot blocks and
the remaining defensive eligibility branches are not yet complete.

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

1. Deepen the partial dispatcher handlers with dynamic FCEUX branch captures, especially `$20/$23/$24` and `$30/$31`.
2. Translate the remaining `$D99A` CPU decision/search branches and pass-lane rejection.
3. Translate the higher-level block, contested-rebound, and general rim-contact eligibility branches around `$B473`.
4. Match the clock's presentation-state call gaps and implement fourth-period/final match end conditions.
5. Add steals, blocks, fouls, and non-scripted out-of-bounds handling.
