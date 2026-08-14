#include "dd_gameplay.h"

#include <limits.h>
#include <string.h>

#define DD_FORMATION_VISIBLE_FRAME 144u
#define DD_TOSS_START_FRAME 270u
#define DD_JUMPER_START_FRAME 302u
#define DD_AWARD_FRAME 330u
#define DD_LIVE_FRAME 356u
#define DD_USER_JUMP_WIN_FRAME 301u
#define DD_USER_AWARD_FRAME 331u
#define DD_USER_LIVE_FRAME 355u
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
static const uint8_t DD_LIVE_INITIAL_TARGET[DD_GAMEPLAY_PLAYER_COUNT] = {
    0xBBu, 0xCAu, 0x2Du, 0x55u, 0xD9u,
    0xD4u, 0x54u, 0xD7u, 0x96u, 0xECu
};
static const uint8_t DD_INBOUND_TARGET[DD_GAMEPLAY_PLAYER_COUNT] = {
    0x4Fu, 0x48u, 0xCCu, 0xB5u, 0x79u,
    0x6Fu, 0x54u, 0xD7u, 0xA9u, 0x44u
};
/* Original frame 3572, immediately after the inbound pass reaches object $08.
   Object slots $02-$0B map directly to native players 0-9. */
static const uint8_t DD_POST_INBOUND_ACTION[DD_GAMEPLAY_PLAYER_COUNT] = {
    0x0Fu, 0x20u, 0x20u, 0x22u, 0x20u, 0x40u, 0x25u, 0x37u, 0x3Cu, 0x3Eu
};
static const uint8_t DD_POST_INBOUND_TARGET[DD_GAMEPLAY_PLAYER_COUNT] = {
    0xE8u, 0x48u, 0xCCu, 0xB5u, 0x79u, 0x21u, 0xA6u, 0xD7u, 0xA9u, 0x8Cu
};
static const int32_t DD_POST_INBOUND_X[DD_GAMEPLAY_PLAYER_COUNT] = {
    0x008200, 0x006F00, 0x00AA00, 0x015700, 0x017900,
    0x001800, 0x006A00, 0x017900, 0x009600, 0x004B00
};
static const int32_t DD_POST_INBOUND_DEPTH[DD_GAMEPLAY_PLAYER_COUNT] = {
    0x007700, 0x002F00, 0x005D00, 0x006300, 0x003F00,
    0x009800, 0x005800, 0x006800, 0x005800, 0x002800
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

static int32_t dd_absolute(int32_t value) {
    return value < 0 ? -value : value;
}

/* $ABAB expands the original packed court target into native world axes. */
static void dd_unpack_cpu_target(uint8_t packed, int32_t *x, int32_t *depth) {
    *x = (int32_t)((((uint32_t)packed & 0x1Fu) << 4u) + 8u) << 8;
    *depth = (int32_t)((((uint32_t)packed >> 1u) & 0x70u) + 8u) << 8;
}

/* $AB72 compresses the two court axes for region and occupancy decisions. */
static uint8_t dd_pack_cpu_position(const DDPlayerState *player) {
    uint32_t x = (uint32_t)dd_clamp(player->court_x >> 8, 0, 0x1FF);
    uint32_t depth = (uint32_t)dd_clamp(player->court_depth >> 8, 0, 0x7F);
    return (uint8_t)(((depth << 1u) & 0xE0u) | ((x >> 4u) & 0x1Fu));
}

/* Bank 0 $AC2A divides the packed court into seven decision regions. */
static uint8_t dd_cpu_region(uint8_t packed) {
    uint8_t column = (uint8_t)(packed & 0x1Fu);
    uint8_t region;
    if (column >= 0x10u) return 0u;
    region = column >= 9u ? 3u : 6u;
    if (packed >= 0x60u) --region;
    if (packed >= 0xA0u) --region;
    return region;
}

static void dd_set_cpu_target(DDPlayerState *player, uint8_t packed) {
    player->target_zone = packed;
    dd_unpack_cpu_target(packed, &player->target_x, &player->target_depth);
}

static int dd_cpu_at_target(const DDPlayerState *player, int32_t tolerance) {
    return dd_absolute(player->target_x - player->court_x) <= tolerance &&
           dd_absolute(player->target_depth - player->court_depth) <= tolerance;
}

static int dd_cpu_target_occupied(const DDGameplayState *state, uint32_t player,
                                  uint8_t target) {
    uint32_t other;
    for (other = 0u; other < DD_GAMEPLAY_PLAYER_COUNT; ++other) {
        if (other != player && state->players[other].role != 2u &&
            (state->players[other].target_zone == target ||
             dd_pack_cpu_position(&state->players[other]) == target)) return 1;
    }
    return 0;
}

static void dd_move_cpu_player(DDPlayerState *player, uint32_t player_index,
                               uint32_t live_frame, int32_t speed) {
    int32_t old_x = player->court_x;
    int32_t old_depth = player->court_depth;
    player->court_x = dd_approach(player->court_x, player->target_x, speed);
    player->court_depth = dd_approach(player->court_depth, player->target_depth, speed);
    player->velocity_x = player->court_x - old_x;
    player->velocity_depth = player->court_depth - old_depth;
    player->facing = dd_facing_from_velocity(player->velocity_x, player->velocity_depth,
                                             player->facing);
    if (player->velocity_x != 0 || player->velocity_depth != 0) {
        player->animation = dd_animation_for_facing(player->facing,
                                                     live_frame / 3u + player_index);
    }
}

static void dd_choose_spacing_target(const DDTipoffAssetsHeader *assets,
                                     DDGameplayState *state, uint32_t player_index) {
    DDPlayerState *player = &state->players[player_index];
    uint8_t region = dd_cpu_region(dd_pack_cpu_position(player));
    uint8_t phase = (uint8_t)((state->cpu_global_frame >> 1u) & 1u);
    uint8_t target = assets->cpu_spacing_targets[(uint32_t)region * 2u + phase];
    if (dd_cpu_target_occupied(state, player_index, target)) {
        target = assets->cpu_spacing_targets[(uint32_t)region * 2u + (phase ^ 1u)];
    }
    dd_set_cpu_target(player, target);
}

static void dd_claim_loose_ball(DDGameplayState *state, uint32_t player_index) {
    DDPlayerState *player = &state->players[player_index];
    state->carrier = (uint8_t)player_index;
    state->ball.owner = (uint8_t)player_index;
    state->ball.receiver = DD_NO_OWNER;
    state->ball.action = DD_BALL_DRIBBLE;
    state->ball.action_age = 0u;
    state->ball.velocity_x = 0;
    state->ball.velocity_depth = 0;
    state->ball.velocity_height = 0;
    player->action = player_index == state->controlled_player
        ? DD_PLAYER_LIVE_USER_CARRIER : DD_PLAYER_LIVE_CARRIER;
    player->action_age = 0u;
    ++state->possession_count;
}

static int32_t dd_scripted_jump_height(const DDTipoffAssetsHeader *assets,
                                       uint32_t scene_frame, uint32_t start_frame) {
    int32_t height = 0x1000;
    uint32_t step;
    if (scene_frame < start_frame) return height;
    if (scene_frame <= start_frame + 28u) {
        uint32_t steps = (scene_frame - start_frame) / 2u;
        if (steps > 12u) steps = 12u;
        for (step = 0u; step < steps; ++step) height += (int32_t)assets->height_scripts[11u + step] * 256;
        return height;
    }
    height = 0x2600;
    {
        uint32_t steps = (scene_frame - (start_frame + 28u) + 1u) / 2u;
        if (steps > 12u) steps = 12u;
        for (step = 0u; step < steps; ++step) height -= (int32_t)assets->height_scripts[22u - step] * 256;
    }
    return height < 0x1000 ? 0x1000 : height;
}

static int32_t dd_jump_height(const DDTipoffAssetsHeader *assets, uint32_t scene_frame) {
    return dd_scripted_jump_height(assets, scene_frame, DD_JUMPER_START_FRAME);
}

static void dd_attach_ball(const DDTipoffAssetsHeader *assets, DDGameplayState *state, uint32_t table) {
    const DDPlayerState *owner = &state->players[state->carrier];
    uint32_t offset = table * 16u + (uint32_t)(owner->facing & 7u) * 2u;
    state->ball.court_x = owner->court_x + (int32_t)assets->held_ball_offsets[offset] * 256;
    state->ball.court_depth = owner->court_depth + (int32_t)assets->held_ball_offsets[offset + 1u] * 256;
}

static uint32_t dd_cpu_pass_receiver(const DDGameplayState *state, uint32_t carrier) {
    uint32_t first = carrier < 5u ? 0u : 5u;
    uint32_t last = first + 5u;
    uint32_t receiver = carrier;
    int32_t best = INT_MAX;
    uint32_t player;
    for (player = first; player < last; ++player) {
        int32_t progress;
        if (player == carrier) continue;
        progress = state->possession_direction == 0u
            ? state->players[player].court_x : 0x020000 - state->players[player].court_x;
        if (progress < best) {
            best = progress;
            receiver = player;
        }
    }
    return receiver;
}

static void dd_begin_pass(DDGameplayState *state, uint32_t carrier, uint32_t receiver) {
    DDBallState *ball = &state->ball;
    if (carrier >= DD_GAMEPLAY_PLAYER_COUNT || receiver >= DD_GAMEPLAY_PLAYER_COUNT ||
        carrier == receiver) return;
    ball->action = DD_BALL_PASS;
    ball->owner = DD_NO_OWNER;
    ball->receiver = (uint8_t)receiver;
    ball->action_age = 0u;
    ball->velocity_x = (state->players[receiver].court_x - ball->court_x) / 19;
    ball->velocity_depth = (state->players[receiver].court_depth - ball->court_depth) / 19;
    ball->velocity_height = 0x0300;
    state->players[carrier].action = DD_PLAYER_LIVE_SHOOTER_RECOVER;
    state->players[carrier].action_age = 0u;
    state->carrier = DD_NO_OWNER;
}

static void dd_begin_shot(DDGameplayState *state, uint32_t shooter) {
    if (shooter >= DD_GAMEPLAY_PLAYER_COUNT) return;
    state->ball.action = DD_BALL_SHOT_GATHER;
    state->ball.owner = (uint8_t)shooter;
    state->ball.receiver = DD_NO_OWNER;
    state->ball.action_age = 0u;
    state->ball.outcome = 1u;
    state->players[shooter].action = DD_PLAYER_LIVE_CARRIER_DECIDE;
    state->players[shooter].action_age = 0u;
}

static void dd_cpu_decide_possession(DDGameplayState *state, uint32_t carrier) {
    int32_t x = state->players[carrier].court_x;
    int in_shooting_region = state->possession_direction == 0u ? x <= 0x7800 : x >= 0x018800;
    if (in_shooting_region) {
        dd_begin_shot(state, carrier);
    } else {
        dd_begin_pass(state, carrier, dd_cpu_pass_receiver(state, carrier));
    }
}

static void dd_update_cpu_player(const DDTipoffAssetsHeader *assets, DDGameplayState *state,
                                 uint32_t player_index, uint32_t live_frame) {
    DDPlayerState *player = &state->players[player_index];
    int32_t speed = 0x0180;
    ++player->cpu_updates;
    if (player->action_age != UINT16_MAX) ++player->action_age;
    switch (player->action) {
        case DD_PLAYER_LIVE_CARRIER:
            speed = 0x0320;
            if (player->route_step == 4u) {
                /* At original frame 3572 state $25 holds the inbound receiver in
                   place; state $27/ball state $04 begin 28 frames later. */
                speed = 0;
                if (player->action_age >= 14u && state->ball.action == DD_BALL_DRIBBLE &&
                    state->carrier == player_index) {
                    player->action = DD_PLAYER_LIVE_CARRIER_DECIDE;
                    player->action_age = 0u;
                    dd_cpu_decide_possession(state, player_index);
                }
            } else if (player->route_step == 0u) {
                dd_set_cpu_target(player, 0x70u);
                player->route_step = 1u;
            } else if (player->route_step == 1u && player->action_age >= 12u &&
                       dd_cpu_at_target(player, speed)) {
                dd_set_cpu_target(player, 0x6Cu);
                player->route_step = 2u;
            } else if (player->route_step == 2u && player->action_age >= 37u &&
                       dd_cpu_at_target(player, speed)) {
                dd_set_cpu_target(player, 0x85u);
                player->route_step = 3u;
                player->action = DD_PLAYER_LIVE_CPU_SETUP;
                player->action_age = 0u;
            }
            break;
        case DD_PLAYER_LIVE_CPU_SETUP:
            speed = 0x0280;
            if (state->cpu_global_frame == 0x80u) {
                player->action = DD_PLAYER_LIVE_CARRIER_ROUTE;
                player->action_age = 0u;
            }
            break;
        case DD_PLAYER_LIVE_CARRIER_ROUTE:
            speed = 0x0280;
            player->action = DD_PLAYER_LIVE_CARRIER_DECIDE;
            player->action_age = 0u;
            dd_cpu_decide_possession(state, player_index);
            break;
        case DD_PLAYER_LIVE_CARRIER_DECIDE:
            speed = 0x0180;
            if (state->ball.action == DD_BALL_DRIBBLE && state->carrier == player_index) {
                dd_cpu_decide_possession(state, player_index);
            }
            break;
        case DD_PLAYER_LIVE_SHOOTER_RECOVER:
        case DD_PLAYER_LIVE_SHOOTER_RESET:
            speed = 0x0180;
            if (player->action_age >= 32u) {
                player->action = DD_PLAYER_LIVE_CPU;
                player->action_age = 0u;
            }
            break;
        case DD_PLAYER_TIP_CPU:
        case DD_PLAYER_TIP_CPU_AIRBORNE:
            /* $8DD2/$8DF7 are tip-off animation/height states. The formation
               phase advances their traced script; live dispatch keeps them
               stable if a handoff frame still exposes either value. */
            speed = 0;
            player->velocity_x = 0;
            player->velocity_depth = 0;
            break;
        case DD_PLAYER_LIVE_TEAMMATE: {
            uint32_t opponent = player_index < 5u ? player_index + 5u : player_index - 5u;
            int32_t basket_side = player_index < 5u ? 0x1400 : -0x1400;
            speed = 0x0200;
            player->target_x = dd_clamp(state->players[opponent].court_x + basket_side,
                                        0x001000, 0x01F000);
            player->target_depth = dd_clamp(state->players[opponent].court_depth,
                                            0x0400, 0x9800);
            player->target_zone = dd_pack_cpu_position(player);
            break;
        }
        case DD_PLAYER_LIVE_FOLLOW_TARGET:
            /* $8A3A: fixed-target mover $D978, then return to state $20. */
            speed = 0x0200;
            if (dd_cpu_at_target(player, speed)) {
                player->action = DD_PLAYER_LIVE_TEAMMATE;
                player->action_age = 0u;
            }
            break;
        case DD_PLAYER_LIVE_PAIRED_DEFENDER: {
            uint32_t opponent = player_index < 5u ? player_index + 5u : player_index - 5u;
            speed = 0;
            player->target_x = player->court_x;
            player->target_depth = player->court_depth;
            player->velocity_x = 0;
            player->velocity_depth = 0;
            player->facing = state->players[opponent].facing;
            player->animation = state->players[opponent].animation;
            break;
        }
        case DD_PLAYER_JUMP_START:
            /* $8AF4 installs the jump height script and immediately advances
               the object dispatcher to state $24. */
            speed = 0;
            player->height = 0x1000;
            player->velocity_height = 0x0380;
            player->action = DD_PLAYER_JUMP_CONTEST;
            player->action_age = 0u;
            break;
        case DD_PLAYER_JUMP_CONTEST:
            /* $8B12 runs the height script, tests loose-ball contact, and
               chooses possession/recovery when the player lands. */
            speed = 0;
            if (state->ball.owner == DD_NO_OWNER &&
                dd_absolute(state->ball.court_x - player->court_x) <= 0x0C00 &&
                dd_absolute(state->ball.court_depth - player->court_depth) <= 0x0C00 &&
                dd_absolute(state->ball.height - player->height) <= 0x1800) {
                dd_claim_loose_ball(state, player_index);
            }
            player->height += player->velocity_height;
            player->velocity_height -= 0x0070;
            if (player->action_age >= 2u && player->height <= 0x1000) {
                player->height = 0x1000;
                player->velocity_height = 0;
                if (state->carrier == player_index) {
                    player->action = player_index == state->controlled_player
                        ? DD_PLAYER_LIVE_USER_CARRIER : DD_PLAYER_LIVE_CARRIER;
                } else {
                    player->action = DD_PLAYER_LIVE_SHOOTER_RECOVER;
                }
                player->action_age = 0u;
            }
            break;
        case DD_PLAYER_LIVE_CPU:
            speed = 0x0500;
            if ((state->cpu_global_frame & 0x70u) == 0u || dd_cpu_at_target(player, speed)) {
                uint32_t target_index = player->role + (player_index >= 5u ? 5u : 0u) +
                    ((state->cpu_global_frame & 0x80u) != 0u ? 10u : 0u);
                dd_set_cpu_target(player, assets->cpu_role_targets[target_index]);
            }
            break;
        case DD_PLAYER_LIVE_CPU_CUT:
            speed = 0x0230;
            if (player->action_age >= 35u && dd_cpu_at_target(player, speed)) {
                uint8_t target = assets->cpu_spacing_targets[13u];
                if (dd_cpu_target_occupied(state, player_index, target)) {
                    target = assets->cpu_spacing_targets[12u];
                }
                dd_set_cpu_target(player, target);
                player->action = DD_PLAYER_LIVE_CPU_CUT_RUN;
                player->action_age = 0u;
            }
            break;
        case DD_PLAYER_LIVE_CPU_ROUTE:
            speed = 0x0220;
            if (dd_cpu_at_target(player, speed)) {
                uint8_t region = dd_cpu_region(dd_pack_cpu_position(player));
                uint8_t phase = (uint8_t)((state->cpu_global_frame >> 1u) & 1u);
                uint8_t target = assets->cpu_spacing_targets[(uint32_t)region * 2u + phase];
                if (dd_cpu_target_occupied(state, player_index, target)) {
                    target = assets->cpu_spacing_targets[(uint32_t)region * 2u + (phase ^ 1u)];
                }
                dd_set_cpu_target(player, target);
            }
            break;
        case DD_PLAYER_LIVE_CPU_CUT_RUN:
            speed = 0x0230;
            break;
        case DD_PLAYER_LIVE_CONTINUE:
        case DD_PLAYER_LIVE_CONTINUE_33:
        case DD_PLAYER_LIVE_CONTINUE_34:
            /* $8BC5 is the shared $D98A movement/animation continuation used
               by dispatcher states $2C, $33, and $34. */
            speed = 0x0180;
            break;
        case DD_PLAYER_REBOUND_CHASE:
            /* $8E71 reaches the selected rebound point before state $2E. */
            speed = 0x0280;
            if (dd_cpu_at_target(player, speed)) {
                player->action = DD_PLAYER_REBOUND_CLAIM;
                player->action_age = 0u;
            }
            break;
        case DD_PLAYER_REBOUND_CLAIM:
            /* $8E88 excludes ball states $05/$06, claims the loose ball, then
               installs packed return target $BD/$A1 and state $2F. */
            speed = 0;
            if (state->ball.action != DD_BALL_AIRBORNE &&
                state->ball.action != DD_BALL_SCORE) {
                dd_claim_loose_ball(state, player_index);
                dd_set_cpu_target(player, state->possession_direction == 0u ? 0xBDu : 0xA1u);
                player->action = DD_PLAYER_REBOUND_RETURN;
                player->action_age = 0u;
            }
            break;
        case DD_PLAYER_REBOUND_RETURN:
            /* $8EBF moves to the return point and enters hold state $30. */
            speed = 0x0280;
            if (dd_cpu_at_target(player, speed)) {
                player->action = DD_PLAYER_INBOUND_HOLD;
                player->action_age = 0u;
            }
            break;
        case DD_PLAYER_INBOUND_HOLD:
        case DD_PLAYER_INBOUND_READY:
            /* $8EE2/$8FE0 are stationary held/ready continuations. */
            speed = 0;
            player->velocity_x = 0;
            player->velocity_depth = 0;
            break;
        case DD_PLAYER_FORMATION_CPU:
            speed = 0x0200;
            break;
        case DD_PLAYER_INBOUND_FORMATION:
            /* Bank 0 $904D calls fixed target mover $D978 and advances to
               stationary state $37 only after the packed target is reached. */
            speed = 0x0200;
            if (dd_cpu_at_target(player, speed)) {
                player->action = DD_PLAYER_LIVE_SET;
                player->action_age = 0u;
            }
            break;
        case DD_PLAYER_LIVE_SET:
            speed = 0;
            player->target_x = player->court_x;
            player->target_depth = player->court_depth;
            player->velocity_x = 0;
            player->velocity_depth = 0;
            break;
        case DD_PLAYER_ROUTE_INIT:
            /* $8195 changes to $39, calls route selector $8468, and resumes
               normal movement through $D98D. */
            dd_choose_spacing_target(assets, state, player_index);
            player->action = DD_PLAYER_ROUTE_APPROACH;
            player->action_age = 0u;
            speed = 0x0220;
            break;
        case DD_PLAYER_ROUTE_APPROACH:
            /* $81A2 routes role zero to $32; arrivals feed the $3C/$3E/$38
               spacing family according to the current court region. */
            speed = 0x0220;
            if (player->role == 0u) {
                player->action = DD_PLAYER_LIVE_CPU_SETUP;
                player->action_age = 0u;
            } else if (dd_cpu_at_target(player, speed)) {
                uint8_t region = dd_cpu_region(dd_pack_cpu_position(player));
                player->action = region == 0u ? DD_PLAYER_ROUTE_INIT
                    : (player->role == 1u ? DD_PLAYER_LIVE_CPU_CUT : DD_PLAYER_LIVE_CPU_ROUTE);
                player->action_age = 0u;
            }
            break;
        case DD_PLAYER_ROUTE_ADJUST:
            /* $8266 is the shorter companion route: role zero returns to
               $32, otherwise target arrival enters $3C (or $38 in region 0). */
            speed = 0x0220;
            if (player->role == 0u) {
                player->action = DD_PLAYER_LIVE_CPU_SETUP;
                player->action_age = 0u;
            } else if (dd_cpu_at_target(player, speed)) {
                player->action = dd_cpu_region(dd_pack_cpu_position(player)) == 0u
                    ? DD_PLAYER_ROUTE_INIT : DD_PLAYER_LIVE_CPU_CUT;
                player->action_age = 0u;
            }
            break;
        case DD_PLAYER_ROUTE_WAIT:
            /* $8297 is literally RTS: preserve the object unchanged. */
            speed = 0;
            player->velocity_x = 0;
            player->velocity_depth = 0;
            break;
        case DD_PLAYER_LIVE_RENDER_ONLY:
            /* $8460 only calls $B503 and the common animation/render tail. */
            speed = 0;
            player->velocity_x = 0;
            player->velocity_depth = 0;
            break;
        case DD_PLAYER_INBOUNDER:
            /* $8C6B moves to the inbound point, becomes ball owner, then
               advances to hold state $30. The scripted inbound path calls the
               same transition at its traced frame. */
            speed = 0x0200;
            if (dd_cpu_at_target(player, speed)) {
                dd_claim_loose_ball(state, player_index);
                player->action = DD_PLAYER_INBOUND_HOLD;
                player->action_age = 0u;
            }
            break;
        default:
            break;
    }
    dd_move_cpu_player(player, player_index, live_frame, speed);
}

static void dd_begin_live(DDGameplayState *state, uint32_t winner) {
    uint32_t player;
    state->phase = DD_GAMEPLAY_LIVE;
    state->live_frame = 0u;
    state->carrier = (uint8_t)winner;
    state->controlled_player = 0u;
    state->ball.owner = state->carrier;
    state->ball.action = DD_BALL_DRIBBLE;
    state->ball.height = 0x10C0;
    state->controlled_flash_palette = 1u;
    state->cpu_global_frame = 0xDCu;
    state->cpu_priority_player = 8u;
    state->possession_direction = winner < 5u ? 1u : 0u;
    state->possession_count = 0u;
    state->inbound_age = 0u;
    for (player = 0u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
        if (winner == 5u) {
            state->players[player].action = DD_LIVE_ACTION[player];
        } else if (player == winner) {
            state->players[player].action = DD_PLAYER_LIVE_USER_CARRIER;
        } else if (player < 5u) {
            state->players[player].action = DD_PLAYER_LIVE_TEAMMATE;
        } else {
            state->players[player].action = DD_PLAYER_LIVE_CPU;
        }
        state->players[player].height = 0x1000;
        state->players[player].animation = DD_LIVE_ANIMATION[player];
        state->players[player].role = (uint8_t)(player % 5u);
        state->players[player].action_age = 0u;
        state->players[player].cpu_updates = 0u;
        state->players[player].route_step = 0u;
        dd_set_cpu_target(&state->players[player], DD_LIVE_INITIAL_TARGET[player]);
    }
    /* These two weak-side players begin moving during the handoff frame. */
    state->players[8].court_x = 0x0121E2;
    state->players[8].court_depth = 0x0030AC;
    state->players[9].court_x = 0x011E04;
    state->players[9].court_depth = 0x007032;
    /* The original handoff retains the centered $7F camera for this frame. */
    state->camera_x = 0x7F00;
    state->camera_chr_side = 1u;
    state->hud_split_y = 48u;
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
    state->ball.receiver = DD_NO_OWNER;
    state->tip_winner = DD_NO_OWNER;
    state->tip_user_jump_frame = UINT_MAX;
    state->live_start_frame = DD_LIVE_FRAME;
    state->camera_chr_side = 1u;
    state->hud_split_y = 64u;
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

static void dd_update_camera(DDGameplayState *state) {
    int32_t focus_x = state->ball.court_x;
    int32_t camera;
    if (state->carrier < DD_GAMEPLAY_PLAYER_COUNT) {
        focus_x = state->players[state->carrier].court_x;
    }
    camera = dd_clamp(focus_x - 0x8000, 0, 0x010000);
    state->camera_x = camera;
    if ((camera >> 8) < 0x78) {
        state->camera_chr_side = 0u;
    } else if ((camera >> 8) >= 0x88) {
        state->camera_chr_side = 2u;
    }
}

static void dd_step_dribble(const DDTipoffAssetsHeader *assets, DDGameplayState *state) {
    uint32_t phase;
    uint32_t index;
    int32_t height = 0x10C0;
    if (state->carrier >= DD_GAMEPLAY_PLAYER_COUNT) return;
    state->ball.owner = state->carrier;
    dd_attach_ball(assets, state, 0u);
    phase = state->live_frame % 18u;
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

static void dd_finish_ball_reception(const DDTipoffAssetsHeader *assets,
                                     DDGameplayState *state, uint32_t receiver) {
    if (receiver >= DD_GAMEPLAY_PLAYER_COUNT) return;
    state->carrier = (uint8_t)receiver;
    state->ball.owner = (uint8_t)receiver;
    state->ball.receiver = DD_NO_OWNER;
    state->ball.action = DD_BALL_DRIBBLE;
    state->ball.action_age = 0u;
    state->ball.velocity_x = 0;
    state->ball.velocity_depth = 0;
    state->ball.velocity_height = 0;
    state->players[receiver].action = receiver == state->controlled_player
        ? DD_PLAYER_LIVE_USER_CARRIER : DD_PLAYER_LIVE_CARRIER;
    state->players[receiver].action_age = 0u;
    state->players[receiver].route_step = 0u;
    ++state->possession_count;
    dd_step_dribble(assets, state);
}

static void dd_step_ball(const DDTipoffAssetsHeader *assets, DDGameplayState *state) {
    DDBallState *ball = &state->ball;
    if (ball->action_age != UINT16_MAX) ++ball->action_age;
    switch (ball->action) {
        case DD_BALL_AWARDED:
            /* $ACB6 projects the held/awarded ball from its owner. */
            if (ball->owner < DD_GAMEPLAY_PLAYER_COUNT) {
                state->carrier = ball->owner;
                dd_attach_ball(assets, state, 1u);
                ball->height = state->players[ball->owner].height + 0x1800;
            }
            break;
        case DD_BALL_DRIBBLE:
            dd_step_dribble(assets, state);
            break;
        case DD_BALL_PASS:
            ball->court_x += ball->velocity_x;
            ball->court_depth += ball->velocity_depth;
            ball->height += ball->velocity_height;
            ball->velocity_height -= 0x0030;
            if (ball->action_age >= 19u && ball->receiver < DD_GAMEPLAY_PLAYER_COUNT) {
                dd_finish_ball_reception(assets, state, ball->receiver);
            }
            break;
        case DD_BALL_PASS_BOUNCE:
            /* $ADF2 runs rim/contact helper $B473, free-flight integrators
               $9CA0/$9CF6, and the common ball physics tail $B3E9. */
            ball->court_x += ball->velocity_x;
            ball->court_depth = dd_clamp(ball->court_depth + ball->velocity_depth,
                                         0x0400, 0x9800);
            ball->height += ball->velocity_height;
            ball->velocity_height -= 0x0060;
            if (ball->height <= 0x1000) {
                ball->height = 0x1000;
                ball->velocity_height = dd_absolute(ball->velocity_height) / 2;
            }
            if (ball->action_age >= 6u && ball->receiver < DD_GAMEPLAY_PLAYER_COUNT &&
                dd_absolute(state->players[ball->receiver].court_x - ball->court_x) <= 0x1000 &&
                dd_absolute(state->players[ball->receiver].court_depth - ball->court_depth) <= 0x1000) {
                dd_finish_ball_reception(assets, state, ball->receiver);
            }
            break;
        case DD_BALL_SHOT_GATHER:
            if (ball->owner < DD_GAMEPLAY_PLAYER_COUNT) {
                dd_attach_ball(assets, state, 2u);
                ball->height = state->players[ball->owner].height + 0x1200;
            }
            if (ball->action_age > 26u) {
                int32_t hoop_x = state->possession_direction == 0u ? 0x001C00 : 0x01E400;
                ball->action = DD_BALL_AIRBORNE;
                ball->action_age = 0u;
                ball->owner = DD_NO_OWNER;
                ball->velocity_x = (hoop_x - ball->court_x) / 21;
                ball->velocity_depth = (0x005800 - ball->court_depth) / 21;
                ball->velocity_height = 0x0500;
                state->carrier = DD_NO_OWNER;
            }
            break;
        case DD_BALL_AIRBORNE:
            ball->court_x += ball->velocity_x;
            ball->court_depth += ball->velocity_depth;
            ball->height += ball->velocity_height;
            ball->velocity_height -= 0x0080;
            if (ball->action_age >= 21u) {
                ball->action = DD_BALL_SCORE;
                ball->action_age = 0u;
                ball->height = 0x3200;
                state->possession_direction ^= 1u;
            }
            break;
        case DD_BALL_SCORE:
            if (ball->action_age >= 13u) {
                ball->action = DD_BALL_REBOUND;
                ball->action_age = 0u;
                ball->velocity_x = state->possession_direction == 0u ? -0x0100 : 0x0100;
                ball->velocity_depth = -0x0040;
                ball->velocity_height = 0x0200;
            }
            break;
        case DD_BALL_REBOUND:
            ball->court_x += ball->velocity_x;
            ball->court_depth = dd_clamp(ball->court_depth + ball->velocity_depth, 0x0400, 0x9800);
            ball->height += ball->velocity_height;
            ball->velocity_height -= 0x0040;
            if (ball->height < 0x1000) {
                ball->height = 0x1000;
                ball->velocity_height = -ball->velocity_height / 2;
            }
            if (ball->action_age >= 161u) {
                state->carrier = state->controlled_player;
                ball->owner = state->carrier;
                ball->action = DD_BALL_DRIBBLE;
                ball->action_age = 0u;
                state->players[state->carrier].action = DD_PLAYER_LIVE_USER_CARRIER;
                dd_step_dribble(assets, state);
            }
            break;
        case DD_BALL_LOOSE_LAUNCH:
            /* $AF72 clears ownership, seeds height/velocity, plays SFX $14,
               and increments the ball dispatcher to state $09. */
            state->carrier = DD_NO_OWNER;
            ball->owner = DD_NO_OWNER;
            ball->height = 0x3800;
            ball->velocity_x = state->possession_direction == 0u ? -0x0180 : 0x0180;
            ball->velocity_depth = (state->cpu_global_frame & 1u) != 0u ? 0x00C0 : -0x00C0;
            ball->velocity_height = 0x0100;
            ball->action = DD_BALL_LOOSE_AIRBORNE;
            ball->action_age = 0u;
            break;
        case DD_BALL_LOOSE_AIRBORNE:
            /* $AFDD integrates both court axes and height until its threshold,
               then switches to rebound state $07. */
            ball->court_x = dd_clamp(ball->court_x + ball->velocity_x,
                                     0x001000, 0x01F000);
            ball->court_depth = dd_clamp(ball->court_depth + ball->velocity_depth,
                                         0x0400, 0x9800);
            ball->height += ball->velocity_height;
            ball->velocity_height -= 0x0040;
            if (ball->action_age >= 5u && ball->height <= 0x1000) {
                ball->height = 0x1000;
                ball->action = DD_BALL_REBOUND;
                ball->action_age = 0u;
                ball->velocity_height = 0x0200;
            }
            break;
        case DD_BALL_SHOT_LAUNCH: {
            /* $B017 is an initializer, not a wait state: it seeds the launch
               terms and writes ball action $05 before returning. */
            int32_t hoop_x = state->possession_direction == 0u ? 0x001C00 : 0x01E400;
            ball->owner = DD_NO_OWNER;
            state->carrier = DD_NO_OWNER;
            ball->velocity_x = (hoop_x - ball->court_x) / 21;
            ball->velocity_depth = (0x005800 - ball->court_depth) / 21;
            ball->velocity_height = 0x0500;
            ball->action = DD_BALL_AIRBORNE;
            ball->action_age = 0u;
            break;
        }
        case DD_BALL_DEAD:
        case DD_BALL_HIDDEN:
            /* $ACAB is shared by states $0B/$0C: zero height and project the
               non-live object without advancing its dispatcher state. */
            ball->height = 0;
            ball->velocity_x = 0;
            ball->velocity_depth = 0;
            ball->velocity_height = 0;
            break;
        default:
            break;
    }
}

static void dd_restore_post_inbound(DDGameplayState *state) {
    uint32_t player;
    for (player = 0u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
        DDPlayerState *object = &state->players[player];
        object->court_x = DD_POST_INBOUND_X[player];
        object->court_depth = DD_POST_INBOUND_DEPTH[player];
        object->height = 0x1000;
        object->action = DD_POST_INBOUND_ACTION[player];
        object->action_age = 0u;
        object->route_step = player == 6u ? 4u : 0u;
        dd_set_cpu_target(object, DD_POST_INBOUND_TARGET[player]);
    }
    state->carrier = 6u;
    state->ball.owner = 6u;
    state->ball.receiver = DD_NO_OWNER;
    state->ball.action = DD_BALL_DRIBBLE;
    state->ball.action_age = 0u;
    state->possession_direction = 0u;
    state->phase = DD_GAMEPLAY_LIVE;
}

static void dd_begin_inbound(DDGameplayState *state) {
    uint32_t player;
    state->phase = DD_GAMEPLAY_INBOUND;
    state->inbound_age = 0u;
    state->carrier = DD_NO_OWNER;
    state->ball.action = DD_BALL_DEAD;
    state->ball.owner = DD_NO_OWNER;
    state->ball.receiver = DD_NO_OWNER;
    state->ball.action_age = 0u;
    for (player = 0u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
        state->players[player].action = DD_PLAYER_INBOUND_FORMATION;
        state->players[player].action_age = 0u;
        dd_set_cpu_target(&state->players[player], DD_INBOUND_TARGET[player]);
    }
    state->players[5].action = DD_PLAYER_INBOUNDER;
}

static void dd_step_inbound(const DDTipoffAssetsHeader *assets, DDGameplayState *state,
                            uint32_t live_frame) {
    uint32_t player;
    if (state->inbound_age != UINT16_MAX) ++state->inbound_age;
    for (player = 0u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
        dd_move_cpu_player(&state->players[player], player, live_frame, 0x0200);
    }
    if (state->inbound_age == 177u) {
        state->carrier = 5u;
        state->ball.owner = 5u;
        state->ball.action = DD_BALL_DRIBBLE;
        state->ball.action_age = 0u;
        state->players[5].action = DD_PLAYER_INBOUND_HOLD;
        dd_step_dribble(assets, state);
    } else if (state->inbound_age == 221u) {
        state->ball.action = DD_BALL_AWARDED;
        state->ball.action_age = 0u;
        state->players[5].action = DD_PLAYER_INBOUND_READY;
    } else if (state->inbound_age == 229u) {
        state->ball.owner = 5u;
        dd_attach_ball(assets, state, 0u);
        dd_begin_pass(state, 5u, 6u);
        state->players[5].action = DD_PLAYER_INBOUND_READY;
    } else if (state->inbound_age > 229u && state->ball.action == DD_BALL_PASS) {
        dd_step_ball(assets, state);
        if (state->ball.action == DD_BALL_DRIBBLE) {
            dd_restore_post_inbound(state);
            dd_step_dribble(assets, state);
        }
    } else if (state->ball.action == DD_BALL_DRIBBLE) {
        dd_step_dribble(assets, state);
    }
}

static void dd_step_live(const DDTipoffAssetsHeader *assets, DDGameplayState *state,
                         uint32_t input_mask) {
    DDPlayerState *controlled = &state->players[state->controlled_player];
    uint32_t player;
    uint32_t live_frame = state->live_frame + 1u;
    uint32_t cpu_start;
    uint32_t cpu_end;
    int32_t input_x = 0;
    int32_t input_depth = 0;
    if (state->phase == DD_GAMEPLAY_INBOUND) {
        dd_step_inbound(assets, state, live_frame);
        dd_update_camera(state);
        state->controlled_flash_palette = (uint8_t)(((live_frame / 2u) & 1u) == 0u);
        state->players[state->controlled_player].attributes = state->controlled_flash_palette;
        state->live_frame = live_frame;
        state->previous_input = input_mask;
        return;
    }
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
    ++state->cpu_global_frame;
    if ((state->cpu_global_frame & 1u) != 0u) {
        cpu_start = 1u;
        cpu_end = 5u;
    } else {
        state->cpu_priority_player = (uint8_t)(state->cpu_priority_player + 1u);
        if (state->cpu_priority_player >= DD_GAMEPLAY_PLAYER_COUNT) state->cpu_priority_player = 5u;
        cpu_start = 5u;
        cpu_end = DD_GAMEPLAY_PLAYER_COUNT;
    }
    for (player = cpu_start; player < cpu_end; ++player) {
        dd_update_cpu_player(assets, state, player, live_frame);
    }
    if (state->carrier == state->controlled_player && state->ball.action == DD_BALL_DRIBBLE) {
        uint32_t pressed = input_mask & ~state->previous_input;
        if ((pressed & DD_INPUT_B) != 0u) {
            dd_begin_shot(state, state->controlled_player);
        } else if ((pressed & DD_INPUT_A) != 0u) {
            dd_begin_pass(state, state->controlled_player,
                          dd_cpu_pass_receiver(state, state->controlled_player));
        }
    }
    dd_step_ball(assets, state);
    if (live_frame == 447u && state->possession_count == 0u) {
        state->carrier = DD_NO_OWNER;
        state->ball.owner = DD_NO_OWNER;
        state->ball.action = DD_BALL_AWARDED;
        state->ball.action_age = 0u;
    }
    if (live_frame == 767u && state->possession_count == 0u) {
        dd_begin_inbound(state);
    }
    dd_update_camera(state);
    state->controlled_flash_palette = (uint8_t)(((live_frame / 2u) & 1u) == 0u);
    controlled->attributes = state->controlled_flash_palette;
    state->live_frame = live_frame;
    state->previous_input = input_mask;
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
    if (state->scene_frame >= DD_TOSS_START_FRAME && state->scene_frame <= DD_AWARD_FRAME &&
        state->tip_user_jump_frame == UINT_MAX && (input_mask & DD_INPUT_B) != 0u) {
        state->tip_user_jump_frame = state->scene_frame;
        state->players[0].action = DD_PLAYER_TIP_USER_AIRBORNE;
        state->players[0].animation = 0x21u;
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
        if (state->tip_user_jump_frame != UINT_MAX) {
            state->players[0].height = dd_scripted_jump_height(
                assets, state->scene_frame, state->tip_user_jump_frame);
        }
    }
    if (state->scene_frame == DD_AWARD_FRAME) {
        state->hud_split_y = 48u;
        state->phase = DD_GAMEPLAY_AWARD;
        state->carrier = 5u;
        state->tip_winner = 5u;
        state->ball.owner = state->carrier;
        state->ball.action = DD_BALL_AWARDED;
    }
    if (state->scene_frame == DD_USER_AWARD_FRAME &&
        state->tip_user_jump_frame == DD_USER_JUMP_WIN_FRAME) {
        state->carrier = 0u;
        state->tip_winner = 0u;
        state->ball.owner = 0u;
        state->live_start_frame = DD_USER_LIVE_FRAME;
    }
    if (state->scene_frame > DD_AWARD_FRAME && state->scene_frame < state->live_start_frame) {
        if (state->tip_user_jump_frame != UINT_MAX) {
            state->players[0].height = dd_scripted_jump_height(
                assets, state->scene_frame, state->tip_user_jump_frame);
        }
        state->players[5].height = dd_jump_height(assets, state->scene_frame);
        dd_attach_ball(assets, state, 0u);
        state->ball.height = state->players[state->carrier].height + 0x1800;
    }
    if (state->scene_frame == state->live_start_frame) {
        dd_begin_live(state, state->tip_winner);
        state->previous_input = input_mask;
    } else if (state->phase == DD_GAMEPLAY_LIVE || state->phase == DD_GAMEPLAY_INBOUND) {
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
