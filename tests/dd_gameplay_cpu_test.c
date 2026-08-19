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

static int prepare_level_state(const DDAssetPack *pack, DDGameplayState *state,
                               uint8_t level) {
    memset(state, 0, sizeof(*state));
    return dd_gameplay_configure(pack, state, 0u, 0u, level) &&
        dd_gameplay_advance_to(pack, state, 356u, 0u);
}

static void prepare_contact_fixture(const DDAssetPack *pack,
                                    DDGameplayState *state, uint8_t level,
                                    uint32_t defender, uint32_t owner) {
    uint32_t player;
    check(prepare_level_state(pack, state, level),
          "prepare configured LEVEL contact fixture");
    state->phase = DD_GAMEPLAY_LIVE;
    state->controlled_player = owner < 5u ? (uint8_t)owner : 0u;
    state->carrier = (uint8_t)owner;
    state->last_touch_player = (uint8_t)owner;
    state->possession_direction = defender >= 5u ? 1u : 0u;
    state->contact_lock_timer = 0u;
    state->score_contact_gate = 0u;
    state->possession_foul_timer = 0xFFu;
    state->ball.action = DD_BALL_DRIBBLE;
    state->ball.owner = (uint8_t)owner;
    state->ball.receiver = 0xFFu;
    for (player = 0u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
        state->players[player].action = DD_PLAYER_ROUTE_WAIT;
        state->players[player].contact_age = 0u;
        state->players[player].velocity_x = 0;
        state->players[player].velocity_depth = 0;
    }
    state->players[defender].action = DD_PLAYER_LIVE_SET;
    state->players[defender].role = defender < 5u ? 1u : 0u;
    state->players[defender].paired_player = (uint8_t)owner;
    state->players[owner].paired_player = (uint8_t)defender;
    state->players[owner].court_x = state->players[defender].court_x;
    state->players[owner].court_depth = state->players[defender].court_depth;
    state->players[owner].height = state->players[defender].height;
    state->ball.court_x = state->players[defender].court_x;
    state->ball.court_depth = state->players[defender].court_depth;
    state->ball.height = state->players[defender].height + 0x0800;
}

static void check_level_gameplay(const DDAssetPack *pack) {
    static const uint8_t contact_limit[3] = {0x14u, 0x0Cu, 0x06u};
    static const uint8_t tracking_limit[3] = {0x40u, 0x28u, 0x1Au};
    static const uint8_t gameplay_level[3] = {0u, 4u, 8u};
    DDGameplayState state;
    DDGameplayState copy;
    uint32_t level;
    uint32_t side;

    for (level = 0u; level < 3u; ++level) {
        check(prepare_level_state(pack, &state, (uint8_t)level) &&
              state.match_level_index == level &&
              state.gameplay_level == gameplay_level[level] &&
              state.possession_contact_limit == contact_limit[level] &&
              state.paired_tracking_limit == tracking_limit[level],
              "$A593 installs immutable selection, mutable `$07E8`, `$0068`, and `$006C`");
        for (side = 0u; side < 2u; ++side) {
            uint32_t defender = side == 0u ? 5u : 1u;
            uint32_t owner = side == 0u ? 0u : 5u;
            prepare_contact_fixture(pack, &state, (uint8_t)level, defender, owner);
            state.players[defender].contact_age =
                (uint8_t)(contact_limit[level] - 2u);
            run_cpu_dispatch(pack, &state, defender);
            check(state.ball.owner == owner &&
                  state.players[defender].contact_age == contact_limit[level] - 1u,
                  "configured contact remains below `$0068` one scheduled update early");
            run_cpu_dispatch(pack, &state, defender);
            check(state.ball.owner == defender &&
                  state.players[defender].contact_age == 0u,
                  "configured contact resolves on the exact `$0068` scheduled update");
        }

        /* `$8A57` consumes the second LEVEL table. Keep state `$22` in
           contact with offensive role zero and prove the exact `$006C`
           transition, including the ninth packed-coordinate bit. */
        check(prepare_level_state(pack, &state, (uint8_t)level),
              "prepare configured LEVEL paired-tracking fixture");
        state.phase = DD_GAMEPLAY_INBOUND;
        state.possession_direction = 1u;
        state.score_contact_gate = 1u;
        state.carrier = 0xFFu;
        state.ball.action = DD_BALL_DEAD;
        state.ball.owner = 0xFFu;
        state.players[0].role = 0u;
        state.players[0].action = DD_PLAYER_ROUTE_WAIT;
        state.players[0].court_x = 0x00A800;
        state.players[0].court_depth = 0x008800;
        state.players[5].court_x = state.players[0].court_x;
        state.players[5].court_depth = state.players[0].court_depth;
        state.players[5].velocity_x = 0;
        state.players[5].velocity_depth = 0;
        state.players[5].action = DD_PLAYER_LIVE_PAIRED_DEFENDER;
        state.players[5].paired_player = 0u;
        state.players[5].tracked_zone = 0x0Au;
        state.players[5].tracking_age = (uint8_t)(tracking_limit[level] - 2u);
        run_cpu_dispatch(pack, &state, 5u);
        check(state.players[5].action == DD_PLAYER_LIVE_PAIRED_DEFENDER &&
              state.players[5].tracking_age == tracking_limit[level] - 1u,
              "paired tracking remains in `$22` one scheduled update below `$006C`");
        run_cpu_dispatch(pack, &state, 5u);
        check(state.players[5].action == DD_PLAYER_LIVE_FOLLOW_TARGET &&
              state.players[5].target_zone == 0x0Au &&
              state.players[5].target_depth >= 0x8000,
              "`$8A57-$8A97` copies both packed target bytes at exact `$006C`");
    }

    prepare_contact_fixture(pack, &state, 0u, 5u, 0u);
    state.players[5].contact_age = 7u;
    state.contact_lock_timer = 0xFFu;
    run_cpu_dispatch(pack, &state, 5u);
    check(state.players[5].contact_age == 7u && state.ball.owner == 0u,
          "`$91A6` preserves `$06A0` while `$001D` is nonzero");
    state.contact_lock_timer = 0u;
    state.score_contact_gate = 1u;
    run_cpu_dispatch(pack, &state, 5u);
    check(state.players[5].contact_age == 7u && state.ball.owner == 0u,
          "`$91A6` preserves `$06A0` while `$0056` is nonzero");

    prepare_contact_fixture(pack, &state, 0u, 1u, 5u);
    state.players[1].contact_age = 7u;
    state.contact_lock_timer = 0xFFu;
    run_cpu_dispatch(pack, &state, 1u);
    check(state.players[1].contact_age == 7u && state.ball.owner == 5u,
          "`$9FA3` preserves `$06A0` while `$001D` is nonzero");
    state.contact_lock_timer = 0u;
    state.score_contact_gate = 1u;
    run_cpu_dispatch(pack, &state, 1u);
    check(state.players[1].contact_age == 7u && state.ball.owner == 5u,
          "`$9FA3` preserves `$06A0` while `$0056` is nonzero");

    prepare_contact_fixture(pack, &state, 0u, 1u, 5u);
    state.players[1].role = 0u;
    state.players[1].contact_age = 7u;
    run_cpu_dispatch(pack, &state, 1u);
    check(state.players[1].contact_age == 7u,
          "`$9FA3` role-zero early return preserves its contact counter");
    state.players[1].role = 1u;
    state.players[1].action = DD_PLAYER_LIVE_USER_CARRIER;
    run_cpu_dispatch(pack, &state, 1u);
    check(state.players[1].contact_age == 0u,
          "`$9FA3` current-player state `$02` clears its contact counter");

    for (side = 0u; side < 2u; ++side) {
        uint32_t defender = side == 0u ? 5u : 1u;
        uint32_t owner = side == 0u ? 0u : 5u;
        prepare_contact_fixture(pack, &state, 0u, defender, owner);
        state.possession_direction ^= 1u;
        state.players[defender].contact_age = 7u;
        run_cpu_dispatch(pack, &state, defender);
        check(state.players[defender].contact_age == 0u,
              "wrong `$0050` direction clears `$06A0` in both contact mirrors");

        prepare_contact_fixture(pack, &state, 0u, defender, owner);
        state.players[defender].paired_player =
            (uint8_t)(owner == 0u ? 1u : 6u);
        state.players[defender].contact_age = 7u;
        run_cpu_dispatch(pack, &state, defender);
        check(state.players[defender].contact_age == 0u,
              "low-LEVEL pair failure clears `$06A0` in both contact mirrors");

        prepare_contact_fixture(pack, &state, 0u, defender, owner);
        state.ball.court_x = state.players[defender].court_x + 0x0A00;
        state.players[owner].court_x = state.ball.court_x;
        state.players[defender].contact_age = 7u;
        run_cpu_dispatch(pack, &state, defender);
        check(state.players[defender].contact_age == 0u,
              "`$B435` miss clears `$06A0` in both contact mirrors");
    }

    /* LEVEL pair matrix: 5 always requires the pair, 6/7 waive it only for
       negative `$001A`, and 8 waives it in both phase halves. */
    for (level = 5u; level <= 8u; ++level) {
        uint32_t negative;
        for (negative = 0u; negative < 2u; ++negative) {
            int bypass = level >= 8u ||
                ((level == 6u || level == 7u) && negative != 0u);
            prepare_contact_fixture(pack, &state, 0u, 5u, 0u);
            state.gameplay_level = (uint8_t)level;
            state.cpu_global_frame = negative != 0u ? 0x90u : 0x10u;
            state.players[5].paired_player = 1u;
            state.players[5].contact_age =
                (uint8_t)(state.possession_contact_limit - 1u);
            run_cpu_dispatch(pack, &state, 5u);
            check((state.ball.owner == 5u) == bypass,
                  "`$91A6` matches LEVEL 5/6/7/8 signed-phase pair matrix");
        }
    }

    prepare_contact_fixture(pack, &state, 1u, 5u, 0u);
    state.players[5].paired_player = 1u;
    state.players[5].contact_age =
        (uint8_t)(state.possession_contact_limit - 1u);
    run_cpu_dispatch(pack, &state, 5u);
    check(state.gameplay_level == 4u && state.ball.owner == 0u,
          "middle menu LEVEL enters gameplay as 4 and still requires its pair");
    prepare_contact_fixture(pack, &state, 2u, 5u, 0u);
    state.players[5].paired_player = 1u;
    state.players[5].contact_age =
        (uint8_t)(state.possession_contact_limit - 1u);
    run_cpu_dispatch(pack, &state, 5u);
    check(state.gameplay_level == 8u && state.ball.owner == 5u,
          "highest menu LEVEL enters gameplay as 8 and waives its pair");

    prepare_contact_fixture(pack, &state, 0u, 5u, 0u);
    state.gameplay_level = 6u;
    state.cpu_global_frame = 0x90u;
    state.players[5].paired_player = 1u;
    state.ball.action = DD_BALL_PASS;
    state.ball.owner = 0xFFu;
    state.ball.receiver = 0xFFu;
    state.players[5].contact_age = 0u;
    run_cpu_dispatch(pack, &state, 5u);
    check(state.ball.owner == 5u,
          "LEVEL 6 pass state `$02` resolves immediately after `$B435`");
    prepare_contact_fixture(pack, &state, 0u, 1u, 5u);
    state.gameplay_level = 6u;
    state.cpu_global_frame = 0x90u;
    state.players[1].paired_player = 6u;
    state.ball.action = DD_BALL_PASS;
    state.ball.owner = 0xFFu;
    state.ball.receiver = 0xFFu;
    state.players[1].contact_age = 0u;
    run_cpu_dispatch(pack, &state, 1u);
    check(state.ball.owner == 1u,
          "mirrored LEVEL 6 pass state `$02` resolves immediately after `$B435`");

    prepare_contact_fixture(pack, &state, 0u, 5u, 0u);
    state.players[5].contact_age = 0xFFu;
    run_cpu_dispatch(pack, &state, 5u);
    check(state.ball.owner == 0u && state.players[5].contact_age == 0u,
          "`INC $06A0` retains byte-wrap behavior instead of saturating");

    check(prepare_level_state(pack, &state, 0u),
          "prepare `$8A57` reset/wrong-pair fixtures");
    state.phase = DD_GAMEPLAY_INBOUND;
    state.possession_direction = 1u;
    state.score_contact_gate = 1u;
    state.carrier = 0xFFu;
    state.ball.action = DD_BALL_DEAD;
    state.ball.owner = 0xFFu;
    state.players[0].role = 0u;
    state.players[0].action = DD_PLAYER_ROUTE_WAIT;
    set_packed_position(&state.players[0], 0x8Au);
    state.players[5].court_x = state.players[0].court_x;
    state.players[5].court_depth = state.players[0].court_depth;
    state.players[5].velocity_x = 0;
    state.players[5].velocity_depth = 0;
    state.players[5].action = DD_PLAYER_LIVE_PAIRED_DEFENDER;
    state.players[5].paired_player = 0u;
    state.players[5].tracked_zone = 0x89u;
    state.players[5].tracking_age = 55u;
    run_cpu_dispatch(pack, &state, 5u);
    check(state.players[5].tracking_age == 0u &&
          state.players[5].tracked_zone == 0x8Au,
          "changed `$05B0` resets `$0600` through `$FF->$00`");
    copy = state;
    copy.players[5].paired_player = 1u;
    copy.players[1].court_x = copy.players[5].court_x;
    copy.players[1].court_depth = copy.players[5].court_depth;
    copy.players[5].tracking_age = 7u;
    run_cpu_dispatch(pack, &copy, 5u);
    check(copy.players[5].tracking_age == 7u,
          "`$8A57` does not advance `$0600` for the wrong paired object");
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
    const uint32_t width = pack->tipoff_meta.width;
    const uint32_t height = pack->tipoff_meta.height;
    const size_t pixel_count = (size_t)pack->tipoff_meta.width * pack->tipoff_meta.height;
    uint32_t *new_york = (uint32_t *)malloc(pixel_count * sizeof(*new_york));
    uint32_t *los_angeles = (uint32_t *)malloc(pixel_count * sizeof(*los_angeles));
    DDGameplayState first;
    DDGameplayState second;
    uint32_t user_changed = 0u;
    uint32_t cpu_changed = 0u;
    uint32_t x;
    uint32_t y;
    uint32_t player;
    int ok = new_york != NULL && los_angeles != NULL;
    memset(&first, 0, sizeof(first));
    memset(&second, 0, sizeof(second));
    if (ok) ok = dd_gameplay_configure(pack, &first, 0u, 0u, 0u) &&
        dd_gameplay_configure(pack, &second, 2u, 3u, 2u);
    if (ok) {
        first.phase = second.phase = DD_GAMEPLAY_LIVE;
        first.scene_frame = second.scene_frame = 356u;
        first.hud_split_y = second.hud_split_y = 64u;
        first.camera_x = second.camera_x = 0;
        first.ball.animation = second.ball.animation = 0xFFu;
        for (player = 0u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
            first.players[player].animation = 0xFFu;
            second.players[player].animation = 0xFFu;
        }
        /* Isolate one 1P object using palettes 0/1 and one CPU object using
           palettes 2/3.  This catches the former whole-frame false positive,
           where a team choice changed only the opponent's pixels. */
        first.players[0].animation = second.players[0].animation = 0x1Cu;
        first.players[0].attributes = second.players[0].attributes = 0u;
        first.players[0].court_x = second.players[0].court_x = 0x004000;
        first.players[0].court_depth = second.players[0].court_depth = 0x004000;
        first.players[0].height = second.players[0].height = 0x1000;
        first.players[5].animation = second.players[5].animation = 0xFFu;
        first.players[5].attributes = second.players[5].attributes = 2u;
        first.players[5].court_x = second.players[5].court_x = 0x00C000;
        first.players[5].court_depth = second.players[5].court_depth = 0x004000;
        first.players[5].height = second.players[5].height = 0x1000;
        ok = dd_render_gameplay(pack, &first, new_york,
                                width, height) &&
            dd_render_gameplay(pack, &second, los_angeles,
                               width, height);
        for (y = 96u; ok && y < 220u && y < height; ++y) {
            for (x = 24u; x < 112u && x < width; ++x) {
                if (new_york[(size_t)y * width + x] !=
                    los_angeles[(size_t)y * width + x]) ++user_changed;
            }
        }
        first.players[0].animation = second.players[0].animation = 0xFFu;
        first.players[5].animation = second.players[5].animation = 0x21u;
        if (ok) ok = dd_render_gameplay(pack, &first, new_york, width, height) &&
            dd_render_gameplay(pack, &second, los_angeles, width, height);
        for (y = 96u; ok && y < 220u && y < height; ++y) {
            for (x = 152u; x < 240u && x < width; ++x) {
                if (new_york[(size_t)y * width + x] !=
                    los_angeles[(size_t)y * width + x]) ++cpu_changed;
            }
        }
    }
    check(ok && second.match_time_index == 2u && second.match_time_bcd == 0x20u &&
          second.clock_minutes == 0x20u && second.match_team_index == 3u &&
          second.match_level_index == 2u,
          "configuration time, team, and level persist in native match state");
    check(ok && user_changed != 0u,
          "selected `$0482` team color changes the isolated 1P gameplay player");
    check(ok && cpu_changed == 0u,
          "fixed `$0483` CPU jersey remains unchanged across 1P team selections");
    free(los_angeles);
    free(new_york);
}

