#include "dd_gameplay.h"

#include <limits.h>
#include <string.h>

#define DD_FORMATION_VISIBLE_FRAME 144u
#define DD_TOSS_START_FRAME 270u
#define DD_JUMPER_START_FRAME 302u
#define DD_AWARD_FRAME 330u
#define DD_LIVE_FRAME 356u
#define DD_NO_OWNER 0xFFu

static const uint16_t DD_INITIAL_X[DD_GAMEPLAY_PLAYER_COUNT] = {
    0x00F6u, 0x00F6u, 0x00F6u, 0x0110u, 0x0110u,
    0x0108u, 0x00F0u, 0x00E0u, 0x0120u, 0x0120u
};
static const uint8_t DD_INITIAL_DEPTH[DD_GAMEPLAY_PLAYER_COUNT] = {
    0x5Au, 0x20u, 0x80u, 0x20u, 0x80u, 0x5Au, 0x30u, 0x70u, 0x30u, 0x70u
};
static const uint8_t DD_INITIAL_FACING[DD_GAMEPLAY_PLAYER_COUNT] = {
    0u, 6u, 2u, 6u, 3u, 4u, 7u, 1u, 5u, 3u
};
static const uint8_t DD_INITIAL_ANIMATION[DD_GAMEPLAY_PLAYER_COUNT] = {
    0x1Cu, 0x03u, 0x06u, 0x03u, 0x0Cu, 0x16u, 0x0Fu, 0x12u, 0x09u, 0x0Cu
};
static const uint8_t DD_INITIAL_ATTRIBUTES[DD_GAMEPLAY_PLAYER_COUNT] = {
    0x01u, 0x00u, 0x01u, 0x00u, 0x01u, 0x03u, 0x02u, 0x03u, 0x03u, 0x02u
};
static const uint8_t DD_LIVE_ACTION[DD_GAMEPLAY_PLAYER_COUNT] = {
    DD_PLAYER_LIVE_USER,
    DD_PLAYER_LIVE_TEAMMATE, DD_PLAYER_LIVE_TEAMMATE,
    DD_PLAYER_LIVE_TEAMMATE, DD_PLAYER_LIVE_TEAMMATE,
    DD_PLAYER_LIVE_CARRIER,
    DD_PLAYER_LIVE_CPU, DD_PLAYER_LIVE_CPU,
    DD_PLAYER_LIVE_CPU_CUT, DD_PLAYER_LIVE_CPU_ROUTE
};
static const uint8_t DD_LIVE_ANIMATION[DD_GAMEPLAY_PLAYER_COUNT] = {
    0x1Cu, 0x04u, 0x07u, 0x04u, 0x0Du, 0x21u, 0x10u, 0x13u, 0x1Eu, 0x18u
};
static const uint16_t DD_LIVE_TARGET_X[DD_GAMEPLAY_PLAYER_COUNT] = {
    0x00D8u, 0x00C0u, 0x0108u, 0x00E8u, 0x0118u,
    0x0080u, 0x00A0u, 0x00C8u, 0x00F0u, 0x0118u
};
static const uint8_t DD_LIVE_TARGET_DEPTH[DD_GAMEPLAY_PLAYER_COUNT] = {
    0x58u, 0x34u, 0x78u, 0x44u, 0x68u, 0x3Cu, 0x2Cu, 0x6Cu, 0x3Cu, 0x74u
};

static uint8_t dd_animation_for_facing(uint8_t facing, uint32_t phase) {
    static const uint8_t base[8] = {0x1Bu, 0x12u, 0x06u, 0x0Cu, 0x15u, 0x09u, 0x03u, 0x0Fu};
    uint32_t count = facing == 0u || facing == 4u ? 6u : 3u;
    return (uint8_t)(base[facing & 7u] + (phase % count));
}

static uint8_t dd_facing_from_velocity(int32_t x, int32_t depth, uint8_t fallback) {
    if (x == 0 && depth == 0) return fallback;
    if (x > 0) {
        if (depth > 0) return 7u;
        if (depth < 0) return 1u;
        return 0u;
    }
    if (x < 0) {
        if (depth > 0) return 5u;
        if (depth < 0) return 3u;
        return 4u;
    }
    return depth > 0 ? 6u : 2u;
}

static int32_t dd_approach(int32_t value, int32_t target, int32_t speed) {
    if (value < target) return value + speed > target ? target : value + speed;
    if (value > target) return value - speed < target ? target : value - speed;
    return value;
}

