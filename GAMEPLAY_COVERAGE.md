# Gameplay Translation Coverage

This ledger tracks how much of the original gameplay loop has been translated from Ghidra/6502 behavior into native C. Update it whenever a gameplay handler or subsystem changes. The checked-in `tools/Measure-GameplayCoverage.ps1` inventory validates the denominators and recalculates the headline during every build. The headline is a weighted engineering measure, not a claim that the match is halfway feature-complete.

## Current headline

**Address discovery/mapping inventory: Incomplete (216 portable routines currently cataloged; denominator still under audit)**

**Behavior-weighted catalog coverage: 96.7% (not an end-to-end parity claim)**

**Match-rules catalog coverage: 92.3% (11 Verified, 2 Partial, 0 Missing)**

**Current-manifest routine verification: 99.8% (215 Verified, 1 Partial, 0 Missing; 7 NES mechanisms excluded)**

Address discovery is no longer claimed complete. The independent audit found
known gameplay roots missing from the 223-node manifest, including bank-0
`$852F-$89AF` free-throw handlers, `$93AE/$9431/$9490` period/clock code, and
fixed `$D40F/$D5F9` dunk logic. The 99.8% figure describes verification only
inside the current incomplete manifest. A branch audit also found that user dispatcher `$A3E2` is
only Partial: the native port covers paired jump contests and loose-ball pickup,
but not the ordinary live-dribble steal branch
`$A42D->$B435->$A347/$A44B`. Therefore neither the routine-verification score nor
the gameplay-behavior scores may be reported as 100%. Seven
mapper/APU-register/PPU mechanisms remain explicitly outside the portable
denominator.

The targeted fixed-bank CPU region-two chain is routine-verified. Broader CPU
possession/contact behavior remains Partial because ordinary live-dribble user
steal root `$A3E2->$A42D` is still absent. The bounded LEVEL slice is now
translated: `$A593` installs `$0068/$006C`, `$A631` maps visible choices to
mutable gameplay LEVEL 0/4/8, `$91A6` and `$9FA3`
consume its contact rules, and `$8A57` consumes the paired-tracking
threshold.
A deterministic four-period no-input match now requires CPU passes, shots, and
inbounds, rejects orphan dribbles/out-of-bounds players, detects a stationary
CPU carrier beyond 640 frames, and reaches GAME SET/return-to-title.

The shared collision primitives at `$9B42`, `$B435`, and `$B473` are now
routine-verified. In particular, the seven-sample rim sweep distinguishes the
ordinary-contact `$16` audio request from the active-special-finish `$003F`
latch while preserving the original owner-clear, camera-carrier, and velocity
reflection side effects.

Mapped possession arbitration `$91A6/$91FB/$9208/$A347/$A44B` has substantial
native coverage, including its LEVEL-dependent thresholds. Native state now
preserves the original `$20` contact lock, exact `$0056` score-return gate,
mutable `$07E8`, entropy-seeded foul
countdown, paired-player and role gates, `$10` ordinary-contact cue, `$30/$1A`
exceptional foul, role-zero swaps, and the distinct blocker versus ordinary
pickup team resets.

Coverage statuses have fixed values:

- **Verified (V) = 1.0**: handler control/data flow is substantially translated, tied to Ghidra plus a dynamic FCEUX trace, and exercised by a native check or capture.
- **Partial (P) = 0.5**: the state is playable or scripted, but important original branches/helpers remain missing.
- **Missing (M) = 0.0**: no native behavior corresponding to that original state or subsystem.

The portable denominator excludes mapper/bank switching, PPU/CHR streaming,
metasprite/OAM construction, and exact dynamic OAM ordering. Those renderer paths
can still exist in the port, but they are NES presentation mechanisms rather than
native gameplay behavior and do not count toward the completed 100% portable target.

The weighted headline is:

