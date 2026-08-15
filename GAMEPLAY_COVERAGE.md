# Gameplay Translation Coverage

This ledger tracks how much of the original gameplay loop has been translated from Ghidra/6502 behavior into native C. Update it whenever a gameplay handler or subsystem changes. The checked-in `tools/Measure-GameplayCoverage.ps1` inventory validates the denominators and recalculates the headline during every build. The headline is a weighted engineering measure, not a claim that the match is halfway feature-complete.

## Current headline

**Portable Ghidra-to-C gameplay-loop coverage: 91%**

**Match-rules completeness: 73.1%**

The first number measures the currently catalogued dispatcher and loop work. The second is deliberately more conservative: all basket-result outcomes, period/final-match transitions, and the foul/free-throw dispatcher spine now have native paths, while exact presentation-gated clock cadence, blocks, general out-of-bounds handling, free-throw formation movement, and broad CPU decision coverage remain incomplete.

Coverage statuses have fixed values:

- **Verified (V) = 1.0**: handler control/data flow is substantially translated, tied to Ghidra plus a dynamic FCEUX trace, and exercised by a native check or capture.
- **Partial (P) = 0.5**: the state is playable or scripted, but important original branches/helpers remain missing.
- **Missing (M) = 0.0**: no native behavior corresponding to that original state or subsystem.

The portable denominator excludes mapper/bank switching, PPU/CHR streaming,
metasprite/OAM construction, and exact dynamic OAM ordering. Those renderer paths
can still exist in the port, but they are NES presentation mechanisms rather than
native gameplay behavior and do not count toward the user's 99% portable target.

The weighted headline is:

`player actions × 30% + ball actions × 25% + portable core loop × 25% + match rules × 20% = 91.0%`, rounded to **91%**.

## Player action dispatcher — 100%

Ghidra anchor `$89B2` subtracts `$20` from player action `$0340+slot` and dispatches through the 34-entry table at `$89C0`. `ExportGameplayLoopEvidence.java` prints all entries so this denominator is reproducible.

| Status | Count | Original states |
| --- | ---: | --- |
| V | 34 | `$20-$41` |
| P | 0 | none |
| M | 0 | none |

Score: `34 / 34 = 100%`.

Every table entry now has an explicit native handler. States `$28/$29` reproduce
the traced `$20` countdown, rotating-priority ball target, immediate `$B435`
contact, and `$A44B` possession handoff. States `$2D-$2F` have repeated dynamic
traces plus isolated native checks for target arrival, ball-state exclusions,
possession claim, direction-dependent return target, and hold-state arrival.
Higher-level movement, collision, and decision caveats are tracked under the
core-loop and match-rule components, especially the remaining `$D759-$DA39`
CPU branches.

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
uses the same alternating-frame timer. Controlled state `$30` probes cover the
standard `$9018->$31` arrangement, mode-bit `$40` action `$0D` arrangement,
and `$002C`'s additional opposite-role-zero `$0F` assignment. Both states are V.

States `$2C/$33/$34/$35` share the literal `$8BC5->$D98A->$A84C` tail. `$A84C`
calls the longitudinal fixed-point integrator twice and the depth integrator
twice; it does not calculate a new target vector. Controlled probes seed
`$0123/$FEDC` and preserve both terms while positions advance by
`$0246/$FDB8` on every scheduled dispatch. Original frame 2601 produces the
same `$00F358/$39D8` position for injected states `$33`, `$34`, and `$35`; state `$2C`
continues identically through frames 2601-2607. Native tests reproduce the
two-add result for all four states, promoting them to V.

States `$2A/$2B` now expose their dispatcher behavior directly as well as
through the existing tip-off sequence. `$8DD2` waits for shared phase `$004A`
to reach `$20`, installs pointer `$9B34`, clears its direction, and enters
`$2B`. `$8DF7` runs the same verified `$9ABD` interpreter and awards `$25`
when the jumper owns the ball; otherwise it restores that five-player side to
`$20`. Natural original frames 2471-2503 prove phase `$00->$20` and `$2A->$2B`;
frames 2505-2557 prove height `$10->$26->$10`, tip ownership `$07`, and
`$2B->$25`. Isolated native checks cover the below-threshold, winner, and loser
branches, promoting both states to V.

