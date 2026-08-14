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

int main(int argc, char **argv) {
    DDAssetPack pack;
    DDGameplayState state;
    DDGameplayState jump_state;
    DDGameplayState pass_state;
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

    memset(&jump_state, 0, sizeof(jump_state));
    check(dd_gameplay_advance_to(&pack, &jump_state, 300u, 0u),
          "advance to the original user jump window");
    check(dd_gameplay_step(&pack, &jump_state, DD_INPUT_B), "B starts the tip-off jump");
    check(jump_state.players[0].action == DD_PLAYER_TIP_USER_AIRBORNE &&
          jump_state.tip_user_jump_frame == 301u,
          "native B timing reproduces original frame 2502 state $10->$11");
    check(dd_gameplay_advance_to(&pack, &jump_state, 330u, 0u), "advance to CPU contact");
    check(jump_state.carrier == 5u, "CPU slot wins the first contact frame");
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

    dd_asset_pack_unload(&pack);
    if (failures != 0) {
        fprintf(stderr, "%d CPU gameplay regression check(s) failed.\n", failures);
        return 1;
    }
    puts("Gameplay regression checks passed: tip jump, camera CHR, moving CPU players, pass/shot states, rebound, and inbound reception.");
    return 0;
}