`player actions × 30% + ball actions × 25% + portable core loop × 25% + match rules × 20% = 96.7%`.

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
Higher-level movement, collision, and decision behavior is cross-checked under
the core-loop and match-rule components. The targeted
`$8B5A->$D99A->$D759/$D77B-$D8B0` region-two path is Verified; broader CPU
behavior remains Partial pending the independent audit's remaining roots.

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
controlled `$EC==$EC` probe records `$21->$20` and `$55->$00`. Original state
`$41` uses the same packed arrival rule before copying the inbounder's position
to the ball: natural frame 3501 records position/target/ball `$0121`,
owner/carrier `$07`, and `$41->$30`. The native state handler preserves that
data flow with the installed `$ABCD` vector, rotating `$004D` refresh, doubled
`$D98D->$A84C` bounded integrations, and legal fractional endpoint. Both
explicit state handlers and their shared route/formation helpers are V.

States `$23/$24` now translate `$8AF4->$8B12->$9ABD` instead of using a
ballistic approximation. `$8AF4->$B503` clears all three motion vectors and
installs ROM pointer `$9B34` (asset index 11). The native byte interpreter adds
the signed integer-height deltas, preserves the fractional byte, reverses both
pointer direction and sign at `$81`, and restores integer height `$10` at the
backward `$80` sentinel. Controlled FCEUX frames 2601-2657 record
`$23->$24`, height `$1055->$2655->$1055`, pointer `$9B34->$9B3F->$9B33`,
direction `0->1`, then `$24->$28` with timer `$10->$0F`. Native regression
checks reproduce the 27 scheduled interpreter updates. A natural user shot and
an opt-in contact probe additionally prove `$22->$23->$24`, `$A6C3`'s shifted
4-by-4 contact, `$8B27` taking an owned airborne ball, and `$8B44->$9208`
delaying the possession/team reset until landing. Both states are therefore V.

State `$31` now follows `$9018->$B0B8->$8FE0`'s complete release countdown.
Natural original frames 3545-3563 record the pass angle/facing/vector being
installed on entry, timer `$08->$FF`, ball `$00->$02` and camera carrier
`$07->$00` at timer `$04`, preserved owner `$005B`, metasprite index
subtraction by eight below `$06`, and the underflow transition `$31->$40`.
Controlled probes separately prove the launch and underflow branches. The
native inbound phase now enters `$31` from state `$30` only when every
remaining `$36` formation object has reached `$37`, prepares the pass once on
entry, and uses the timer-four gate only to expose flight. Controlled state
`$30` probes cover the standard `$9018->$31` arrangement, mode-bit `$40`
action `$0D` arrangement, and `$002C`'s additional opposite-role-zero `$0F`
assignment. Both states are V.

The inbound path is also state-driven end to end rather than checkpoint-driven.
The made-basket return installs `$2D/$36` from ROM tables `$8503/$8507` on
the same per-object cadence observed at frame 2790, reaches
`$2E/$2F/$30/$31` or `$0D` through scheduled dispatches, and feeds the frame-3324
`$36/$41` turnover inbound after `$0D`'s recovered 320-frame limit. Direction+A
now follows `$A129/$A21F`: natural frame 3010 selects object `$04`, frame 3012
launches ball `$02` and receiver `$0C`, and frame 3051 transfers control and
ownership. `$A482` restores the role-based `$40/$3C/$3E` and opposite `$20`
layout. `$05D7/$05E7=$21/$01` remains extended baseline depth `$98`, while
`$AD6D` installs the ordinary receiver and `$25/$3C/$3E` actions without
overwriting coordinates. This strengthens the verified dispatcher and user
control entries. The shared `$9651` setup, opposite possession direction,
sloped court boundary, possession violations, and every portable helper reached
by those branches are inventoried separately below, promoting match-level
inbound to Verified.