static void check_long_run_native_match(const DDAssetPack *pack) {
    DDGameplayState state;
    uint32_t frame;
    uint32_t player;
    uint32_t cpu_stall = 0u;
    uint32_t passes = 0u;
    uint32_t shots = 0u;
    uint32_t inbounds = 0u;
    int32_t previous_x = INT32_MIN;
    int32_t previous_depth = INT32_MIN;
    uint8_t previous_action = 0xFFu;
    uint8_t previous_phase = 0xFFu;
    uint8_t previous_ball_action = 0xFFu;
    int valid = 1;
    memset(&state, 0, sizeof(state));
    valid = dd_gameplay_advance_to(pack, &state, 356u, 0u);
    /* One-minute periods keep this full four-period smoke test bounded while
       exercising the same BCD clock, reset, tip, live, and GAME SET paths. */
    state.match_time_bcd = 0x01u;
    state.clock_minutes = 0x01u;
    state.clock_seconds = 0u;
    state.clock_expired = 0;
    state.next_clock_frame = state.scene_frame + 1u;
    for (frame = 0u; valid && frame < 25000u && !state.return_to_title; ++frame) {
        valid = dd_gameplay_step(pack, &state, 0u);
        if (!valid) break;
        if (state.phase == DD_GAMEPLAY_INBOUND && previous_phase != DD_GAMEPLAY_INBOUND) {
            ++inbounds;
        }
        if (state.ball.action == DD_BALL_PASS && previous_ball_action != DD_BALL_PASS) {
            ++passes;
        }
        if ((state.ball.action == DD_BALL_SHOT_GATHER ||
             state.ball.action == DD_BALL_AIRBORNE) &&
            previous_ball_action != DD_BALL_SHOT_GATHER &&
            previous_ball_action != DD_BALL_AIRBORNE) {
            ++shots;
        }
        if (state.phase == DD_GAMEPLAY_LIVE) {
            if (state.ball.action == DD_BALL_DRIBBLE &&
                state.ball.owner >= DD_GAMEPLAY_PLAYER_COUNT) {
                valid = 0;
                break;
            }
            for (player = 0u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
                if (state.players[player].court_x < 0x001000 ||
                    state.players[player].court_x > 0x01F1FF ||
                    state.players[player].court_depth < 0x000500 ||
                    state.players[player].court_depth > 0x0098FF) {
                    valid = 0;
                    break;
                }
            }
            if (state.ball.action == DD_BALL_DRIBBLE && state.ball.owner >= 5u &&
                state.ball.owner < DD_GAMEPLAY_PLAYER_COUNT) {
                const DDPlayerState *carrier = &state.players[state.ball.owner];
                if (carrier->court_x == previous_x &&
                    carrier->court_depth == previous_depth &&
                    carrier->action == previous_action) {
                    ++cpu_stall;
                } else {
                    cpu_stall = 0u;
                }
                previous_x = carrier->court_x;
                previous_depth = carrier->court_depth;
                previous_action = carrier->action;
                if (cpu_stall > 640u) valid = 0;
            } else {
                cpu_stall = 0u;
                previous_x = INT32_MIN;
                previous_depth = INT32_MIN;
                previous_action = 0xFFu;
            }
        }
        previous_phase = state.phase;
        previous_ball_action = state.ball.action;
    }
    check(valid, "long-run native match preserves bounds, ownership, and CPU progress");
    check(state.return_to_title && state.period == 4u &&
          state.phase == DD_GAMEPLAY_GAME_SET &&
          state.gameplay_level == 1u &&
          state.possession_contact_limit == 0x10u &&
          state.paired_tracking_limit == 0x31u,
          "four one-minute periods reach GAME SET and return-to-title without a stuck state");
    check(passes != 0u && shots != 0u && inbounds != 0u,
          "long-run CPU match exercises pass, shot, and inbound decisions");
}

static void check_user_offense_cpu_defense(const DDAssetPack *pack) {
    DDGameplayState state;
    int32_t previous_x[DD_GAMEPLAY_PLAYER_COUNT];
    int32_t previous_depth[DD_GAMEPLAY_PLAYER_COUNT];
    uint32_t moved[DD_GAMEPLAY_PLAYER_COUNT] = {0};
    uint32_t frame;
    uint32_t player;
    memset(&state, 0, sizeof(state));
    check(dd_gameplay_advance_to(pack, &state, 743u, 0u),
          "reach the rebound-return user-control regression frame");
    check(state.carrier == state.controlled_player &&
          state.ball.action == DD_BALL_DRIBBLE &&
          state.players[state.controlled_player].action != DD_PLAYER_LIVE_USER_CARRIER,
          "rebound return can own the dribbling ball before live control is restored");
    check(dd_gameplay_step(pack, &state, DD_INPUT_A | DD_INPUT_B),
          "press pass and shoot during the rebound-return transition");
    check(state.ball.action == DD_BALL_DRIBBLE &&
          state.players[state.controlled_player].action != DD_PLAYER_USER_SHOOT,
          "non-live rebound ownership rejects premature A/B instead of freezing formation state `$37`");

    memset(&state, 0, sizeof(state));
    if (!dd_gameplay_advance_to(pack, &state, 803u, 0u)) return;
    dd_gameplay_step(pack, &state, DD_INPUT_A | DD_INPUT_RIGHT);
    for (frame = 0u; frame < 180u &&
         (state.ball.action != DD_BALL_DRIBBLE ||
          state.carrier != state.controlled_player); ++frame) {
        dd_gameplay_step(pack, &state, 0u);
    }
    check(frame < 180u && state.ball.action == DD_BALL_DRIBBLE,
          "user inbound pass reaches the selected live carrier");
    for (player = 0u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
        previous_x[player] = state.players[player].court_x;
        previous_depth[player] = state.players[player].court_depth;
    }
    for (frame = 0u; frame < 128u && state.phase == DD_GAMEPLAY_LIVE; ++frame) {
        uint32_t input = (frame & 0x40u) == 0u ? DD_INPUT_RIGHT : DD_INPUT_LEFT;
        if ((frame & 0x80u) != 0u) input |= DD_INPUT_DOWN;
        if (!dd_gameplay_step(pack, &state, input)) break;
        for (player = 0u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
            if (state.players[player].court_x != previous_x[player] ||
                state.players[player].court_depth != previous_depth[player]) {
                ++moved[player];
            }
            previous_x[player] = state.players[player].court_x;
            previous_depth[player] = state.players[player].court_depth;
        }
    }
    check(moved[5] != 0u && moved[6] != 0u && moved[7] != 0u &&
          moved[8] != 0u && moved[9] != 0u,
          "all five CPU defenders keep responding while the user offense owns the ball");
}

