#include "dd_gameplay.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void check(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

static void check_checkpoint(const DDGameplayState *state, uint32_t live_frame,
                             uint32_t player, uint8_t action, uint8_t target) {
    char message[160];
    snprintf(message, sizeof(message), "live %u player %u action expected $%02X, got $%02X",
             live_frame, player, action, state->players[player].action);
    check(state->players[player].action == action, message);
    snprintf(message, sizeof(message), "live %u player %u target expected $%02X, got $%02X",
             live_frame, player, target, state->players[player].target_zone);
    check(state->players[player].target_zone == target, message);
}

static void run_cpu_dispatch(const DDAssetPack *pack, DDGameplayState *state,
                             uint32_t player) {
    uint32_t before = state->players[player].cpu_updates;
    uint32_t attempts;
    for (attempts = 0u; attempts < 3u && state->players[player].cpu_updates == before;
         ++attempts) {
        check(dd_gameplay_step(pack, state, 0u), "step isolated CPU dispatcher state");
    }
    check(state->players[player].cpu_updates != before,
          "isolated CPU dispatcher state receives its scheduled update");
}

int main(int argc, char **argv) {
    static const uint8_t post_inbound_action[DD_GAMEPLAY_PLAYER_COUNT] = {
        0x0Fu, 0x20u, 0x20u, 0x22u, 0x20u, 0x40u, 0x25u, 0x37u, 0x3Cu, 0x3Eu
    };
    static const uint8_t post_inbound_target[DD_GAMEPLAY_PLAYER_COUNT] = {
        0xE8u, 0x48u, 0xCCu, 0xB5u, 0x79u, 0x21u, 0xA6u, 0xD7u, 0xA9u, 0x8Cu
    };
    DDAssetPack pack;
    DDGameplayState state;
    DDGameplayState jump_state;
    DDGameplayState pass_state;
    DDGameplayState dispatch_base;
    DDGameplayState dispatch_state;
    DDGameplayState period_state;
    const DDTipoffAssetsHeader *assets;
    int32_t live_start_x[DD_GAMEPLAY_PLAYER_COUNT];
    int32_t live_start_depth[DD_GAMEPLAY_PLAYER_COUNT];
    uint32_t player;
    if (argc != 2 || !dd_asset_pack_load(argv[1], &pack)) {
        fputs("usage: dd_gameplay_cpu_test <assetpack>\n", stderr);
        return 2;
    }
    assets = (const DDTipoffAssetsHeader *)pack.tipoff_assets;
    check(assets->cpu_role_targets[6] == 0xD5u && assets->cpu_role_targets[7] == 0x5Au &&
          assets->cpu_role_targets[16] == 0x54u && assets->cpu_role_targets[17] == 0xD7u,
          "asset pack exposes the observed role targets at both half-court phases");
    check(assets->cpu_spacing_targets[3] == 0x8Cu && assets->cpu_spacing_targets[13] == 0x4Cu,
          "asset pack exposes the observed opening spacing and cut targets");
    check(memcmp(assets->court_chr_left, assets->court_chr_right,
                 sizeof(assets->court_chr_left)) != 0,
          "asset pack exposes distinct camera-triggered left and right court CHR streams");
    check(pack.tipoff_meta.gameplay_audio_frames == 18u && pack.gameplay_audio_count == 20u,
          "asset pack exposes the observed 18-frame live dribble APU sequence");

    memset(&jump_state, 0, sizeof(jump_state));
    check(dd_gameplay_advance_to(&pack, &jump_state, 300u, 0u),
          "advance to the original user jump window");
    check(jump_state.clock_minutes == 0x04u && jump_state.clock_seconds == 0x59u,
          "bank-0 $9431/$9490 clock reaches the traced 04:59 rollover");
    check(dd_gameplay_step(&pack, &jump_state, DD_INPUT_B), "B starts the tip-off jump");
    check(jump_state.players[0].action == DD_PLAYER_TIP_USER_AIRBORNE &&
          jump_state.tip_user_jump_frame == 301u,
          "native B timing reproduces original frame 2502 state $10->$11");
    check(dd_gameplay_advance_to(&pack, &jump_state, 330u, 0u), "advance to CPU contact");
    check(jump_state.carrier == 5u, "CPU slot wins the first contact frame");
    check(jump_state.hud_split_y == 48u,
          "tip contact changes the raster split so 1ST PERIOD START leaves the fixed HUD");
    check(dd_gameplay_advance_to(&pack, &jump_state, 331u, 0u), "advance to user contact override");
    check(jump_state.carrier == 0u && jump_state.tip_winner == 0u,
          "well-timed user jump overrides possession on the next contact frame");
    check(dd_gameplay_advance_to(&pack, &jump_state, 355u, 0u), "advance to user live handoff");
    check(jump_state.phase == DD_GAMEPLAY_LIVE && jump_state.carrier == 0u &&
          jump_state.ball.action == DD_BALL_DRIBBLE,
          "user jump win reaches movable user possession one frame before the CPU branch");

    memset(&state, 0, sizeof(state));
    check(dd_gameplay_advance_to(&pack, &state, 356u, 0u), "advance to live handoff");
    check(state.cpu_global_frame == 0xDCu, "live handoff preserves original $001A phase");
    check_checkpoint(&state, 0u, 5u, DD_PLAYER_LIVE_CARRIER, 0xD4u);
    for (player = 0u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
        live_start_x[player] = state.players[player].court_x;
        live_start_depth[player] = state.players[player].court_depth;
    }

    check(dd_gameplay_step(&pack, &state, 0u), "first live CPU step");
    check(state.cpu_global_frame == 0xDDu, "first live step advances CPU frame phase");
    check(state.players[1].cpu_updates == 1u && state.players[5].cpu_updates == 0u,
          "odd CPU frame updates only native team slots 1-4");

    check(dd_gameplay_step(&pack, &state, 0u), "second live CPU step");
    check(state.cpu_global_frame == 0xDEu, "second live step advances CPU frame phase");
    check(state.players[1].cpu_updates == 1u && state.players[5].cpu_updates == 1u,
          "even CPU frame updates only native team slots 5-9");
    check(state.cpu_priority_player == 9u, "priority CPU player rotates on the even team frame");
    check_checkpoint(&state, 2u, 5u, DD_PLAYER_LIVE_CARRIER, 0x70u);

    check(dd_gameplay_advance_to(&pack, &state, 380u, 0u), "advance to carrier reroute");
    check_checkpoint(&state, 24u, 5u, DD_PLAYER_LIVE_CARRIER, 0x6Cu);

    check(dd_gameplay_advance_to(&pack, &state, 392u, 0u), "advance to first formation phase boundary");
    check(state.cpu_global_frame == 0x00u, "live 36 reaches the original $001A wrap");
    check_checkpoint(&state, 36u, 6u, DD_PLAYER_LIVE_CPU, 0xD5u);
    check_checkpoint(&state, 36u, 7u, DD_PLAYER_LIVE_CPU, 0x5Au);
    for (player = 1u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
        char message[128];
        snprintf(message, sizeof(message), "live 36 player %u receives CPU updates", player);
        check(state.players[player].cpu_updates != 0u, message);
        snprintf(message, sizeof(message), "live 36 player %u is not frozen", player);
        check(state.players[player].court_x != live_start_x[player] ||
              state.players[player].court_depth != live_start_depth[player], message);
    }

    check(dd_gameplay_advance_to(&pack, &state, 426u, 0u), "advance through the opening cut");
    check_checkpoint(&state, 70u, 8u, DD_PLAYER_LIVE_CPU_CUT_RUN, 0x4Cu);

    check(dd_gameplay_advance_to(&pack, &state, 438u, 0u), "advance through the first spacing route");
    check_checkpoint(&state, 82u, 9u, DD_PLAYER_LIVE_CPU_ROUTE, 0x8Cu);

    check(dd_gameplay_advance_to(&pack, &state, 520u, 0u), "advance to the second formation phase boundary");
    check(state.cpu_global_frame == 0x80u, "live 164 reaches the original half-court phase bit");
    check_checkpoint(&state, 164u, 6u, DD_PLAYER_LIVE_CPU, 0x54u);
    check_checkpoint(&state, 164u, 7u, DD_PLAYER_LIVE_CPU, 0xD7u);
    check_checkpoint(&state, 164u, 5u, DD_PLAYER_LIVE_CARRIER_ROUTE, 0x85u);

    check(dd_gameplay_advance_to(&pack, &state, 522u, 0u), "advance to the carrier decision state");
    check_checkpoint(&state, 166u, 5u, DD_PLAYER_LIVE_CARRIER_DECIDE, 0x85u);
    check(state.ball.action == DD_BALL_SHOT_GATHER,
          "live 166 follows $8D1F into original shot-gather ball state $04");

    check(dd_gameplay_advance_to(&pack, &state, 548u, 0u), "advance to shot release");
    check(state.live_frame == 192u && state.ball.action == DD_BALL_AIRBORNE,
          "live 192 follows $AE25 into airborne shot state $05");
    check(dd_gameplay_advance_to(&pack, &state, 569u, 0u), "advance to scoring result");
    check(state.live_frame == 213u && state.ball.action == DD_BALL_SCORE,
          "live 213 reaches original score/rim state $06");
    check(state.score[1] == 2u && state.score[0] == 0u,
          "first CPU make updates the native right-side score to two points");
    check(dd_gameplay_advance_to(&pack, &state, 582u, 0u), "advance to rebound state");
    check(state.live_frame == 226u && state.ball.action == DD_BALL_REBOUND,
          "live 226 reaches original rebound state $07");
    check(dd_gameplay_advance_to(&pack, &state, 743u, 0u), "advance to loose-ball recovery");
    check(state.live_frame == 387u && state.ball.action == DD_BALL_DRIBBLE && state.carrier == 0u,
          "live 387 reproduces the observed user-side recovery");
    check(dd_gameplay_advance_to(&pack, &state, 803u, 0u), "advance to out-of-bounds ball");
    check(state.live_frame == 447u && state.ball.action == DD_BALL_AWARDED && state.carrier == 0xFFu,
          "live 447 releases the recovered ball before the inbound decision");
    check(dd_gameplay_advance_to(&pack, &state, 1123u, 0u), "advance to inbound setup");
    check(state.live_frame == 767u && state.phase == DD_GAMEPLAY_INBOUND &&
          state.ball.action == DD_BALL_DEAD &&
          state.players[5].action == DD_PLAYER_INBOUNDER,
          "live 767 reproduces $9583/$9645 dead-ball formation and inbounder state $41");
    check(dd_gameplay_advance_to(&pack, &state, 1300u, 0u), "advance to inbound hold");
    check(state.live_frame == 944u && state.carrier == 5u &&
          state.players[5].action == DD_PLAYER_INBOUND_HOLD,
          "live 944 gives the inbounder the held ball in state $30");
    check(dd_gameplay_advance_to(&pack, &state, 1352u, 0u), "advance to inbound pass");
    check(state.live_frame == 996u && state.ball.action == DD_BALL_PASS &&
          state.ball.receiver == 6u,
          "live 996 starts original pass state $02 toward the adjacent receiver");
    check(dd_gameplay_advance_to(&pack, &state, 1371u, 0u), "advance to inbound reception");
    check(state.live_frame == 1015u && state.phase == DD_GAMEPLAY_LIVE &&
          state.ball.action == DD_BALL_DRIBBLE && state.carrier == 6u,
          "live 1015 completes the inbound and resumes CPU possession decisions");
    check(state.clock_minutes == 0x04u && state.clock_seconds == 0x26u,
          "native HUD clock matches original frame 3572 at 04:26");
    for (player = 0u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
        check_checkpoint(&state, 1015u, player,
                         post_inbound_action[player], post_inbound_target[player]);
        live_start_x[player] = state.players[player].court_x;
        live_start_depth[player] = state.players[player].court_depth;
    }
    check(dd_gameplay_advance_to(&pack, &state, 1399u, 0u),
          "advance 28 frames through the original post-inbound decision window");
    check(state.ball.action == DD_BALL_SHOT_GATHER && state.carrier == 6u,
          "post-inbound state $25 reaches $27/ball state $04 instead of replaying the basket run");
    check(state.players[1].court_x != live_start_x[1] ||
          state.players[1].court_depth != live_start_depth[1],
          "off-ball state $20 continues moving after the inbound reception");
    check(state.players[8].court_x != live_start_x[8] ||
          state.players[8].court_depth != live_start_depth[8],
          "off-ball cut state $3C continues moving after the inbound reception");

    memset(&pass_state, 0, sizeof(pass_state));
    check(dd_gameplay_advance_to(&pack, &pass_state, 356u, 0u), "prepare far-court pass decision");
    pass_state.players[5].action = DD_PLAYER_LIVE_CARRIER_DECIDE;
    pass_state.players[5].court_x = 0x015000;
    pass_state.ball.court_x = pass_state.players[5].court_x;
    pass_state.ball.court_depth = pass_state.players[5].court_depth;
    check(dd_gameplay_step(&pack, &pass_state, 0u) && dd_gameplay_step(&pack, &pass_state, 0u),
          "run far-court CPU decision turn");
    check(pass_state.ball.action == DD_BALL_PASS && pass_state.ball.receiver >= 5u &&
          pass_state.ball.receiver < DD_GAMEPLAY_PLAYER_COUNT,
          "carrier decision selects a teammate pass outside the shooting region");

    memset(&dispatch_base, 0, sizeof(dispatch_base));
    check(dd_gameplay_advance_to(&pack, &dispatch_base, 356u, 0u),
          "prepare isolated dispatcher checks");

    dispatch_state = dispatch_base;
    dispatch_state.players[5].action = DD_PLAYER_LIVE_FOLLOW_TARGET;
    dispatch_state.players[5].target_x = dispatch_state.players[5].court_x;
    dispatch_state.players[5].target_depth = dispatch_state.players[5].court_depth;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_LIVE_TEAMMATE,
          "player state $21 returns to $20 on target arrival");

    dispatch_state = dispatch_base;
    dispatch_state.players[5].action = DD_PLAYER_JUMP_START;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_JUMP_CONTEST &&
          dispatch_state.players[5].velocity_height > 0,
          "player state $23 installs the jump and advances to $24");

    dispatch_state = dispatch_base;
    dispatch_state.players[5].action = DD_PLAYER_JUMP_CONTEST;
    dispatch_state.players[5].action_age = 3u;
    dispatch_state.players[5].height = 0x1000;
    dispatch_state.players[5].velocity_height = -0x0100;
    dispatch_state.ball.owner = 0xFFu;
    dispatch_state.carrier = 0xFFu;
    dispatch_state.ball.court_x = 0x001000;
    dispatch_state.ball.court_depth = 0x0400;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_LIVE_SHOOTER_RECOVER &&
          dispatch_state.players[5].height == 0x1000,
          "player state $24 lands into recovery when no loose ball is contacted");

    dispatch_state = dispatch_base;
    dispatch_state.players[5].action = DD_PLAYER_JUMP_CONTEST;
    dispatch_state.players[5].height = 0x2000;
    dispatch_state.players[5].velocity_height = 0;
    dispatch_state.ball.owner = 0xFFu;
    dispatch_state.carrier = 0xFFu;
    dispatch_state.ball.court_x = dispatch_state.players[5].court_x - 0x0600;
    dispatch_state.ball.height = dispatch_state.players[5].height + 0x0800;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.ball.owner == 5u && dispatch_state.carrier == 5u,
          "player state $24 uses $A6C3's shifted 4x4 boxes to claim a contacted ball");

    dispatch_state = dispatch_base;
    dispatch_state.players[5].action = DD_PLAYER_LIVE_CONTINUE;
    dispatch_state.players[5].target_x += 0x2000;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].velocity_x != 0,
          "shared movement state $2C executes the $8BC5 continuation");

    dispatch_state = dispatch_base;
    dispatch_state.players[5].action = DD_PLAYER_REBOUND_CHASE;
    dispatch_state.players[5].target_x = dispatch_state.players[5].court_x;
    dispatch_state.players[5].target_depth = dispatch_state.players[5].court_depth;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_REBOUND_CLAIM,
          "player state $2D advances to rebound claim $2E at its target");

    dispatch_state = dispatch_base;
    dispatch_state.players[5].action = DD_PLAYER_REBOUND_CLAIM;
    dispatch_state.ball.action = DD_BALL_REBOUND;
    dispatch_state.ball.owner = 0xFFu;
    dispatch_state.carrier = 0xFFu;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_REBOUND_RETURN &&
          dispatch_state.ball.action == DD_BALL_DRIBBLE && dispatch_state.ball.owner == 5u,
          "player state $2E claims a non-shot ball and advances to $2F");

    dispatch_state = dispatch_base;
    dispatch_state.players[5].action = DD_PLAYER_REBOUND_RETURN;
    dispatch_state.players[5].target_x = dispatch_state.players[5].court_x;
    dispatch_state.players[5].target_depth = dispatch_state.players[5].court_depth;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_INBOUND_HOLD,
          "player state $2F advances to held state $30 at its return point");

    for (player = DD_PLAYER_LIVE_CONTINUE_33; player <= DD_PLAYER_LIVE_CONTINUE_34; ++player) {
        dispatch_state = dispatch_base;
        dispatch_state.players[5].action = (uint8_t)player;
        dispatch_state.players[5].target_x += 0x2000;
        run_cpu_dispatch(&pack, &dispatch_state, 5u);
        check(dispatch_state.players[5].velocity_x != 0,
              "shared movement states $33/$34 execute the $8BC5 continuation");
    }

    dispatch_state = dispatch_base;
    dispatch_state.players[5].action = DD_PLAYER_FORMATION_CPU;
    dispatch_state.players[5].target_x += 0x2000;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].velocity_x != 0,
          "player formation state $35 has an explicit movement continuation");

    dispatch_state = dispatch_base;
    dispatch_state.players[5].action = DD_PLAYER_ROUTE_INIT;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_ROUTE_APPROACH,
          "player state $38 selects a spacing target and advances to $39");

    dispatch_state = dispatch_base;
    dispatch_state.players[5].action = DD_PLAYER_ROUTE_APPROACH;
    dispatch_state.players[5].role = 0u;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_LIVE_CPU_SETUP,
          "player state $39 routes role zero to state $32");

    dispatch_state = dispatch_base;
    dispatch_state.players[5].action = DD_PLAYER_ROUTE_ADJUST;
    dispatch_state.players[5].role = 1u;
    dispatch_state.players[5].target_x = dispatch_state.players[5].court_x;
    dispatch_state.players[5].target_depth = dispatch_state.players[5].court_depth;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action != DD_PLAYER_ROUTE_ADJUST,
          "player state $3A advances into the regional route family");

    dispatch_state = dispatch_base;
    dispatch_state.players[5].action = DD_PLAYER_ROUTE_WAIT;
    dispatch_state.players[5].velocity_x = 0;
    live_start_x[5] = dispatch_state.players[5].court_x;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_ROUTE_WAIT &&
          dispatch_state.players[5].court_x == live_start_x[5],
          "player state $3B preserves the original RTS no-op behavior");

    dispatch_state = dispatch_base;
    dispatch_state.players[5].action = DD_PLAYER_LIVE_RENDER_ONLY;
    live_start_x[5] = dispatch_state.players[5].court_x;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_LIVE_RENDER_ONLY &&
          dispatch_state.players[5].court_x == live_start_x[5],
          "player state $3F executes only its render/animation continuation");

    dispatch_state = dispatch_base;
    dispatch_state.players[5].action = DD_PLAYER_INBOUNDER;
    dispatch_state.players[5].target_x = dispatch_state.players[5].court_x;
    dispatch_state.players[5].target_depth = dispatch_state.players[5].court_depth;
    dispatch_state.ball.owner = 0xFFu;
    dispatch_state.carrier = 0xFFu;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_INBOUND_HOLD &&
          dispatch_state.ball.owner == 5u,
          "player state $41 claims the inbound ball and advances to $30");

    dispatch_state = dispatch_base;
    dispatch_state.ball.action = DD_BALL_PASS;
    dispatch_state.ball.owner = 0xFFu;
    dispatch_state.ball.receiver = 0u;
    dispatch_state.ball.action_age = 18u;
    dispatch_state.ball.court_x = dispatch_state.players[0].court_x;
    dispatch_state.ball.court_depth = dispatch_state.players[0].court_depth;
    check(dd_gameplay_step(&pack, &dispatch_state, 0u),
          "step pass at the original reception age inside $B138 boxes");
    check(dispatch_state.ball.action == DD_BALL_DRIBBLE && dispatch_state.ball.owner == 0u,
          "ball state $02 catches only after $B138 reports receiver overlap");

    dispatch_state = dispatch_base;
    dispatch_state.ball.action = DD_BALL_PASS;
    dispatch_state.ball.owner = 0xFFu;
    dispatch_state.ball.receiver = 0u;
    dispatch_state.ball.action_age = 18u;
    dispatch_state.ball.court_x = dispatch_state.players[0].court_x + 0x0E00;
    dispatch_state.ball.court_depth = dispatch_state.players[0].court_depth;
    check(dd_gameplay_step(&pack, &dispatch_state, 0u),
          "step pass at $B138's exclusive collision boundary");
    check(dispatch_state.ball.action == DD_BALL_PASS && dispatch_state.ball.owner == 0xFFu,
          "$B138's combined 14-unit half extent excludes its upper boundary");

    dispatch_state = dispatch_base;
    dispatch_state.ball.action = DD_BALL_PASS_BOUNCE;
    dispatch_state.ball.owner = 5u;
    dispatch_state.carrier = 5u;
    dispatch_state.ball.receiver = 0xFFu;
    dispatch_state.ball.court_x = 0x004500;
    dispatch_state.ball.court_depth = 0x005800;
    dispatch_state.ball.height = 0x004600;
    dispatch_state.ball.velocity_x = 0x0100;
    dispatch_state.ball.velocity_depth = 0;
    dispatch_state.ball.velocity_height = 0;
    dispatch_state.possession_direction = 0u;
    check(dd_gameplay_step(&pack, &dispatch_state, 0u),
          "step ball through $B473's first left-rim sample");
    check(dispatch_state.ball.rim_contact == 1u &&
          dispatch_state.ball.owner == 0xFFu && dispatch_state.carrier == 5u &&
          dispatch_state.ball.velocity_x == -0x0100,
          "$B473 latches rim contact, clears ownership, retains camera follow, and reverses velocity");

    dispatch_state = dispatch_base;
    dispatch_state.ball.action = DD_BALL_PASS_BOUNCE;
    dispatch_state.ball.owner = 0xFFu;
    dispatch_state.ball.receiver = 5u;
    dispatch_state.ball.action_age = 5u;
    dispatch_state.ball.court_x = dispatch_state.players[5].court_x;
    dispatch_state.ball.court_depth = dispatch_state.players[5].court_depth;
    dispatch_state.ball.height = 0x1000;
    dispatch_state.ball.velocity_x = 0;
    dispatch_state.ball.velocity_depth = 0;
    dispatch_state.ball.velocity_height = 0;
    check(dd_gameplay_step(&pack, &dispatch_state, 0u), "step ball bounce-pass state $03");
    check(dispatch_state.ball.action == DD_BALL_HIDDEN,
          "ball state $03 follows $ADF2's zero vertical-term branch into state $0C");

    dispatch_state = dispatch_base;
    dispatch_state.ball.action = DD_BALL_AIRBORNE;
    dispatch_state.ball.owner = 0xFFu;
    dispatch_state.ball.court_x = 0x004C00;
    dispatch_state.ball.court_depth = 0x005800;
    dispatch_state.ball.height = 0x003600;
    dispatch_state.ball.velocity_x = -0x00FA;
    dispatch_state.ball.velocity_depth = 0x0032;
    dispatch_state.last_shooter = 5u;
    dispatch_state.possession_direction = 0u;
    dispatch_state.score[1] = 0u;
    check(dd_gameplay_step(&pack, &dispatch_state, 0u),
          "step $B377 result-four hoop contact");
    check(dispatch_state.ball.action == DD_BALL_LOOSE_LAUNCH &&
          dispatch_state.ball.outcome == 4u && dispatch_state.score[1] == 0u,
          "$B377 result four follows $AE25's missed-shot branch into state $08");
    check(dd_gameplay_step(&pack, &dispatch_state, 0u),
          "step traced result-four loose-ball initializer");
    check(dispatch_state.ball.action == DD_BALL_LOOSE_AIRBORNE &&
          dispatch_state.ball.height == 0x3800 &&
          dispatch_state.ball.velocity_x == 0x007D &&
          dispatch_state.ball.velocity_depth == -0x0019,
          "$AF72 reverses and halves both miss velocities before state $09");

    dispatch_state = dispatch_base;
    dispatch_state.ball.action = DD_BALL_LOOSE_LAUNCH;
    check(dd_gameplay_step(&pack, &dispatch_state, 0u), "step loose-ball launch state $08");
    check(dispatch_state.ball.action == DD_BALL_LOOSE_AIRBORNE &&
          dispatch_state.ball.owner == 0xFFu,
          "ball state $08 initializes velocity and advances to $09");

    dispatch_state = dispatch_base;
    dispatch_state.ball.action = DD_BALL_LOOSE_AIRBORNE;
    dispatch_state.ball.action_age = 0u;
    dispatch_state.ball.height = 0x38D8;
    dispatch_state.ball.velocity_height = 0x0100;
    for (player = 0u; player < 60u; ++player) {
        check(dd_gameplay_step(&pack, &dispatch_state, 0u),
              "step traced loose-ball airborne arc");
    }
    check(dispatch_state.ball.action == DD_BALL_LOOSE_AIRBORNE,
          "ball state $09 remains airborne through its traced sixtieth frame");
    check(dd_gameplay_step(&pack, &dispatch_state, 0u), "finish loose-ball airborne state $09");
    check(dispatch_state.ball.action == DD_BALL_REBOUND,
          "ball state $09 reaches rebound state $07 on traced frame 61");

    dispatch_state = dispatch_base;
    dispatch_state.ball.action = DD_BALL_SHOT_LAUNCH;
    check(dd_gameplay_step(&pack, &dispatch_state, 0u), "step shot initializer state $0A");
    check(dispatch_state.ball.action == DD_BALL_AIRBORNE &&
          dispatch_state.ball.velocity_height == 0x0500,
          "ball state $0A initializes and advances to airborne state $05");

    dispatch_state = dispatch_base;
    dispatch_state.ball.action = DD_BALL_HIDDEN;
    dispatch_state.ball.height = 0x2200;
    check(dd_gameplay_step(&pack, &dispatch_state, 0u), "step hidden ball state $0C");
    check(dispatch_state.ball.action == DD_BALL_HIDDEN && dispatch_state.ball.height == 0,
          "ball state $0C shares the original $ACAB zero-height handler");

    period_state = dispatch_base;
    period_state.clock_minutes = 0u;
    period_state.clock_seconds = 0u;
    period_state.clock_expired = 1;
    period_state.clock_expired_frame = period_state.scene_frame - 214u;
    check(dd_gameplay_step(&pack, &period_state, 0u), "step traced end-of-period reset delay");
    check(period_state.period == 2u && period_state.phase == DD_GAMEPLAY_FORMATION &&
          period_state.clock_minutes == 0x05u && period_state.clock_seconds == 0u &&
          period_state.players[0].action == DD_PLAYER_FORMATION_USER &&
          period_state.players[5].action == DD_PLAYER_TIP_CPU,
          "expired period resets the 2ND PERIOD formation and five-minute clock");

    dd_asset_pack_unload(&pack);
    if (failures != 0) {
        fprintf(stderr, "%d CPU gameplay regression check(s) failed.\n", failures);
        return 1;
    }
    puts("Gameplay regression checks passed: tip jump, dispatcher states, camera CHR, moving off-ball players, pass/shot states, rebound, inbound reception, and live audio data.");
    return 0;
}