The controlled user miss supplies the opposite low-byte wrap case. Original
frame 2818 enters `$AF46` for the third rebound dispatch, reaches `$9635`, and
passes source packed position `$009D` to `$9651`. `$9763[$9D]=$80`; `$96C1`
adds that offset only to the low byte, yielding `$05D7/$05E7=$1D/$00` rather
than carrying into `$011D`. The native regression asserts target
`x=$01D800, depth=$000800` and state `$41`, preventing the earlier malformed
formation where the inbounder remained at depth `$88`.

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
player’s projected coordinates. Controlled probes prove contact `$20->$22`
with latch `$10` while preserving vectors, separation `$22->$20` while clearing
both vectors, and paired action `$03` causing `$20->$23`. The same `$9139`
branch now runs from already-latched state `$22`, matching the natural contest.
Together with the
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
curve byte `$0C`, while preserving owner/carrier fields the original does not
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

## Portable core per-frame loop — 92.9%

Seven portable subsystems produce this score.

| Status | Subsystems |
| --- | --- |
| V (6) | native object state, alternating 30 Hz team scheduler, packed target coordinates, state-driven dribble audio, fixed-point height and movement physics, general player/ball collision resolution |
| P (1) | user control/control ownership (offense, passing, switching, and contests verified; controlled live-dribble stealing missing) |
| M (0) | none |

Score: `(6 + 0.5) / 7 = 92.9%`.

Fixed-point height and movement physics is Verified. Bank-0 `$9CA0` adds a
signed 8.8 velocity to the 16.8 longitudinal coordinate, accepting integer
positions `$0010-$01F1`; `$9CF6` does the same for depth rows `$05-$98`.
Rejected candidates do not clamp: the old coordinate, including its fractional
byte, survives and only that axis velocity becomes zero. `$A84C` calls each
integrator twice for the shared player states, while ball states
`$02/$03/$05/$07/$09` use the same bounded primitives at their recovered call
counts.

Ball height now follows `$9B84->$C3C5`: after incrementing elapsed phase, each
step adds `base_vertical - floor((elapsed << 8) / curve)` with 16-bit wrap.
`$B412` preserves the wrapped height fraction, clears integer height, subtracts
`$0050` from the next-bounce base, and saturates a signed-negative result to
zero. The shot initializer follows `$B189->$9D2D->$9BB0` and the recovered
`$9DEB/$9C1C/$9C5E` angle/vector tables; bounce-pass `$B167` remains its
separate integer-height path.

The user-shooting slice is verified end to end. `$AA75` installs player state
`$03`, ball state `$04`, release gate `$04E0=1`, and height-script pointer
`$9B34` without clearing the takeoff court vectors. `$A504->$A84C` consumes
those vectors twice per scheduled update, so momentum continues in the air
while fresh steering is disabled. `$A896` resolves state `$03` through the
pointer table at `$A8E6` and selects one of
`$22,$28,$23,$27,$21,$25,$24,$26` from `$A9DC` by facing. CPU
`$8D1F->$AAEE->$AA98->$B503` faces the active hoop, stops court motion, and
uses the same pose table in state `$27`. `$A516-$A520` keeps ball `$04`
attached while controller bit `$40` remains held; clearing B reaches
`$A522->$B189` and releases to ball state
`$05`; `$AE25->$B377` produces result `$01` for a make or results `$02-$04`
for `$08->$09` miss/rebound handling. DDAP v20 stores the eight recovered pose
indices, six four-tile net frames, and `$8503/$8507` post-basket formation
data. Controlled original moving/make/miss traces and shipping-loop native
tests prove momentum, both shooter pose paths, and both outcomes. Metasprite
construction itself remains excluded from the percentage as NES/OAM
presentation; the portable state, flight, and outcome handlers were already
present in the Verified dispatcher denominators.