static void check_cpu_free_throw_level_policy(const DDAssetPack *pack) {
    DDGameplayState state;
    uint32_t i;

    /* 1. Verify Menu configuration mapping to gameplay LEVEL ($A631) */
    memset(&state, 0, sizeof(state));
    check(dd_gameplay_configure(pack, &state, 0u, 0u, 0u), "configure level 0");
    check(state.gameplay_level == 0u, "menu level 0 maps to gameplay level 0");
    memset(&state, 0, sizeof(state));
    check(dd_gameplay_configure(pack, &state, 0u, 0u, 1u), "configure level 1");
    check(state.gameplay_level == 4u, "menu level 1 maps to gameplay level 4");
    memset(&state, 0, sizeof(state));
    check(dd_gameplay_configure(pack, &state, 0u, 0u, 2u), "configure level 2");
    check(state.gameplay_level == 8u, "menu level 2 maps to gameplay level 8");

    /* Base setup for CPU free throw shooter at line ($46) */
    memset(&state, 0, sizeof(state));
    check(dd_gameplay_advance_to(pack, &state, 356u, 0u), "advance to live");
    state.phase = DD_GAMEPLAY_FREE_THROW;
    state.foul_shooter = 5u;
    state.foul_offender = 0u;
    state.free_throw_initialized = 1u;
    state.free_throw_attempts = 0u;
    state.carrier = 5u;
    state.ball.owner = 5u;
    state.ball.action = DD_BALL_AWARDED;
    state.ball.court_x = 0x006E00;
    state.ball.court_depth = 0x005800;
    state.possession_direction = 0u;
    for (i = 0u; i < DD_GAMEPLAY_PLAYER_COUNT; ++i) {
        state.players[i].action = (i == 5u) ? DD_PLAYER_FREE_THROW_SET : DD_PLAYER_FREE_THROW_SPOT;
        state.players[i].court_x = 0x006E00;
        state.players[i].court_depth = 0x005800;
        state.players[i].facing = 4u;
    }

    /* 2. Test $0067 delay timer decrement and transition */
    {
        DDGameplayState test_state = state;
        test_state.gameplay_level = 8u;
        test_state.cpu_global_frame = 0x11u; /* next step makes it 0x12 (even/scheduled, positive signed) */
        test_state.free_throw_timer = 2u;
        test_state.free_throw_aim = 0x54u;
        test_state.score_contact_gate = 0xFFu;

        /* Step 1: timer decrements 2 -> 1, BNE $884C holds */
        check(dd_gameplay_step(pack, &test_state, 0u), "step free throw timer 2->1");
        check(test_state.free_throw_timer == 1u, "$8836 decrements free_throw_timer from 2 to 1");
        check(test_state.players[5].action == DD_PLAYER_FREE_THROW_SET,
              "$8838 BNE holds shooter in $46 when timer remains nonzero");
        check(test_state.ball.action == DD_BALL_AWARDED, "ball remains awarded while timer > 0");

        /* Step 2: timer decrements 1 -> 0, falls through to $883A. Since level < 9 and phase positive, shoots! */
        test_state.cpu_global_frame = 0x11u; /* keep scheduled */
        check(dd_gameplay_step(pack, &test_state, 0u), "step free throw timer 1->0");
        check(test_state.free_throw_timer == 0u, "$8836 decrements free_throw_timer from 1 to 0");
        check(test_state.players[5].action == DD_PLAYER_FREE_THROW_GATHER,
              "$8838 BNE falls through when timer reaches 0, entering gather $47");
        check(test_state.ball.action == DD_BALL_SHOT_GATHER,
              "ball transitions to $04 gather on shot trigger");
        check(test_state.score_contact_gate == 0u,
              "$8882 explicitly clears score_contact_gate $0056 on shot release");
        check(test_state.shot_value == 1u, "free-throw shot value is 1 point");
    }

    /* 3. Decision Matrix: Level < 9, positive phase (0x10), aim != 0x60 (0x54) -> SHOOTS */
    {
        DDGameplayState test_state = state;
        test_state.gameplay_level = 8u;
        test_state.cpu_global_frame = 0x0Fu; /* next step makes it 0x10 (scheduled, positive signed) */
        test_state.free_throw_timer = 0u;
        test_state.free_throw_aim = 0x54u;
        check(dd_gameplay_step(pack, &test_state, 0u), "step level 8 positive phase");
        check(test_state.players[5].action == DD_PLAYER_FREE_THROW_GATHER &&
              test_state.ball.action == DD_BALL_SHOT_GATHER,
              "LEVEL < 9 with positive signed $001A takes $8843 BPL and shoots with aim != $60");
    }

    /* 4. Decision Matrix: Level < 9, negative phase (0x90), aim != 0x60 (0x54) -> HOLDS */
    {
        DDGameplayState test_state = state;
        test_state.gameplay_level = 8u;
        test_state.cpu_global_frame = 0x8Fu; /* next step makes it 0x90 (scheduled, negative signed) */
        test_state.free_throw_timer = 0u;
        test_state.free_throw_aim = 0x54u;
        check(dd_gameplay_step(pack, &test_state, 0u), "step level 8 negative phase hold");
        check(test_state.players[5].action == DD_PLAYER_FREE_THROW_SET &&
              test_state.ball.action == DD_BALL_AWARDED,
              "LEVEL < 9 with negative signed $001A and aim != $60 holds in $46 via $884C");
    }

    /* 5. Decision Matrix: Level < 9, negative phase (0x90), aim == 0x60 -> SHOOTS */
    {
        DDGameplayState test_state = state;
        test_state.gameplay_level = 8u;
        test_state.cpu_global_frame = 0x8Fu; /* next step makes it 0x90 (scheduled, negative signed) */
        test_state.free_throw_timer = 0u;
        test_state.free_throw_aim = 0x62u; /* decrements by 2 to 0x60 */
        test_state.free_throw_aim_direction = 0;
        check(dd_gameplay_step(pack, &test_state, 0u), "step level 8 negative phase with aim 0x60");
        check(test_state.free_throw_aim == 0x60u, "aim decrements to exact 0x60");
        check(test_state.players[5].action == DD_PLAYER_FREE_THROW_GATHER &&
              test_state.ball.action == DD_BALL_SHOT_GATHER,
              "LEVEL < 9 with negative signed $001A and aim == $60 takes $884A BEQ and shoots");
    }

    /* 6. Decision Matrix: Level >= 9, positive phase (0x10), aim != 0x60 (0x54) -> HOLDS */
    {
        DDGameplayState test_state = state;
        test_state.gameplay_level = 9u;
        test_state.cpu_global_frame = 0x0Fu; /* next step makes it 0x10 (scheduled, positive signed) */
        test_state.free_throw_timer = 0u;
        test_state.free_throw_aim = 0x54u;
        check(dd_gameplay_step(pack, &test_state, 0u), "step level 9 positive phase hold");
        check(test_state.players[5].action == DD_PLAYER_FREE_THROW_SET &&
              test_state.ball.action == DD_BALL_AWARDED,
              "LEVEL >= 9 bypasses $8841 BPL via $883F BCS and holds when aim != $60");
    }

    /* 7. Decision Matrix: Level >= 9, negative phase (0x90), aim != 0x60 (0x54) -> HOLDS */
    {
        DDGameplayState test_state = state;
        test_state.gameplay_level = 9u;
        test_state.cpu_global_frame = 0x8Fu; /* next step makes it 0x90 (scheduled, negative signed) */
        test_state.free_throw_timer = 0u;
        test_state.free_throw_aim = 0x54u;
        check(dd_gameplay_step(pack, &test_state, 0u), "step level 9 negative phase hold");
        check(test_state.players[5].action == DD_PLAYER_FREE_THROW_SET &&
              test_state.ball.action == DD_BALL_AWARDED,
              "LEVEL >= 9 with negative phase and aim != $60 holds in $46");
    }

    /* 8. Decision Matrix: Level >= 9, aim == 0x60 -> SHOOTS */
    {
        DDGameplayState test_state = state;
        test_state.gameplay_level = 9u;
        test_state.cpu_global_frame = 0x8Fu; /* negative phase */
        test_state.free_throw_timer = 0u;
        test_state.free_throw_aim = 0x62u; /* decrements to 0x60 */
        test_state.free_throw_aim_direction = 0;
        check(dd_gameplay_step(pack, &test_state, 0u), "step level 9 with aim 0x60");
        check(test_state.free_throw_aim == 0x60u, "aim reaches 0x60");
        check(test_state.players[5].action == DD_PLAYER_FREE_THROW_GATHER &&
              test_state.ball.action == DD_BALL_SHOT_GATHER,
              "LEVEL >= 9 with aim == $60 takes $884A BEQ and shoots");
    }

    /* 9. Progression from shot gather through height-script release to airborne flight */
    {
        DDGameplayState test_state = state;
        test_state.gameplay_level = 9u;
        test_state.cpu_global_frame = 0x8Fu;
        test_state.free_throw_timer = 0u;
        test_state.free_throw_aim = 0x62u;
        test_state.free_throw_aim_direction = 0;
        check(dd_gameplay_step(pack, &test_state, 0u), "initiate level 9 aim 60 shot");
        check(test_state.players[5].action == DD_PLAYER_FREE_THROW_GATHER, "shooter in gather");

        /* Step height script through apex release $81 */
        for (i = 0u; i < 50u && test_state.ball.action == DD_BALL_SHOT_GATHER; ++i) {
            check(dd_gameplay_step(pack, &test_state, 0u), "advance gather height script");
        }
        check(test_state.ball.action == DD_BALL_AIRBORNE,
              "height script apex $81 releases free throw to DD_BALL_AIRBORNE $05");
        check(test_state.last_shooter == 5u && test_state.shot_value == 1u,
              "shooter and 1-point value preserved across airborne flight");
    }
}

