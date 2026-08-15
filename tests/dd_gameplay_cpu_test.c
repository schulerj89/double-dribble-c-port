#include "dd_gameplay.h"
#include "dd_renderer.h"

#include <stdio.h>
#include <stdlib.h>
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

static void set_packed_position(DDPlayerState *player, uint8_t packed) {
    player->court_x = (int32_t)((((uint32_t)packed & 0x1Fu) << 4u) + 8u) << 8u;
    player->court_depth = (int32_t)((((uint32_t)packed >> 1u) & 0x70u) + 8u) << 8u;
}

static void prepare_cpu_policy(const DDAssetPack *pack, DDGameplayState *state,
                               uint8_t phase, uint8_t carrier_packed) {
    memset(state, 0, sizeof(*state));
    check(dd_gameplay_advance_to(pack, state, 356u, 0u),
          "prepare isolated fixed-bank CPU policy");
    state->carrier = 5u;
    state->ball.owner = 5u;
    state->ball.action = DD_BALL_DRIBBLE;
    state->ball.height = 0x1000;
    state->clock_minutes = 1u;
    state->clock_seconds = 0x30u;
    state->possession_direction = 0u;
    state->cpu_global_frame = (uint8_t)(phase - 1u);
    state->players[5].action = DD_PLAYER_LIVE_CPU_SETUP;
    state->players[5].role = 0u;
    state->players[5].route_step = 0u;
    state->players[5].decision_timer = 10u;
    state->players[5].paired_player = 0u;
    set_packed_position(&state->players[5], carrier_packed);
    state->players[5].target_x = state->players[5].court_x;
    state->players[5].target_depth = state->players[5].court_depth;
    state->players[5].target_zone = carrier_packed;
    state->ball.court_x = state->players[5].court_x;
    state->ball.court_depth = state->players[5].court_depth;
    set_packed_position(&state->players[0], 0x42u);
}

static void check_unlimited_native_gameplay_sprites(const DDAssetPack *pack) {
    const uint32_t width = pack->tipoff_meta.width;
    const uint32_t height = pack->tipoff_meta.height;
    const size_t pixel_count = (size_t)width * height;
    uint32_t *baseline = (uint32_t *)malloc(pixel_count * sizeof(*baseline));
    uint32_t *expected = (uint32_t *)malloc(pixel_count * sizeof(*expected));
    uint32_t *solo = (uint32_t *)malloc(pixel_count * sizeof(*solo));
    uint32_t *combined = (uint32_t *)malloc(pixel_count * sizeof(*combined));
    DDGameplayState base;
    DDGameplayState view;
    uint32_t changed = 0u;
    uint32_t player;
    int hud_clean = 1;
    int top_edge_visible = 0;
    size_t pixel;
    int rendered = baseline != NULL && expected != NULL && solo != NULL && combined != NULL;
    memset(&base, 0, sizeof(base));
    base.phase = DD_GAMEPLAY_LIVE;
    base.scene_frame = 356u;
    base.period = 1u;
    base.hud_split_y = 64u;
    base.ball.animation = 0xFFu;
    for (player = 0u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
        base.players[player].animation = 0xFFu;
    }
    if (rendered) rendered = dd_render_gameplay(pack, &base, baseline, width, height);
    if (rendered) memcpy(expected, baseline, pixel_count * sizeof(*expected));
    for (player = 0u; rendered && player < 3u; ++player) {
        view = base;
        view.players[player].animation = 0x1Bu;
        view.players[player].court_x = (int32_t)((32u + player * 80u) << 8u);
        view.players[player].court_depth = 0x004000;
        view.players[player].height = 0x1000;
        rendered = dd_render_gameplay(pack, &view, solo, width, height);
        for (pixel = 0u; rendered && pixel < pixel_count; ++pixel) {
            if (solo[pixel] != baseline[pixel]) {
                expected[pixel] = solo[pixel];
                ++changed;
            }
        }
    }
    view = base;
    for (player = 0u; player < 3u; ++player) {
        view.players[player].animation = 0x1Bu;
        view.players[player].court_x = (int32_t)((32u + player * 80u) << 8u);
        view.players[player].court_depth = 0x004000;
        view.players[player].height = 0x1000;
    }
    if (rendered) rendered = dd_render_gameplay(pack, &view, combined, width, height);
    check(rendered && changed != 0u &&
          memcmp(expected, combined, pixel_count * sizeof(*expected)) == 0,
          "native gameplay draws every metasprite piece past the NES eight-sprites-per-scanline limit");
    /* Live objects use signed projected coordinates. Exercise every base Y
       around the viewport: records clip at the 64-pixel court raster, while
       an anchor between $40 and $5F must retain any lower visible records. */
    for (player = 0u; rendered && player <= 287u; ++player) {
        uint32_t row;
        int32_t base_y = (int32_t)player - 32;
        view = base;
        view.players[0].animation = 0x1Bu;
        view.players[0].court_x = 0x008000;
        view.players[0].court_depth =
            (240 - base_y - 16) * 256;
        view.players[0].height = 0x1000;
        rendered = dd_render_gameplay(pack, &view, solo, width, height);
        if (rendered && base_y >= (int32_t)base.hud_split_y && base_y < 0x60 &&
            memcmp(solo, baseline, pixel_count * sizeof(*solo)) != 0) {
            top_edge_visible = 1;
        }
        for (row = 0u; rendered && row < base.hud_split_y; ++row) {
            if (memcmp(solo + (size_t)row * width,
                       baseline + (size_t)row * width,
                       width * sizeof(*solo)) != 0) {
                hud_clean = 0;
                break;
            }
        }
    }
    check(rendered && hud_clean,
          "signed gameplay metasprites clip at the court raster instead of wrapping into the scoreboard");
    check(rendered && top_edge_visible,
          "top-edge players retain visible metasprite records instead of disappearing at anchor Y $60");
    free(combined);
    free(solo);
    free(expected);
    free(baseline);
}