Dynamic FCEUX instrumentation records 13,810 entries apiece at `$9CA0` and
`$9CF6`, 6,556 at `$A84C`, 574 height steps at `$9B84`, and four natural shot
initializers at `$B189` in the bounded run through frame 6000. The no-input
shot at original frames 2749-2750 proves X `$005700->$005643` with `$FF43`,
depth `$004B00->$004BAB` with `$00AB`, and height `$3800->$39CD` from base
`$0200`, elapsed `$01`, curve `$05`. Native tests assert that exact height,
the next `$3B67` sample, valid double additions, and preserve-and-stop behavior
at all four court limits for both dispatcher and user-controlled movement.
The exact `$AA07->$9E2D/$9E4C` user vectors, `$ABCD->$9D2D/$AA98/$9BB0`
route installer outputs, `$B035->$B0AB` pass initializer and all three
longitudinal/depth hand-offset tables, `$9018->$B0B8` early inbound pass
initializer, contact-only `$AD41` reception, and byte-exact `$B189` short-shot
duration/curve are translated and covered by native regressions. Controlled
original probes now execute all four `$9CA0/$9CF6` rejection exits at frame
2558: `$01F1.80 + $0100`, `$0010.80 + $FF00`, `$98.80 + $0100`, and
`$05.80 + $FF00` each preserve the coordinate and return with the attempted
axis velocity cleared. The primitive routines and their scheduler consumers are
Verified. The native 30 Hz route scheduler retains `$ABCD`'s signed 8.8 vectors,
applies the exact selective `$8C02` wrapping 5/4 scale, and follows each
recovered `$D98A/$D98D->$A84C` call cadence, including the distinct `$D978`
extended inbound arrival comparison.

User control/control ownership is Partial. Its offense, reception, and switching
paths are verified from bank-0 `$A129`, `$AD41`, and
`$A29D`. Direction+A uses the original directional half-plane scoring and
later-slot tie rule. Reception writes the receiver to both owner `$005B` and
camera/control `$0048`, and the native scheduler now excludes that dynamic
controlled slot rather than always excluding player zero. On defense, B uses
the original two-candidate screen-distance search and transfers action `$0F`
to the selected teammate while restoring `$20` to the former user. Focused
FCEUX traces cover four pass directions, both reception branches, and the
defensive switch; deterministic native checks cover handoff and post-catch
movement. The `$A29D` transfer now also follows `$99D9/$9A31`: role zero,
mutable `$0580` opponent links, and their reciprocal links move with the
selected defender. A B-switch regression immediately enters `$A607` with A,
proving the newly selected player can attempt the paired block. The remaining
gap is controlled ground stealing from a normally dribbling CPU carrier.

The verified portion of user defense follows `$A3E2->$A607->$A638`. Ball states `$01/$04/$05`
require the A-button edge while `$07/$09` enter without input; the mutable
`$0580` opponent must expose `$26/$27/$03`. `$A607` installs user state `$11`
and the `$9B26` jump stream. `$A638` tests `$A6C3` only when the next stream
byte is zero, changes contact to ball state `$00`, queues SFX `$20`, and delays
`$92BD->$A44B` possession transfer until landing. A miss exposes `$10` for one
dispatch before `$A5D0` restores `$0F`. Natural FCEUX frame 2748 proves the
A-edge `$0F->$11` path against paired shooter `$07`; native tests cover contact,
delayed ownership, and miss recovery. The opening reciprocal pair map is now
the observed `$02<->$07` and `$06<->$08`; the later inbound `$99D9/$9A31`
swap produces the separately observed `$02<->$08` mapping. However, when the
paired opponent is not in `$26/$27/$03`, original `$A3E2` continues through
`$A42D->$B435->$A347/$A44B`. The C dispatcher currently rejects that ordinary
live-dribble contact path, so `$A3E2` and this subsystem remain Partial.

Excluded NES-only presentation mechanisms: camera-driven CHR streaming,
metasprite/OAM construction, and exact dynamic OAM ordering. Mapper and bank
switching are likewise excluded globally and are not represented as gameplay
coverage entries.

## Match rules and possession flow — 92.3%

Thirteen tracked match-level capabilities produce this score.