static void check_user_ordinary_steal(const DDAssetPack *pack) {
    DDGameplayState base;
    DDGameplayState state;
    uint32_t player;
    uint32_t i;

    memset(&base, 0, sizeof(base));
    check(dd_gameplay_advance_to(pack, &base, 356u, 0u), "advance to live for steal fixture");

    /* 1. Rejection when defender is in live defense without A button on ball $01 */
    state = base;
    state.phase = DD_GAMEPLAY_LIVE;
    state.cpu_global_frame = 0u;
    state.controlled_player = 0u;
    state.carrier = 5u;
    state.contact_lock_timer = 0u;
    state.score_contact_gate = 0u;
    state.ball.action = DD_BALL_DRIBBLE;
    state.ball.owner = 5u;
    state.players[0].action = DD_PLAYER_LIVE_USER;
    state.players[5].action = DD_PLAYER_LIVE_CARRIER;
    state.players[0].court_x = state.players[5].court_x;
    state.players[0].court_depth = state.players[5].court_depth;
    state.ball.court_x = state.players[5].court_x;
    state.ball.court_depth = state.players[5].court_depth;
    state.ball.height = state.players[5].height + 0x0800;
    check(dd_gameplay_step(pack, &state, 0u), "step user defense without A input");
    check(state.carrier == 5u && state.ball.owner == 5u &&
          state.players[0].action == DD_PLAYER_LIVE_USER,
          "$A408 BCC rejects live dribble steal when A button is not pressed");

    /* 2. Rejection when contact lock $001D is nonzero */
    state = base;
    state.phase = DD_GAMEPLAY_LIVE;
    state.cpu_global_frame = 0u;
    state.controlled_player = 0u;
    state.carrier = 5u;
    state.contact_lock_timer = 2u;
    state.score_contact_gate = 0u;
    state.ball.action = DD_BALL_DRIBBLE;
    state.ball.owner = 5u;
    state.players[0].action = DD_PLAYER_LIVE_USER;
    state.players[5].action = DD_PLAYER_LIVE_CARRIER;
    state.players[0].court_x = state.players[5].court_x;
    state.players[0].court_depth = state.players[5].court_depth;
    state.ball.court_x = state.players[5].court_x;
    state.ball.court_depth = state.players[5].court_depth;
    state.ball.height = state.players[5].height + 0x0800;
    check(dd_gameplay_step(pack, &state, DD_INPUT_A), "step user defense with $001D nonzero");
    check(state.carrier == 5u && state.ball.owner == 5u &&
          state.players[0].action == DD_PLAYER_LIVE_USER,
          "$A40A BNE rejects live dribble steal when $001D is nonzero");

    /* 3. Rejection when dead-ball/score gate $0056 is nonzero */
    state = base;
    state.phase = DD_GAMEPLAY_LIVE;
    state.cpu_global_frame = 0u;
    state.controlled_player = 0u;
    state.carrier = 5u;
    state.contact_lock_timer = 0u;
    state.score_contact_gate = 1u;
    state.ball.action = DD_BALL_DRIBBLE;
    state.ball.owner = 5u;
    state.players[0].action = DD_PLAYER_LIVE_USER;
    state.players[5].action = DD_PLAYER_LIVE_CARRIER;
    state.players[0].court_x = state.players[5].court_x;
    state.players[0].court_depth = state.players[5].court_depth;
    state.ball.court_x = state.players[5].court_x;
    state.ball.court_depth = state.players[5].court_depth;
    state.ball.height = state.players[5].height + 0x0800;
    check(dd_gameplay_step(pack, &state, DD_INPUT_A), "step user defense with $0056 nonzero");
    check(state.carrier == 5u && state.ball.owner == 5u &&
          state.players[0].action == DD_PLAYER_LIVE_USER,
          "$A40E BNE rejects live dribble steal when $0056 is nonzero");

    /* 4. Rejection when ball state is invalid (e.g. ball $00 awarded) */
    state = base;
    state.phase = DD_GAMEPLAY_LIVE;
    state.cpu_global_frame = 0u;
    state.controlled_player = 0u;
    state.carrier = 5u;
    state.contact_lock_timer = 0u;
    state.score_contact_gate = 0u;
    state.ball.action = DD_BALL_AWARDED;
    state.ball.owner = 5u;
    state.players[0].action = DD_PLAYER_LIVE_USER;
    state.players[5].action = DD_PLAYER_LIVE_CARRIER;
    state.players[0].court_x = state.players[5].court_x;
    state.players[0].court_depth = state.players[5].court_depth;
    state.ball.court_x = state.players[5].court_x;
    state.ball.court_depth = state.players[5].court_depth;
    state.ball.height = state.players[5].height + 0x0800;
    check(dd_gameplay_step(pack, &state, DD_INPUT_A), "step user defense with ball $00");
    check(state.carrier == 5u && state.ball.owner == 5u &&
          state.players[0].action == DD_PLAYER_LIVE_USER,
          "$A400 BNE rejects when ball state is neither loose nor live dribble/shot");

    /* 5. Redirection to jump contest $A607 when paired opponent is in carrier route/decide $26/$27 */
    state = base;
    state.phase = DD_GAMEPLAY_LIVE;
    state.cpu_global_frame = 0u;
    state.controlled_player = 0u;
    state.carrier = 5u;
    state.contact_lock_timer = 0u;
    state.score_contact_gate = 0u;
    state.ball.action = DD_BALL_DRIBBLE;
    state.ball.owner = 5u;
    state.players[0].action = DD_PLAYER_LIVE_USER;
    state.players[0].paired_player = 5u;
    state.players[5].action = DD_PLAYER_LIVE_CARRIER_ROUTE;
    check(dd_gameplay_step(pack, &state, DD_INPUT_A), "step user defense with paired carrier in $26");
    check(state.players[0].action == DD_PLAYER_USER_CONTEST &&
          state.players[0].height_script_index == 11u &&
          state.carrier == 5u,
          "$A41A BEQ $A426 redirects paired carrier state $26 to jump contest $A607");

    /* 6. Collision miss via $B435 outside 10px boundary */
    state = base;
    state.phase = DD_GAMEPLAY_LIVE;
    state.cpu_global_frame = 0u;
    state.controlled_player = 0u;
    state.carrier = 5u;
    state.contact_lock_timer = 0u;
    state.score_contact_gate = 0u;
    state.ball.action = DD_BALL_DRIBBLE;
    state.ball.owner = 5u;
    state.players[0].action = DD_PLAYER_LIVE_USER;
    state.players[0].paired_player = 5u;
    state.players[5].action = DD_PLAYER_LIVE_CARRIER;
    state.ball.court_x = 0x010000;
    state.ball.court_depth = 0x005800;
    state.ball.height = 0x1000;
    /* Defender at dx = 11 px (0x0B00) > 10 px radius */
    state.players[0].court_x = state.ball.court_x + 0x0B00;
    state.players[0].court_depth = state.ball.court_depth;
    state.players[0].height = state.ball.height;
    check(dd_gameplay_step(pack, &state, DD_INPUT_A), "step user steal at distance 11px");
    check(state.carrier == 5u && state.ball.owner == 5u &&
          state.players[0].action == DD_PLAYER_LIVE_USER,
          "$A434 BCS rejects steal when defender is outside the $B435 10px collision radius");

    /* 7. Collision hit via $B435 at exact 10px boundary (dx = 10 px, dz = 10 px) */
    state = base;
    state.phase = DD_GAMEPLAY_LIVE;
    state.cpu_global_frame = 0u;
    state.controlled_player = 0u;
    state.carrier = 5u;
    state.contact_lock_timer = 0u;
    state.score_contact_gate = 0u;
    state.possession_foul_timer = 0x50u;
    state.ball.action = DD_BALL_DRIBBLE;
    state.ball.owner = 5u;
    state.players[0].action = DD_PLAYER_LIVE_USER;
    state.players[0].paired_player = 5u;
    state.players[5].action = DD_PLAYER_LIVE_CARRIER;
    state.players[0].facing = 0u;
    state.players[5].facing = 4u;
    state.ball.court_x = 0x010000;
    state.ball.court_depth = 0x005800;
    state.ball.height = 0x1000;
    /* Defender at exact boundary dx = 10 px (0x0A00), dz = 10 px (0x0A00) */
    state.players[0].court_x = state.ball.court_x + 0x0A00;
    state.players[0].court_depth = state.ball.court_depth + 0x0A00;
    state.players[0].height = state.ball.height;
    check(dd_gameplay_step(pack, &state, DD_INPUT_A), "step user steal at exact 10px boundary");
    check(state.carrier == 0u && state.ball.owner == 0u &&
          state.players[0].action == DD_PLAYER_LIVE_USER_CARRIER &&
          state.possession_direction == 1u,
          "$B435 recognizes exact 10px boundary overlap and transfers possession through $A44B");

    /* 8. Exceptional foul $1A via $A347 when $0025 == 0, ball is $01, and facings match */
    state = base;
    state.phase = DD_GAMEPLAY_LIVE;
    state.cpu_global_frame = 0u;
    state.controlled_player = 0u;
    state.carrier = 5u;
    state.contact_lock_timer = 0u;
    state.score_contact_gate = 0u;
    state.possession_foul_timer = 0u; /* $0025 == 0 */
    state.ball.action = DD_BALL_DRIBBLE;
    state.ball.owner = 5u;
    state.players[0].action = DD_PLAYER_LIVE_USER;
    state.players[0].paired_player = 5u;
    state.players[5].action = DD_PLAYER_LIVE_CARRIER;
    state.players[0].facing = 4u;
    state.players[5].facing = 4u; /* matching facing direction */
    state.ball.court_x = 0x010000;
    state.ball.court_depth = 0x005800;
    state.ball.height = 0x1000;
    state.players[5].court_x = state.ball.court_x;
    state.players[5].court_depth = state.ball.court_depth;
    state.players[5].height = state.ball.height;
    state.players[0].court_x = state.ball.court_x;
    state.players[0].court_depth = state.ball.court_depth;
    state.players[0].height = state.ball.height;
    check(dd_gameplay_step(pack, &state, DD_INPUT_A), "step user steal during zero $0025 with same facing");
    check(state.phase == DD_GAMEPLAY_FREE_THROW &&
          state.foul_shooter == 5u &&
          state.free_throw_coarse_age == 0u,
          "$A347 triggers exceptional foul free throw sequence $1A when $0025 is zero and facings match");

    /* 9. Same-player violation via $A439 when defender == owner */
    state = base;
    state.phase = DD_GAMEPLAY_LIVE;
    state.cpu_global_frame = 0u;
    state.controlled_player = 0u;
    state.carrier = 0u;
    state.contact_lock_timer = 0u;
    state.score_contact_gate = 0u;
    state.possession_foul_timer = 0x50u;
    state.ball.action = DD_BALL_DRIBBLE;
    state.ball.owner = 0u; /* defender owns the ball already */
    state.players[0].action = DD_PLAYER_LIVE_USER;
    state.players[0].paired_player = 5u;
    state.players[5].action = DD_PLAYER_LIVE_CPU;
    state.ball.court_x = 0x010000;
    state.ball.court_depth = 0x005800;
    state.ball.height = 0x1000;
    state.players[0].court_x = state.ball.court_x;
    state.players[0].court_depth = state.ball.court_depth;
    state.players[0].height = state.ball.height;
    check(dd_gameplay_step(pack, &state, DD_INPUT_A), "step user steal when defender is already owner");
    check(state.phase == DD_GAMEPLAY_INBOUND &&
          state.inbound_reason == 0x0Fu &&
          state.audio_event == 0x2Cu,
          "$A439-$A444 takes whistle $2C and reason-$0F turnover inbound when defender equals owner");

    /* 10. Complete $A44B state reset and role-zero swap with reciprocal paired link swap */
    state = base;
    state.phase = DD_GAMEPLAY_LIVE;
    state.cpu_global_frame = 0u;
    state.controlled_player = 2u; /* player 2 (role 2) is the controlled defender */
    state.carrier = 5u;
    state.contact_lock_timer = 0u;
    state.score_contact_gate = 0u;
    state.possession_foul_timer = 0x50u;
    state.ball.action = DD_BALL_DRIBBLE;
    state.ball.owner = 5u;
    state.ball.velocity_x = 0x0123;
    state.ball.velocity_depth = 0x0045;
    state.ball.velocity_height = 0x0067;
    for (player = 0u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
        state.players[player].role = (uint8_t)(player % 5u);
        state.players[player].height = 0x2A00;
    }
    state.players[0].paired_player = 5u;
    state.players[2].paired_player = 7u;
    state.players[5].paired_player = 0u;
    state.players[7].paired_player = 2u;
    state.players[2].action = DD_PLAYER_LIVE_USER;
    state.players[5].action = DD_PLAYER_LIVE_CARRIER;
    state.ball.court_x = 0x010000;
    state.ball.court_depth = 0x005800;
    state.ball.height = 0x1000;
    state.players[2].court_x = state.ball.court_x;
    state.players[2].court_depth = state.ball.court_depth;
    state.players[2].height = state.ball.height;
    check(dd_gameplay_step(pack, &state, DD_INPUT_A), "step user steal on non-role-zero defender");
    check(state.carrier == 2u && state.ball.owner == 2u &&
          state.controlled_player == 2u &&
          state.possession_direction == 1u &&
          state.score_contact_gate == 0u &&
          state.ball.action == DD_BALL_DRIBBLE &&
          state.ball.velocity_x == 0 && state.ball.velocity_depth == 0 && state.ball.velocity_height == 0 &&
          state.players[2].role == 0u &&
          state.players[0].role == 2u &&
          state.players[2].paired_player == 5u &&
          state.players[5].paired_player == 2u &&
          state.players[2].action == DD_PLAYER_LIVE_USER_CARRIER &&
          state.players[0].action == DD_PLAYER_LIVE_TEAMMATE &&
          state.players[1].action == DD_PLAYER_LIVE_TEAMMATE &&
          state.players[3].action == DD_PLAYER_LIVE_TEAMMATE &&
          state.players[4].action == DD_PLAYER_LIVE_TEAMMATE &&
          state.players[5].action == DD_PLAYER_LIVE_CPU &&
          state.players[8].action == DD_PLAYER_LIVE_CPU_CUT &&
          state.players[9].action == DD_PLAYER_LIVE_CPU_ROUTE &&
          state.players[0].height == 0x1000 &&
          state.players[2].height == 0x1000 &&
          state.players[5].height == 0x1000,
          "$A44B resets owner, carrier, direction, role links, and defensive formations");

    /* 11. Multi-frame natural live approach: defender runs toward carrier, presses A, steals and dribbles downcourt */
    state = base;
    state.phase = DD_GAMEPLAY_LIVE;
    state.controlled_player = 0u;
    state.carrier = 5u;
    state.contact_lock_timer = 0u;
    state.score_contact_gate = 0u;
    state.possession_foul_timer = 0x50u;
    state.ball.action = DD_BALL_DRIBBLE;
    state.ball.owner = 5u;
    state.ball.court_x = 0x010000;
    state.ball.court_depth = 0x005800;
    state.ball.height = 0x1000;
    state.players[5].court_x = 0x010000;
    state.players[5].court_depth = 0x005800;
    state.players[5].height = 0x1000;
    state.players[5].action = DD_PLAYER_LIVE_CARRIER;
    /* Defender starts 16px to the left */
    state.players[0].court_x = 0x010000 - (16 * 256);
    state.players[0].court_depth = 0x005800;
    state.players[0].height = 0x1000;
    state.players[0].action = DD_PLAYER_LIVE_USER;

    /* Advance defender right toward carrier */
    for (i = 0u; i < 10u; ++i) {
        check(dd_gameplay_step(pack, &state, DD_INPUT_RIGHT), "approach carrier moving right");
    }
    check(state.players[0].action == DD_PLAYER_LIVE_USER && state.carrier == 5u,
          "defender remains in live defense while approaching");

    /* Ensure defender is at carrier position and press A */
    state.players[0].court_x = state.ball.court_x;
    state.players[0].court_depth = state.ball.court_depth;
    check(dd_gameplay_step(pack, &state, DD_INPUT_RIGHT | DD_INPUT_A), "execute steal upon arrival");
    check(state.carrier == 0u && state.ball.owner == 0u &&
          state.players[0].action == DD_PLAYER_LIVE_USER_CARRIER,
          "natural user approach and steal successfully acquires ball into live user carrier");

    /* Dribble downcourt as user carrier */
    for (i = 0u; i < 10u; ++i) {
        check(dd_gameplay_step(pack, &state, DD_INPUT_RIGHT), "dribble right downcourt");
    }
    check(state.players[0].action == DD_PLAYER_LIVE_USER_CARRIER &&
          state.ball.action == DD_BALL_DRIBBLE &&
          state.players[0].court_x > 0x010000,
          "stealer advances downcourt in full control of live dribble");
}