State `$20` now follows `$8A16->$9102/$9139` rather than remaining a generic
off-ball target seeker. `$9102` uses two-unit half extents against the paired
player's projected coordinates. Controlled probes prove contact `$20->$22`
with latch `$10` while preserving vectors, separation `$22->$20` while clearing
both vectors, and paired action `$03` causing `$20->$23`. Together with the
already translated common tail, states `$20/$22` are V and the player action
dispatcher is fully Verified.

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

## Portable core per-frame loop — 85.7%

Seven portable subsystems produce this score.

| Status | Subsystems |
| --- | --- |
| V (5) | native object state, alternating 30 Hz team scheduler, packed target coordinates, state-driven dribble audio, user control/control ownership |
| P (2) | fixed-point height and movement physics, general player/ball collision resolution |
| M (0) | none |

Score: `(5 + 2 × 0.5) / 7 = 85.7%`.

User control/control ownership is Verified from bank-0 `$A129`, `$AD41`, and
`$A29D`. Direction+A uses the original directional half-plane scoring and
later-slot tie rule. Reception writes the receiver to both owner `$005B` and
camera/control `$0048`, and the native scheduler now excludes that dynamic
controlled slot rather than always excluding player zero. On defense, B uses
the original two-candidate screen-distance search and transfers action `$0F`
to the selected teammate while restoring `$20` to the former user. Focused
FCEUX traces cover four pass directions, both reception branches, and the
defensive switch; deterministic native checks cover handoff and post-catch
movement.

Excluded NES-only presentation mechanisms: camera-driven CHR streaming,
metasprite/OAM construction, and exact dynamic OAM ordering. Mapper and bank
switching are likewise excluded globally and are not represented as gameplay
coverage entries.

## Match rules and possession flow — 73.1%

Thirteen tracked match-level capabilities produce this score.

| Status | Capabilities |
| --- | --- |
| V (6) | opening tip-off and possession award; made-shot sequence; score/HUD updates; missed-shot outcomes; period transitions/end conditions; live user control |
| P (7) | CPU pass/shot choice, rebound sequence, inbound sequence, possession transfer, game clock, defensive steals/blocks, fouls and general out-of-bounds rules |
| M (0) | none |

Score: `(6 + 7 × 0.5) / 13 = 73.1%`.

The defensive entry is Partial: the original sustained-contact steal path and
immediate loose-ball possession arbitration are now native, but shot blocks and
the remaining defensive eligibility branches are not yet complete.

Missed-shot outcomes are Verified from `$AE25->$B377->$AF72/$AFDD`: controlled
FCEUX probes independently produce classifier results `$01-$04`, the `$04F0`
wrap/arming return is covered, and native checks exercise all three miss-vector
branches plus the traced 61-frame loose-ball arc. The combined foul/out-of-bounds
entry is now Partial because the zero-clock same-facing foul/free-throw path is
native; general sideline and baseline out-of-bounds decisions remain missing.

The made-shot and score/HUD entries are Verified from `$AE25->$AEDE` and fixed
bank `$C477/$C6AD`. The natural score trace enters state `$06` with counter
`$0C`, writes both score copies only on `$09->$08`, lowers height for `$05-$00`,
and enters rebound `$07` on underflow. Native checks prove the deferred ordinary
two-point award and foul-shot one-point award; the renderer derives the same
blank-leading two decimal HUD tiles from native score state. Buffered PPU queue
mechanics remain excluded as NES-only presentation machinery.

Period transitions/end conditions are Verified. The earlier long trace proves
the delayed period-one reset to a second-period formation. The final-match trace
adds the distinct bank-0 `$93AE` gate: only a zero clock with player slot two
below action `$41` and ball action `$01` or `$07` reaches `$93EC-$93FE`. Original
frame 45337 renders period four at 00:00; frame 45338 writes `$0059=00`, advances
the `$07E8` presentation index, and installs `$0068=0C/$006C=25` while displaying
`GAME SET`. Frames 45596-45597 are solid blue and frame 45620 is the title.
Native tests cover both the in-flight rejection and rebound acceptance, then the
258-frame game-set hold and 282-frame title return. Exact variable clock call
gaps remain isolated under the separate Partial game-clock capability.

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

1. Translate the free-throw formation states `$42-$4A`, starting with `$8594/$85BE/$860A/$8682`.
2. Translate the remaining CPU decision branches and pass-lane rejection.
3. Translate the higher-level block, contested-rebound, and general rim-contact eligibility branches around `$B473`.
4. Match the clock's presentation-state call gaps.
5. Add non-scripted sideline and baseline out-of-bounds handling.