| Status | Capabilities |
| --- | --- |
| V (11) | opening tip-off and possession award; made-shot sequence; score/HUD updates; missed-shot outcomes; period transitions/end conditions; live user control; inbound sequence; rebound sequence; possession transfer; game clock; fouls/free-throw continuation |
| P (2) | CPU pass/shot choice; defensive steals/blocks (CPU and jump-contest paths covered; controlled live-dribble steal missing) |
| M (0) | none |

Score: `(11 + 0.5 + 0.5) / 13 = 92.3%`.

CPU pass/shot choice remains Partial overall. The targeted
`$8B5A->$D99A->$D759/$D77B-$D8B0` region-two chain is Verified: helper-produced
projection high, separate role-zero and paired rejection, `$D857` fallback,
sole `$85/$9A`, exact `$8BF8` scaling, and same-dispatch movement. Headless
Ghidra covers fixed-bank
`$D759-$DA39`, including the `$D8B9/$D8D5` route tables and `$D94E` receiver
eligibility rows. A fresh natural FCEUX run through frame 12000 reaches 320
decision roots, six pass searches, seven pass initializers, eleven forced or
region shots, and 463 obstacle lookaheads. Native checks cover both phase-based
receiver roles, different-region acceptance, same-region rejection, the
two-decision cooldown, eight-tick queued release, mirrored route tables,
region-five and last-five-seconds shots, the 24-tick possession limit, decision
timer underflow, lane targeting, and paired-defender avoidance. The pass
initializer regression additionally requires `$9018->$B0B8` to attach ball
object zero and install a nonzero five-unit signed vector before `$8FE0`
releases state `$02`; this closes the stationary orphan-ball failure in both
ordinary CPU passing and CPU inbound.

The independent commit-gate audit explicitly limits that result to this branch;
it does not certify all CPU behavior. The formerly unresolved LEVEL chain is now
a bounded Verified slice: configuration `$A593-$A5BD` feeds `$0068/$006C`,
bank-0 `$91A6-$91FB` and mirrored `$9FA3-$A011` consume mutable LEVEL `$07E8`
with exact `$001D/$0056` preserve exits and the level-6/7/8 pair matrix, and
`$8A57-$8A97` consumes `$006C` before entering player state `$21`. The broader
possession category remains Partial for the separately listed `$A3E2` steal gap.
This does not claim every LEVEL consumer: CPU free-throw release
`$883A-$884F` still needs its `<9`/signed-phase/aim matrix translated and
remains inside the separately Partial free-throw/match-rule area.

The inbound entry is Verified. Ghidra `$A780/$A129/$A21F/$A482` and natural
FCEUX frames 3004-3051 prove directional receiver selection, held-ball
ownership, delayed release, live-role restoration, reception, and control
transfer. The no-input trace reaches `$A795->$9651` on frame 3324, exactly 320
frames after `$0D` begins. Natural frame 5673 proves the opposite-direction
reason `$16` formation. Controlled original probes prove `$A1CC` reasons
`$13/$14` and `$9583` reason `$15`, including their common `$9651` tail.

### Inbound-only portable coverage — 100%

This bounded denominator contains every portable branch/helper reached from
the recovered inbound paths; it excludes only mapper, PPU, and OAM machinery.

| Status | Count | Capabilities |
| --- | ---: | --- |
| V | 32 | sequence/dispatch (10): `$2D` chase, `$2E` claim, `$2F` return, standard `$30`, alternate `$30`, `$002C` `$30`, `$31` release, user `$0D`, ball `$02` flight/contact, reception/control; placement/helpers (10): direction flip, `$9395`, `$D6BD`, `$9097`, `$9763`, ninth packed bit, opposite role-zero offset, lane clamp, receiving role-one offset, `$ABCD/$D978/$D98D` refresh/walk/arrival; causes/rules (9): held-shot landing `$0F`, `$12`, `$13`, `$14`, `$15`, `$16`, rebound-state preservation, exact SFX `$2C`, exceptional `$17/$1A`; role/ownership (3): `$AD6D` actions, `$99D9` role swap, `$9A31` reciprocal pair swap |
| P | 0 | none |
| M | 0 | none |