int main(int argc, char **argv) {
    static const uint8_t post_inbound_action[DD_GAMEPLAY_PLAYER_COUNT] = {
        0x0Fu, 0x20u, 0x20u, 0x20u, 0x20u, 0x40u, 0x25u, 0x37u, 0x3Du, 0x3Eu
    };
    static const uint8_t post_inbound_target[DD_GAMEPLAY_PLAYER_COUNT] = {
        0xE8u, 0x48u, 0xCCu, 0xB5u, 0x79u, 0x21u, 0xA6u, 0xD7u, 0x8Cu, 0x4Cu
    };
    static const uint8_t priority_cycle[10] = {
        3u, 9u, 4u, 5u, 0u, 6u, 1u, 7u, 2u, 8u
    };
    DDAssetPack pack;
    DDGameplayState state;
    DDGameplayState priority_state;
    DDGameplayState jump_state;
    DDGameplayState pass_state;
    DDGameplayState receipt_state;
    DDGameplayState inbound_pass_state;
    DDGameplayState dispatch_base;
    DDGameplayState dispatch_state;
    DDGameplayState contest_state;
    DDGameplayState held_shot_state;
    DDGameplayState free_throw_state;
    DDGameplayState free_throw_rearm_state;
    DDGameplayState free_throw_timeout_state;
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
    check_level_gameplay(&pack);
    check_long_run_native_match(&pack);
    check_user_offense_cpu_defense(&pack);
    check_cpu_free_throw_level_policy(&pack);
    check_user_ordinary_steal(&pack);
    check(assets->cpu_role_targets[6] == 0xD5u && assets->cpu_role_targets[7] == 0x5Au &&
          assets->cpu_role_targets[16] == 0x54u && assets->cpu_role_targets[17] == 0xD7u,
          "asset pack exposes the observed role targets at both half-court phases");
    check(assets->cpu_spacing_targets[3] == 0x8Cu && assets->cpu_spacing_targets[13] == 0x4Cu,
          "asset pack exposes the observed opening spacing and cut targets");
    check(assets->cpu_region_targets[0] == 0x96u &&
          assets->cpu_region_targets[1] == 0xECu &&
          assets->cpu_region_targets[6] == 0x25u,
          "asset pack exposes $AC78's seven route-init region targets");
    {
        static const uint8_t first_dunk_objects[12] = {
            0x50u, 0x10u, 0x00u, 0x00u,
            0xA0u, 0x11u, 0x00u, 0x00u,
            0xC0u, 0x12u, 0x00u, 0x00u
        };
        check(memcmp(assets->dunk_object[0][0], first_dunk_objects,
                     sizeof(first_dunk_objects)) == 0,
              "DDAP carries `$D55A->$919F` variant-zero stage-zero cinematic objects");
        check(assets->dunk_ppu[0][0][0x204Au] == 0x87u &&
              assets->dunk_ppu[0][0][0x3F00u] != 0u,
              "DDAP carries decoded `$D403/$D409/$D501` dunk background and live palette");
    }
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
    check(pack.tipoff_meta.cpu_block_audio_frames == 4u &&
          pack.cpu_block_audio_count == 8u &&
          pack.cpu_block_audio[0].period == 384u &&
          pack.cpu_block_audio[1].channel == 3u &&
          pack.cpu_block_audio[4].frame == 2u &&
          pack.cpu_block_audio[4].period == 336u,
          "DDAP v20 exposes controlled `$10` CPU block streams `$87A4/$87AD`");
    check(pack.tipoff_meta.user_block_audio_frames == 13u &&
          pack.user_block_audio_count == 15u &&
          pack.user_block_audio[0].period == 416u &&
          pack.user_block_audio[1].channel == 3u &&
          pack.user_block_audio[2].period == 356u &&
          pack.user_block_audio[14].frame == 12u &&
          pack.user_block_audio[14].volume == 0u,
          "DDAP v20 exposes controlled `$20` user block streams `$87DD/$866B`");
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
          "DDAP v20 exposes the isolated $18 then $1F/$22 made-basket score cue");
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
    check(assets->free_throw_formation[0] == 0xB6u &&
          assets->free_throw_formation[1] == DD_PLAYER_FREE_THROW_SHOOTER &&
          assets->free_throw_formation[20] == 0xA9u &&
          assets->free_throw_formation[21] == DD_PLAYER_FREE_THROW_SHOOTER &&
          assets->free_throw_facing[3] == 0x01u &&
          assets->free_throw_facing[10] == 0x04u &&
          assets->free_throw_facing[18] == 0x04u,
          "DDAP v20 exposes `$85C7/$86AF` free-throw formation and facing tables");

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
    {
        static const uint16_t jump_start[10] = {
            285u, 289u, 293u, 297u, 301u,
            305u, 309u, 313u, 317u, 321u
        };
        static const uint8_t user_wins[10] = {
            0u, 0u, 0u, 0u, 1u, 0u, 1u, 1u, 0u, 1u
        };
        uint32_t timing;
        for (timing = 0u; timing < 10u; ++timing) {
            memset(&jump_state, 0, sizeof(jump_state));
            check(dd_gameplay_advance_to(&pack, &jump_state,
                                         jump_start[timing] - 1u, 0u) &&
                  dd_gameplay_step(&pack, &jump_state, DD_INPUT_B) &&
                  dd_gameplay_advance_to(&pack, &jump_state, 356u, 0u),
                  "$A638 tip timing sweep reaches the live boundary");
            check((jump_state.tip_winner == 0u) == (user_wins[timing] != 0u),
                  "$9ABD zero-byte plateau plus $A6C3 contact matches the FCEUX B sweep");
        }
    }

    memset(&state, 0, sizeof(state));
    check(dd_gameplay_advance_to(&pack, &state, 356u, 0u), "advance to live handoff");
    check(state.cpu_global_frame == 0xDCu, "live handoff preserves original $001A phase");
    check_checkpoint(&state, 0u, 5u, DD_PLAYER_LIVE_CARRIER, 0xD4u);
    for (player = 0u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
        live_start_x[player] = state.players[player].court_x;
        live_start_depth[player] = state.players[player].court_depth;
    }

    priority_state = state;
    for (player = 0u; player < 10u; ++player) {
        char message[128];
        check(dd_gameplay_step(&pack, &priority_state, 0u),
              "$9E70 priority-cycle step succeeds");
        snprintf(message, sizeof(message),
                 "$9E70/$9E90 alternating priority cycle step %u", player + 1u);
        check(priority_state.cpu_priority_player == priority_cycle[player], message);
        check(priority_state.hud_split_y == 64u,
              "$9E70 priority scheduling preserves the native HUD split");
    }
    check(priority_state.cpu_global_frame == 0xE6u,
          "$9E70 ten-frame priority cycle advances `$001A` `$DC->$E6`");

    dispatch_state = state;
    dispatch_state.cpu_entropy = 0x12u;
    dispatch_state.cpu_global_frame = 0x20u;
    check(dd_gameplay_step(&pack, &dispatch_state, 0u),
          "advance fixed `$C02B` gameplay entropy recurrence");
    check(dispatch_state.cpu_entropy == 0x33u,
          "$C02B` adds phase `$001A` plus one to gameplay entropy `$0063`");

    check(dd_gameplay_step(&pack, &state, 0u), "first live CPU step");
    check(state.cpu_global_frame == 0xDDu, "first live step advances CPU frame phase");
    check(state.players[1].cpu_updates == 1u && state.players[5].cpu_updates == 0u,
          "odd CPU frame updates only native team slots 1-4");
    check(state.players[3].court_x == 0x010E26 &&
          state.players[3].court_depth == 0x0021AA,
          "original frame 2558 `$20->$90B3/$8BF8/$A84C` exact sub-cell position");

    check(dd_gameplay_step(&pack, &state, 0u), "second live CPU step");
    check(state.cpu_global_frame == 0xDEu, "second live step advances CPU frame phase");
    check(state.players[1].cpu_updates == 1u && state.players[5].cpu_updates == 1u,
          "even CPU frame updates only native team slots 5-9");
    check(state.cpu_priority_player == 9u,
          "$004D rotates from original slot $05 to $0B on the even team frame");
    check(state.players[5].court_x == 0x01081E &&
          state.players[5].court_depth == 0x005782 &&
          state.players[8].court_x == 0x0123C4 &&
          state.players[8].court_depth == 0x003158 &&
          state.players[9].court_x == 0x011C08 &&
          state.players[9].court_depth == 0x007064,
          "original frame 2559 installed vectors reproduce all moving-team sub-cells");
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
    check(state.live_frame == 213u && state.ball.action == DD_BALL_SCORE &&
          state.score_contact_gate == 2u,
          "`$AE25` reaches score state `$06` with flipped-direction `$0056=02`");
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
    {
        DDGameplayState smooth_inbound = state;
        int32_t previous_x = smooth_inbound.players[0].court_x;
        int32_t previous_depth = smooth_inbound.players[0].court_depth;
        int32_t prior_x_delta = 0;
        int32_t prior_depth_delta = 0;
        uint32_t route_reversals = 0u;
        uint32_t route_frame;
        for (route_frame = 0u; route_frame < 400u &&
             smooth_inbound.players[0].action != DD_PLAYER_LIVE_USER_INBOUND;
             ++route_frame) {
            int32_t x_delta;
            int32_t depth_delta;
            check(dd_gameplay_step(&pack, &smooth_inbound, 0u),
                  "advance made-basket rebound route without axis shuffle");
            x_delta = smooth_inbound.players[0].court_x - previous_x;
            depth_delta = smooth_inbound.players[0].court_depth - previous_depth;
            if (x_delta != 0 && prior_x_delta != 0 &&
                ((x_delta < 0) != (prior_x_delta < 0))) ++route_reversals;
            if (depth_delta != 0 && prior_depth_delta != 0 &&
                ((depth_delta < 0) != (prior_depth_delta < 0))) ++route_reversals;
            if (x_delta != 0) prior_x_delta = x_delta;
            if (depth_delta != 0) prior_depth_delta = depth_delta;
            previous_x = smooth_inbound.players[0].court_x;
            previous_depth = smooth_inbound.players[0].court_depth;
        }
        check(route_frame < 400u && route_reversals == 0u &&
              smooth_inbound.players[0].action == DD_PLAYER_LIVE_USER_INBOUND,
              "$8E71/$8EBF latch reached packed axes and complete inbound without shuffling");
    }
    check(dd_gameplay_advance_to(&pack, &state, 743u, 0u), "advance to loose-ball recovery");
    check(state.live_frame == 387u && state.ball.action == DD_BALL_DRIBBLE &&
          state.carrier == 0u && state.score_contact_gate == 2u,
          "rebound-return possession retains `$AE25`'s `$0056=02` gate");
    check(dd_gameplay_advance_to(&pack, &state, 803u, 0u), "advance to out-of-bounds ball");
    check(state.live_frame == 447u && state.ball.action == DD_BALL_AWARDED &&
          state.ball.owner == 0u && state.carrier == 0u &&
          state.score_contact_gate == 0u &&
          state.players[0].action == DD_PLAYER_LIVE_USER_INBOUND,
          "`$8F50` clears `$0056` before user inbound state `$0D`");
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
    check(dd_gameplay_advance_to(&pack, &state, 1290u, 0u), "advance to inbound hold");
    check(state.live_frame == 934u && state.carrier == 5u &&
          state.players[5].action == DD_PLAYER_INBOUND_HOLD,
          "live 934 gives the inbounder the held ball in state $30");
    check(state.players[5].court_depth >= 0x009000 &&
          state.players[5].court_depth <= 0x0098FF &&
          state.players[5].target_depth == 0x009800,
          "inbounder reaches extended target cell $0121 in the legal baseline depth band");
    check(dd_gameplay_advance_to(&pack, &state, 1362u, 0u), "advance to inbound release setup");
    check(state.live_frame == 1006u &&
          state.players[5].action == DD_PLAYER_INBOUND_READY &&
          state.players[5].release_timer == 8u &&
          state.ball.action == DD_BALL_AWARDED &&
          (state.ball.velocity_x != 0 || state.ball.velocity_depth != 0),
          "live 1006 follows $9018->$B0B8 into state $31 with the CPU inbound vector installed");
    check(dd_gameplay_advance_to(&pack, &state, 1378u, 0u), "advance to inbound pass");
    check(state.live_frame == 1022u && state.ball.action == DD_BALL_PASS &&
          state.ball.receiver == 6u,
          "live 1022 starts original pass state $02 toward the adjacent receiver");
    check(dd_gameplay_advance_to(&pack, &state, 1396u, 0u), "advance to inbound reception");
    check(state.live_frame == 1040u && state.phase == DD_GAMEPLAY_LIVE &&
          state.ball.action == DD_BALL_DRIBBLE && state.carrier == 6u,
          "live 1040 completes the inbound and resumes CPU possession decisions");
    check(state.clock_minutes == 0x04u && state.clock_seconds == 0x25u,
          "native HUD clock remains synchronized at the exact-route inbound reception");
    check(state.players[5].role == 1u && state.players[6].role == 0u &&
          state.players[5].paired_player == 4u &&
          state.players[4].paired_player == 5u &&
          state.players[6].paired_player == 0u &&
          state.players[0].paired_player == 6u,
          "$993A/$99D9/$9A31 preserve inbound role and reciprocal pair swaps");
    for (player = 0u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
        check_checkpoint(&state, 1040u, player,
                         post_inbound_action[player], post_inbound_target[player]);
        live_start_x[player] = state.players[player].court_x;
        live_start_depth[player] = state.players[player].court_depth;
    }
    check(dd_gameplay_advance_to(&pack, &state, 1424u, 0u),
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
    /* Natural original frame 2679 reaches `$D772` from exact 24-bit/16-bit
       coordinates `$00DA6E/$3C48`, installs `$85`, scales `$FF02/$0019`
       through `$8BF8` to `$FEC2/$001F`, then `$D98D->$A84C` integrates each
       axis twice on the same dispatch. */
    pass_state.players[5].court_x = 0x00DA6E;
    pass_state.players[5].court_depth = 0x003C48;
    pass_state.players[5].target_zone = 0x6Du;
    pass_state.players[5].target_x = pass_state.players[5].court_x;
    pass_state.players[5].target_depth = pass_state.players[5].court_depth;
    pass_state.players[5].paired_player = 0u;
    pass_state.players[0].role = 0u;
    set_packed_position(&pass_state.players[0], 0xAFu);
    pass_state.cpu_projection_high = 0u;
    check(dd_gameplay_step(&pack, &pass_state, 0u),
          "run region-two center-lane reservation");
    check(pass_state.players[5].target_zone == 0x85u &&
          pass_state.players[5].decision_timer == 10u &&
          (uint16_t)pass_state.players[5].route_velocity_x == 0xFEC2u &&
          (uint16_t)pass_state.players[5].route_velocity_depth == 0x001Fu &&
          pass_state.players[5].court_x == 0x00D7F2 &&
          pass_state.players[5].court_depth == 0x003C86,
          "$D77B-$D8B3 reserves only $85 and applies exact 5/4 route scaling plus same-dispatch movement");

    prepare_cpu_policy(&pack, &pass_state, 0x54u, 0x6Du);
    pass_state.players[5].paired_player = 0u;
    pass_state.players[0].role = 0u;
    set_packed_position(&pass_state.players[0], 0x85u);
    pass_state.cpu_projection_high = 0u;
    check(dd_gameplay_step(&pack, &pass_state, 0u),
          "keep $85 when role zero and the pair occupy $85");
    check(pass_state.players[5].target_zone == 0x85u &&
          pass_state.players[5].decision_timer == 10u,
          "$D795 compares shared projected high byte $0031, not candidate target $85");

    prepare_cpu_policy(&pack, &pass_state, 0x54u, 0x6Du);
    pass_state.players[5].paired_player = 1u;
    pass_state.players[0].role = 0u;
    set_packed_position(&pass_state.players[0], 0x00u);
    pass_state.players[1].role = 1u;
    set_packed_position(&pass_state.players[1], 0xAFu);
    pass_state.cpu_projection_high = 0u;
    check(dd_gameplay_step(&pack, &pass_state, 0u),
          "force $D795 role-zero projected-high rejection");
    check(pass_state.players[5].target_zone == 0xAAu &&
          pass_state.players[5].decision_timer == 9u,
          "$D7C5->$D857 installs the phase-table fallback and decrements $04F0");

    prepare_cpu_policy(&pack, &pass_state, 0x54u, 0x6Du);
    pass_state.players[5].paired_player = 1u;
    pass_state.players[0].role = 0u;
    set_packed_position(&pass_state.players[0], 0xAFu);
    pass_state.players[1].role = 1u;
    set_packed_position(&pass_state.players[1], 0x00u);
    pass_state.cpu_projection_high = 0u;
    check(dd_gameplay_step(&pack, &pass_state, 0u),
          "force $D7A2 paired-player projected-high rejection");
    check(pass_state.players[5].target_zone == 0xAAu &&
          pass_state.players[5].decision_timer == 9u,
          "paired rejection shares `$D857->$D7DE` policy, timer, and scaled route installation");

    prepare_cpu_policy(&pack, &pass_state, 0x54u, 0x6Du);
    pass_state.players[5].paired_player = 0u;
    pass_state.players[0].role = 0u;
    set_packed_position(&pass_state.players[0], 0xAFu);
    pass_state.players[1].role = 1u;
    set_packed_position(&pass_state.players[1], 0x85u);
    pass_state.cpu_projection_high = 0u;
    check(dd_gameplay_step(&pack, &pass_state, 0u),
          "ignore unrelated $85 occupancy in the region-two special branch");
    check(pass_state.players[5].target_zone == 0x85u,
          "$D77B does not call the broad native target-occupancy scan");

    prepare_cpu_policy(&pack, &pass_state, 0x54u, 0x72u);
    pass_state.possession_direction = 1u;
    pass_state.players[5].paired_player = 0u;
    pass_state.players[0].role = 0u;
    set_packed_position(&pass_state.players[0], 0xAFu);
    pass_state.cpu_projection_high = 0u;
    check(dd_gameplay_step(&pack, &pass_state, 0u),
          "run mirrored region-two center-lane reservation");
    check(pass_state.players[5].target_zone == 0x9Au &&
          pass_state.players[5].decision_timer == 10u,
          "$AC64/$AC5C mirrors the sole region-two target from $85 to $9A");

    /* Reach the same branch through shipping gameplay: `$D8FA/$9018`
       queues and releases a CPU pass, `$AD41->$B138->$AD6D` awards it to
       the receiver in state `$25`, and its fourteenth scheduled turn must
       run `$8B5A->$D99A->$D978->$D759->$D772`. */
    prepare_cpu_policy(&pack, &receipt_state, 0x44u, 0xA5u);
    receipt_state.players[8].role = 3u;
    receipt_state.players[8].paired_player = 0u;
    receipt_state.players[8].facing = 4u;
    receipt_state.players[8].court_x = 0x00DA6E;
    receipt_state.players[8].court_depth = 0x003C48;
    receipt_state.players[8].target_x = receipt_state.players[8].court_x;
    receipt_state.players[8].target_depth = receipt_state.players[8].court_depth;
    receipt_state.players[8].target_zone = 0x6Du;
    receipt_state.players[0].role = 0u;
    set_packed_position(&receipt_state.players[0], 0xAFu);
    check(dd_gameplay_step(&pack, &receipt_state, 0u) &&
          receipt_state.ball.action == DD_BALL_AWARDED &&
          receipt_state.ball.receiver == 8u,
          "normal `$D8FA/$9018` gameplay queues the region-two receiver");
    for (player = 0u; player < 4u; ++player) {
        run_cpu_dispatch(&pack, &receipt_state, 5u);
    }
    for (player = 0u; player < 180u &&
         (receipt_state.ball.action != DD_BALL_DRIBBLE ||
          receipt_state.ball.owner != 8u); ++player) {
        check(dd_gameplay_step(&pack, &receipt_state, 0u),
              "advance normal CPU pass through `$AD41->$B138->$AD6D`");
    }
    check(player < 180u &&
          receipt_state.players[8].action == DD_PLAYER_LIVE_CARRIER &&
          receipt_state.players[8].route_step == 5u,
          "normal CPU pass reception enters state `$25` route five");
    for (player = 0u; player < 14u &&
         receipt_state.players[8].action == DD_PLAYER_LIVE_CARRIER; ++player) {
        run_cpu_dispatch(&pack, &receipt_state, 8u);
    }
    check(receipt_state.cpu_projection_high == 0u &&
          receipt_state.players[8].action == DD_PLAYER_LIVE_CPU_SETUP &&
          receipt_state.players[8].target_zone == 0x85u &&
          (uint16_t)receipt_state.players[8].route_velocity_x == 0xFEC2u &&
          (uint16_t)receipt_state.players[8].route_velocity_depth == 0x001Fu,
          "normal receipt reproduces `$25->$D99A->$32->$D772->$D8B0` with helper-produced `$0031`");

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
    dispatch_state.players[5].velocity_x = 0;
    dispatch_state.players[5].velocity_depth = 0;
    dispatch_state.players[5].route_velocity_x = 0;
    dispatch_state.players[5].route_velocity_depth = 0;
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
    check(dispatch_state.players[5].action == DD_PLAYER_LIVE_TEAMMATE &&
          dispatch_state.players[5].target_x != dispatch_state.players[0].court_x,
          "$90B3 targets the linked player's projected $914E collision anchor");
    dispatch_state.players[5].court_x = dispatch_state.players[5].target_x;
    dispatch_state.players[5].court_depth = dispatch_state.players[5].target_depth;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_LIVE_PAIRED_DEFENDER &&
          dispatch_state.players[5].paired_timer == 0x10u,
          "player state $20 follows $9102 anchor contact into $22 with latch $10");
    dispatch_state.players[0].court_x = 0x014000;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_LIVE_TEAMMATE &&
          dispatch_state.players[5].velocity_x == 0 &&
          dispatch_state.players[5].velocity_depth == 0,
          "player state $22 returns to $20 when $9102 reports separation");

    dispatch_state = dispatch_base;
    dispatch_state.players[5].action = DD_PLAYER_LIVE_TEAMMATE;
    dispatch_state.players[5].paired_player = 2u;
    dispatch_state.players[2].paired_player = 5u;
    dispatch_state.players[5].court_x = 0x010000;
    dispatch_state.players[5].court_depth = 0x005800;
    dispatch_state.players[0].court_x = 0x004000;
    dispatch_state.players[0].court_depth = 0x002000;
    dispatch_state.players[2].court_x = 0x014000;
    dispatch_state.players[2].court_depth = 0x006800;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].target_x != dispatch_state.players[0].court_x &&
          dispatch_state.players[5].target_depth != dispatch_state.players[0].court_depth &&
          (dispatch_state.players[5].velocity_x != 0 ||
           dispatch_state.players[5].velocity_depth != 0),
          "$8A28->$90B3 follows mutable $0580 through the projected anchor and $8BF8 vector");
    dispatch_state.players[5].court_x = dispatch_state.players[5].target_x;
    dispatch_state.players[5].court_depth = dispatch_state.players[5].target_depth;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_LIVE_PAIRED_DEFENDER,
          "$9102 contact uses the mutable pair rather than arithmetic slot+5");

    dispatch_state = dispatch_base;
    dispatch_state.players[5].action = DD_PLAYER_LIVE_TEAMMATE;
    dispatch_state.players[0].action = 0x03u;
    dispatch_state.players[5].paired_player = 0u;
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
          dispatch_state.ball.action == DD_BALL_AWARDED &&
          dispatch_state.audio_event == 0x10u &&
          dispatch_state.audio_event_serial != 0u,
          "$8B12 block contact takes an owned airborne shot and requests `$10` without transferring early");
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
    dispatch_state.contact_lock_timer = 0xFFu;
    dispatch_state.score_contact_gate = 0xFFu;
    dispatch_state.players[5].paired_player = 1u;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.ball.owner == 5u && dispatch_state.carrier == 5u &&
          dispatch_state.players[5].action == DD_PLAYER_LIVE_CARRIER &&
          dispatch_state.score_contact_gate == 0u,
          "`$91FB->$9208` ignores seeded gates then clears `$0056` at `$927C`");

    dispatch_state = dispatch_base;
    dispatch_state.players[9].action = DD_PLAYER_LIVE_SHOOTER_RESET;
    dispatch_state.ball.action = DD_BALL_REBOUND;
    dispatch_state.ball.owner = 0xFFu;
    dispatch_state.carrier = 0xFFu;
    dispatch_state.ball.court_x = dispatch_state.players[9].court_x;
    dispatch_state.ball.court_depth = dispatch_state.players[9].court_depth;
    dispatch_state.ball.height = dispatch_state.players[9].height + 0x0800;
    dispatch_state.players[9].court_x = 0x008000;
    dispatch_state.ball.court_x = dispatch_state.players[9].court_x;
    run_cpu_dispatch(&pack, &dispatch_state, 9u);
    check(dispatch_state.carrier == 9u && dispatch_state.ball.owner == 9u &&
          dispatch_state.players[9].role == 0u && dispatch_state.players[5].role == 4u &&
          dispatch_state.players[9].action == DD_PLAYER_LIVE_CARRIER &&
          dispatch_state.players[6].action == DD_PLAYER_LIVE_CPU &&
          dispatch_state.players[7].action == DD_PLAYER_LIVE_CPU &&
          dispatch_state.players[8].action == DD_PLAYER_ROUTE_INIT &&
          dispatch_state.players[5].action == DD_PLAYER_ROUTE_INIT &&
          dispatch_state.players[0].action == DD_PLAYER_LIVE_USER &&
          (dispatch_state.players[9].target_zone & 0x1Fu) == 0x05u &&
          dispatch_state.audio_event == 0x10u,
          "$91FB->$9208 swaps the winner into role zero and installs `$40/$40/$38/$38` by role");

    dispatch_state = dispatch_base;
    dispatch_state.possession_direction = 1u;
    dispatch_state.controlled_player = 0u;
    dispatch_state.carrier = 0u;
    dispatch_state.ball.owner = 0u;
    dispatch_state.ball.action = DD_BALL_DRIBBLE;
    dispatch_state.ball.court_x = dispatch_state.players[5].court_x;
    dispatch_state.ball.court_depth = dispatch_state.players[5].court_depth;
    dispatch_state.ball.height = dispatch_state.players[5].height + 0x0800;
    dispatch_state.players[5].paired_player = 0u;
    dispatch_state.players[0].paired_player = 5u;
    dispatch_state.players[5].contact_age = 19u;
    dispatch_state.contact_lock_timer = 2u;
    dispatch_state.cpu_global_frame = 1u;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.ball.owner == 0u &&
          dispatch_state.players[5].contact_age == 19u,
          "$91A6` preserves contact accumulation while `$001D` remains nonzero");
    dispatch_state.contact_lock_timer = 0u;
    dispatch_state.possession_foul_timer = 1u;
    dispatch_state.players[5].contact_age = 19u;
    dispatch_state.ball.court_x = dispatch_state.players[5].court_x;
    dispatch_state.ball.court_depth = dispatch_state.players[5].court_depth;
    dispatch_state.ball.height = dispatch_state.players[5].height + 0x0800;
    dispatch_state.cpu_global_frame = 1u;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.ball.owner == 5u && dispatch_state.carrier == 5u &&
          dispatch_state.possession_direction == 0u &&
          dispatch_state.players[5].action == DD_PLAYER_LIVE_CARRIER &&
          (dispatch_state.players[5].target_zone & 0x1Fu) == 0x0Cu &&
          dispatch_state.audio_event == 0x10u,
          "$91A6->$A347->$10->$9208 transfers after the exact paired-contact threshold");

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
    dispatch_state.cpu_priority_player = 4u;
    dispatch_state.cpu_global_frame = 1u;
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
    dispatch_state.cpu_priority_player = 4u;
    dispatch_state.cpu_global_frame = 1u;
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
    dispatch_state.contact_lock_timer = 0u;
    dispatch_state.players[1].paired_player = 5u;
    dispatch_state.players[5].paired_player = 1u;
    dispatch_state.players[0].height = 0x2A00;
    dispatch_state.players[1].contact_age = 19u;
    run_cpu_dispatch(&pack, &dispatch_state, 1u);
    check(dispatch_state.carrier == 1u && dispatch_state.ball.owner == 1u &&
          dispatch_state.controlled_player == 1u &&
          dispatch_state.players[1].action == DD_PLAYER_LIVE_USER_CARRIER &&
          dispatch_state.players[0].action == DD_PLAYER_LIVE_TEAMMATE &&
          dispatch_state.players[3].action == DD_PLAYER_LIVE_TEAMMATE &&
          dispatch_state.players[5].action == DD_PLAYER_LIVE_CPU &&
          dispatch_state.players[1].role == 0u &&
          dispatch_state.players[0].role == 1u &&
          dispatch_state.players[0].height == 0x1000 &&
          dispatch_state.possession_direction == 1u &&
          dispatch_state.audio_event == 0x10u,
          "$B435/$9FA3->$A44B sustained paired contact swaps role zero and installs both teams by role");

    dispatch_state = dispatch_base;
    dispatch_state.carrier = 5u;
    dispatch_state.ball.owner = 5u;
    dispatch_state.ball.action = DD_BALL_DRIBBLE;
    dispatch_state.ball.court_x = dispatch_state.players[1].court_x;
    dispatch_state.ball.court_depth = dispatch_state.players[1].court_depth;
    dispatch_state.ball.height = dispatch_state.players[1].height + 0x0800;
    dispatch_state.contact_lock_timer = 0u;
    dispatch_state.possession_foul_timer = 0u;
    dispatch_state.players[1].paired_player = 5u;
    dispatch_state.players[5].paired_player = 1u;
    dispatch_state.players[1].contact_age = 19u;
    dispatch_state.players[1].facing = dispatch_state.players[5].facing;
    run_cpu_dispatch(&pack, &dispatch_state, 1u);
    check(dispatch_state.phase == DD_GAMEPLAY_FREE_THROW &&
          dispatch_state.foul_shooter == 5u && dispatch_state.foul_offender == 1u &&
          dispatch_state.inbound_reason == 0x1Au && dispatch_state.dead_ball_latch == 0xFFu &&
          dispatch_state.score_contact_gate == 0xFFu &&
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
    dispatch_state.possession_foul_timer = 0u;
    check(dd_gameplay_step(&pack, &dispatch_state, DD_INPUT_LEFT),
          "step controlled $A37D exceptional contact");
    check(dispatch_state.phase == DD_GAMEPLAY_FREE_THROW &&
          dispatch_state.foul_shooter == 5u && dispatch_state.foul_offender == 0u &&
          dispatch_state.inbound_reason == 0x17u && dispatch_state.dead_ball_latch == 0xFFu &&
          dispatch_state.score_contact_gate == 0xFFu &&
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
    {
        int saw_award = 0;
        int saw_ready = 0;
        for (player = 0u; player < 2000u &&
             dispatch_state.players[5].action != DD_PLAYER_FREE_THROW_SET; ++player) {
            check(dd_gameplay_step(&pack, &dispatch_state, 0u),
                  "advance Ghidra-derived free-throw formation dispatcher");
            if (dispatch_state.ball.action == DD_BALL_DRIBBLE &&
                dispatch_state.ball.owner == 5u) saw_award = 1;
            if (dispatch_state.players[5].action == DD_PLAYER_FREE_THROW_READY) saw_ready = 1;
        }
        check(saw_award,
              "$85EF reaches the ball and awards state $01 to the role-zero shooter");
        check(saw_ready && dispatch_state.players[5].action == DD_PLAYER_FREE_THROW_SET &&
              dispatch_state.ball.action == DD_BALL_AWARDED,
              "$862A/$872F reaches shooter $45 then waits for every `$44` formation slot");
        check(dispatch_state.players[0].action == DD_PLAYER_FREE_THROW_SPOT &&
              dispatch_state.players[9].action == DD_PLAYER_FREE_THROW_SPOT,
              "all nine non-shooters finish the `$86AF` formation before the aim gate");
    }
    for (player = 0u; player < 400u &&
         dispatch_state.players[5].action != DD_PLAYER_FREE_THROW_GATHER; ++player) {
        check(dd_gameplay_step(&pack, &dispatch_state, 0u),
              "advance CPU `$87C0` timer/aim decision");
    }
    check(dispatch_state.ball.action == DD_BALL_SHOT_GATHER &&
          dispatch_state.players[5].action == DD_PLAYER_FREE_THROW_GATHER &&
          dispatch_state.shot_value == 1u &&
          dispatch_state.free_throw_aim >= 0x50u && dispatch_state.free_throw_aim <= 0x60u,
          "$87C0->$884F enters shooter $47/ball $04 with a one-point oscillating aim");
    free_throw_rearm_state = dispatch_state;
    free_throw_rearm_state.players[5].action = DD_PLAYER_FREE_THROW_FOLLOW;
    free_throw_rearm_state.ball.action = DD_BALL_REBOUND;
    free_throw_rearm_state.ball.velocity_height = 0u;
    for (player = 0u; player < 4u &&
         free_throw_rearm_state.players[5].action != DD_PLAYER_FREE_THROW_RECOVER; ++player) {
        check(dd_gameplay_step(&pack, &free_throw_rearm_state, 0u),
              "finish first free-throw result into `$49`");
    }
    check(free_throw_rearm_state.free_throw_attempts == 1u &&
          free_throw_rearm_state.players[5].action == DD_PLAYER_FREE_THROW_RECOVER &&
          free_throw_rearm_state.free_throw_timer == 0x50u,
          "$88DE increments the first result and installs `$49/$0067=$50`");
    for (player = 0u; player < 200u &&
         free_throw_rearm_state.players[5].action != DD_PLAYER_FREE_THROW_SET; ++player) {
        check(dd_gameplay_step(&pack, &free_throw_rearm_state, 0u),
              "count `$894C` toward second free throw");
    }
    check(free_throw_rearm_state.players[5].action == DD_PLAYER_FREE_THROW_SET &&
          free_throw_rearm_state.ball.action == DD_BALL_AWARDED &&
          free_throw_rearm_state.ball.owner == 5u &&
          free_throw_rearm_state.free_throw_attempts == 1u,
          "$894C/$897A-$899E` reattaches the ball at `$20` and starts attempt two");
    dispatch_state.players[5].action = DD_PLAYER_FREE_THROW_FOLLOW;
    dispatch_state.free_throw_aim = 0x60u;
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

    dispatch_state = free_throw_state;
    dispatch_state.free_throw_initialized = 1u;
    dispatch_state.free_throw_attempts = 0u;
    dispatch_state.free_throw_aim = 0x58u;
    dispatch_state.shot_value = 1u;
    dispatch_state.last_shooter = 5u;
    dispatch_state.players[5].action = DD_PLAYER_FREE_THROW_FOLLOW;
    dispatch_state.ball.action = DD_BALL_AIRBORNE;
    dispatch_state.ball.action_age = 1u;
    dispatch_state.ball.court_x = 0x004800;
    dispatch_state.ball.court_depth = 0x005800;
    dispatch_state.ball.height = 0x003500;
    dispatch_state.ball.velocity_x = 0;
    dispatch_state.ball.velocity_depth = 0;
    dispatch_state.ball.velocity_height = 0;
    check(dd_gameplay_step(&pack, &dispatch_state, 0u),
          "classify off-timing free throw at rim height");
    check(dispatch_state.ball.action == DD_BALL_LOOSE_LAUNCH &&
          dispatch_state.ball.outcome == 2u,
          "$B38D-$B39F` makes only aim `$60`; every other free-throw aim returns result two");

    dispatch_state = dispatch_base;
    dispatch_state.phase = DD_GAMEPLAY_FREE_THROW;
    dispatch_state.foul_shooter = 0u;
    dispatch_state.foul_offender = 5u;
    dispatch_state.possession_direction = 1u;
    dispatch_state.ball.action = DD_BALL_DEAD;
    dispatch_state.ball.court_x = dispatch_state.players[0].court_x;
    dispatch_state.ball.court_depth = dispatch_state.players[0].court_depth;
    dispatch_state.free_throw_initialized = 0u;
    for (player = 0u; player < 2000u &&
         dispatch_state.players[0].action != DD_PLAYER_FREE_THROW_SET; ++player) {
        check(dd_gameplay_step(&pack, &dispatch_state, 0u),
              "advance controlled user free throw to `$46`");
    }
    check(dispatch_state.players[0].action == DD_PLAYER_FREE_THROW_SET,
          "user free throw reaches the original oscillating `$46` input state");
    free_throw_timeout_state = dispatch_state;
    free_throw_timeout_state.free_throw_coarse_age = 5u * 64u - 1u;
    check(dd_gameplay_step(&pack, &free_throw_timeout_state, 0u) &&
          free_throw_timeout_state.phase == DD_GAMEPLAY_INBOUND &&
          free_throw_timeout_state.inbound_reason == 0x12u &&
          free_throw_timeout_state.audio_event == 0x2Cu,
          "$8806-$882A` five-tick free-throw timeout publishes reason `$12` and whistle `$2C`");
    for (player = 0u; player < 8u; ++player) {
        check(dd_gameplay_step(&pack, &dispatch_state, 0u),
              "hold user free throw without B");
    }
    check(dispatch_state.players[0].action == DD_PLAYER_FREE_THROW_SET &&
          dispatch_state.ball.action == DD_BALL_AWARDED,
          "$87FF waits indefinitely for controller bit `$40` while the aim oscillates");
    check(dd_gameplay_step(&pack, &dispatch_state, DD_INPUT_B),
          "press B in user free-throw aim state");
    check(dd_gameplay_step(&pack, &dispatch_state, DD_INPUT_B),
          "reach the user's scheduled free-throw dispatch");
    check(dispatch_state.players[0].action == DD_PLAYER_FREE_THROW_GATHER &&
          dispatch_state.ball.action == DD_BALL_SHOT_GATHER,
          "$87FF->$884F launches the user free throw on B press");

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
    dispatch_state.contact_lock_timer = 0u;
    dispatch_state.score_contact_gate = 0u;
    dispatch_state.players[1].paired_player = 5u;
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
    contest_state.contact_lock_timer = 0u;
    contest_state.score_contact_gate = 0u;
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
    contest_state.contact_lock_timer = 0u;
    contest_state.score_contact_gate = 0u;
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
    contest_state.contact_lock_timer = 0u;
    contest_state.score_contact_gate = 0u;
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
    dispatch_state.contact_lock_timer = 0u;
    dispatch_state.score_contact_gate = 0u;
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
          dispatch_state.ball.velocity_x == -0x0100 &&
          dispatch_state.audio_event == 0x16u,
          "$B473 latches rim contact, clears ownership, retains camera follow, reverses velocity, and requests rim SFX `$16`");

    dispatch_state = dispatch_base;
    dispatch_state.ball.action = DD_BALL_PASS_BOUNCE;
    dispatch_state.ball.owner = 5u;
    dispatch_state.carrier = 5u;
    dispatch_state.ball.court_x = 0x004500;
    dispatch_state.ball.court_depth = 0x005800;
    dispatch_state.ball.height = 0x004600;
    dispatch_state.ball.velocity_x = 0x0100;
    dispatch_state.ball.velocity_height = 0;
    dispatch_state.possession_direction = 0u;
    dispatch_state.dunk_active = 1u;
    dispatch_state.audio_event = 0u;
    check(dd_gameplay_step(&pack, &dispatch_state, 0u),
          "step `$B473` special-finish rim branch");
    check(dispatch_state.dunk_rim_contact == 1u &&
          dispatch_state.audio_event != 0x16u &&
          dispatch_state.ball.velocity_x == -0x0100,
          "`$B473` special finish writes `$003F` instead of requesting ordinary rim SFX");

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
          dispatch_state.score_contact_gate == 0xFFu &&
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

    /* Exercise the opposite award direction as well.  Common inbound mode
       three is the user-side `$A780` handoff; treating it as the CPU release
       branch changed control to an opponent even though `$9651` awarded 1P. */
    dispatch_state = dispatch_base;
    dispatch_state.phase = DD_GAMEPLAY_LIVE;
    dispatch_state.possession_direction = 0u;
    dispatch_state.ball.action = DD_BALL_HIDDEN;
    dispatch_state.ball.owner = 0xFFu;
    dispatch_state.ball.court_x = 0x010000;
    dispatch_state.ball.court_depth = 0x001000;
    dispatch_state.last_touch_player = 5u;
    check(dd_gameplay_step(&pack, &dispatch_state, 0u),
          "call out of bounds after a CPU-side last touch");
    check(dispatch_state.phase == DD_GAMEPLAY_INBOUND &&
          dispatch_state.possession_direction == 1u &&
          dispatch_state.inbound_variant == 3u &&
          dispatch_state.score_contact_gate == 0xFFu &&
          dispatch_state.players[0].action == DD_PLAYER_INBOUNDER,
          "$9635->$9651 awards CPU-last-touch OOB to the 1P role-zero object");
    {
        int32_t previous_x = dispatch_state.players[0].court_x;
        int32_t previous_depth = dispatch_state.players[0].court_depth;
        int32_t prior_x_delta = 0;
        int32_t prior_depth_delta = 0;
        uint32_t route_reversals = 0u;
        for (player = 0u; player < 400u &&
             dispatch_state.players[dispatch_state.controlled_player].action !=
                 DD_PLAYER_LIVE_USER_INBOUND; ++player) {
            int32_t x_delta;
            int32_t depth_delta;
            check(dd_gameplay_step(&pack, &dispatch_state, 0u),
                  "advance user-awarded common inbound through `$41->$30->$0D`");
            x_delta = dispatch_state.players[0].court_x - previous_x;
            depth_delta = dispatch_state.players[0].court_depth - previous_depth;
            if (dispatch_state.players[0].action == DD_PLAYER_INBOUNDER) {
                if (x_delta != 0 && prior_x_delta != 0 &&
                    ((x_delta < 0) != (prior_x_delta < 0))) ++route_reversals;
                if (depth_delta != 0 && prior_depth_delta != 0 &&
                    ((depth_delta < 0) != (prior_depth_delta < 0))) ++route_reversals;
                if (x_delta != 0) prior_x_delta = x_delta;
                if (depth_delta != 0) prior_depth_delta = depth_delta;
            }
            previous_x = dispatch_state.players[0].court_x;
            previous_depth = dispatch_state.players[0].court_depth;
        }
        check(player < 400u && route_reversals == 0u &&
              dispatch_state.phase == DD_GAMEPLAY_LIVE &&
              dispatch_state.score_contact_gate == 0u,
              "user-awarded `$41` reaches live `$30->$0D` without route-axis shuffling");
    }
    check(dispatch_state.controlled_player < 5u &&
          dispatch_state.ball.owner == dispatch_state.controlled_player &&
          dispatch_state.carrier == dispatch_state.controlled_player &&
          dispatch_state.players[dispatch_state.controlled_player].action ==
              DD_PLAYER_LIVE_USER_INBOUND,
          "common inbound mode three keeps a 1P award on the user team");
    inbound_receiver = dispatch_state.controlled_player;
    check(dd_gameplay_step(&pack, &dispatch_state,
                           DD_INPUT_RIGHT | DD_INPUT_A),
          "send direction+A through user common-inbound state `$0D`");
    check(dispatch_state.ball.receiver < 5u &&
          dispatch_state.ball.receiver != inbound_receiver,
          "$A780->$A129->$A21F selects a same-team common-inbound receiver");
    for (player = 0u; player < 180u &&
         (dispatch_state.ball.action != DD_BALL_DRIBBLE ||
          dispatch_state.carrier == inbound_receiver); ++player) {
        check(dd_gameplay_step(&pack, &dispatch_state, 0u),
              "advance the user common-inbound pass through `$AD41->$AD58`");
    }
    check(player < 180u && dispatch_state.phase == DD_GAMEPLAY_LIVE &&
          dispatch_state.carrier < 5u &&
          dispatch_state.carrier == dispatch_state.controlled_player &&
          dispatch_state.ball.owner == dispatch_state.carrier &&
          dispatch_state.players[dispatch_state.carrier].action ==
              DD_PLAYER_LIVE_USER_CARRIER,
          "user common inbound completes reception with control on the receiver");

    /* A made-basket receiver outside the recovered near-rim window must
       return to `$D759` at the traced seven-turn cadence instead of taking the
       old fixed-delay half-court shot. */
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
    dispatch_state.players[5].action_age = 6u;
    dispatch_state.players[5].court_x = 0x00DA6E;
    dispatch_state.players[5].court_depth = 0x003C48;
    dispatch_state.players[5].facing = 4u;
    dispatch_state.players[5].paired_player = 0u;
    dispatch_state.players[0].court_x = 0x004800;
    dispatch_state.players[0].court_depth = 0x003800;
    dispatch_state.cpu_projection_high = 0x7Fu;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action != DD_PLAYER_LIVE_CARRIER &&
          dispatch_state.players[5].action != DD_PLAYER_LIVE_CARRIER_DECIDE &&
          dispatch_state.ball.action != DD_BALL_AIRBORNE,
          "post-inbound `$25` re-enters `$D759` after seven turns without a half-court shot");
    check(dispatch_state.cpu_projection_high == 0u &&
          dispatch_state.players[5].action == DD_PLAYER_LIVE_CPU_SETUP &&
          dispatch_state.players[5].target_zone == 0x85u &&
          (uint16_t)dispatch_state.players[5].route_velocity_x == 0xFEC2u &&
          (uint16_t)dispatch_state.players[5].route_velocity_depth == 0x001Fu,
          "$8B5A->$D99A->$8C36` produces `$0031` before `$25->$32->$D772->$D8B0`");

    /* Accepted `$D99A` contact bypasses `$D772`, retains state `$25`, and
       reaches `$8BBF->$ABCD->$8BF8->$D98D` on that same object dispatch. */
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
    dispatch_state.players[5].action_age = 6u;
    dispatch_state.players[5].court_x = 0x010800;
    dispatch_state.players[5].court_depth = 0x005800;
    dispatch_state.players[5].facing = 4u;
    dispatch_state.players[5].paired_player = 0u;
    dispatch_state.players[0].court_x = 0x00F800;
    dispatch_state.players[0].court_depth = 0x005800;
    pass_release_x = dispatch_state.players[5].court_x;
    pass_release_depth = dispatch_state.players[5].court_depth;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.players[5].action == DD_PLAYER_LIVE_CARRIER &&
          dispatch_state.players[5].target_zone == 0x70u &&
          (dispatch_state.players[5].route_velocity_x != 0 ||
           dispatch_state.players[5].route_velocity_depth != 0) &&
          dispatch_state.players[5].velocity_x ==
              dispatch_state.players[5].route_velocity_x &&
          dispatch_state.players[5].velocity_depth ==
              dispatch_state.players[5].route_velocity_depth &&
          (dispatch_state.players[5].court_x != pass_release_x ||
           dispatch_state.players[5].court_depth != pass_release_depth),
          "$8B5A accepted avoidance stays in `$25`, scales once, and moves on the same dispatch");

    /* A left-edge projection proves `$0031` is produced by `$8C36` even
       when the signed packed step crosses the byte boundary. No test writes
       `cpu_projection_high` for this state. */
    dispatch_state = dispatch_base;
    dispatch_state.phase = DD_GAMEPLAY_LIVE;
    dispatch_state.carrier = 5u;
    dispatch_state.ball.owner = 5u;
    dispatch_state.ball.action = DD_BALL_DRIBBLE;
    dispatch_state.inbound_variant = 1u;
    dispatch_state.players[5].action = DD_PLAYER_LIVE_CARRIER;
    dispatch_state.players[5].route_step = 4u;
    dispatch_state.players[5].court_x = 0x000800;
    dispatch_state.players[5].court_depth = 0x000800;
    dispatch_state.players[5].facing = 4u;
    dispatch_state.players[5].paired_player = 0u;
    set_packed_position(&dispatch_state.players[0], 0xAFu);
    dispatch_state.ball.court_x = dispatch_state.players[5].court_x;
    dispatch_state.ball.court_depth = dispatch_state.players[5].court_depth;
    run_cpu_dispatch(&pack, &dispatch_state, 5u);
    check(dispatch_state.cpu_projection_high == 0xFFu &&
          dispatch_state.players[5].action == DD_PLAYER_LIVE_CARRIER,
          "$8B5A->$D99A->$8C36` retains the signed-overflow high byte without a direct test assignment");

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
    check(dispatch_state.dunk_active == 0u &&
          dispatch_state.ball.action == DD_BALL_SHOT_GATHER,
          "`$AA75` starts held shot gather without sampling `$B189` dunk eligibility");
    check(dd_gameplay_step(&pack, &dispatch_state, 0u),
          "release the recovered right-rim dunk gate");
    check(dispatch_state.dunk_active != 0u &&
          dispatch_state.ball.action == DD_BALL_SHOT_GATHER,
          "close-rim B release enters the native bank-2-derived dunk presentation");
    dispatch_state.dunk_outcome = 1u;
    for (player = 0u; player < 90u && dispatch_state.dunk_active != 0u; ++player) {
        check(dd_gameplay_step(&pack, &dispatch_state, 0u),
              "advance dunk make animation to rim contact");
    }
    check(dispatch_state.ball.action == DD_BALL_SCORE &&
          dispatch_state.net_animation_phase == 2u &&
          dispatch_state.audio_event == 0x18u,
          "dunk make enters score/net/audio flow at the rim");
    check(player == 84u,
          "`$D40F` preserves all six fourteen-tick cinematic stages before returning");

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
    check(dd_gameplay_step(&pack, &dispatch_state, 0u),
          "release close-rim dunk miss proof");
    check(dispatch_state.dunk_active != 0u,
          "close-rim miss path activates only at `$B189` release");
    dispatch_state.dunk_outcome = 4u;
    for (player = 0u; player < 90u && dispatch_state.dunk_active != 0u; ++player) {
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

    /* `$B189-$B1DC` uses four literal `$AB53` packed cells for each side.
       Cover all eight cells and the immediately adjacent rejected columns. */
    {
        static const uint8_t user_dunk_cell[4] = {0xBAu, 0xBBu, 0x9Cu, 0x9Du};
        static const uint8_t cpu_dunk_cell[4] = {0xA5u, 0xA4u, 0x83u, 0x84u};
        uint32_t cell;
        for (cell = 0u; cell < 4u; ++cell) {
            dispatch_state = dispatch_base;
            dispatch_state.phase = DD_GAMEPLAY_LIVE;
            dispatch_state.controlled_player = 0u;
            dispatch_state.carrier = 0u;
            dispatch_state.ball.owner = 0u;
            dispatch_state.ball.action = DD_BALL_DRIBBLE;
            dispatch_state.players[0].action = DD_PLAYER_LIVE_USER_CARRIER;
            set_packed_position(&dispatch_state.players[0], user_dunk_cell[cell]);
            check(dd_gameplay_step(&pack, &dispatch_state, DD_INPUT_B) &&
                  dd_gameplay_step(&pack, &dispatch_state, 0u) &&
                  dispatch_state.dunk_active != 0u,
                  "each literal user `$B189` dunk cell activates on shipping B release");

            dispatch_state = dispatch_base;
            dispatch_state.phase = DD_GAMEPLAY_LIVE;
            dispatch_state.controlled_player = 0u;
            dispatch_state.carrier = 5u;
            dispatch_state.ball.owner = 5u;
            dispatch_state.ball.action = DD_BALL_DRIBBLE;
            dispatch_state.players[5].action = DD_PLAYER_LIVE_CARRIER_ROUTE;
            set_packed_position(&dispatch_state.players[5], cpu_dunk_cell[cell]);
            run_cpu_dispatch(&pack, &dispatch_state, 5u);
            for (player = 0u; player < 48u && dispatch_state.dunk_active == 0u &&
                 dispatch_state.ball.action == DD_BALL_SHOT_GATHER; ++player) {
                check(dd_gameplay_step(&pack, &dispatch_state, 0u),
                      "advance CPU shot height stream to `$B189` dunk gate");
            }
            check(dispatch_state.dunk_active != 0u,
                  "each literal CPU `$B189` dunk cell activates at state `$27` apex");
        }
        dispatch_state = dispatch_base;
        dispatch_state.phase = DD_GAMEPLAY_LIVE;
        dispatch_state.controlled_player = 0u;
        dispatch_state.carrier = 0u;
        dispatch_state.ball.owner = 0u;
        dispatch_state.ball.action = DD_BALL_DRIBBLE;
        dispatch_state.players[0].action = DD_PLAYER_LIVE_USER_CARRIER;
        set_packed_position(&dispatch_state.players[0], 0xB9u);
        check(dd_gameplay_step(&pack, &dispatch_state, DD_INPUT_B) &&
              dd_gameplay_step(&pack, &dispatch_state, 0u) &&
              dispatch_state.dunk_active == 0u,
              "packed cell `$B9` beside the user dunk lane remains an ordinary shot");
        dispatch_state = dispatch_base;
        dispatch_state.phase = DD_GAMEPLAY_LIVE;
        dispatch_state.carrier = 5u;
        dispatch_state.ball.owner = 5u;
        dispatch_state.ball.action = DD_BALL_DRIBBLE;
        dispatch_state.players[5].action = DD_PLAYER_LIVE_CARRIER_ROUTE;
        set_packed_position(&dispatch_state.players[5], 0xA6u);
        run_cpu_dispatch(&pack, &dispatch_state, 5u);
        for (player = 0u; player < 48u && dispatch_state.ball.action == DD_BALL_SHOT_GATHER;
             ++player) {
            check(dd_gameplay_step(&pack, &dispatch_state, 0u),
                  "advance adjacent-cell CPU shot through `$B189`");
        }
        check(dispatch_state.dunk_active == 0u,
              "packed cell `$A6` beside the CPU dunk lane remains an ordinary shot");
    }

    /* `$A504` keeps the takeoff vector while state `$03` is airborne. Start
       in rejected cell `$B9`, carry right into `$BA`, and release there: this
       is the natural-play ordering that press-time eligibility could not do. */
    dispatch_state = dispatch_base;
    dispatch_state.phase = DD_GAMEPLAY_LIVE;
    dispatch_state.controlled_player = 0u;
    dispatch_state.carrier = 0u;
    dispatch_state.ball.owner = 0u;
    dispatch_state.ball.action = DD_BALL_DRIBBLE;
    dispatch_state.possession_direction = 1u;
    dispatch_state.players[0].action = DD_PLAYER_LIVE_USER_CARRIER;
    set_packed_position(&dispatch_state.players[0], 0xB9u);
    for (player = 1u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
        dispatch_state.players[player].action = DD_PLAYER_ROUTE_WAIT;
    }
    check(dd_gameplay_step(&pack, &dispatch_state, DD_INPUT_RIGHT | DD_INPUT_B),
          "begin running dunk outside the eligible packed lane");
    for (player = 0u; player < 8u; ++player) {
        check(dd_gameplay_step(&pack, &dispatch_state, DD_INPUT_B),
              "hold running dunk while takeoff momentum enters the lane");
    }
    check(dd_gameplay_step(&pack, &dispatch_state, 0u),
          "release running dunk after crossing into the eligible lane");
    check(dispatch_state.dunk_active != 0u &&
          dispatch_state.ball.action == DD_BALL_SHOT_GATHER,
          "`$A504->$B189` activates a natural running dunk from the release-time cell");

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
          period_state.game_set_age == 0u && !period_state.return_to_title &&
          period_state.gameplay_level == 1u &&
          period_state.possession_contact_limit == 0x10u &&
          period_state.paired_tracking_limit == 0x31u,
          "`$9408` advances mutable LEVEL and `$9419/$9425` before GAME SET");
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