static int32_t dd_clamp(int32_t value, int32_t minimum, int32_t maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static int32_t dd_jump_height(const DDTipoffAssetsHeader *assets, uint32_t scene_frame) {
    int32_t height = 0x1000;
    uint32_t step;
    if (scene_frame < DD_JUMPER_START_FRAME) return height;
    if (scene_frame <= DD_AWARD_FRAME) {
        uint32_t steps = (scene_frame - DD_JUMPER_START_FRAME) / 2u;
        if (steps > 12u) steps = 12u;
        for (step = 0u; step < steps; ++step) height += (int32_t)assets->height_scripts[11u + step] * 256;
        return height;
    }
    height = 0x2600;
    {
        uint32_t steps = (scene_frame - DD_AWARD_FRAME + 1u) / 2u;
        if (steps > 12u) steps = 12u;
        for (step = 0u; step < steps; ++step) height -= (int32_t)assets->height_scripts[22u - step] * 256;
    }
    return height < 0x1000 ? 0x1000 : height;
}

static void dd_attach_ball(const DDTipoffAssetsHeader *assets, DDGameplayState *state, uint32_t table) {
    const DDPlayerState *owner = &state->players[state->carrier];
    uint32_t offset = table * 16u + (uint32_t)(owner->facing & 7u) * 2u;
    state->ball.court_x = owner->court_x + (int32_t)assets->held_ball_offsets[offset] * 256;
    state->ball.court_depth = owner->court_depth + (int32_t)assets->held_ball_offsets[offset + 1u] * 256;
}

static void dd_begin_live(DDGameplayState *state) {
    uint32_t player;
    state->phase = DD_GAMEPLAY_LIVE;
    state->live_frame = 0u;
    state->carrier = 5u;
    state->controlled_player = 0u;
    state->ball.owner = state->carrier;
    state->ball.action = DD_BALL_DRIBBLE;
    state->ball.height = 0x10C0;
    state->controlled_flash_palette = 1u;
    for (player = 0u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
        state->players[player].action = DD_LIVE_ACTION[player];
        state->players[player].height = 0x1000;
        state->players[player].animation = DD_LIVE_ANIMATION[player];
    }
    /* These two weak-side players begin moving during the handoff frame. */
    state->players[8].court_x = 0x0121E2;
    state->players[8].court_depth = 0x0030AC;
    state->players[9].court_x = 0x011E04;
    state->players[9].court_depth = 0x007032;
}

void dd_gameplay_reset(DDGameplayState *state) {
    uint32_t player;
    if (state == NULL) return;
    memset(state, 0, sizeof(*state));
    state->scene_frame = DD_FORMATION_VISIBLE_FRAME - 1u;
    state->phase = DD_GAMEPLAY_FORMATION;
    state->camera_x = 0x7F00;
    state->controlled_player = 0u;
    state->carrier = DD_NO_OWNER;
    state->controlled_flash_palette = 1u;
    state->ball.court_x = 0x00FF00;
    state->ball.court_depth = 0x5A00;
    state->ball.height = 0x1800;
    state->ball.animation = 1u;
    state->ball.owner = DD_NO_OWNER;
    for (player = 0u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
        DDPlayerState *object = &state->players[player];
        object->court_x = (int32_t)DD_INITIAL_X[player] << 8;
        object->court_depth = (int32_t)DD_INITIAL_DEPTH[player] << 8;
        object->height = 0x1000;
        object->facing = DD_INITIAL_FACING[player];
        object->animation = DD_INITIAL_ANIMATION[player];
        object->attributes = DD_INITIAL_ATTRIBUTES[player];
        object->action = player == 0u ? DD_PLAYER_FORMATION_USER :
            (player == 5u ? DD_PLAYER_TIP_CPU :
             (player < 5u ? DD_PLAYER_FORMATION_TEAMMATE : DD_PLAYER_FORMATION_CPU));
    }
    state->initialized = 1;
}

static void dd_step_live(const DDTipoffAssetsHeader *assets, DDGameplayState *state,
                         uint32_t input_mask) {
    DDPlayerState *controlled = &state->players[state->controlled_player];
    uint32_t player;
    uint32_t live_frame = state->live_frame + 1u;
    int32_t input_x = 0;
    int32_t input_depth = 0;
    if ((input_mask & DD_INPUT_LEFT) != 0u) input_x -= 0x0140;
    if ((input_mask & DD_INPUT_RIGHT) != 0u) input_x += 0x0140;
    if ((input_mask & DD_INPUT_UP) != 0u) input_depth += 0x0140;
    if ((input_mask & DD_INPUT_DOWN) != 0u) input_depth -= 0x0140;
    controlled->velocity_x = input_x;
    controlled->velocity_depth = input_depth;
    controlled->court_x = dd_clamp(controlled->court_x + input_x, 0x001000, 0x01F000);
    controlled->court_depth = dd_clamp(controlled->court_depth + input_depth, 0x0400, 0x9800);
    controlled->facing = dd_facing_from_velocity(input_x, input_depth, controlled->facing);
    if (input_x != 0 || input_depth != 0) {
        controlled->animation = dd_animation_for_facing(controlled->facing, live_frame / 3u);
    } else {
        controlled->animation = (uint8_t)(0x1Bu + ((live_frame + 3u) / 8u) % 6u);
    }
    for (player = 1u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
        DDPlayerState *object = &state->players[player];
        int32_t target_x = (int32_t)DD_LIVE_TARGET_X[player] << 8;
        int32_t target_depth = (int32_t)DD_LIVE_TARGET_DEPTH[player] << 8;
        int32_t old_x = object->court_x;
        int32_t old_depth = object->court_depth;
        int32_t speed = player == state->carrier ? 0x0130 : 0x00B0;
        if (player == state->carrier) {
            target_x = 0x004800;
            target_depth = 0x3800;
        }
        object->court_x = dd_approach(object->court_x, target_x, speed);
        object->court_depth = dd_approach(object->court_depth, target_depth, speed);
        object->velocity_x = object->court_x - old_x;
        object->velocity_depth = object->court_depth - old_depth;
        object->facing = dd_facing_from_velocity(object->velocity_x, object->velocity_depth, object->facing);
        if (object->velocity_x != 0 || object->velocity_depth != 0) {
            object->animation = dd_animation_for_facing(object->facing, live_frame / 3u + player);
        }
    }
    dd_attach_ball(assets, state, 0u);
    {
        uint32_t phase = live_frame % 18u;
        int32_t height = 0x10C0;
        uint32_t index;
        if (phase >= 6u && phase <= 9u) {
            for (index = 5u; index < phase; ++index) {
                height += (int32_t)assets->height_scripts[index] * 256;
            }
        } else if (phase >= 10u && phase <= 13u) {
            for (index = 5u; index <= 8u; ++index) {
                height += (int32_t)assets->height_scripts[index] * 256;
            }
            for (index = 0u; index < phase - 9u; ++index) {
                height -= (int32_t)assets->height_scripts[8u - index] * 256;
            }
        }
        state->ball.height = height;
    }
    state->camera_x = dd_clamp(state->players[state->carrier].court_x - 0x8000, 0, 0x010000);
    state->controlled_flash_palette = (uint8_t)(((live_frame / 2u) & 1u) == 0u);
    controlled->attributes = state->controlled_flash_palette;
    state->live_frame = live_frame;
}

int dd_gameplay_step(const DDAssetPack *pack, DDGameplayState *state, uint32_t input_mask) {
    const DDTipoffAssetsHeader *assets;
    uint32_t age;
    if (pack == NULL || state == NULL || pack->tipoff_assets == NULL ||
        pack->tipoff_assets_size < sizeof(DDTipoffAssetsHeader)) return 0;
    if (!state->initialized) dd_gameplay_reset(state);
    assets = (const DDTipoffAssetsHeader *)pack->tipoff_assets;
    if (state->scene_frame == UINT_MAX) return 0;
    ++state->scene_frame;
    if (state->scene_frame < DD_TOSS_START_FRAME) return 1;
    if (state->scene_frame == DD_TOSS_START_FRAME) {
        state->phase = DD_GAMEPLAY_TOSS;
        state->ball.action = DD_BALL_AIRBORNE;
        state->ball.owner = DD_NO_OWNER;
    }
    if (state->scene_frame <= DD_AWARD_FRAME) {
        age = state->scene_frame - DD_TOSS_START_FRAME;
        state->ball.height = 0x1800;
        if (age != 0u) {
            uint32_t tick;
            for (tick = 1u; tick <= age; ++tick) {
                state->ball.height += 0x0305 - (int32_t)((tick * 64u) / 3u);
            }
        }
        state->players[5].height = dd_jump_height(assets, state->scene_frame);
        if (state->scene_frame >= DD_JUMPER_START_FRAME) {
            state->players[5].animation = 0x21u;
            state->players[5].action = DD_PLAYER_TIP_CPU_AIRBORNE;
        }
    }
    if (state->scene_frame == DD_AWARD_FRAME) {
        state->phase = DD_GAMEPLAY_AWARD;
        state->carrier = 5u;
        state->ball.owner = state->carrier;
        state->ball.action = DD_BALL_AWARDED;
    }
    if (state->scene_frame > DD_AWARD_FRAME && state->scene_frame < DD_LIVE_FRAME) {
        state->players[5].height = dd_jump_height(assets, state->scene_frame);
        dd_attach_ball(assets, state, 0u);
        state->ball.height = state->players[5].height + 0x1800;
    }
    if (state->scene_frame == DD_LIVE_FRAME) {
        dd_begin_live(state);
    } else if (state->phase == DD_GAMEPLAY_LIVE) {
        dd_step_live(assets, state, input_mask);
    }
    return 1;
}

int dd_gameplay_advance_to(const DDAssetPack *pack, DDGameplayState *state,
                           uint32_t scene_frame, uint32_t input_mask) {
    if (state == NULL || scene_frame < DD_FORMATION_VISIBLE_FRAME) return 0;
    if (!state->initialized || state->scene_frame > scene_frame) dd_gameplay_reset(state);
    while (state->scene_frame < scene_frame) {
        if (!dd_gameplay_step(pack, state, input_mask)) return 0;
    }
    return 1;
}