Score: `32 / 32 = 100%`.

SFX `$2C` is no longer a marker-only request. Fixed `$C141` banks through
`$CC99`; the bank-1 driver resolves streams `$86F7/$8702/$870D`. Controlled
FCEUX APU frames 2601-2612 prove two silent request frames, eight alternating
pulse pairs (`37/52`, `43/40`), noise period `6` at volume `3`, then explicit
stops. DDAP v13 stores those 23 normalized events in `whistle.audio`, and the
Win32 event serial interrupts the dribble loop to play the bounded 12-frame WAV.

Reasons `$17/$1A` now share the literal exceptional `$965A->$98A3` native
helper: dead-ball latch `$0065=$FF`, ball `$0B`, cleared carrier `$0048`, and
gate `$0056=$FF`, without `$D6BD` ordinary inbound formation. `$A347` supplies
`$1A`; `$A1CC->$A37D` supplies `$17` when an opposing state-`$22` object shares
the packed target and its facing maps through `$A375` (`+4 mod 8`). Controlled
original frame 2602 proves reason `$17` and all four `$98A3` writes; frame 2822
then awards the foul shot to that defender. Native tests cover both reasons and
their shooter/offender ownership.

The defensive entry is Partial: sustained-contact steals, paired CPU contests, user
`$A3E2->$A607->$A638` contests, owned-ball arbitration, apex-only `$A6C3`
contact, landing-delayed possession, miss recovery, and loose-ball possession
are native. The `$001D` presentation gate, full `$0056` contact/dead-ball
lifetime, and pack-backed block requests `$10/$20` follow their recovered
branches and have deterministic coverage. The missing branch is the controlled
defender's A-edge against a live CPU dribbler through
`$A42D->$B435->$A347/$A44B`.

Missed-shot outcomes are Verified from `$AE25->$B377->$AF72/$AFDD`: controlled
FCEUX probes independently produce classifier results `$01-$04`, the `$04F0`
wrap/arming return is covered, and native checks exercise all three miss-vector
branches plus the traced 61-frame loose-ball arc. The combined foul/out-of-bounds
entry is Verified. The shared sloped boundary, ordinary reasons `$13-$16`,
exceptional reasons `$17/$1A`, foul ownership, free-throw continuation,
whistle selection, and post-final-attempt rebound/inbound variants are native
and covered.

The made-shot and score/HUD entries are Verified from `$A7EA/$A834`,
`$AE25->$AEDE`, `$98B5->$990A/$9922`, and fixed bank `$C477/$C6AD`.
`$A7EA` mirrors the two court
directions and applies the exact 23-byte curved line at `$A834`; the native
release stores shot kind 0/1 for two/three points and preserves kind 2 for a
one-point free throw. The score trace enters state `$06` with counter `$0C`,
writes both score copies only on `$09->$08`, lowers height for `$05-$00`, and
enters rebound `$07` on underflow. The same counter selects net phase 2 on the
make, phase 1 at `$08`, and phase 0 on underflow; DDAP v20's exact 24-byte
table supplies three four-tile frames for each basket side. Controlled
inside/outside and net-phase traces cover the boundary in both directions, and
native checks prove deferred one-, two-, and three-point awards plus the
`2->1->0` net sequence. DDAP v20 also retains the recovered `$09` line-call
and `$25` three-point scoring cues, plus the complete clean-make `$18` and
underflow `$1F/$22` cue from bank-1 `$87B6/$87CA/$886D/$8922`. The renderer derives the same blank-leading
two decimal HUD tiles and pack-backed net phases from native score state.
Buffered PPU queue mechanics remain excluded as NES-only presentation
machinery.

