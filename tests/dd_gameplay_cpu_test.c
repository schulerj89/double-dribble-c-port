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
    check(assets->cpu_region_targets[0] == 0x96u &&
          assets->cpu_region_targets[1] == 0xECu &&
          assets->cpu_region_targets[6] == 0x25u,
          "asset pack exposes $AC78's seven route-init region targets");
    check(memcmp(assets->court_chr_left, assets->court_chr_right,
                 sizeof(assets->court_chr_left)) != 0,
          "asset pack exposes distinct camera-triggered left and right court CHR streams");
    check(pack.tipoff_meta.gameplay_audio_frames == 18u && pack.gameplay_audio_count == 20u,
          "asset pack exposes the observed 18-frame live dribble APU sequence");
    check((uint8_t)assets->height_scripts[10] == 0x80u &&
          assets->height_scripts[11] == 5 &&
          (uint8_t)assets->height_scripts[24] == 0x81u,
          "asset pack exposes $9B34's jump stream with reverse and landing sentinels");

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
    check(state.score[1] == 0u && state.score[0] == 0u,
          "entering score state $06 does not award points before $AEDE counter $08");
    check(dd_gameplay_advance_to(&pack, &state, 573u, 0u),
          "advance four score-state dispatches to $AEDE counter $08");
    check(state.live_frame == 217u && state.ball.action == DD_BALL_SCORE &&
          state.score[1] == 2u && state.score[0] == 0u,
          "$AEDE counter $08 updates the native right-side score to two points");
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
    check(dd_gameplay_advance_to(&pack, &state, 1344u, 0u), "advance to inbound release setup");
    check(state.live_frame == 988u &&
          state.players[5].action == DD_PLAYER_INBOUND_READY &&
          state.players[5].release_timer == 8u &&
          state.ball.action == DD_BALL_AWARDED,
          "live 988 follows formation readiness through $9018 into state $31");
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
    dispatch_state.players[5].target_zone = (uint8_t)(
        (((uint32_t)(dispatch_state.players[5].court_depth >> 8) << 1u) & 0xE0u) |
        (((uint32_t)dispatch_state.players[5].court_x >> 12u) & 0x1Fu));
    dispatch_state.players[5].route_step = 5u;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_LIVE_TEAMMATE &&
          dispatch_state.players[5].route_step == 0u,
          "player state $21 follows $D978 packed arrival and clears $0600 before $20");

    dispatch_state = dispatch_base;
    dispatch_state.players[5].action = DD_PLAYER_LIVE_CARRIER;
    dispatch_state.players[5].route_step = 1u;
    dispatch_state.players[5].action_age = 0u;
    dispatch_state.players[5].court_x = 0x010800;
    dispatch_state.players[5].court_depth = 0x005800;
    dispatch_state.players[5].facing = 4u;
    dispatch_state.players[5].target_zone = 0xD4u;
    dispatch_state.players[0].court_x = 0x00F800;
    dispatch_state.players[0].court_depth = 0x005800;
    dispatch_state.ball.court_x = 0x001000;
    dispatch_state.ball.court_depth = 0x002000;
    dispatch_state.cpu_global_frame = 0xDCu;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].target_zone == 0x70u,
          "$D99A reproduces original frame 2559 packed avoidance target $D4->$70");

    dispatch_state = dispatch_base;
    dispatch_state.players[5].action = DD_PLAYER_LIVE_CPU_SETUP;
    dispatch_state.players[5].court_x = 0x010800;
    dispatch_state.players[5].court_depth = 0x005800;
    dispatch_state.players[5].facing = 4u;
    dispatch_state.players[5].target_zone = 0x85u;
    dispatch_state.players[0].court_x = 0x004800;
    dispatch_state.players[0].court_depth = 0x003800;
    dispatch_state.ball.court_x = 0x001000;
    dispatch_state.ball.court_depth = 0x002000;
    dispatch_state.cpu_global_frame = 0xDCu;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].target_zone == 0x85u,
          "$D99A leaves the CPU target unchanged when forward lookahead is clear");

    dispatch_state = dispatch_base;
    dispatch_state.players[5].action = DD_PLAYER_LIVE_TEAMMATE;
    dispatch_state.players[5].court_x = 0x010000;
    dispatch_state.players[5].court_depth = 0x005800;
    dispatch_state.players[0].court_x = 0x010000;
    dispatch_state.players[0].court_depth = 0x005800;
    dispatch_state.ball.owner = 0u;
    dispatch_state.carrier = 0u;
    dispatch_state.ball.court_x = 0x001000;
    dispatch_state.ball.court_depth = 0x001000;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_LIVE_PAIRED_DEFENDER &&
          dispatch_state.players[5].paired_timer == 0x10u,
          "player state $20 follows $9102 contact into $22 with latch $10");
    dispatch_state.players[0].court_x = 0x014000;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_LIVE_TEAMMATE &&
          dispatch_state.players[5].velocity_x == 0 &&
          dispatch_state.players[5].velocity_depth == 0,
          "player state $22 returns to $20 when $9102 reports separation");
    dispatch_state.players[0].action = 0x03u;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_JUMP_START,
          "player state $20 follows $9139 paired action $03 into jump state $23");

    dispatch_state = dispatch_base;
    dispatch_state.players[5].action = DD_PLAYER_JUMP_START;
    dispatch_state.players[5].height = 0x1055;
    dispatch_state.players[5].velocity_x = 0x0123;
    dispatch_state.players[5].velocity_depth = -0x0124;
    dispatch_state.players[5].velocity_height = 0x0345;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_JUMP_CONTEST &&
          dispatch_state.players[5].height == 0x1055 &&
          dispatch_state.players[5].velocity_x == 0 &&
          dispatch_state.players[5].velocity_depth == 0 &&
          dispatch_state.players[5].velocity_height == 0 &&
          dispatch_state.players[5].height_script_index == 11u &&
          dispatch_state.players[5].height_script_reverse == 0u,
          "player state $23 clears motion and installs $9B34 before advancing to $24");

    dispatch_state = dispatch_base;
    dispatch_state.players[5].action = DD_PLAYER_JUMP_CONTEST;
    dispatch_state.players[5].height = 0x1055;
    dispatch_state.players[5].height_script_index = 11u;
    dispatch_state.ball.owner = 0u;
    dispatch_state.carrier = 0u;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].height == 0x1555 &&
          dispatch_state.players[5].height_script_index == 12u,
          "$9ABD applies $9B34's first +5 integer-height delta and preserves fraction");
    for (player = 1u; player < 14u; ++player) {
        run_cpu_dispatch(&pack, &dispatch_state, 5u);
    }
    check(dispatch_state.players[5].height == 0x2655 &&
          dispatch_state.players[5].height_script_reverse == 1u &&
          dispatch_state.players[5].height_script_index == 22u,
          "$9ABD reaches $26, consumes $81, and reverses the jump stream");
    for (player = 14u; player < 27u; ++player) {
        run_cpu_dispatch(&pack, &dispatch_state, 5u);
    }
    check(dispatch_state.players[5].action == DD_PLAYER_LIVE_SHOOTER_RECOVER &&
          dispatch_state.players[5].height == 0x1055 &&
          dispatch_state.players[5].action_age == 16u,
          "player state $24 lands through $80 into the original $10 recovery countdown");

    dispatch_state = dispatch_base;
    dispatch_state.players[5].action = DD_PLAYER_TIP_CPU;
    dispatch_state.object_phase = 0x1Fu;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_TIP_CPU,
          "player state $2A waits while shared object phase remains below $20");
    dispatch_state.object_phase = 0x20u;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_TIP_CPU_AIRBORNE &&
          dispatch_state.players[5].height_script_index == 11u &&
          dispatch_state.players[5].height_script_reverse == 0u,
          "player state $2A installs $9B34 and advances to $2B at phase $20");

    dispatch_state = dispatch_base;
    dispatch_state.players[5].action = DD_PLAYER_TIP_CPU_AIRBORNE;
    dispatch_state.players[5].height = 0x1055;
    dispatch_state.players[5].height_script_index = 11u;
    dispatch_state.ball.owner = 5u;
    dispatch_state.carrier = 0xFFu;
    for (player = 0u; player < 27u; ++player) {
        run_cpu_dispatch(&pack, &dispatch_state, 5u);
    }
    check(dispatch_state.players[5].action == DD_PLAYER_LIVE_CARRIER &&
          dispatch_state.players[5].height == 0x1055 &&
          dispatch_state.ball.action == DD_BALL_DRIBBLE &&
          dispatch_state.carrier == 5u,
          "player state $2B completes $9ABD and awards state $25 to the tip owner");

    dispatch_state = dispatch_base;
    dispatch_state.players[5].action = DD_PLAYER_TIP_CPU_AIRBORNE;
    dispatch_state.players[5].height_script_index = 10u;
    dispatch_state.ball.owner = 0u;
    dispatch_state.carrier = 0u;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_LIVE_TEAMMATE &&
          dispatch_state.players[9].action == DD_PLAYER_LIVE_TEAMMATE,
          "player state $2B returns the losing five-player side to state $20");

    dispatch_state = dispatch_base;
    dispatch_state.players[5].action = DD_PLAYER_JUMP_CONTEST;
    dispatch_state.players[5].height = 0x2000;
    dispatch_state.players[5].height_script_index = 11u;
    dispatch_state.ball.owner = 0xFFu;
    dispatch_state.carrier = 0xFFu;
    dispatch_state.ball.court_x = dispatch_state.players[5].court_x - 0x0600;
    dispatch_state.ball.height = dispatch_state.players[5].height + 0x0800;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.ball.owner == 5u && dispatch_state.carrier == 5u,
          "player state $24 uses $A6C3's shifted 4x4 boxes to claim a contacted ball");

    dispatch_state = dispatch_base;
    dispatch_state.players[5].action = DD_PLAYER_LIVE_CONTINUE;
    dispatch_state.players[5].velocity_x = 0x0123;
    dispatch_state.players[5].velocity_depth = -0x0124;
    live_start_x[5] = dispatch_state.players[5].court_x;
    live_start_depth[5] = dispatch_state.players[5].court_depth;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].court_x == live_start_x[5] + 0x0246 &&
          dispatch_state.players[5].court_depth == live_start_depth[5] - 0x0248 &&
          dispatch_state.players[5].velocity_x == 0x0123 &&
          dispatch_state.players[5].velocity_depth == -0x0124,
          "shared state $2C integrates both fixed-point vectors twice through $A84C");

    dispatch_state = dispatch_base;
    dispatch_state.players[5].action = DD_PLAYER_LIVE_SHOOTER_RECOVER;
    dispatch_state.players[5].action_age = 0u;
    dispatch_state.ball.action = DD_BALL_HIDDEN;
    for (player = 0u; player < 32u; ++player) {
        run_cpu_dispatch(&pack, &dispatch_state, 5u);
    }
    check(dispatch_state.players[5].action == DD_PLAYER_LIVE_SHOOTER_RECOVER &&
          dispatch_state.players[5].action_age == 32u,
          "player state $28 retains the original $04F0=$20 countdown through zero");
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_LIVE_SHOOTER_RESET,
          "player state $28 advances to $29 on its 33rd scheduled update");

    dispatch_state = dispatch_base;
    dispatch_state.players[5].action = DD_PLAYER_LIVE_SHOOTER_RESET;
    /* The even-team scheduler advances 9->5 immediately before slot 5. */
    dispatch_state.cpu_priority_player = 9u;
    dispatch_state.ball.action = DD_BALL_LOOSE_AIRBORNE;
    dispatch_state.ball.owner = 0xFFu;
    dispatch_state.carrier = 0xFFu;
    dispatch_state.ball.court_x = dispatch_state.players[5].court_x + 0x2000;
    dispatch_state.ball.court_depth = dispatch_state.players[5].court_depth + 0x1000;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_LIVE_SHOOTER_RESET &&
          dispatch_state.players[5].target_x == dispatch_state.ball.court_x &&
          dispatch_state.players[5].target_depth == dispatch_state.ball.court_depth,
          "priority player state $29 copies the live ball target before chasing it");

    dispatch_state = dispatch_base;
    dispatch_state.players[5].action = DD_PLAYER_LIVE_SHOOTER_RESET;
    dispatch_state.ball.action = DD_BALL_REBOUND;
    dispatch_state.ball.owner = 0xFFu;
    dispatch_state.carrier = 0xFFu;
    dispatch_state.ball.court_x = dispatch_state.players[5].court_x;
    dispatch_state.ball.court_depth = dispatch_state.players[5].court_depth;
    dispatch_state.ball.height = dispatch_state.players[5].height + 0x0800;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.ball.owner == 5u && dispatch_state.carrier == 5u &&
          dispatch_state.players[5].action == DD_PLAYER_LIVE_CARRIER,
          "player state $29 uses immediate $91FB/$B435 contact to run $A44B possession transfer");

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

    dispatch_state = dispatch_base;
    for (player = 0u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
        dispatch_state.players[player].action = DD_PLAYER_LIVE_SET;
    }
    dispatch_state.players[5].action = DD_PLAYER_INBOUND_HOLD;
    dispatch_state.players[5].hold_timer = 0x0Bu;
    dispatch_state.ball.owner = 5u;
    dispatch_state.carrier = 5u;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_INBOUND_READY &&
          dispatch_state.players[5].hold_timer == 0x0Au &&
          dispatch_state.players[5].release_timer == 8u &&
          dispatch_state.ball.action == DD_BALL_AWARDED &&
          dispatch_state.ball.receiver == 6u &&
          dispatch_state.players[0].action == DD_PLAYER_LIVE_USER &&
          dispatch_state.players[1].action == DD_PLAYER_LIVE_TEAMMATE,
          "player state $30 waits for formation $37 then installs $9018 release state $31");

    dispatch_state = dispatch_base;
    for (player = 0u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
        dispatch_state.players[player].action = DD_PLAYER_LIVE_SET;
        dispatch_state.players[player].role = (uint8_t)(player % 5u);
    }
    dispatch_state.players[5].action = DD_PLAYER_INBOUND_HOLD;
    dispatch_state.players[5].hold_timer = 0x0Au;
    dispatch_state.inbound_variant = 1u;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_LIVE_USER_INBOUND &&
          dispatch_state.controlled_player == 5u &&
          dispatch_state.ball.action == DD_BALL_AWARDED,
          "player state $30 mode-bit $40 branch installs selected action $0D");
    dispatch_state = dispatch_base;
    for (player = 0u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
        dispatch_state.players[player].action = DD_PLAYER_LIVE_SET;
        dispatch_state.players[player].role = (uint8_t)(player % 5u);
    }
    dispatch_state.players[5].action = DD_PLAYER_INBOUND_HOLD;
    dispatch_state.players[5].hold_timer = 0x0Au;
    dispatch_state.inbound_variant = 2u;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_LIVE_USER_INBOUND &&
          dispatch_state.players[0].action == DD_PLAYER_LIVE_USER,
          "player state $30 $002C branch also installs opposite role-zero action $0F");

    dispatch_state = dispatch_base;
    dispatch_state.players[5].action = DD_PLAYER_INBOUND_READY;
    dispatch_state.players[5].release_timer = 8u;
    dispatch_state.players[5].animation = 0x40u;
    dispatch_state.ball.owner = 5u;
    dispatch_state.ball.receiver = 6u;
    dispatch_state.carrier = 5u;
    for (player = 0u; player < 4u; ++player) {
        run_cpu_dispatch(&pack, &dispatch_state, 5u);
    }
    check(dispatch_state.players[5].action == DD_PLAYER_INBOUND_READY &&
          dispatch_state.players[5].release_timer == 4u &&
          dispatch_state.players[5].animation == 0x30u &&
          dispatch_state.ball.action == DD_BALL_PASS &&
          dispatch_state.carrier == 0xFFu,
          "player state $31 launches ball $02 and lifts its sprite at countdown $04");
    dispatch_state.players[5].release_timer = 0u;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_LIVE_CPU &&
          dispatch_state.players[5].release_timer == 0xFFu,
          "player state $31 underflow follows $8FE0->$9014 into state $40");

    for (player = DD_PLAYER_LIVE_CONTINUE_33; player <= DD_PLAYER_LIVE_CONTINUE_34; ++player) {
        dispatch_state = dispatch_base;
        dispatch_state.players[5].action = (uint8_t)player;
        dispatch_state.players[5].velocity_x = 0x0123;
        dispatch_state.players[5].velocity_depth = -0x0124;
        live_start_x[5] = dispatch_state.players[5].court_x;
        live_start_depth[5] = dispatch_state.players[5].court_depth;
        run_cpu_dispatch(&pack, &dispatch_state, 5u);
        check(dispatch_state.players[5].court_x == live_start_x[5] + 0x0246 &&
              dispatch_state.players[5].court_depth == live_start_depth[5] - 0x0248,
              "shared movement states $33/$34 execute $8BC5's double integration");
    }

    dispatch_state = dispatch_base;
    dispatch_state.players[5].action = DD_PLAYER_FORMATION_CPU;
    dispatch_state.players[5].velocity_x = 0x0123;
    dispatch_state.players[5].velocity_depth = -0x0124;
    live_start_x[5] = dispatch_state.players[5].court_x;
    live_start_depth[5] = dispatch_state.players[5].court_depth;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].court_x == live_start_x[5] + 0x0246 &&
          dispatch_state.players[5].court_depth == live_start_depth[5] - 0x0248,
          "player state $35 shares $8BC5's double fixed-point integration");

    dispatch_state = dispatch_base;
    dispatch_state.players[5].action = DD_PLAYER_ROUTE_INIT;
    dispatch_state.players[5].court_x = 0x00B000;
    dispatch_state.players[5].court_depth = 0x7000;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_ROUTE_APPROACH &&
          dispatch_state.players[5].target_zone == 0xECu,
          "player state $38 uses $AC2A region one and $AC78 target $EC before $39");

    dispatch_state = dispatch_base;
    dispatch_state.players[5].action = DD_PLAYER_ROUTE_APPROACH;
    dispatch_state.players[5].role = 0u;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_LIVE_CPU_SETUP,
          "player state $39 routes role zero to state $32");

    dispatch_state = dispatch_base;
    dispatch_state.possession_direction = 0u;
    dispatch_state.cpu_global_frame = 1u;
    dispatch_state.players[5].action = DD_PLAYER_ROUTE_APPROACH;
    dispatch_state.players[5].role = 4u;
    dispatch_state.players[5].court_x = 0x00C000;
    dispatch_state.players[5].court_depth = 0x7000;
    dispatch_state.players[5].target_zone = 0x8Cu;
    dispatch_state.players[9].role = 0u;
    dispatch_state.players[9].court_x = 0x00B000;
    dispatch_state.players[9].court_depth = 0x7000;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_ROUTE_ADJUST &&
          dispatch_state.players[5].target_zone == 0xABu,
          "$81A2 same-region search rejects overflowing +95 then accepts -65 target $AB");

    dispatch_state = dispatch_base;
    dispatch_state.possession_direction = 0u;
    dispatch_state.cpu_global_frame = 0x73u;
    dispatch_state.players[5].action = DD_PLAYER_ROUTE_APPROACH;
    dispatch_state.players[5].role = 3u;
    dispatch_state.players[5].court_x = 0x00C000;
    dispatch_state.players[5].court_depth = 0x7000;
    dispatch_state.players[5].target_zone = 0xECu;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_LIVE_CPU_ROUTE &&
          dispatch_state.players[5].target_zone == 0x8Cu,
          "$81A2 arrival uses $842F bit-two phase to enter $3E with target $8C");

    dispatch_state = dispatch_base;
    dispatch_state.players[5].action = DD_PLAYER_ROUTE_ADJUST;
    dispatch_state.players[5].role = 1u;
    dispatch_state.players[5].target_x = dispatch_state.players[5].court_x;
    dispatch_state.players[5].target_depth = dispatch_state.players[5].court_depth;
    dispatch_state.players[5].target_zone = (uint8_t)(
        (((uint32_t)(dispatch_state.players[5].court_depth >> 8) << 1u) & 0xE0u) |
        (((uint32_t)dispatch_state.players[5].court_x >> 12u) & 0x1Fu));
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action != DD_PLAYER_ROUTE_ADJUST,
          "player state $3A advances into the regional route family");

    dispatch_state = dispatch_base;
    dispatch_state.players[5].action = DD_PLAYER_ROUTE_WAIT;
    dispatch_state.players[5].velocity_x = 0x0123;
    dispatch_state.players[5].velocity_depth = -0x0456;
    dispatch_state.players[5].velocity_height = 0x0789;
    dispatch_state.players[5].action_age = 9u;
    live_start_x[5] = dispatch_state.players[5].court_x;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_ROUTE_WAIT &&
          dispatch_state.players[5].court_x == live_start_x[5] &&
          dispatch_state.players[5].velocity_x == 0x0123 &&
          dispatch_state.players[5].velocity_depth == -0x0456 &&
          dispatch_state.players[5].velocity_height == 0x0789 &&
          dispatch_state.players[5].action_age == 9u,
          "player state $3B preserves the original bare-RTS object state");

    dispatch_state = dispatch_base;
    dispatch_state.players[5].action = DD_PLAYER_LIVE_RENDER_ONLY;
    dispatch_state.players[5].velocity_x = 0x0123;
    dispatch_state.players[5].velocity_depth = -0x0456;
    dispatch_state.players[5].velocity_height = 0x0789;
    live_start_x[5] = dispatch_state.players[5].court_x;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_LIVE_RENDER_ONLY &&
          dispatch_state.players[5].court_x == live_start_x[5] &&
          dispatch_state.players[5].velocity_x == 0 &&
          dispatch_state.players[5].velocity_depth == 0 &&
          dispatch_state.players[5].velocity_height == 0,
          "player state $3F reproduces $8460->$B503's three-vector clear");

    dispatch_state = dispatch_base;
    dispatch_state.players[5].action = DD_PLAYER_INBOUNDER;
    dispatch_state.players[5].target_x = dispatch_state.players[5].court_x;
    dispatch_state.players[5].target_depth = dispatch_state.players[5].court_depth;
    dispatch_state.players[5].target_zone = (uint8_t)(
        (((uint32_t)(dispatch_state.players[5].court_depth >> 8) << 1u) & 0xE0u) |
        (((uint32_t)dispatch_state.players[5].court_x >> 12u) & 0x1Fu));
    dispatch_state.ball.owner = 0xFFu;
    dispatch_state.carrier = 0xFFu;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_INBOUND_HOLD &&
          dispatch_state.ball.owner == 5u,
          "player state $41 claims and aligns the inbound ball before state $30");

    dispatch_state = dispatch_base;
    dispatch_state.carrier = 5u;
    dispatch_state.ball.owner = 5u;
    dispatch_state.ball.action = DD_BALL_DRIBBLE;
    dispatch_state.ball.court_x = dispatch_state.players[1].court_x;
    dispatch_state.ball.court_depth = dispatch_state.players[1].court_depth;
    dispatch_state.ball.height = dispatch_state.players[1].height + 0x0800;
    dispatch_state.players[1].contact_age = 19u;
    run_cpu_dispatch(&pack, &dispatch_state, 1u);
    check(dispatch_state.carrier == 1u && dispatch_state.ball.owner == 1u &&
          dispatch_state.controlled_player == 1u &&
          dispatch_state.players[1].action == DD_PLAYER_LIVE_USER_CARRIER &&
          dispatch_state.players[0].action == DD_PLAYER_LIVE_CPU &&
          dispatch_state.players[3].action == DD_PLAYER_LIVE_CPU_CUT &&
          dispatch_state.players[5].action == DD_PLAYER_LIVE_TEAMMATE &&
          dispatch_state.possession_direction == 1u,
          "$B435/$9FA3->$A44B sustained contact transfers possession, control, and both team states");

    dispatch_state = dispatch_base;
    dispatch_state.carrier = 5u;
    dispatch_state.ball.owner = 5u;
    dispatch_state.ball.action = DD_BALL_DRIBBLE;
    dispatch_state.ball.court_x = dispatch_state.players[1].court_x;
    dispatch_state.ball.court_depth = dispatch_state.players[1].court_depth;
    dispatch_state.ball.height = dispatch_state.players[1].height + 0x0800;
    dispatch_state.players[1].contact_age = 19u;
    dispatch_state.players[1].facing = dispatch_state.players[5].facing;
    dispatch_state.scene_frame = dispatch_state.next_clock_frame - 1u;
    run_cpu_dispatch(&pack, &dispatch_state, 1u);
    check(dispatch_state.phase == DD_GAMEPLAY_FREE_THROW &&
          dispatch_state.foul_shooter == 5u && dispatch_state.foul_offender == 1u &&
          dispatch_state.ball.action == DD_BALL_DEAD &&
          dispatch_state.players[5].action == DD_PLAYER_FREE_THROW_SHOOTER &&
          dispatch_state.players[1].action == DD_PLAYER_FREE_THROW_FORMATION,
          "$A347 zero-clock same-facing contact enters the foul/free-throw dead ball");
    for (player = 0u; player < 194u; ++player) {
        check(dd_gameplay_step(&pack, &dispatch_state, 0u),
              "advance traced free-throw dead-ball formation");
    }
    check(dispatch_state.free_throw_age == 194u &&
          dispatch_state.ball.action == DD_BALL_DRIBBLE &&
          dispatch_state.ball.owner == 5u,
          "free throw reproduces frame 2802's $0B->$01 shooter award");
    for (player = 194u; player < 214u; ++player) {
        check(dd_gameplay_step(&pack, &dispatch_state, 0u),
              "advance free throw to ready state");
    }
    check(dispatch_state.ball.action == DD_BALL_AWARDED &&
          dispatch_state.players[5].action == DD_PLAYER_FREE_THROW_READY &&
          dispatch_state.players[5].court_x == 0x009200,
          "free throw reproduces frame 2822's ball $00 and shooter state $45");
    for (player = 214u; player < 348u; ++player) {
        check(dd_gameplay_step(&pack, &dispatch_state, 0u),
              "advance free throw to gather state");
    }
    check(dispatch_state.ball.action == DD_BALL_SHOT_GATHER &&
          dispatch_state.players[5].action == DD_PLAYER_FREE_THROW_GATHER &&
          dispatch_state.shot_value == 1u,
          "free throw reproduces frame 2956's shooter $47/ball $04 one-point gather");
    dispatch_state.ball.action = DD_BALL_AIRBORNE;
    dispatch_state.ball.action_age = 0u;
    dispatch_state.ball.court_x = 0x004900;
    dispatch_state.ball.court_depth = 0x005800;
    dispatch_state.ball.height = 0x003500;
    dispatch_state.ball.velocity_x = 0;
    dispatch_state.ball.velocity_depth = 0;
    dispatch_state.ball.velocity_height = 0;
    dispatch_state.score[1] = 0u;
    check(dd_gameplay_step(&pack, &dispatch_state, 0u),
          "score controlled free-throw result one");
    check(dispatch_state.ball.action == DD_BALL_SCORE && dispatch_state.score[1] == 0u,
          "$B377 result one enters score state before the deferred point award");
    for (player = 0u; player < 4u; ++player) {
        check(dd_gameplay_step(&pack, &dispatch_state, 0u),
              "advance free-throw score counter to $08");
    }
    check(dispatch_state.score[1] == 1u,
          "$AEDE counter $08 awards one point while the foul shot is active");

    dispatch_state = dispatch_base;
    dispatch_state.carrier = 5u;
    dispatch_state.ball.owner = 5u;
    dispatch_state.ball.action = DD_BALL_DRIBBLE;
    dispatch_state.ball.court_x = dispatch_state.players[1].court_x + 0x0A00;
    dispatch_state.ball.court_depth = dispatch_state.players[1].court_depth;
    dispatch_state.ball.height = dispatch_state.players[1].height + 0x0800;
    dispatch_state.players[1].contact_age = 19u;
    run_cpu_dispatch(&pack, &dispatch_state, 1u);
    check(dispatch_state.ball.owner == 5u && dispatch_state.players[1].contact_age == 0u,
          "$B435's exclusive 10-unit court boundary resets the contact timer");

    dispatch_state = dispatch_base;
    dispatch_state.phase = DD_GAMEPLAY_LIVE;
    dispatch_state.ball.action = DD_BALL_AWARDED;
    dispatch_state.ball.owner = 5u;
    dispatch_state.ball.held_height_offset = 0x18u;
    dispatch_state.players[5].height = 0x2600;
    check(dd_gameplay_step(&pack, &dispatch_state, 0u), "step tip-awarded ball state $00");
    check(dispatch_state.ball.height == 0x3E00 && dispatch_state.carrier == 5u,
          "ball state $00 applies $ACB6's traced tip-award height offset $18");

    dispatch_state = dispatch_base;
    dispatch_state.phase = DD_GAMEPLAY_LIVE;
    dispatch_state.ball.action = DD_BALL_AWARDED;
    dispatch_state.ball.owner = 0u;
    dispatch_state.ball.held_height_offset = 0x08u;
    dispatch_state.players[0].height = 0x1000;
    check(dd_gameplay_step(&pack, &dispatch_state, 0u), "step ordinary awarded ball state $00");
    check(dispatch_state.ball.height == 0x1800 && dispatch_state.carrier == 0u,
          "ball state $00 applies $ACB6's ordinary held-ball height offset $08");

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
    dispatch_state.ball.action = DD_BALL_PASS_BOUNCE;
    dispatch_state.ball.height = 0x0B00;
    dispatch_state.ball.velocity_height = 0x0100;
    dispatch_state.ball.vertical_phase = 0x3Du;
    check(dd_gameplay_step(&pack, &dispatch_state, 0u),
          "step $B167's traced negative bounce-height branch");
    check(dispatch_state.ball.action == DD_BALL_PASS_BOUNCE &&
          dispatch_state.ball.height == 0x0B00 &&
          dispatch_state.ball.velocity_height == 0 &&
          dispatch_state.ball.vertical_phase == 0u,
          "ball state $03 reproduces original frame 2602 velocity decrement and phase reset");
    check(dd_gameplay_step(&pack, &dispatch_state, 0u),
          "step completed bounce pass into hidden state");
    check(dispatch_state.ball.action == DD_BALL_HIDDEN,
          "ball state $03 enters $0C after vertical velocity reaches zero");

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
    dispatch_state.ball.action = DD_BALL_AIRBORNE;
    dispatch_state.ball.action_age = 0x00FFu;
    dispatch_state.ball.owner = 0xFFu;
    dispatch_state.ball.court_x = 0x004900;
    dispatch_state.ball.court_depth = 0x005800;
    dispatch_state.ball.height = 0x003500;
    dispatch_state.ball.velocity_x = 0;
    dispatch_state.ball.velocity_depth = 0;
    dispatch_state.ball.velocity_height = 0;
    dispatch_state.last_shooter = 5u;
    dispatch_state.possession_direction = 0u;
    check(dd_gameplay_step(&pack, &dispatch_state, 0u),
          "step $AE25/$B377 counter-wrap arming frame");
    check(dispatch_state.ball.action == DD_BALL_AIRBORNE &&
          dispatch_state.ball.outcome == 0u,
          "$B377 rearms wrapped $04F0 and returns without classifying contact");
    check(dd_gameplay_step(&pack, &dispatch_state, 0u),
          "step armed $B377 result-one frame");
    check(dispatch_state.ball.action == DD_BALL_SCORE &&
          dispatch_state.ball.outcome == 1u,
          "$B377 classifies the same hoop contact after its arming return");

    dispatch_state = dispatch_base;
    dispatch_state.ball.action = DD_BALL_LOOSE_LAUNCH;
    dispatch_state.ball.outcome = 4u;
    dispatch_state.ball.velocity_x = 0x0100;
    dispatch_state.ball.velocity_depth = 0x0080;
    dispatch_state.ball.rim_contact = 1u;
    dispatch_state.carrier = 5u;
    check(dd_gameplay_step(&pack, &dispatch_state, 0u), "step outcome $04 loose launch");
    check(dispatch_state.ball.action == DD_BALL_LOOSE_AIRBORNE &&
          dispatch_state.ball.owner == 0xFFu && dispatch_state.carrier == 5u &&
          dispatch_state.ball.velocity_x == -0x0080 &&
          dispatch_state.ball.velocity_depth == -0x0040 &&
          dispatch_state.ball.rim_contact == 1u,
          "ball state $08 reverses outcome $04 while retaining camera follow and rim latch");

    dispatch_state = dispatch_base;
    dispatch_state.ball.action = DD_BALL_LOOSE_LAUNCH;
    dispatch_state.ball.outcome = 2u;
    dispatch_state.ball.velocity_x = 0x00B4;
    dispatch_state.ball.velocity_depth = 0x00B4;
    check(dd_gameplay_step(&pack, &dispatch_state, 0u), "step outcome $02 loose launch");
    check(dispatch_state.ball.velocity_x == 0x005A &&
          dispatch_state.ball.velocity_depth == 0x005A,
          "$AF72 outcome $02 preserves direction and halves its vector");

    dispatch_state = dispatch_base;
    dispatch_state.ball.action = DD_BALL_LOOSE_LAUNCH;
    dispatch_state.ball.outcome = 3u;
    dispatch_state.ball.velocity_x = 0x0080;
    dispatch_state.ball.velocity_depth = 0x0040;
    check(dd_gameplay_step(&pack, &dispatch_state, 0u), "step outcome $03 loose launch");
    check(dispatch_state.ball.velocity_x == 0 && dispatch_state.ball.velocity_depth == 0,
          "$AF72 outcome $03 retains the setup path's cleared horizontal terms");

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
    check(dispatch_state.ball.action == DD_BALL_REBOUND &&
          dispatch_state.ball.velocity_height == 0x02E0 &&
          dispatch_state.ball.rim_contact == 0u,
          "ball state $09 crosses $AFDD's $E0 threshold into rebound state $07");

    dispatch_state = dispatch_base;
    dispatch_state.ball.action = DD_BALL_SHOT_LAUNCH;
    dispatch_state.ball.owner = 5u;
    dispatch_state.carrier = 5u;
    check(dd_gameplay_step(&pack, &dispatch_state, 0u), "step shot initializer state $0A");
    check(dispatch_state.ball.action == DD_BALL_AIRBORNE &&
          dispatch_state.ball.velocity_height == 0x0305 &&
          dispatch_state.ball.flight_curve == 0xD8u &&
          dispatch_state.ball.owner == 5u && dispatch_state.carrier == 5u,
          "ball state $0A reproduces $B017's launch terms and advances to $05");

    dispatch_state = dispatch_base;
    dispatch_state.ball.action = DD_BALL_DEAD;
    dispatch_state.ball.height = 0x34AAu;
    check(dd_gameplay_step(&pack, &dispatch_state, 0u), "step dead ball state $0B");
    check(dispatch_state.ball.action == DD_BALL_DEAD &&
          dispatch_state.ball.height == 0x00AAu,
          "ball state $0B clears only the integer height through $ACAB");

    dispatch_state = dispatch_base;
    dispatch_state.ball.action = DD_BALL_HIDDEN;
    dispatch_state.ball.height = 0x227Bu;
    dispatch_state.ball.velocity_x = 0x0123;
    dispatch_state.ball.velocity_depth = -0x0045;
    dispatch_state.ball.velocity_height = 0x0067;
    check(dd_gameplay_step(&pack, &dispatch_state, 0u), "step hidden ball state $0C");
    check(dispatch_state.ball.action == DD_BALL_HIDDEN &&
          dispatch_state.ball.height == 0x007Bu &&
          dispatch_state.ball.velocity_x == 0x0123 &&
          dispatch_state.ball.velocity_depth == -0x0045 &&
          dispatch_state.ball.velocity_height == 0x0067,
          "ball state $0C shares $ACAB's integer-height clear without changing velocity");

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