static void check_team_palette_continuity(const DDAssetPack *pack) {
    const size_t pixel_count = (size_t)pack->tipoff_meta.width * pack->tipoff_meta.height;
    uint32_t *new_york = (uint32_t *)malloc(pixel_count * sizeof(*new_york));
    uint32_t *los_angeles = (uint32_t *)malloc(pixel_count * sizeof(*los_angeles));
    DDGameplayState first;
    DDGameplayState second;
    int ok = new_york != NULL && los_angeles != NULL;
    memset(&first, 0, sizeof(first));
    memset(&second, 0, sizeof(second));
    if (ok) ok = dd_gameplay_configure(pack, &first, 0u, 0u, 0u) &&
        dd_gameplay_configure(pack, &second, 2u, 3u, 2u);
    if (ok) {
        first.scene_frame = 144u;
        second.scene_frame = 144u;
        ok = dd_render_gameplay(pack, &first, new_york,
                                pack->tipoff_meta.width, pack->tipoff_meta.height) &&
            dd_render_gameplay(pack, &second, los_angeles,
                               pack->tipoff_meta.width, pack->tipoff_meta.height);
    }
    check(ok && second.match_time_index == 2u && second.match_time_bcd == 0x20u &&
          second.clock_minutes == 0x20u && second.match_team_index == 3u &&
          second.match_level_index == 2u,
          "configuration time, team, and level persist in native match state");
    check(ok && memcmp(new_york, los_angeles,
                       pixel_count * sizeof(*new_york)) != 0,
          "selected 1P team palette changes the tip-off/gameplay framebuffer");
    free(los_angeles);
    free(new_york);
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
    DDGameplayState inbound_pass_state;
    DDGameplayState dispatch_base;
    DDGameplayState dispatch_state;
    DDGameplayState contest_state;
    DDGameplayState held_shot_state;
    DDGameplayState free_throw_state;
    DDGameplayState period_state;
    DDGameplayState net_state;
    const DDTipoffAssetsHeader *assets;
    int32_t live_start_x[DD_GAMEPLAY_PLAYER_COUNT];
    int32_t live_start_depth[DD_GAMEPLAY_PLAYER_COUNT];
    int32_t pass_release_x;
    int32_t pass_release_depth;
    uint32_t inbound_receiver;
    uint32_t moved_count;
    uint32_t player;
    int links_reciprocal;
    if (argc != 2 || !dd_asset_pack_load(argv[1], &pack)) {
        fputs("usage: dd_gameplay_cpu_test <assetpack>\n", stderr);
        return 2;
    }
    assets = (const DDTipoffAssetsHeader *)pack.tipoff_assets;
    check_unlimited_native_gameplay_sprites(&pack);
    check_team_palette_continuity(&pack);
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
    check(pack.tipoff_meta.whistle_audio_frames == 12u && pack.whistle_audio_count == 23u &&
          pack.whistle_audio[3].frame == 2u && pack.whistle_audio[3].period == 37u &&
          pack.whistle_audio[4].period == 52u && pack.whistle_audio[5].channel == 3u &&
          pack.whistle_audio[5].period == 6u && pack.whistle_audio[5].volume == 3u,
          "asset pack exposes exact $2C pulse/noise whistle playback data");
    check(pack.tipoff_meta.three_call_audio_frames == 189u && pack.three_call_audio_count == 3u &&
          pack.three_call_audio[0].period == 256u && pack.three_call_audio[0].reserved == 1u &&
          pack.three_call_audio[1].frame == 95u && pack.three_call_audio[1].period == 163u &&
          pack.three_call_audio[1].reserved == 2u && pack.three_call_audio[2].frame == 188u,
          "asset pack exposes exact $09 down/up three-point call sweep");
    check(pack.tipoff_meta.three_score_audio_frames == 42u && pack.three_score_audio_count == 78u &&
          pack.three_score_audio[2].frame == 1u && pack.three_score_audio[2].period == 592u &&
          pack.three_score_audio[2].channel == 1u && pack.three_score_audio[2].volume == 15u,
          "asset pack exposes the controlled $25 two-pulse scoring cue");
    check(pack.tipoff_meta.score_audio_frames == 437u && pack.score_audio_count == 104u &&
          pack.score_audio[0].frame == 0u && pack.score_audio[0].period == 144u &&
          pack.score_audio[0].channel == 0u && pack.score_audio[16].frame == 15u &&
          pack.score_audio[16].period == 213u && pack.score_audio[16].channel == 2u &&
          pack.score_audio[103].frame == 436u && pack.score_audio[103].volume == 0u,
          "DDAP v18 exposes the isolated $18 then $1F/$22 made-basket score cue");
    check((uint8_t)assets->height_scripts[10] == 0x80u &&
          assets->height_scripts[11] == 5 &&
          (uint8_t)assets->height_scripts[24] == 0x81u,
          "asset pack exposes $9B34's jump stream with reverse and landing sentinels");
    check(assets->shot_animation[0] == 0x22u &&
          assets->shot_animation[1] == 0x28u &&
          assets->shot_animation[2] == 0x23u &&
          assets->shot_animation[3] == 0x27u &&
          assets->shot_animation[4] == 0x21u &&
          assets->shot_animation[5] == 0x25u &&
          assets->shot_animation[6] == 0x24u &&
          assets->shot_animation[7] == 0x26u,
          "asset pack exposes $A9DC's eight facing-indexed shot poses");
    check(assets->net_animation_tiles[0] == 0xB4u &&
          assets->net_animation_tiles[4] == 0xD3u &&
          assets->net_animation_tiles[8] == 0xB4u &&
          assets->net_animation_tiles[10] == 0xD1u &&
          assets->net_animation_tiles[12] == 0xB0u &&
          assets->net_animation_tiles[16] == 0xD3u &&
          assets->net_animation_tiles[20] == 0xB0u &&
          assets->net_animation_tiles[23] == 0xD2u,
          "asset pack exposes $9922's left/right three-phase net tiles");
    check((uint8_t)assets->rebound_target_phase[0] == 0xFEu &&
          (uint8_t)assets->rebound_target_phase[1] == 0xFFu &&
          assets->rebound_target_phase[2] == 0x01 &&
          assets->rebound_target_phase[3] == 0x02 &&
          assets->rebound_formation[0] == 0xA4u &&
          assets->rebound_formation[1] == DD_PLAYER_REBOUND_CHASE &&
          assets->rebound_formation[20] == 0xBBu &&
          assets->rebound_formation[21] == DD_PLAYER_REBOUND_CHASE,
          "asset pack exposes $8503/$8507's two-direction rebound formation tables");

    memset(&jump_state, 0, sizeof(jump_state));
    check(dd_gameplay_advance_to(&pack, &jump_state, 300u, 0u),
          "advance to the original user jump window");
    check(jump_state.clock_minutes == 0x04u && jump_state.clock_seconds == 0x59u,
          "bank-0 $9431/$9490 clock reaches the traced 04:59 rollover");
    check(dd_gameplay_step(&pack, &jump_state, DD_INPUT_B), "B starts the tip-off jump");
    check(jump_state.players[0].action == DD_PLAYER_TIP_USER_AIRBORNE &&
          jump_state.tip_user_jump_frame == 301u &&
          jump_state.players[0].facing == 0u &&
          jump_state.players[0].animation == 0x22u,
          "native B timing reproduces original frame 2502 state $10->$11 and facing-zero pose $22");
    check(dd_gameplay_advance_to(&pack, &jump_state, 330u, 0u), "advance to CPU contact");
    check(jump_state.carrier == 5u, "CPU slot wins the first contact frame");
    check(jump_state.hud_split_y == 64u,
          "tip contact retains all eight native HUD rows for rule-message rendering");
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
    check(state.players[5].animation ==
              assets->shot_animation[state.players[5].facing & 7u] &&
          state.players[5].velocity_x == 0 &&
          state.players[5].velocity_depth == 0,
          "$8D1F faces the hoop, clears route motion, and retains $A9DC's CPU shot pose");

    check(dd_gameplay_advance_to(&pack, &state, 548u, 0u), "advance to shot release");
    check(state.live_frame == 192u && state.ball.action == DD_BALL_AIRBORNE,
          "live 192 follows $AE25 into airborne shot state $05");
    check(state.ball.court_x == 0x005700 && state.ball.court_depth == 0x004B00 &&
          state.ball.height == 0x3800 &&
          state.ball.velocity_x == -0x00BD &&
          state.ball.velocity_depth == 0x00AB &&
          state.ball.flight_angle == 0x62u &&
          state.shot_value == 2u &&
          state.ball.flight_duration == 0x14u &&
          state.ball.flight_curve == 0x05u &&
          state.ball.velocity_height == 0x0200,
          "$B189 short shot matches the frame-2749 $B29C/$B376 vector, duration, and arc");
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
    check(state.live_frame == 447u && state.ball.action == DD_BALL_AWARDED &&
          state.ball.owner == 0u && state.carrier == 0u &&
          state.players[0].action == DD_PLAYER_LIVE_USER_INBOUND,
          "live 447 preserves the recovered ball for user inbound state $0D");
    inbound_pass_state = state;
    check(dd_gameplay_step(&pack, &inbound_pass_state,
                           DD_INPUT_A | DD_INPUT_RIGHT),
          "direction+A selects a user inbound receiver");
    check(inbound_pass_state.players[0].action == DD_PLAYER_USER_PASS_RECOVER &&
          inbound_pass_state.ball.action == DD_BALL_AWARDED &&
          inbound_pass_state.ball.owner == 0u &&
          inbound_pass_state.ball.receiver == 2u,
          "$A129/$A21F queue the traced right-side receiver while the ball remains held");
    check(inbound_pass_state.players[1].action == DD_PLAYER_LIVE_CPU &&
          inbound_pass_state.players[2].action == DD_PLAYER_LIVE_CPU &&
          inbound_pass_state.players[3].action == DD_PLAYER_LIVE_CPU_CUT &&
          inbound_pass_state.players[4].action == DD_PLAYER_LIVE_CPU_ROUTE &&
          inbound_pass_state.players[5].action == DD_PLAYER_LIVE_TEAMMATE,
          "$A482 restores the post-inbound dispatcher roles");
    check(dd_gameplay_step(&pack, &inbound_pass_state, 0u) &&
          dd_gameplay_step(&pack, &inbound_pass_state, 0u),
          "advance to the queued user inbound release");
    check(inbound_pass_state.ball.action == DD_BALL_PASS &&
          inbound_pass_state.ball.receiver == 2u &&
          inbound_pass_state.players[2].action == DD_PLAYER_USER_PASS_RECEIVE,
          "the next user-side dispatcher turn launches ball $02 and receiver $0C");
    check(dd_gameplay_advance_to(&pack, &state, 1123u, 0u), "advance to inbound setup");
    check(state.live_frame == 767u && state.phase == DD_GAMEPLAY_INBOUND &&
          state.ball.action == DD_BALL_DEAD &&
          state.players[5].action == DD_PLAYER_INBOUNDER,
          "live 767 reproduces $9583/$9645 dead-ball formation and inbounder state $41");
    check(dd_gameplay_advance_to(&pack, &state, 1300u, 0u), "advance to inbound hold");
    check(state.live_frame == 944u && state.carrier == 5u &&
          state.players[5].action == DD_PLAYER_INBOUND_HOLD,
          "live 944 gives the inbounder the held ball in state $30");
    check(state.players[5].court_depth == 0x009800 &&
          state.players[5].target_depth == 0x009800,
          "inbounder preserves extended target $0121 as baseline depth $98");
    check(dd_gameplay_advance_to(&pack, &state, 1344u, 0u), "advance to inbound release setup");
    check(state.live_frame == 988u &&
          state.players[5].action == DD_PLAYER_INBOUND_READY &&
          state.players[5].release_timer == 8u &&
          state.ball.action == DD_BALL_AWARDED &&
          (state.ball.velocity_x != 0 || state.ball.velocity_depth != 0),
          "live 988 follows $9018->$B0B8 into state $31 with the CPU inbound vector installed");
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
    check(state.players[5].role == 1u && state.players[6].role == 0u &&
          state.players[5].paired_player == 4u &&
          state.players[4].paired_player == 5u &&
          state.players[6].paired_player == 0u &&
          state.players[0].paired_player == 6u,
          "$993A/$99D9/$9A31 preserve inbound role and reciprocal pair swaps");
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
    check(dd_gameplay_advance_to(&pack, &pass_state, 356u, 0u),
          "prepare fixed-bank CPU pass decision");
    pass_state.carrier = 5u;
    pass_state.ball.owner = 5u;
    pass_state.ball.action = DD_BALL_DRIBBLE;
    pass_state.ball.court_x = 0x005800;
    pass_state.ball.court_depth = 0x005800;
    pass_state.clock_minutes = 1u;
    pass_state.clock_seconds = 0x30u;
    pass_state.possession_direction = 0u;
    pass_state.cpu_global_frame = 0x7Fu;
    pass_state.players[5].action = DD_PLAYER_LIVE_CPU_SETUP;
    pass_state.players[5].role = 0u;
    pass_state.players[5].court_x = 0x005800;
    pass_state.players[5].court_depth = 0x005800;
    pass_state.players[5].target_x = 0x005800;
    pass_state.players[5].target_depth = 0x005800;
    pass_state.players[5].target_zone = 0xA5u;
    pass_state.players[5].decision_timer = 10u;
    pass_state.players[5].paired_player = 0u;
    pass_state.players[0].court_x = 0x010800;
    pass_state.players[0].court_depth = 0x003800;
    pass_state.players[9].role = 4u;
    pass_state.players[9].court_x = 0x00C800;
    pass_state.players[9].court_depth = 0x007800;
    check(dd_gameplay_step(&pack, &pass_state, 0u),
          "run natural $001A=$80 CPU pass branch");
    check(pass_state.ball.action == DD_BALL_AWARDED &&
          pass_state.ball.owner == 5u && pass_state.ball.receiver == 9u &&
          (pass_state.ball.velocity_x != 0 || pass_state.ball.velocity_depth != 0) &&
          pass_state.players[5].action == DD_PLAYER_INBOUND_READY &&
          pass_state.players[5].release_timer == 8u &&
          pass_state.players[9].action == DD_PLAYER_LIVE_SET &&
          pass_state.cpu_pass_cooldown == 2u,
          "$D8FA/$D94E selects role four and $9018->$B0B8 installs its pass vector");
    for (player = 0u; player < 4u; ++player) run_cpu_dispatch(&pack, &pass_state, 5u);
    check(pass_state.ball.action == DD_BALL_PASS && pass_state.ball.receiver == 9u &&
          (pass_state.ball.velocity_x != 0 || pass_state.ball.velocity_depth != 0),
          "$8FE0 releases the queued CPU pass on timer four instead of launching immediately");
    pass_release_x = pass_state.ball.court_x;
    pass_release_depth = pass_state.ball.court_depth;
    check(dd_gameplay_step(&pack, &pass_state, 0u),
          "advance the released CPU pass with its preinstalled vector");
    check(pass_state.ball.court_x != pass_release_x ||
          pass_state.ball.court_depth != pass_release_depth ||
          pass_state.ball.action == DD_BALL_DRIBBLE,
          "$B0B8 prevents a released CPU pass from becoming an orphaned stationary ball");

    prepare_cpu_policy(&pack, &pass_state, 0x22u, 0x4Cu);
    pass_state.players[8].role = 3u;
    set_packed_position(&pass_state.players[8], 0x4Du);
    check(dd_gameplay_step(&pack, &pass_state, 0u),
          "run region-three packed route selection");
    check(pass_state.ball.action == DD_BALL_DRIBBLE &&
          pass_state.players[5].action == DD_PLAYER_LIVE_CPU_SETUP &&
          pass_state.players[5].target_zone == 0x4Bu &&
          pass_state.players[5].decision_timer == 9u,
          "$D8B9/$D8D5 selects target $4B while $D94E rejects a same-region role-three pass");

    prepare_cpu_policy(&pack, &pass_state, 0x22u, 0x53u);
    pass_state.possession_direction = 1u;
    pass_state.players[8].role = 3u;
    set_packed_position(&pass_state.players[8], 0x52u);
    check(dd_gameplay_step(&pack, &pass_state, 0u),
          "run mirrored region-three packed route selection");
    check(pass_state.players[5].target_zone == 0x54u,
          "$AC64 mirrors the recovered $4B policy target to $54 for the opposite basket");

    prepare_cpu_policy(&pack, &pass_state, 0x54u, 0x6Du);
    check(dd_gameplay_step(&pack, &pass_state, 0u),
          "run region-two center-lane reservation");
    check(pass_state.players[5].target_zone == 0x85u &&
          pass_state.players[5].decision_timer == 10u,
          "$D77B-$D7C2 reserves packed center target $85 without consuming the decision timer");

    prepare_cpu_policy(&pack, &pass_state, 0x22u, 0x48u);
    pass_state.cpu_entropy = 0u;
    check(dd_gameplay_step(&pack, &pass_state, 0u),
          "run region-six lane target selection");
    check(pass_state.players[5].target_zone == 0x88u,
          "$D834 keeps the packed column and clamps the ball depth band to $80");

    prepare_cpu_policy(&pack, &pass_state, 0x22u, 0x44u);
    pass_state.cpu_entropy = 0xC5u;
    check(dd_gameplay_step(&pack, &pass_state, 0u),
          "run entropy-selected high lane target");
    check(pass_state.players[5].target_zone == 0xE4u,
          "$C02B/$0063 high bits feed $D834's recovered $E4 lane target");

    prepare_cpu_policy(&pack, &pass_state, 0x20u, 0x85u);
    check(dd_gameplay_step(&pack, &pass_state, 0u),
          "run region-five shooting decision");
    check(pass_state.players[5].action == DD_PLAYER_LIVE_CARRIER_ROUTE,
          "$D7C8-$D7CC converts a region-five arrival into shot state $26");

    prepare_cpu_policy(&pack, &pass_state, 0x44u, 0xA5u);
    pass_state.players[8].role = 3u;
    set_packed_position(&pass_state.players[8], 0xECu);
    check(dd_gameplay_step(&pack, &pass_state, 0u),
          "run natural $001A=$44 role-three pass branch");
    check(pass_state.ball.action == DD_BALL_AWARDED &&
          pass_state.ball.receiver == 8u,
          "$D8FA selects role three while phase bit seven is clear");

    prepare_cpu_policy(&pack, &pass_state, 0x80u, 0xA6u);
    pass_state.players[5].target_zone = 0x85u;
    pass_state.players[5].target_x = 0x008800;
    pass_state.players[5].target_depth = 0x005800;
    pass_state.players[9].role = 4u;
    set_packed_position(&pass_state.players[9], 0xECu);
    check(dd_gameplay_step(&pack, &pass_state, 0u),
          "run phase-gated moving-carrier pass branch");
    check(pass_state.ball.action == DD_BALL_AWARDED &&
          pass_state.ball.receiver == 9u,
          "$D885's $001A=$80 gate tries $D8FA before the moving carrier's region-four shot");

    prepare_cpu_policy(&pack, &pass_state, 0x44u, 0xA5u);
    pass_state.players[8].role = 3u;
    set_packed_position(&pass_state.players[8], 0xECu);
    pass_state.cpu_pass_cooldown = 1u;
    check(dd_gameplay_step(&pack, &pass_state, 0u),
          "run CPU pass cooldown rejection");
    check(pass_state.ball.action == DD_BALL_DRIBBLE &&
          pass_state.cpu_pass_cooldown == 0u,
          "$D8F1 decrements nonzero $005C and suppresses that pass decision");

    prepare_cpu_policy(&pack, &pass_state, 0x20u, 0x4Cu);
    pass_state.clock_minutes = 0u;
    pass_state.clock_seconds = 0x04u;
    check(dd_gameplay_step(&pack, &pass_state, 0u),
          "run last-five-seconds forced shot decision");
    check(pass_state.players[5].action == DD_PLAYER_LIVE_CARRIER_ROUTE &&
          pass_state.ball.action == DD_BALL_DRIBBLE,
          "$D759 forces state $26 when the match clock is below five seconds");
    run_cpu_dispatch(&pack, &pass_state, 5u);
    check(pass_state.players[5].action == DD_PLAYER_LIVE_CARRIER_DECIDE &&
          pass_state.ball.action == DD_BALL_SHOT_GATHER,
          "$8D1F follows the forced $26 decision with state $27 and ball $04");

    prepare_cpu_policy(&pack, &pass_state, 0x20u, 0x4Cu);
    pass_state.possession_rule_age = 24u * 64u;
    check(dd_gameplay_step(&pack, &pass_state, 0u),
          "run 24-tick possession forced shot decision");
    check(pass_state.players[5].action == DD_PLAYER_LIVE_CARRIER_ROUTE,
          "$D763-$D768 forces state $26 at coarse possession tick $18");

    prepare_cpu_policy(&pack, &pass_state, 0x22u, 0x4Cu);
    pass_state.players[5].decision_timer = 0u;
    check(dd_gameplay_step(&pack, &pass_state, 0u),
          "run expired CPU route countdown");
    check(pass_state.players[5].action == DD_PLAYER_LIVE_CARRIER_ROUTE,
          "$D810-$D813 treats the $04F0 decrement to $FF as a shot decision");

    memset(&dispatch_base, 0, sizeof(dispatch_base));
    check(dd_gameplay_advance_to(&pack, &dispatch_base, 356u, 0u),
          "prepare isolated dispatcher checks");

    dispatch_state = dispatch_base;
    dispatch_state.carrier = 0xFFu;
    dispatch_state.ball.owner = 0xFFu;
    dispatch_state.ball.action = DD_BALL_HIDDEN;
    dispatch_state.players[5].action = DD_PLAYER_LIVE_CPU_CUT_RUN;
    dispatch_state.players[5].court_x = 0x011000;
    dispatch_state.players[5].court_depth = 0x002000;
    dispatch_state.players[5].target_x = 0x012000;
    dispatch_state.players[5].target_depth = 0x001800;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].route_velocity_x == 0x00E1 &&
          dispatch_state.players[5].route_velocity_depth == -0x0078 &&
          dispatch_state.players[5].route_facing == 1u,
          "$ABCD installs $9D2D/$AA98/$9BB0 route vector $00E1/$FF88 and facing one");

    {
        static const uint32_t input[8] = {
            DD_INPUT_RIGHT, DD_INPUT_RIGHT | DD_INPUT_DOWN, DD_INPUT_DOWN,
            DD_INPUT_LEFT | DD_INPUT_DOWN, DD_INPUT_LEFT,
            DD_INPUT_LEFT | DD_INPUT_UP, DD_INPUT_UP,
            DD_INPUT_RIGHT | DD_INPUT_UP
        };
        static const int16_t expected_x[8] = {
            0x0130, 0x00C0, 0, -0x00C0, -0x0140, -0x00C0, 0, 0x00C0
        };
        static const int16_t expected_depth[8] = {
            0, -0x00C0, -0x0100, -0x00C0, 0, 0x00C0, 0x0100, 0x00C0
        };
        uint32_t direction;
        for (direction = 0u; direction < 8u; ++direction) {
            dispatch_state = dispatch_base;
            dispatch_state.carrier = 0xFFu;
            dispatch_state.ball.owner = 0xFFu;
            dispatch_state.ball.action = DD_BALL_HIDDEN;
            dispatch_state.controlled_player = 0u;
            dispatch_state.players[0].action = DD_PLAYER_LIVE_USER;
            dispatch_state.players[0].court_x = 0x010000;
            dispatch_state.players[0].court_depth = 0x005800;
            check(dd_gameplay_step(&pack, &dispatch_state, input[direction]),
                  "step one exact $AA07/$9E4C user direction");
            check(dispatch_state.players[0].velocity_x == expected_x[direction] &&
                  dispatch_state.players[0].velocity_depth == expected_depth[direction] &&
                  dispatch_state.players[0].facing == direction,
                  "$AA07/$9E2D installs the exact cardinal/diagonal 8.8 user vector");
        }
    }

    dispatch_state = dispatch_base;
    dispatch_state.carrier = 0xFFu;
    dispatch_state.ball.owner = 0xFFu;
    dispatch_state.ball.action = DD_BALL_AIRBORNE;
    dispatch_state.ball.court_x = 0x010000;
    dispatch_state.ball.court_depth = 0x005000;
    dispatch_state.ball.height = 0x3800;
    dispatch_state.ball.velocity_x = 0;
    dispatch_state.ball.velocity_depth = 0;
    dispatch_state.ball.velocity_height = 0x0200;
    dispatch_state.ball.vertical_phase = 0u;
    dispatch_state.ball.flight_curve = 0x05u;
    check(dd_gameplay_step(&pack, &dispatch_state, 0u),
          "step first traced $9B84 height sample");
    check(dispatch_state.ball.height == 0x39CD &&
          dispatch_state.ball.vertical_phase == 1u,
          "$9B84/$C3C5 adds $0200-floor($0100/5), matching original frame 2750");
    check(dd_gameplay_step(&pack, &dispatch_state, 0u),
          "step second traced $9B84 height sample");
    check(dispatch_state.ball.height == 0x3B67 &&
          dispatch_state.ball.vertical_phase == 2u,
          "$9B84 preserves 8.8 fraction and adds $0200-floor($0200/5) on phase two");

    dispatch_state = dispatch_base;
    dispatch_state.carrier = 0u;
    dispatch_state.ball.owner = 0u;
    dispatch_state.ball.action = DD_BALL_DRIBBLE;
    dispatch_state.players[0].action = DD_PLAYER_LIVE_USER_CARRIER;
    dispatch_state.possession_direction = 1u;
    dispatch_state.possession_rule_age = 10u * 64u - 1u;
    check(dd_gameplay_step(&pack, &dispatch_state, 0u),
          "run $A1CC ten-tick back-court rule");
    check(dispatch_state.phase == DD_GAMEPLAY_INBOUND &&
          dispatch_state.inbound_reason == 0x13u &&
          dispatch_state.possession_direction == 0u &&
          dispatch_state.players[5].action == DD_PLAYER_INBOUNDER &&
          dispatch_state.possession_rule_age == 0u,
          "$A1CC reason $13 flips possession and enters shared inbound setup");

    dispatch_state = dispatch_base;
    dispatch_state.carrier = 0u;
    dispatch_state.ball.owner = 0u;
    dispatch_state.ball.action = DD_BALL_DRIBBLE;
    dispatch_state.players[0].action = DD_PLAYER_LIVE_USER_CARRIER;
    dispatch_state.players[0].court_x = 0x018000;
    dispatch_state.ball.court_x = dispatch_state.players[0].court_x;
    dispatch_state.possession_direction = 1u;
    dispatch_state.possession_rule_age = 24u * 64u - 1u;
    check(dd_gameplay_step(&pack, &dispatch_state, 0u),
          "run $A1CC twenty-four-tick possession rule");
    check(dispatch_state.phase == DD_GAMEPLAY_INBOUND &&
          dispatch_state.inbound_reason == 0x14u &&
          dispatch_state.players[5].action == DD_PLAYER_INBOUNDER,
          "$A1CC checks reason $14 before the ten-tick branch");

    dispatch_state = dispatch_base;
    dispatch_state.carrier = 0u;
    dispatch_state.ball.owner = 0u;
    dispatch_state.ball.action = DD_BALL_DRIBBLE;
    dispatch_state.players[0].action = DD_PLAYER_LIVE_USER_CARRIER;
    dispatch_state.players[0].court_x = 0x008000;
    dispatch_state.ball.court_x = dispatch_state.players[0].court_x;
    dispatch_state.possession_direction = 1u;
    dispatch_state.backcourt_latched = 1u;
    check(dd_gameplay_step(&pack, &dispatch_state, 0u),
          "run $9583 return-to-backcourt latch");
    check(dispatch_state.phase == DD_GAMEPLAY_INBOUND &&
          dispatch_state.inbound_reason == 0x15u &&
          dispatch_state.backcourt_latched == 0u &&
          dispatch_state.players[5].action == DD_PLAYER_INBOUNDER,
          "$9583 reason $15 clears its latch through $9395 and changes sides");

    dispatch_state = dispatch_base;
    dispatch_state.ball.action = DD_BALL_HIDDEN;
    dispatch_state.ball.owner = 0xFFu;
    dispatch_state.ball.court_x = 0x003000;
    dispatch_state.ball.court_depth = 0x005800;
    dispatch_state.possession_direction = 0u;
    check(dd_gameplay_step(&pack, &dispatch_state, 0u),
          "run $95E0 sloped boundary rule");
    check(dispatch_state.phase == DD_GAMEPLAY_INBOUND &&
          dispatch_state.inbound_reason == 0x16u &&
          dispatch_state.possession_direction == 1u &&
          dispatch_state.players[0].action == DD_PLAYER_INBOUNDER &&
          dispatch_state.players[0].target_zone == 0x23u &&
          dispatch_state.players[0].target_depth == 0x009800,
          "$95E0 reason $16 uses $9763 and the ninth packed target bit");

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
    dispatch_state.players[5].paired_player = 0u;
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
    dispatch_state.players[5].action = DD_PLAYER_LIVE_PAIRED_DEFENDER;
    dispatch_state.players[0].action = DD_PLAYER_USER_SHOOT;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_JUMP_START,
          "player state $22 shares $9139 and contests paired user shot state $03");

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
    dispatch_state.ball.action = DD_BALL_AIRBORNE;
    dispatch_state.ball.court_x = 0x001000;
    dispatch_state.ball.height = 0x5000;
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
    check(dispatch_state.ball.owner == 5u && dispatch_state.carrier == 0xFFu &&
          dispatch_state.ball.action == DD_BALL_AWARDED,
          "player state $24 uses $A6C3's boxes and $8B12 converts contact to owned ball state $00");

    dispatch_state = dispatch_base;
    dispatch_state.players[5].action = DD_PLAYER_JUMP_CONTEST;
    dispatch_state.players[5].height = 0x2000;
    dispatch_state.players[5].height_script_index = 11u;
    dispatch_state.ball.action = DD_BALL_AIRBORNE;
    dispatch_state.ball.owner = 0u;
    dispatch_state.carrier = 0xFFu;
    dispatch_state.ball.court_x = dispatch_state.players[5].court_x - 0x0600;
    dispatch_state.ball.height = dispatch_state.players[5].height + 0x0800;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.ball.owner == 5u && dispatch_state.carrier == 0xFFu &&
          dispatch_state.ball.action == DD_BALL_AWARDED,
          "$8B12 block contact takes an owned airborne shot without transferring early");
    for (player = 0u; player < 26u; ++player) {
        run_cpu_dispatch(&pack, &dispatch_state, 5u);
    }
    check(dispatch_state.carrier == 5u && dispatch_state.ball.owner == 5u &&
          dispatch_state.ball.action == DD_BALL_DRIBBLE &&
          dispatch_state.players[5].action == DD_PLAYER_LIVE_CARRIER,
          "$8B44->$9208 transfers possession and resets teams when the blocker lands");

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
    dispatch_state.players[5].action = DD_PLAYER_LIVE_CONTINUE;
    dispatch_state.players[5].court_x = 0x01F180;
    dispatch_state.players[5].court_depth = 0x009880;
    dispatch_state.players[5].velocity_x = 0x0100;
    dispatch_state.players[5].velocity_depth = 0x0100;
    dispatch_state.carrier = 0xFFu;
    dispatch_state.ball.owner = 0xFFu;
    dispatch_state.ball.action = DD_BALL_SHOT_GATHER;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].court_x == 0x01F180 &&
          dispatch_state.players[5].court_depth == 0x009880 &&
          dispatch_state.players[5].velocity_x == 0 &&
          dispatch_state.players[5].velocity_depth == 0,
          "$9CA0/$9CF6 reject upper-bound candidates, preserve both coordinates, and clear speed");

    dispatch_state = dispatch_base;
    dispatch_state.players[5].action = DD_PLAYER_LIVE_CONTINUE;
    dispatch_state.players[5].court_x = 0x001000;
    dispatch_state.players[5].court_depth = 0x000500;
    dispatch_state.players[5].velocity_x = -0x0100;
    dispatch_state.players[5].velocity_depth = -0x0100;
    dispatch_state.carrier = 0xFFu;
    dispatch_state.ball.owner = 0xFFu;
    dispatch_state.ball.action = DD_BALL_SHOT_GATHER;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].court_x == 0x001000 &&
          dispatch_state.players[5].court_depth == 0x000500 &&
          dispatch_state.players[5].velocity_x == 0 &&
          dispatch_state.players[5].velocity_depth == 0,
          "$9CA0/$9CF6 apply the same preserve-and-stop rule at the lower court edges");

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
    dispatch_state.players[5].target_zone = (uint8_t)(
        (((uint32_t)(dispatch_state.players[5].court_depth >> 8) << 1u) & 0xE0u) |
        (((uint32_t)dispatch_state.players[5].court_x >> 12u) & 0x1Fu));
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_REBOUND_CLAIM,
          "player state $2D advances to rebound claim $2E at its target");

    dispatch_state = dispatch_base;
    dispatch_state.players[5].action = DD_PLAYER_REBOUND_CHASE;
    dispatch_state.players[5].court_x = 0x011000;
    dispatch_state.players[5].court_depth = 0x002000;
    dispatch_state.players[5].target_x = 0x012000;
    dispatch_state.players[5].target_depth = 0x001800;
    dispatch_state.players[5].target_zone = (uint8_t)(
        (((uint32_t)(dispatch_state.players[5].target_depth >> 8) << 1u) & 0xE0u) |
        (((uint32_t)dispatch_state.players[5].target_x >> 12u) & 0x1Fu));
    dispatch_state.players[5].velocity_x = 0;
    dispatch_state.players[5].velocity_depth = 0;
    dispatch_state.players[5].animation = 0x21u;
    dispatch_state.cpu_priority_player = 5u;
    inbound_receiver = 0u;
    for (player = 0u; player < 96u &&
         dispatch_state.players[5].action == DD_PLAYER_REBOUND_CHASE; ++player) {
        run_cpu_dispatch(&pack, &dispatch_state, 5u);
        if (dispatch_state.players[5].animation != 0x21u) inbound_receiver = 1u;
        check(dispatch_state.players[5].velocity_x >= 0 &&
              dispatch_state.players[5].velocity_depth <= 0,
              "$8E71 refreshes only priority $004D with a stable exact diagonal toward the loose ball");
    }
    check(dispatch_state.players[5].action == DD_PLAYER_REBOUND_CLAIM,
          "$8E71 reaches rebound claim with the recovered $ABCD vector");
    check(inbound_receiver != 0u,
          "$8E71->$D98D->$D990 advances a run animation while approaching the loose ball");

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
    dispatch_state.players[5].action = DD_PLAYER_REBOUND_RETURN;
    dispatch_state.players[5].target_x = dispatch_state.players[5].court_x;
    dispatch_state.players[5].target_depth = dispatch_state.players[5].court_depth + 0x2000;
    dispatch_state.players[5].target_zone = (uint8_t)(
        (((uint32_t)(dispatch_state.players[5].target_depth >> 8) << 1u) & 0xE0u) |
        (((uint32_t)dispatch_state.players[5].target_x >> 12u) & 0x1Fu));
    dispatch_state.players[5].velocity_x = 0;
    dispatch_state.players[5].velocity_depth = 0x000Cu;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_REBOUND_RETURN,
          "$8EBF/$D978 does not enter inbound hold while only the X target matches");

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
    dispatch_state.players[0].role = 2u;
    dispatch_state.players[2].role = 0u;
    dispatch_state.players[5].action = DD_PLAYER_INBOUND_HOLD;
    dispatch_state.ball.owner = 5u;
    dispatch_state.carrier = 5u;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.controlled_player == 2u &&
          dispatch_state.players[2].action == DD_PLAYER_LIVE_USER &&
          dispatch_state.players[0].action == DD_PLAYER_LIVE_TEAMMATE,
          "$8F8D->$9097 restores user control to role zero rather than physical slot zero");

    dispatch_state = dispatch_base;
    for (player = 0u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
        dispatch_state.players[player].action = DD_PLAYER_LIVE_SET;
        dispatch_state.players[player].role = (uint8_t)(player % 5u);
    }
    dispatch_state.players[5].action = DD_PLAYER_INBOUND_HOLD;
    dispatch_state.players[5].hold_timer = 0x0Au;
    dispatch_state.inbound_variant = 1u;
    dispatch_state.possession_direction = 1u;
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
    dispatch_state.inbound_variant = 1u;
    dispatch_state.possession_direction = 0u;
    dispatch_state.cpu_global_frame = 0u;
    dispatch_state.camera_x = dispatch_state.players[5].court_x - 0x8000;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_INBOUND_READY &&
          dispatch_state.ball.receiver >= 5u &&
          dispatch_state.ball.receiver < DD_GAMEPLAY_PLAYER_COUNT &&
          dispatch_state.ball.receiver != 5u,
          "player state $30 clear-mode made basket follows $A0DA->$9018 automatic inbound");
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
    dispatch_state.players[5].facing = 0u;
    dispatch_state.players[5].target_x = dispatch_state.players[5].court_x;
    dispatch_state.players[5].target_depth = dispatch_state.players[5].court_depth;
    dispatch_state.players[5].target_zone = (uint8_t)(
        (((uint32_t)(dispatch_state.players[5].court_depth >> 8) << 1u) & 0xE0u) |
        (((uint32_t)dispatch_state.players[5].court_x >> 12u) & 0x1Fu));
    dispatch_state.ball.owner = 0xFFu;
    dispatch_state.carrier = 0xFFu;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_INBOUND_HOLD &&
          dispatch_state.ball.owner == 5u &&
          dispatch_state.ball.court_x == dispatch_state.players[5].court_x + 0x0800 &&
          dispatch_state.ball.court_depth == dispatch_state.players[5].court_depth - 0x0100 &&
          dispatch_state.ball.height == 0x10C0,
          "$8C6B->$AD0E->$B035 claims the inbound ball at facing-zero X/depth offsets before state $30");

    dispatch_state = dispatch_base;
    dispatch_state.phase = DD_GAMEPLAY_INBOUND;
    dispatch_state.players[5].action = DD_PLAYER_INBOUNDER;
    dispatch_state.players[5].facing = 0u;
    dispatch_state.players[5].court_x = 0x01F000;
    dispatch_state.players[5].court_depth = 0x000800;
    dispatch_state.players[5].target_x = 0x01F800;
    dispatch_state.players[5].target_depth = 0x000800;
    dispatch_state.players[5].target_zone = 0x1Fu;
    dispatch_state.ball.owner = 0xFFu;
    dispatch_state.carrier = 0xFFu;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_INBOUND_HOLD &&
          dispatch_state.players[5].court_x == 0x01F000 &&
          dispatch_state.players[5].court_x <= 0x01F100 &&
          dispatch_state.ball.owner == 5u,
          "$8C6B/$D978 claims an edge-cell inbound before chasing its unreachable $1F8 center");

    held_shot_state = dispatch_base;
    held_shot_state.phase = DD_GAMEPLAY_LIVE;
    held_shot_state.possession_direction = 1u;
    held_shot_state.controlled_player = 0u;
    held_shot_state.carrier = 0u;
    held_shot_state.cpu_global_frame = 0u;
    held_shot_state.ball.action = DD_BALL_DRIBBLE;
    held_shot_state.ball.owner = 0u;
    held_shot_state.ball.receiver = 0xFFu;
    held_shot_state.players[0].action = DD_PLAYER_LIVE_USER_CARRIER;
    held_shot_state.players[0].facing = 0u;
    held_shot_state.players[0].court_x = 0x00F700;
    held_shot_state.players[0].court_depth = 0x006100;
    held_shot_state.players[0].height = 0x1000;
    for (player = 1u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
        held_shot_state.players[player].action = DD_PLAYER_ROUTE_WAIT;
    }
    check(dd_gameplay_step(&pack, &held_shot_state, DD_INPUT_B),
          "start held user shot through $AA75");
    for (player = 0u; player < 6u; ++player) {
        check(dd_gameplay_step(&pack, &held_shot_state, DD_INPUT_B),
              "keep NES B held through $A504 release gate");
    }
    check(held_shot_state.players[0].action == DD_PLAYER_USER_SHOOT &&
          held_shot_state.ball.action == DD_BALL_SHOT_GATHER &&
          held_shot_state.ball.owner == 0u &&
          held_shot_state.ball.height == held_shot_state.players[0].height + 0x1200,
          "$A516-$A520 keeps ball $04 attached while controller bit $40 remains held");
    check(dd_gameplay_step(&pack, &held_shot_state, 0u),
          "release NES B through $A522->$B189");
    check(held_shot_state.ball.action == DD_BALL_AIRBORNE,
          "clearing controller bit $40 launches the held shot on release");

    held_shot_state = dispatch_base;
    held_shot_state.phase = DD_GAMEPLAY_LIVE;
    held_shot_state.possession_direction = 1u;
    held_shot_state.controlled_player = 0u;
    held_shot_state.carrier = 0u;
    held_shot_state.cpu_global_frame = 0u;
    held_shot_state.ball.action = DD_BALL_DRIBBLE;
    held_shot_state.ball.owner = 0u;
    held_shot_state.players[0].action = DD_PLAYER_LIVE_USER_CARRIER;
    for (player = 1u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
        held_shot_state.players[player].action = DD_PLAYER_ROUTE_WAIT;
    }
    check(dd_gameplay_step(&pack, &held_shot_state, DD_INPUT_B),
          "start long-held user shot");
    for (player = 0u; player < 80u && held_shot_state.phase == DD_GAMEPLAY_LIVE; ++player) {
        check(dd_gameplay_step(&pack, &held_shot_state, DD_INPUT_B),
              "hold user shot through landing");
    }
    check(held_shot_state.phase == DD_GAMEPLAY_INBOUND &&
          held_shot_state.inbound_reason == 0x0Fu &&
          held_shot_state.audio_event == 0x2Cu,
          "$A52B-$A540 turns an unreleased landing into reason-$0F inbound with whistle $2C");

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
          dispatch_state.inbound_reason == 0x1Au && dispatch_state.dead_ball_latch == 0xFFu &&
          dispatch_state.audio_event == 0x30u && dispatch_state.audio_event_serial != 0u &&
          dispatch_state.ball.action == DD_BALL_DEAD &&
          dispatch_state.players[5].action == DD_PLAYER_FREE_THROW_SHOOTER &&
          dispatch_state.players[1].action == DD_PLAYER_FREE_THROW_FORMATION,
          "$A347 zero-clock same-facing contact enters the foul/free-throw dead ball");
    free_throw_state = dispatch_state;

    dispatch_state = dispatch_base;
    dispatch_state.controlled_player = 0u;
    dispatch_state.carrier = 0u;
    dispatch_state.ball.owner = 0u;
    dispatch_state.ball.action = DD_BALL_DRIBBLE;
    dispatch_state.players[0].action = DD_PLAYER_LIVE_USER_CARRIER;
    dispatch_state.players[0].target_zone = 0x80u;
    dispatch_state.players[5].action = DD_PLAYER_LIVE_PAIRED_DEFENDER;
    dispatch_state.players[5].target_zone = 0x80u;
    dispatch_state.players[5].facing = 0u;
    dispatch_state.scene_frame = dispatch_state.next_clock_frame - 1u;
    check(dd_gameplay_step(&pack, &dispatch_state, DD_INPUT_LEFT),
          "step controlled $A37D exceptional contact");
    check(dispatch_state.phase == DD_GAMEPLAY_FREE_THROW &&
          dispatch_state.foul_shooter == 5u && dispatch_state.foul_offender == 0u &&
          dispatch_state.inbound_reason == 0x17u && dispatch_state.dead_ball_latch == 0xFFu &&
          dispatch_state.ball.action == DD_BALL_DEAD && dispatch_state.carrier == 0xFFu &&
          dispatch_state.players[5].animation == 0x29u &&
          dispatch_state.possession_direction == 0u,
          "$A1CC->$A37D reason $17 takes $98A3 and awards the opposing defender");

    dispatch_state = dispatch_base;
    dispatch_state.controlled_player = 0u;
    dispatch_state.carrier = 0u;
    dispatch_state.ball.owner = 0u;
    dispatch_state.ball.action = DD_BALL_DRIBBLE;
    dispatch_state.players[0].action = DD_PLAYER_LIVE_USER_CARRIER;
    dispatch_state.possession_rule_age = 24u * 64u - 1u;
    check(dd_gameplay_step(&pack, &dispatch_state, 0u),
          "step controlled $2C possession-rule whistle request");
    check(dispatch_state.inbound_reason == 0x14u && dispatch_state.audio_event == 0x2Cu &&
          dispatch_state.audio_event_serial != 0u,
          "$A1D9->$C141 queues exact whistle event $2C before common inbound setup");
    dispatch_state = free_throw_state;
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
    check(dispatch_state.ball.action == DD_BALL_SCORE && dispatch_state.score[1] == 0u &&
          dispatch_state.audio_event == 0x18u &&
          dispatch_state.audio_event_serial != 0u,
          "$B377 result one reaches $AE8E's $18 cue before the deferred point award");
    for (player = 0u; player < 4u; ++player) {
        check(dd_gameplay_step(&pack, &dispatch_state, 0u),
              "advance free-throw score counter to $08");
    }
    check(dispatch_state.score[1] == 1u,
          "$AEDE counter $08 awards one point while the foul shot is active");

    dispatch_state = dispatch_base;
    dispatch_state.phase = DD_GAMEPLAY_LIVE;
    dispatch_state.ball.action = DD_BALL_SCORE;
    dispatch_state.ball.action_age = 3u;
    dispatch_state.ball.height = 0x3200;
    dispatch_state.last_shooter = 0u;
    dispatch_state.shot_value = 3u;
    dispatch_state.score[0] = 0u;
    dispatch_state.audio_event = 0u;
    dispatch_state.audio_event_serial = 0u;
    check(dd_gameplay_step(&pack, &dispatch_state, 0u),
          "advance a three-point make to $AEDE counter $08");
    check(dispatch_state.score[0] == 3u &&
          dispatch_state.audio_event == 0x25u &&
          dispatch_state.audio_event_serial != 0u,
          "$AEDE shot kind $01 awards three points and queues SFX $25");

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
    dispatch_state.carrier = 5u;
    dispatch_state.ball.held_height_offset = 0x18u;
    dispatch_state.players[5].height = 0x2600;
    check(dd_gameplay_step(&pack, &dispatch_state, 0u), "step tip-awarded ball state $00");
    check(dispatch_state.ball.height == 0x3E00 && dispatch_state.carrier == 5u,
          "ball state $00 applies $ACB6's traced tip-award height offset $18");

    dispatch_state = dispatch_base;
    dispatch_state.phase = DD_GAMEPLAY_LIVE;
    dispatch_state.ball.action = DD_BALL_AWARDED;
    dispatch_state.ball.owner = 0u;
    dispatch_state.carrier = 0xFFu;
    dispatch_state.ball.held_height_offset = 0x08u;
    dispatch_state.players[0].height = 0x1000;
    check(dd_gameplay_step(&pack, &dispatch_state, 0u), "step ordinary awarded ball state $00");
    check(dispatch_state.ball.height == 0x1800 && dispatch_state.carrier == 0xFFu,
          "ball state $00 attaches to its owner without changing $0048 camera ownership");

    dispatch_state = dispatch_base;
    dispatch_state.ball.action = DD_BALL_PASS;
    dispatch_state.ball.owner = 0xFFu;
    dispatch_state.ball.receiver = 0u;
    dispatch_state.ball.action_age = 0u;
    dispatch_state.ball.court_x = dispatch_state.players[0].court_x;
    dispatch_state.ball.court_depth = dispatch_state.players[0].court_depth;
    check(dd_gameplay_step(&pack, &dispatch_state, 0u),
          "step a new pass already inside $B138 receiver boxes");
    check(dispatch_state.ball.action == DD_BALL_DRIBBLE && dispatch_state.ball.owner == 0u,
          "ball state $02 catches immediately when $B138 reports receiver overlap");

    dispatch_state = dispatch_base;
    dispatch_state.phase = DD_GAMEPLAY_LIVE;
    dispatch_state.controlled_player = 0u;
    dispatch_state.carrier = 0u;
    dispatch_state.ball.action = DD_BALL_DRIBBLE;
    dispatch_state.ball.owner = 0u;
    dispatch_state.previous_input = DD_INPUT_LEFT;
    dispatch_state.camera_x = 0x008000;
    dispatch_state.players[0].action = DD_PLAYER_LIVE_USER_CARRIER;
    dispatch_state.players[0].role = 0u;
    dispatch_state.players[0].court_x = 0x010000;
    dispatch_state.players[0].court_depth = 0x005800;
    dispatch_state.players[0].height = 0;
    dispatch_state.ball.court_x = dispatch_state.players[0].court_x;
    dispatch_state.ball.court_depth = dispatch_state.players[0].court_depth;
    for (player = 1u; player < 5u; ++player) {
        dispatch_state.players[player].action = DD_PLAYER_ROUTE_WAIT;
        dispatch_state.players[player].role = (uint8_t)player;
        dispatch_state.players[player].court_depth = 0x005800;
        dispatch_state.players[player].height = 0;
    }
    dispatch_state.players[1].court_x = 0x014000;
    dispatch_state.players[2].court_x = 0x00C000;
    dispatch_state.players[3].court_x = 0x012000;
    dispatch_state.players[4].court_x = 0x00A000;
    check(dd_gameplay_step(&pack, &dispatch_state, DD_INPUT_LEFT | DD_INPUT_A),
          "start a directional user pass");
    check(dispatch_state.ball.action == DD_BALL_PASS &&
          dispatch_state.ball.receiver == 4u &&
          dispatch_state.players[0].action == DD_PLAYER_USER_PASS_RECOVER &&
          dispatch_state.players[4].action == DD_PLAYER_USER_PASS_RECEIVE &&
          dispatch_state.controlled_player == 0u,
          "$A129 selects the later left-side teammate while $AD41 keeps control on the passer in flight");
    check(dispatch_state.ball.velocity_x == -0x04FB &&
          dispatch_state.ball.velocity_depth == 0x003C &&
          dispatch_state.players[0].facing == 4u,
          "$B0AB multiplies exact-$B035's signed $FF01/$000C unit vector by five");
    live_start_x[0] = dispatch_state.players[0].court_x;
    check(dd_gameplay_step(&pack, &dispatch_state, DD_INPUT_RIGHT),
          "hold movement during user pass recovery");
    check(dispatch_state.players[0].court_x == live_start_x[0],
          "pass-recovery state $05 ignores movement input while the ball is in flight");
    dispatch_state.ball.action_age = 18u;
    dispatch_state.ball.court_x = dispatch_state.players[4].court_x;
    dispatch_state.ball.court_depth = dispatch_state.players[4].court_depth;
    check(dd_gameplay_step(&pack, &dispatch_state, 0u),
          "finish a directional user pass reception");
    check(dispatch_state.ball.action == DD_BALL_DRIBBLE &&
          dispatch_state.ball.owner == 4u && dispatch_state.carrier == 4u &&
          dispatch_state.controlled_player == 4u &&
          dispatch_state.players[4].action == DD_PLAYER_LIVE_USER_CARRIER &&
          dispatch_state.players[0].action == DD_PLAYER_USER_PASS_RECOVER,
          "$AD58 transfers owner, camera/control, and user-carrier state only to the receiver");
    live_start_x[4] = dispatch_state.players[4].court_x;
    check(dd_gameplay_step(&pack, &dispatch_state, DD_INPUT_RIGHT),
          "move the newly controlled receiver");
    check(dispatch_state.players[4].court_x == live_start_x[4] + 0x0130 &&
          dispatch_state.controlled_player == 4u,
          "the dynamic CPU scheduler skips the passed-to user instead of continuing to drive it");

    dispatch_state = dispatch_base;
    dispatch_state.phase = DD_GAMEPLAY_LIVE;
    dispatch_state.controlled_player = 0u;
    dispatch_state.carrier = 0xFFu;
    dispatch_state.players[0].action = DD_PLAYER_LIVE_USER;
    dispatch_state.players[0].court_x = 0x01F180;
    dispatch_state.players[0].court_depth = 0x009880;
    dispatch_state.ball.action = DD_BALL_SHOT_GATHER;
    dispatch_state.ball.owner = 0xFFu;
    dispatch_state.ball.court_x = 0x010000;
    dispatch_state.ball.court_depth = 0x005000;
    check(dd_gameplay_step(&pack, &dispatch_state, DD_INPUT_RIGHT | DD_INPUT_UP),
          "drive the user into both upper fixed-point court bounds");
    check(dispatch_state.players[0].court_x == 0x01F180 &&
          dispatch_state.players[0].court_depth == 0x009880 &&
          dispatch_state.players[0].velocity_x == 0 &&
          dispatch_state.players[0].velocity_depth == 0,
          "controlled movement uses $9CA0/$9CF6 rejection instead of clamping through the edge");

    dispatch_state = dispatch_base;
    dispatch_state.phase = DD_GAMEPLAY_LIVE;
    dispatch_state.controlled_player = 0u;
    dispatch_state.carrier = 0xFFu;
    dispatch_state.players[0].action = DD_PLAYER_LIVE_USER;
    dispatch_state.players[0].court_x = 0x001000;
    dispatch_state.players[0].court_depth = 0x000500;
    dispatch_state.ball.action = DD_BALL_SHOT_GATHER;
    dispatch_state.ball.owner = 0xFFu;
    dispatch_state.ball.court_x = 0x010000;
    dispatch_state.ball.court_depth = 0x005000;
    check(dd_gameplay_step(&pack, &dispatch_state, DD_INPUT_LEFT | DD_INPUT_DOWN),
          "drive the user into both lower fixed-point court bounds");
    check(dispatch_state.players[0].court_x == 0x001000 &&
          dispatch_state.players[0].court_depth == 0x000500 &&
          dispatch_state.players[0].velocity_x == 0 &&
          dispatch_state.players[0].velocity_depth == 0,
          "controlled lower-edge movement preserves sub-cell position and clears rejected axes");

    dispatch_state = dispatch_base;
    dispatch_state.phase = DD_GAMEPLAY_LIVE;
    dispatch_state.cpu_global_frame = 0u;
    dispatch_state.controlled_player = 0u;
    dispatch_state.carrier = 0u;
    dispatch_state.ball.action = DD_BALL_DRIBBLE;
    dispatch_state.ball.owner = 0u;
    dispatch_state.players[0].action = DD_PLAYER_LIVE_USER_CARRIER;
    dispatch_state.players[0].court_x = 0x010000;
    dispatch_state.players[0].court_depth = 0x005800;
    dispatch_state.players[0].height = 0x1000;
    dispatch_state.players[5].action = DD_PLAYER_LIVE_PAIRED_DEFENDER;
    dispatch_state.players[5].court_x = dispatch_state.players[0].court_x;
    dispatch_state.players[5].court_depth = dispatch_state.players[0].court_depth;
    dispatch_state.ball.court_x = dispatch_state.players[0].court_x;
    dispatch_state.ball.court_depth = dispatch_state.players[0].court_depth;
    check(dd_gameplay_step(&pack, &dispatch_state, DD_INPUT_B),
          "start a user shot through the live B-button path");
    check(dispatch_state.players[0].action == DD_PLAYER_USER_SHOOT &&
          dispatch_state.players[0].animation == 0x22u &&
          dispatch_state.ball.action == DD_BALL_SHOT_GATHER &&
          dispatch_state.ball.owner == 0u,
          "$AA75->$A896 exposes state $03, shot pose $22 and ball state $04");
    check(dd_gameplay_step(&pack, &dispatch_state, 0u),
          "advance the user shot to the paired CPU dispatcher");
    check(dispatch_state.players[5].action == DD_PLAYER_JUMP_START &&
          dispatch_state.ball.action == DD_BALL_AIRBORNE &&
          dispatch_state.ball.owner == 0u && dispatch_state.carrier == 0xFFu,
          "$8A98->$9139 starts the contest as $B189 releases owned ball state $05");
    check(dispatch_state.ball.velocity_x == -0x00FF &&
          dispatch_state.ball.velocity_depth == 0x000C &&
          dispatch_state.shot_value == 3u &&
          dispatch_state.audio_event == 0x09u &&
          dispatch_state.ball.flight_duration == 0xBFu &&
          dispatch_state.ball.flight_curve == 0x2Fu &&
          dispatch_state.ball.velocity_height == 0x0222u &&
          dispatch_state.ball.height == 0x2200,
          "$B189->$A7EA->$9D2D/$9BB0 marks the cross-half-court shot as three and installs its arc");

    dispatch_state = dispatch_base;
    dispatch_state.phase = DD_GAMEPLAY_LIVE;
    dispatch_state.cpu_global_frame = 0u;
    dispatch_state.controlled_player = 0u;
    dispatch_state.carrier = 0u;
    dispatch_state.ball.action = DD_BALL_DRIBBLE;
    dispatch_state.ball.owner = 0u;
    dispatch_state.players[0].action = DD_PLAYER_LIVE_USER_CARRIER;
    dispatch_state.players[0].court_x = 0x018000;
    dispatch_state.players[0].court_depth = 0x005800;
    dispatch_state.players[0].height = 0x1000;
    for (player = 1u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
        dispatch_state.players[player].action = DD_PLAYER_ROUTE_WAIT;
    }
    check(dd_gameplay_step(&pack, &dispatch_state, DD_INPUT_B) &&
          dd_gameplay_step(&pack, &dispatch_state, 0u),
          "launch an inside user-side field goal");
    check(dispatch_state.ball.action == DD_BALL_AIRBORNE &&
          dispatch_state.shot_value == 2u && dispatch_state.audio_event != 0x09u,
          "$A7EA user-side depth-$58 ball X beyond boundary $40 is a two");

    dispatch_state = dispatch_base;
    dispatch_state.phase = DD_GAMEPLAY_LIVE;
    dispatch_state.cpu_global_frame = 0u;
    dispatch_state.controlled_player = 0u;
    dispatch_state.carrier = 5u;
    dispatch_state.ball.action = DD_BALL_SHOT_GATHER;
    dispatch_state.ball.owner = 5u;
    dispatch_state.ball.action_age = 0u;
    dispatch_state.players[5].action = DD_PLAYER_LIVE_CARRIER_DECIDE;
    dispatch_state.players[5].height_script_index = 24u;
    dispatch_state.players[5].height_script_reverse = 0u;
    dispatch_state.players[5].court_x = 0x00E000;
    dispatch_state.players[5].court_depth = 0x005800;
    dispatch_state.players[5].height = 0x1000;
    for (player = 1u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
        if (player != 5u) dispatch_state.players[player].action = DD_PLAYER_ROUTE_WAIT;
    }
    check(dd_gameplay_step(&pack, &dispatch_state, 0u),
          "launch an outside CPU-side field goal");
    check(dispatch_state.ball.action == DD_BALL_AIRBORNE &&
          dispatch_state.shot_value == 3u && dispatch_state.audio_event == 0x09u,
          "$A7EA CPU-side depth-$58 ball X beyond mirrored boundary $C0 is a three");

    dispatch_state = dispatch_base;
    dispatch_state.phase = DD_GAMEPLAY_LIVE;
    dispatch_state.cpu_global_frame = 0u;
    dispatch_state.controlled_player = 0u;
    dispatch_state.carrier = 5u;
    dispatch_state.ball.action = DD_BALL_SHOT_GATHER;
    dispatch_state.ball.owner = 5u;
    dispatch_state.ball.action_age = 0u;
    dispatch_state.players[5].action = DD_PLAYER_LIVE_CARRIER_DECIDE;
    dispatch_state.players[5].height_script_index = 24u;
    dispatch_state.players[5].height_script_reverse = 0u;
    dispatch_state.players[5].court_x = 0x008000;
    dispatch_state.players[5].court_depth = 0x005800;
    dispatch_state.players[5].height = 0x1000;
    for (player = 1u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
        if (player != 5u) dispatch_state.players[player].action = DD_PLAYER_ROUTE_WAIT;
    }
    check(dd_gameplay_step(&pack, &dispatch_state, 0u),
          "launch an inside CPU-side field goal");
    check(dispatch_state.ball.action == DD_BALL_AIRBORNE &&
          dispatch_state.shot_value == 2u && dispatch_state.audio_event != 0x09u,
          "$A7EA CPU-side depth-$58 ball X inside mirrored boundary $C0 is a two");

    dispatch_state = dispatch_base;
    dispatch_state.phase = DD_GAMEPLAY_LIVE;
    dispatch_state.controlled_player = 0u;
    dispatch_state.carrier = 0u;
    dispatch_state.possession_direction = 0u;
    dispatch_state.ball.action = DD_BALL_DRIBBLE;
    dispatch_state.ball.owner = 0u;
    dispatch_state.players[0].action = DD_PLAYER_LIVE_USER_CARRIER;
    dispatch_state.players[0].court_x = 0x012000;
    dispatch_state.players[0].court_depth = 0x005800;
    dispatch_state.players[0].height = 0x1000;
    check(dd_gameplay_step(&pack, &dispatch_state, DD_INPUT_B) &&
          dd_gameplay_step(&pack, &dispatch_state, 0u),
          "launch a cross-court user shot");
    check(dispatch_state.ball.action == DD_BALL_AIRBORNE &&
          dispatch_state.shot_value == 3u &&
          dispatch_state.ball.flight_duration == 0xD8u &&
          dispatch_state.ball.flight_curve == 0x36u &&
          dispatch_state.ball.velocity_height == 0x0207,
          "$B343-$B373 applies the cross-court duration/curve/base override");

    dispatch_state = dispatch_base;
    dispatch_state.phase = DD_GAMEPLAY_LIVE;
    dispatch_state.possession_direction = 1u;
    dispatch_state.controlled_player = 0u;
    dispatch_state.carrier = 0u;
    dispatch_state.cpu_global_frame = 0u;
    dispatch_state.ball.action = DD_BALL_DRIBBLE;
    dispatch_state.ball.owner = 0u;
    dispatch_state.players[0].action = DD_PLAYER_LIVE_USER_CARRIER;
    dispatch_state.players[0].facing = 0u;
    dispatch_state.players[0].court_x = 0x00D000;
    dispatch_state.players[0].court_depth = 0x006100;
    dispatch_state.players[0].height = 0x1000;
    for (player = 1u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
        dispatch_state.players[player].action = DD_PLAYER_ROUTE_WAIT;
    }
    check(dd_gameplay_step(&pack, &dispatch_state, DD_INPUT_RIGHT | DD_INPUT_B),
          "start a moving user shot through the real B edge");
    live_start_x[0] = dispatch_state.players[0].court_x;
    check(dispatch_state.players[0].action == DD_PLAYER_USER_SHOOT &&
          dispatch_state.players[0].velocity_x != 0 &&
          dispatch_state.players[0].animation == assets->shot_animation[0],
          "$AA75 preserves takeoff velocity and selects the facing-zero shot pose");
    for (player = 0u; player < 4u; ++player) {
        check(dd_gameplay_step(&pack, &dispatch_state, 0u),
              "advance moving user shot through $A504->$A84C");
    }
    check(dispatch_state.players[0].court_x > live_start_x[0] &&
          dispatch_state.players[0].action == DD_PLAYER_USER_SHOOT &&
          dispatch_state.players[0].animation == assets->shot_animation[0],
          "$A504 double-integrates takeoff momentum while $A896 retains the shot sprite");

    dispatch_state = dispatch_base;
    dispatch_state.phase = DD_GAMEPLAY_LIVE;
    dispatch_state.possession_direction = 1u;
    dispatch_state.controlled_player = 0u;
    dispatch_state.carrier = 0u;
    dispatch_state.ball.action = DD_BALL_DRIBBLE;
    dispatch_state.ball.owner = 0u;
    dispatch_state.players[0].action = DD_PLAYER_LIVE_USER_CARRIER;
    dispatch_state.players[0].facing = 0u;
    dispatch_state.players[0].court_x = 0x00F700;
    dispatch_state.players[0].court_depth = 0x006100;
    dispatch_state.players[0].height = 0x1000;
    for (player = 1u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
        dispatch_state.players[player].action = DD_PLAYER_ROUTE_WAIT;
    }
    check(dd_gameplay_step(&pack, &dispatch_state, DD_INPUT_B),
          "start controlled full-path made-shot proof");
    check(dispatch_state.players[0].animation == 0x22u,
          "$A896 selects facing-zero shooting metasprite $22 during gather");
    check(dd_gameplay_step(&pack, &dispatch_state, 0u),
          "release controlled make through $A504->$B189");
    check(dispatch_state.ball.action == DD_BALL_AIRBORNE &&
          dispatch_state.ball.court_x == 0x00FD00 &&
          dispatch_state.ball.court_depth == 0x006100 &&
          dispatch_state.ball.height == 0x2200 &&
          dispatch_state.ball.velocity_x == 0x00FF &&
          dispatch_state.ball.velocity_depth == -0x000C &&
          dispatch_state.ball.flight_duration == 0xBCu &&
          dispatch_state.ball.flight_curve == 0x2Fu &&
          dispatch_state.ball.velocity_height == 0x021Du,
          "$B189 reproduces the controlled original make launch tuple exactly");
    for (player = 0u; player < 400u &&
         dispatch_state.ball.action != DD_BALL_SCORE &&
         dispatch_state.ball.action != DD_BALL_LOOSE_LAUNCH; ++player) {
        check(dd_gameplay_step(&pack, &dispatch_state, 0u),
              "advance controlled shot through $B189->$AE25->$B377");
    }
    check(dispatch_state.ball.action == DD_BALL_SCORE &&
          dispatch_state.ball.outcome == 1u,
          "original make coordinates reach $B377 result one through the shipping shot loop");
    check(dispatch_state.net_animation_phase == 2u &&
          dispatch_state.net_basket_side == 1u,
          "$AE25 selects right-basket net phase two on the made shot");
    net_state = dispatch_state;
    while (net_state.ball.action == DD_BALL_SCORE &&
           net_state.ball.action_age < 4u) {
        check(dd_gameplay_step(&pack, &net_state, 0u),
              "advance score state to $AEDE counter $08");
    }
    check(net_state.net_animation_phase == 1u,
          "$AEDE counter $08 selects net phase one with the score update");
    while (net_state.ball.action == DD_BALL_SCORE) {
        check(dd_gameplay_step(&pack, &net_state, 0u),
              "advance score state through counter underflow");
    }
    check(net_state.net_animation_phase == 0u,
          "$AEDE counter underflow restores the normal net tiles");
    check(dd_gameplay_step(&pack, &dispatch_state, 0u) &&
          dd_gameplay_step(&pack, &dispatch_state, 0u),
          "dispatch both teams through the pending $8491 formation gate");
    check(dispatch_state.players[5].action == DD_PLAYER_REBOUND_CHASE &&
          dispatch_state.players[0].action == DD_PLAYER_INBOUND_FORMATION,
          "$AE25->$8491 assigns the receiving CPU role zero to $2D after a user make");
    check(dispatch_state.players[5].velocity_x ==
              dispatch_state.players[5].route_velocity_x &&
          dispatch_state.players[5].velocity_depth ==
              dispatch_state.players[5].route_velocity_depth &&
          (dispatch_state.players[5].velocity_x != 0 ||
           dispatch_state.players[5].velocity_depth != 0),
          "$8491->$ABCD installs the exact fixed-point chase vector before state $2D runs");
    for (player = 0u; player < 600u &&
         dispatch_state.ball.action != DD_BALL_PASS; ++player) {
        check(dd_gameplay_step(&pack, &dispatch_state, 0u),
              "advance user make through $2D->$2E->$2F->$30->$31 inbound");
    }
    check(dispatch_state.ball.action == DD_BALL_PASS &&
          dispatch_state.ball.receiver >= 5u &&
          dispatch_state.ball.receiver < DD_GAMEPLAY_PLAYER_COUNT &&
          dispatch_state.ball.receiver != 5u,
          "$8EE2 clear-mode branch automatically inbounds a user make to a CPU teammate");
    inbound_receiver = dispatch_state.ball.receiver;
    for (player = 0u; player < 240u &&
         (dispatch_state.ball.action != DD_BALL_DRIBBLE ||
          dispatch_state.carrier != inbound_receiver); ++player) {
        check(dd_gameplay_step(&pack, &dispatch_state, 0u),
              "advance automatic made-basket inbound through $AD41 reception");
    }
    check(dispatch_state.ball.action == DD_BALL_DRIBBLE &&
          dispatch_state.carrier == inbound_receiver &&
          dispatch_state.ball.owner == inbound_receiver &&
          dispatch_state.inbound_variant == 0u &&
          dispatch_state.players[inbound_receiver].role == 0u &&
          dispatch_state.players[inbound_receiver].action == DD_PLAYER_LIVE_CARRIER &&
          dispatch_state.players[inbound_receiver].route_step == 4u,
          "$AD41->$AD6D completes the made-basket inbound and installs CPU role-zero possession");
    check(dispatch_state.players[dispatch_state.controlled_player].role == 0u &&
          dispatch_state.players[dispatch_state.controlled_player].action == DD_PLAYER_LIVE_USER,
          "post-score reception keeps user control on the defending role-zero player");
    links_reciprocal = 1;
    for (player = 0u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
        uint32_t linked = dispatch_state.players[player].paired_player;
        if (linked >= DD_GAMEPLAY_PLAYER_COUNT ||
            dispatch_state.players[linked].paired_player != player) {
            links_reciprocal = 0;
        }
        live_start_x[player] = dispatch_state.players[player].court_x;
        live_start_depth[player] = dispatch_state.players[player].court_depth;
    }
    check(links_reciprocal,
          "$8F6C/$AD6D role swaps leave every mutable $0580 opponent link reciprocal");
    for (player = 0u; player < 20u; ++player) {
        check(dd_gameplay_step(&pack, &dispatch_state, 0u),
              "advance resumed post-score possession before the carrier decision");
    }
    moved_count = 0u;
    for (player = 0u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
        if (dispatch_state.players[player].court_x != live_start_x[player] ||
            dispatch_state.players[player].court_depth != live_start_depth[player]) {
            ++moved_count;
        }
    }
    check(moved_count >= 2u && dispatch_state.ball.action == DD_BALL_DRIBBLE &&
          dispatch_state.carrier == inbound_receiver,
          "post-score reception resumes multiple off-ball routes while the CPU carrier keeps possession");

    dispatch_state = dispatch_base;
    dispatch_state.phase = DD_GAMEPLAY_LIVE;
    dispatch_state.possession_direction = 1u;
    dispatch_state.controlled_player = 0u;
    dispatch_state.carrier = 0u;
    dispatch_state.ball.action = DD_BALL_DRIBBLE;
    dispatch_state.ball.owner = 0u;
    dispatch_state.players[0].action = DD_PLAYER_LIVE_USER_CARRIER;
    dispatch_state.players[0].facing = 0u;
    dispatch_state.players[0].court_x = 0x00F600;
    dispatch_state.players[0].court_depth = 0x005A00;
    dispatch_state.players[0].height = 0x1000;
    for (player = 1u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
        dispatch_state.players[player].action = DD_PLAYER_ROUTE_WAIT;
    }
    check(dd_gameplay_step(&pack, &dispatch_state, DD_INPUT_B),
          "start untouched full-path missed-shot proof");
    for (player = 0u; player < 400u &&
         dispatch_state.ball.action != DD_BALL_SCORE &&
         dispatch_state.ball.action != DD_BALL_LOOSE_LAUNCH &&
         dispatch_state.ball.action != DD_BALL_REBOUND; ++player) {
        check(dd_gameplay_step(&pack, &dispatch_state, 0u),
              "advance untouched shot through the miss/rebound path");
    }
    check(dispatch_state.ball.action != DD_BALL_SCORE &&
          (dispatch_state.ball.action == DD_BALL_LOOSE_LAUNCH ||
           dispatch_state.ball.action == DD_BALL_REBOUND),
          "original miss coordinates avoid result one and enter a miss/rebound state");
    for (player = 0u; player < 80u &&
         dispatch_state.phase != DD_GAMEPLAY_INBOUND; ++player) {
        check(dd_gameplay_step(&pack, &dispatch_state, 0u),
              "advance missed user shot through $9635 reason-$16 inbound setup");
    }
    check(dispatch_state.phase == DD_GAMEPLAY_INBOUND &&
          dispatch_state.inbound_reason == 0x16u &&
          dispatch_state.players[5].action == DD_PLAYER_INBOUNDER &&
          dispatch_state.players[5].target_zone == 0x1Du &&
          dispatch_state.players[5].target_x == 0x01D800 &&
          dispatch_state.players[5].target_depth == 0x000800,
          "$AF46->$9635->$9651 assigns CPU role zero state $41 at wrapped target $001D");

    contest_state = dispatch_base;
    contest_state.phase = DD_GAMEPLAY_LIVE;
    contest_state.cpu_global_frame = 0u;
    contest_state.controlled_player = 0u;
    contest_state.carrier = 5u;
    contest_state.ball.action = DD_BALL_SHOT_GATHER;
    contest_state.ball.owner = 5u;
    contest_state.players[0].action = DD_PLAYER_LIVE_USER;
    check(contest_state.players[0].paired_player == 5u &&
          contest_state.players[5].paired_player == 0u,
          "$0582=$07/$0587=$02 converts to reciprocal native opening pair 0/5");
    contest_state.players[5].action = DD_PLAYER_LIVE_CARRIER_DECIDE;
    check(dd_gameplay_step(&pack, &contest_state, 0u),
          "reject a held-button user contest without A");
    check(contest_state.players[0].action == DD_PLAYER_LIVE_USER,
          "$A3E2 requires input bit 7 for ball gather state $04");
    check(dd_gameplay_step(&pack, &contest_state, DD_INPUT_A),
          "start the user defender contest through A");
    check(contest_state.players[0].action == DD_PLAYER_USER_CONTEST &&
          contest_state.players[0].height_script_index == 11u &&
          contest_state.players[0].height_script_reverse == 0u &&
          contest_state.players[0].release_timer == 1u,
          "$A3E2->$A607 installs user state $11 and the $9B26 jump stream");
    contest_state.carrier = 0xFFu;
    contest_state.ball.action = DD_BALL_REBOUND;
    contest_state.ball.owner = 0xFFu;
    contest_state.ball.velocity_x = 0;
    contest_state.ball.velocity_depth = 0;
    contest_state.ball.velocity_height = 0;
    contest_state.ball.court_x = contest_state.players[0].court_x + 0x0600;
    contest_state.ball.court_depth = contest_state.players[0].court_depth;
    contest_state.ball.height = 0x2E00;
    contest_state.players[5].action = DD_PLAYER_ROUTE_WAIT;
    for (player = 0u; player < 64u &&
         contest_state.ball.action != DD_BALL_AWARDED; ++player) {
        check(dd_gameplay_step(&pack, &contest_state, 0u),
              "advance the user contest to apex contact");
    }
    check(contest_state.players[0].action == DD_PLAYER_USER_CONTEST &&
          contest_state.ball.action == DD_BALL_AWARDED &&
          contest_state.ball.owner == 0u && contest_state.carrier == 0xFFu &&
          contest_state.audio_event == 0x20u &&
          contest_state.audio_event_serial != 0u,
          "$A638 apex->$A6C3 contact owns ball state $00 and queues SFX $20 without early transfer");
    for (player = 0u; player < 64u && contest_state.carrier != 0u; ++player) {
        check(dd_gameplay_step(&pack, &contest_state, 0u),
              "advance the owned user contest through landing");
    }
    check(contest_state.carrier == 0u && contest_state.ball.owner == 0u &&
          contest_state.ball.action == DD_BALL_DRIBBLE &&
          contest_state.controlled_player == 0u &&
          contest_state.players[0].action == DD_PLAYER_LIVE_USER_CARRIER,
          "$A693->$92BD->$A44B delays full possession transfer until contest landing");

    contest_state = dispatch_base;
    contest_state.phase = DD_GAMEPLAY_LIVE;
    contest_state.cpu_global_frame = 0u;
    contest_state.controlled_player = 0u;
    contest_state.carrier = 0xFFu;
    contest_state.ball.action = DD_BALL_REBOUND;
    contest_state.ball.owner = 0xFFu;
    contest_state.ball.velocity_height = 0;
    contest_state.ball.court_x = contest_state.players[0].court_x + 0x4000;
    contest_state.ball.court_depth = contest_state.players[0].court_depth;
    contest_state.ball.height = 0x2E00;
    contest_state.players[0].action = DD_PLAYER_LIVE_USER;
    contest_state.players[5].action = DD_PLAYER_LIVE_CARRIER_ROUTE;
    check(dd_gameplay_step(&pack, &contest_state, 0u),
          "start the input-free rebound contest");
    check(contest_state.players[0].action == DD_PLAYER_USER_CONTEST,
          "$A3E2 accepts rebound state $07 without an A-button edge");
    contest_state.players[5].action = DD_PLAYER_ROUTE_WAIT;
    for (player = 0u; player < 64u &&
         contest_state.players[0].action != DD_PLAYER_USER_CONTEST_RECOVER; ++player) {
        check(dd_gameplay_step(&pack, &contest_state, 0u),
              "advance missed user contest through landing");
    }
    check(contest_state.players[0].action == DD_PLAYER_USER_CONTEST_RECOVER &&
          contest_state.ball.owner == 0xFFu,
          "$A6AD exposes state $10 after a missed contest landing");
    for (player = 0u; player < 3u &&
         contest_state.players[0].action != DD_PLAYER_LIVE_USER; ++player) {
        check(dd_gameplay_step(&pack, &contest_state, 0u),
              "return missed contest recovery to live defense");
    }
    check(contest_state.players[0].action == DD_PLAYER_LIVE_USER,
          "$A5D0 returns normal live state $0F on the next user dispatch");

    contest_state = dispatch_base;
    contest_state.phase = DD_GAMEPLAY_LIVE;
    contest_state.controlled_player = 0u;
    contest_state.carrier = 0xFFu;
    contest_state.players[0].action = DD_PLAYER_LIVE_USER;
    contest_state.players[5].action = DD_PLAYER_ROUTE_WAIT;
    contest_state.ball.action = DD_BALL_REBOUND;
    contest_state.ball.owner = 0xFFu;
    contest_state.ball.receiver = 0xFFu;
    contest_state.ball.court_x = contest_state.players[0].court_x;
    contest_state.ball.court_depth = contest_state.players[0].court_depth;
    contest_state.ball.height = contest_state.players[0].height + 0x0800;
    check(dd_gameplay_step(&pack, &contest_state, 0u),
          "step user ground contact with an unpaired loose ball");
    check(contest_state.carrier == 0u && contest_state.ball.owner == 0u &&
          contest_state.ball.action == DD_BALL_DRIBBLE &&
          contest_state.players[0].action == DD_PLAYER_LIVE_USER_CARRIER,
          "$A3E2->$A42D->$B435->$A44B lets the user pick up a loose ball without a shooter pair");

    dispatch_state = dispatch_base;
    dispatch_state.phase = DD_GAMEPLAY_LIVE;
    dispatch_state.controlled_player = 0u;
    dispatch_state.players[0].action = DD_PLAYER_LIVE_USER;
    dispatch_state.players[6].action = DD_PLAYER_ROUTE_WAIT;
    dispatch_state.ball.action = DD_BALL_PASS;
    dispatch_state.ball.owner = 0xFFu;
    dispatch_state.ball.receiver = 6u;
    dispatch_state.ball.action_age = 18u;
    dispatch_state.ball.court_x = dispatch_state.players[6].court_x;
    dispatch_state.ball.court_depth = dispatch_state.players[6].court_depth;
    check(dd_gameplay_step(&pack, &dispatch_state, 0u),
          "finish a CPU-team pass reception");
    check(dispatch_state.ball.owner == 6u && dispatch_state.carrier == 6u &&
          dispatch_state.controlled_player == 0u &&
          dispatch_state.players[6].action == DD_PLAYER_LIVE_CARRIER,
          "$AD6D's CPU possession branch leaves user control on the defending team");

    dispatch_state = dispatch_base;
    dispatch_state.phase = DD_GAMEPLAY_LIVE;
    dispatch_state.controlled_player = 0u;
    dispatch_state.carrier = 5u;
    dispatch_state.ball.action = DD_BALL_DRIBBLE;
    dispatch_state.ball.owner = 5u;
    dispatch_state.ball.court_x = 0x010000;
    dispatch_state.ball.court_depth = 0x005800;
    dispatch_state.ball.height = 0;
    for (player = 0u; player < 5u; ++player) {
        dispatch_state.players[player].action = DD_PLAYER_LIVE_TEAMMATE;
        dispatch_state.players[player].court_depth = 0x005800;
        dispatch_state.players[player].height = 0;
    }
    dispatch_state.players[0].action = DD_PLAYER_LIVE_USER;
    dispatch_state.players[0].court_x = 0x004000;
    dispatch_state.players[1].court_x = 0x018000;
    dispatch_state.players[2].court_x = 0x012000;
    dispatch_state.players[3].court_x = 0x01C000;
    dispatch_state.players[4].court_x = 0x000000;
    check(dd_gameplay_step(&pack, &dispatch_state, DD_INPUT_B),
          "switch the controlled defender with B");
    check(dispatch_state.controlled_player == 2u &&
          dispatch_state.players[0].action == DD_PLAYER_LIVE_TEAMMATE &&
          dispatch_state.players[2].action == DD_PLAYER_LIVE_USER &&
          dispatch_state.players[2].role == 0u &&
          dispatch_state.players[2].paired_player == 5u &&
          dispatch_state.players[5].paired_player == 2u &&
          dispatch_state.players[0].paired_player == 8u &&
          dispatch_state.players[8].paired_player == 0u,
          "$A29D->$99D9/$9A31 switches control plus role and reciprocal opponent links");
    dispatch_state.ball.action = DD_BALL_SHOT_GATHER;
    dispatch_state.ball.owner = 5u;
    dispatch_state.players[5].action = DD_PLAYER_LIVE_CARRIER_DECIDE;
    check(dd_gameplay_step(&pack, &dispatch_state, DD_INPUT_A),
          "start a block attempt with the newly selected defender");
    check(dispatch_state.players[2].action == DD_PLAYER_USER_CONTEST &&
          dispatch_state.players[2].height_script_index == 11u,
          "$A607 follows the switched $0580 link into the user block height stream");

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
    dispatch_state.ball.vertical_phase = 0u;
    dispatch_state.ball.flight_curve = 0x10u;
    for (player = 0u; player < 60u; ++player) {
        check(dd_gameplay_step(&pack, &dispatch_state, 0u),
              "step traced loose-ball airborne arc");
    }
    check(dispatch_state.ball.action == DD_BALL_LOOSE_AIRBORNE,
          "ball state $09 remains airborne through its traced sixtieth frame");
    check(dd_gameplay_step(&pack, &dispatch_state, 0u), "finish loose-ball airborne state $09");
    check(dispatch_state.ball.action == DD_BALL_REBOUND &&
          dispatch_state.ball.velocity_height == 0x0290 &&
          dispatch_state.ball.rim_contact == 0u,
          "ball state $09 crosses $AFDD's $E0 threshold and $B412 reduces the rebound base");

    dispatch_state = dispatch_base;
    dispatch_state.ball.action = DD_BALL_SHOT_LAUNCH;
    dispatch_state.ball.owner = 5u;
    dispatch_state.carrier = 5u;
    check(dd_gameplay_step(&pack, &dispatch_state, 0u), "step shot initializer state $0A");
    check(dispatch_state.ball.action == DD_BALL_AIRBORNE &&
          dispatch_state.ball.velocity_height == 0x0305 &&
          dispatch_state.ball.flight_curve == 0x0Cu &&
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

    /* Rule ownership is driven by the offending/last-touch team, matching
       `$9651`'s side flip, rather than by the scrolling direction. */
    dispatch_state = dispatch_base;
    dispatch_state.phase = DD_GAMEPLAY_LIVE;
    dispatch_state.possession_direction = 1u;
    dispatch_state.ball.action = DD_BALL_HIDDEN;
    dispatch_state.ball.owner = 0xFFu;
    dispatch_state.ball.court_x = 0x010000;
    dispatch_state.ball.court_depth = 0x001000;
    dispatch_state.last_touch_player = 0u;
    check(dd_gameplay_step(&pack, &dispatch_state, 0u),
          "call out of bounds after a user-side last touch");
    check(dispatch_state.phase == DD_GAMEPLAY_INBOUND &&
          dispatch_state.inbound_reason == 0x16u &&
          dispatch_state.possession_direction == 0u &&
          dispatch_state.players[5].action == DD_PLAYER_INBOUNDER,
          "$9635->$9651 awards OOB to the opponent of the last-touch team");
    check(dispatch_state.rule_message_age == 0u,
          "$94A5 rule-message flash begins with the text-visible phase");
    for (player = 0u; player < 160u; ++player) {
        check(dd_gameplay_step(&pack, &dispatch_state, 0u),
              "advance the bounded `$94A5` rule-message window");
    }
    check(dispatch_state.rule_message_age == UINT16_MAX,
          "$94A5 clears the green-box message after forty four-frame gates");

    /* A made-basket receiver outside the recovered near-rim window must
       return to `$D759` instead of taking the old fixed-delay half-court shot. */
    dispatch_state = dispatch_base;
    dispatch_state.phase = DD_GAMEPLAY_LIVE;
    dispatch_state.clock_minutes = 1u;
    dispatch_state.clock_seconds = 0x30u;
    dispatch_state.carrier = 5u;
    dispatch_state.ball.owner = 5u;
    dispatch_state.ball.action = DD_BALL_DRIBBLE;
    dispatch_state.possession_direction = 0u;
    dispatch_state.players[5].action = DD_PLAYER_LIVE_CARRIER;
    dispatch_state.players[5].route_step = 4u;
    dispatch_state.players[5].action_age = 13u;
    dispatch_state.players[5].court_x = 0x014000;
    dispatch_state.players[5].court_depth = 0x005800;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.ball.action == DD_BALL_DRIBBLE &&
          dispatch_state.players[5].action != DD_PLAYER_LIVE_CARRIER_DECIDE,
          "post-inbound `$25` cannot launch a half-court shot after fourteen updates");

    /* `$32` consumes the high phase bit over a window.  Starting after $80
       proves the handler cannot freeze by missing one exact host frame. */
    prepare_cpu_policy(&pack, &dispatch_state, 0x84u, 0x85u);
    dispatch_state.players[5].route_step = 3u;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action != DD_PLAYER_LIVE_CPU_SETUP,
          "CPU setup exits after an arrived high-bit phase even when exact `$80` was skipped");

    /* The recovered close-rim gate enters a held-ball dunk and resolves both
       deterministic outcomes without routing through the ordinary parabola. */
    dispatch_state = dispatch_base;
    dispatch_state.phase = DD_GAMEPLAY_LIVE;
    dispatch_state.controlled_player = 0u;
    dispatch_state.carrier = 0u;
    dispatch_state.ball.owner = 0u;
    dispatch_state.ball.action = DD_BALL_DRIBBLE;
    dispatch_state.possession_direction = 1u;
    dispatch_state.players[0].action = DD_PLAYER_LIVE_USER_CARRIER;
    dispatch_state.players[0].court_x = 0x01B400;
    dispatch_state.players[0].court_depth = 0x005700;
    for (player = 1u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
        dispatch_state.players[player].action = DD_PLAYER_ROUTE_WAIT;
    }
    check(dd_gameplay_step(&pack, &dispatch_state, DD_INPUT_B),
          "start the recovered right-rim dunk gate");
    check(dispatch_state.dunk_active != 0u &&
          dispatch_state.ball.action == DD_BALL_SHOT_GATHER,
          "close-rim B press enters the native bank-2-derived dunk gather");
    dispatch_state.dunk_outcome = 1u;
    for (player = 0u; player < 20u && dispatch_state.dunk_active != 0u; ++player) {
        check(dd_gameplay_step(&pack, &dispatch_state, 0u),
              "advance dunk make animation to rim contact");
    }
    check(dispatch_state.ball.action == DD_BALL_SCORE &&
          dispatch_state.net_animation_phase == 2u &&
          dispatch_state.audio_event == 0x18u,
          "dunk make enters score/net/audio flow at the rim");

    dispatch_state = dispatch_base;
    dispatch_state.phase = DD_GAMEPLAY_LIVE;
    dispatch_state.controlled_player = 0u;
    dispatch_state.carrier = 0u;
    dispatch_state.ball.owner = 0u;
    dispatch_state.ball.action = DD_BALL_DRIBBLE;
    dispatch_state.possession_direction = 1u;
    dispatch_state.players[0].action = DD_PLAYER_LIVE_USER_CARRIER;
    dispatch_state.players[0].court_x = 0x01B400;
    dispatch_state.players[0].court_depth = 0x005700;
    for (player = 1u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
        dispatch_state.players[player].action = DD_PLAYER_ROUTE_WAIT;
    }
    check(dd_gameplay_step(&pack, &dispatch_state, DD_INPUT_B),
          "start close-rim dunk miss proof");
    dispatch_state.dunk_outcome = 4u;
    for (player = 0u; player < 20u && dispatch_state.dunk_active != 0u; ++player) {
        check(dd_gameplay_step(&pack, &dispatch_state, 0u),
              "advance dunk miss animation to rim contact");
    }
    check(dispatch_state.ball.action == DD_BALL_LOOSE_LAUNCH &&
          dispatch_state.ball.outcome == 4u,
          "dunk miss enters `$AF72` loose-ball launch rather than scoring");
    check(dd_gameplay_step(&pack, &dispatch_state, 0u),
          "advance `$AF72` dunk miss initializer");
    check(dispatch_state.audio_event == 0x14u,
          "dunk miss requests the original loose-ball SFX `$14`");

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

    period_state = dispatch_base;
    period_state.period = 4u;
    period_state.clock_minutes = 0u;
    period_state.clock_seconds = 0u;
    period_state.clock_expired = 1;
    period_state.clock_expired_frame = period_state.scene_frame;
    period_state.ball.action = DD_BALL_PASS;
    check(dd_gameplay_step(&pack, &period_state, 0u),
          "step fourth-period expiration while ball remains in flight");
    check(period_state.phase != DD_GAMEPLAY_GAME_SET,
          "$93E1 waits for held/rebound ball state before GAME SET");
    period_state.ball.action = DD_BALL_REBOUND;
    period_state.clock_expired_frame = period_state.scene_frame;
    check(dd_gameplay_step(&pack, &period_state, 0u),
          "step fourth-period frame after 00:00");
    check(period_state.phase == DD_GAMEPLAY_GAME_SET &&
          period_state.game_set_age == 0u && !period_state.return_to_title,
          "fourth-period expiration enters GAME SET instead of a fifth tip formation");
    for (player = 0u; player < 258u; ++player) {
        check(dd_gameplay_step(&pack, &period_state, 0u),
              "advance GAME SET hold to original blue frame");
    }
    check(period_state.game_set_age == 258u && !period_state.return_to_title,
          "GAME SET holds 258 frames before the original blue transition");
    for (player = 258u; player < 282u; ++player) {
        check(dd_gameplay_step(&pack, &period_state, 0u),
              "advance blue transition to title return");
    }
    check(period_state.game_set_age == 282u && period_state.return_to_title,
          "GAME SET returns to title 282 frames after its first visible frame");

    dd_asset_pack_unload(&pack);
    if (failures != 0) {
        fprintf(stderr, "%d CPU gameplay regression check(s) failed.\n", failures);
        return 1;
    }
    puts("Gameplay regression checks passed: tip/user contests, dispatcher states, camera CHR, moving off-ball players, pass/shot angles, rebound, inbound reception, and live audio data.");
    return 0;
}