Post-score inbound verification now covers the complete portable handoff, not
only the release boundary. `$8491->$ABCD` installs the exact `$9D2D->$9BB0`
route vector; `$8E71/$8EBF` refresh it only for rotating priority `$004D` and
reach `$2E/$30` with the `$D990->$A896` run-animation tail;
`$8EE2->$9018` retains the original claim-time dribble and releases ball `$02`;
and `$AD41->$AD6D` performs the receiving role/link swap plus `$38/$3C/$3E/$25`
action restoration before returning to dribble `$01`. `$8F8D->$9097` is also
tested with role zero deliberately moved away from physical slot zero. The user
contest translation now includes `$A3E2->$A42D->$B435->$A44B` ground pickup when
the loose ball has no eligible paired shooter. Live native rendering draws all
DDAP metasprite pieces without the NES scanline dropout and clips signed
off-court records before they can wrap into the HUD, while the pre-jump fidelity
renderer keeps its separately verified overflow behavior. These strengthen
existing Verified entries, so the weighted coverage remains **92.6%** rather
than gaining duplicate credit.

The alternating scheduler's cursor semantics are now instruction- and
trace-verified. `$9E76-$9E83` advances persistent `$004D` only on even
`$001A` phases through original slots `$07-$0B`; `$9E86-$9E8D` temporarily
maps it down by five through `$9E90` while dispatching original slots
`$02-$06`, then restores it. Fresh FCEUX instrumentation records `$004D`
alongside `$001A`, and the native regression asserts the complete scheduled
player cycle `8,3,9,4,5,0,6,1,7,2,8`. This closes a scheduler behavior gap but
does not promote an inventory node because `$9E70` is currently a non-function
entry inside the recursive graph; comprehensive coverage remains **86.6%**.
That was the scheduler milestone's score before the additional routine
annotations below.
Native screenshots at frames 356 and 426 remain visually intact. A detached
`0a6ac2e` comparison plus a raw pixel comparison of native frames 357/358
confirms that the complete HUD remains stable across consecutive odd/even
scheduler frames.

Routine-level annotation now closes eight already implemented user/presentation
helpers. `$A129` covers four-direction receiver scoring and its later-slot tie
rule. `$A29D/$A33D/$A342` cover the two-nearest defensive selection, successful
control/role/pair transfer, and no-change exit. `$A85A/$AA20/$AA4A` project and
bound live objects relative to the camera, while `$A896` selects the
state/facing/frame animation. The four directional pass captures, defensive
switch captures, 3,927 natural `$A896` executions, and focused native
receiver/switch/projection/animation regressions promote these eight routines
without changing the already reported component score. Comprehensive coverage
is now **88.4% (166 Verified, 50 Partial)**.

Period transitions/end conditions are Verified. The earlier long trace proves
the delayed period-one reset to a second-period formation. The final-match trace
adds the distinct bank-0 `$93AE` gate: only a zero clock with player slot two
below action `$41` and ball action `$01` or `$07` reaches `$93EC-$93FE`. Original
frame 45337 renders period four at 00:00; frame 45338 writes `$0059=00`, advances
the `$07E8` presentation index, and installs `$0068=0C/$006C=25` while displaying
`GAME SET`. Frames 45596-45597 are solid blue and frame 45620 is the title.
Native tests cover both the in-flight rejection and rebound acceptance, then the
258-frame game-set hold and 282-frame title return. The variable clock call
gaps, timeout gate, period transition, GAME SET hold, and title return are
covered by the Verified game-clock capability.

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

## Closure and maintenance gate

The reachable non-NES address graph is fully inventoried, but behavioral closure
is not complete. The current floor is 215/1/0 for portable routines, 6/1/0 for
the core catalog, and 12/1/0 for match rules. `$A3E2` may return to Verified only
after its live-dribble steal/foul/possession branches are translated, exercised
through shipping input, and reconciled with FCEUX. Any newly discovered reachable
portable routine or branch is added to the denominator immediately and begins
Missing or Partial until it passes the same evidence gates.
