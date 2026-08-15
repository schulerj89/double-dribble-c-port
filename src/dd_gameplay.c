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
#define DD_FIRST_CLOCK_TICK_FRAME 296u
#define DD_CLOCK_FRAMES_PER_SECOND 32u
#define DD_PERIOD_RESET_DELAY 214u
#define DD_POSSESSION_CONTACT_LIMIT 20u
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

/* Bank 0 $9B42 compares two axis-aligned boxes.  Its carry is clear only
   when both axes overlap; the subtraction makes the upper edge exclusive. */
static int dd_axis_boxes_overlap(int32_t moving, int32_t fixed,
                                 int32_t moving_half, int32_t fixed_half) {
    int32_t extent = moving_half + fixed_half;
    int32_t delta = moving - fixed;
    return delta >= -extent && delta < extent;
}

/* $B138: pass receiver uses an 8x8 player box and a 6x6 ball box in the
   two court axes.  The original checks this before integrating the ball. */
static int dd_pass_receiver_contact(const DDGameplayState *state, uint32_t receiver) {
    const DDPlayerState *player;
    if (receiver >= DD_GAMEPLAY_PLAYER_COUNT) return 0;
    player = &state->players[receiver];
    return dd_axis_boxes_overlap(state->ball.court_depth, player->court_depth,
                                 0x0600, 0x0800) &&
           dd_axis_boxes_overlap(state->ball.court_x, player->court_x,
                                 0x0600, 0x0800);
}

/* $A6C3: jumping players use 4x4 boxes.  Object slots $02-$06 shift six
   court units one way and slots $07-$0B the other; native slots are $00-$09. */
static int dd_jump_ball_contact(const DDGameplayState *state, uint32_t player_index) {
    const DDPlayerState *player;
    int32_t shifted_x;
    if (player_index >= DD_GAMEPLAY_PLAYER_COUNT) return 0;
    player = &state->players[player_index];
    shifted_x = player->court_x + (player_index < 5u ? 0x0600 : -0x0600);
    return dd_axis_boxes_overlap(state->ball.height, player->height + 0x0800,
                                 0x0400, 0x0400) &&
           dd_axis_boxes_overlap(state->ball.court_x, shifted_x,
                                 0x0400, 0x0400);
}

/* $B435: ordinary possession contact uses player half extents 4, ball half
   extents 6, then accepts a 34-unit unsigned window around player height+17. */
static int dd_possession_ball_contact(const DDGameplayState *state, uint32_t player_index) {
    const DDPlayerState *player;
    uint8_t height_delta;
    if (player_index >= DD_GAMEPLAY_PLAYER_COUNT) return 0;
    player = &state->players[player_index];
    if (!dd_axis_boxes_overlap(state->ball.court_depth, player->court_depth,
                               0x0600, 0x0400) ||
        !dd_axis_boxes_overlap(state->ball.court_x, player->court_x,
                               0x0600, 0x0400)) return 0;
    height_delta = (uint8_t)(((player->height + 0x1100 - state->ball.height) >> 8) & 0xFF);
    return height_delta < 0x22u;
}

/* $A347's exceptional branch runs only when clock countdown $0025 is zero,
   the ball is in dribble state $01, and the two players face the same way.
   It preserves the fouled owner in $006A, installs animation $29, requests
   whistle SFX $30/mode $1A, and jumps to $9645's dead-ball setup. */
static void dd_begin_free_throw(DDGameplayState *state, uint32_t shooter,
                                uint32_t offender) {
    uint32_t player;
    state->phase = DD_GAMEPLAY_FREE_THROW;
    state->free_throw_age = 0u;
    state->game_set_age = 0u;
    state->return_to_title = 0;
    state->foul_shooter = (uint8_t)shooter;
    state->foul_offender = (uint8_t)offender;
    state->carrier = DD_NO_OWNER;
    state->ball.action = DD_BALL_DEAD;
    state->ball.receiver = DD_NO_OWNER;
    state->ball.action_age = 0u;
    state->ball.velocity_x = 0;
    state->ball.velocity_depth = 0;
    state->ball.velocity_height = 0;
    state->players[shooter].animation = 0x29u;
    for (player = 0u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
        state->players[player].action = player == shooter
            ? DD_PLAYER_FREE_THROW_SHOOTER : DD_PLAYER_FREE_THROW_FORMATION;
        state->players[player].action_age = 0u;
        state->players[player].contact_age = 0u;
    }
}

/* $9FA3 calls $A347 first.  If that special branch returns, its tail jumps
   to $A44B, which performs the ordinary possession and ten-player reset. */
static void dd_transfer_contact_possession(DDGameplayState *state, uint32_t winner) {
    static const uint8_t route_actions[4] = {
        DD_PLAYER_LIVE_CPU, DD_PLAYER_LIVE_CPU,
        DD_PLAYER_LIVE_CPU_CUT, DD_PLAYER_LIVE_CPU_ROUTE
    };
    uint32_t first = winner < 5u ? 0u : 5u;
    uint32_t route = 0u;
    uint32_t player;
    state->carrier = (uint8_t)winner;
    state->ball.owner = (uint8_t)winner;
    state->ball.receiver = DD_NO_OWNER;
    state->ball.action = DD_BALL_DRIBBLE;
    state->ball.action_age = 0u;
    state->ball.velocity_x = 0;
    state->ball.velocity_depth = 0;
    state->ball.velocity_height = 0;
    state->possession_direction = winner < 5u ? 1u : 0u;
    if (winner < 5u) state->controlled_player = (uint8_t)winner;
    ++state->possession_count;
    for (player = 0u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
        DDPlayerState *object = &state->players[player];
        object->action_age = 0u;
        object->contact_age = 0u;
        object->route_step = 0u;
        if (player == winner) {
            object->action = winner < 5u
                ? DD_PLAYER_LIVE_USER_CARRIER : DD_PLAYER_LIVE_CARRIER;
        } else if (player >= first && player < first + 5u) {
            object->action = route_actions[route++];
        } else {
            object->action = player == state->controlled_player
                ? DD_PLAYER_LIVE_USER : DD_PLAYER_LIVE_TEAMMATE;
        }
    }
}

static int dd_step_possession_contact(DDGameplayState *state, uint32_t player_index) {
    DDPlayerState *player = &state->players[player_index];
    uint32_t owner = state->ball.owner;
    if (state->ball.action != DD_BALL_DRIBBLE || owner >= DD_GAMEPLAY_PLAYER_COUNT ||
        owner == player_index || (owner < 5u) == (player_index < 5u) ||
        !dd_possession_ball_contact(state, player_index)) {
        player->contact_age = 0u;
        return 0;
    }
    if (player->contact_age != UINT8_MAX) ++player->contact_age;
    if (player->contact_age < DD_POSSESSION_CONTACT_LIMIT) return 0;
    if (state->match_clock_pulse != 0u &&
        state->players[owner].facing == player->facing) {
        dd_begin_free_throw(state, owner, player_index);
        return 1;
    }
    dd_transfer_contact_possession(state, player_index);
    return 1;
}

/* $B377 only classifies the ball while its integer height is $34-$37.  It
   expands a box around the hoop from result 1 through result 4; result 1 is
   the clean make and 2-4 are progressively wider rim misses. */
static uint8_t dd_basket_contact_result(const DDGameplayState *state) {
    int32_t hoop_x = state->possession_direction == 0u ? 0x004800 : 0x01B800;
    uint8_t height = (uint8_t)(state->ball.height >> 8);
    uint8_t result;
    if (height < 0x34u || height > 0x37u) return 0u;
    /* $AE25 increments byte counter $04F0 before calling $B377.  If that
       increment wraps to zero, $B377 rearms it and returns without contact. */
    if ((uint8_t)state->ball.action_age == 0u) return 0u;
    for (result = 1u; result <= 4u; ++result) {
        int32_t ball_half = (int32_t)result << 8;
        if (dd_axis_boxes_overlap(state->ball.court_depth, 0x005800,
                                  ball_half, 0x0100) &&
            dd_axis_boxes_overlap(state->ball.court_x, hoop_x,
                                  ball_half, 0x0100)) return result;
    }
    return 0u;
}

static void dd_apply_made_basket_score(DDGameplayState *state) {
    if (state->last_shooter < DD_GAMEPLAY_PLAYER_COUNT) {
        uint32_t team = state->last_shooter < 5u ? 0u : 1u;
        uint32_t points = state->shot_value == 1u ? 1u : 2u;
        if (state->score[team] <= 999u - points) {
            state->score[team] = (uint16_t)(state->score[team] + points);
        }
    }
}

/* $B473 sweeps seven two-axis samples along the rim/backboard diagonal.  On
   the first contact it latches $0490 and reverses longitudinal velocity. */
static int dd_rim_sweep_contact(DDGameplayState *state) {
    DDBallState *ball = &state->ball;
    uint32_t sample;
    if (ball->rim_contact != 0u) return 0;
    for (sample = 0u; sample < 7u; ++sample) {
        int32_t rim_x = state->possession_direction == 0u
            ? 0x004500 - (int32_t)sample * 0x0100
            : 0x01BB00 + (int32_t)sample * 0x0100;
        int32_t rim_axis = 0x009E00 - (int32_t)sample * 0x0200;
        if (dd_axis_boxes_overlap(ball->court_depth + ball->height, rim_axis,
                                  0x0100, 0x0800) &&
            dd_axis_boxes_overlap(ball->court_x, rim_x, 0x0100, 0x0100)) {
            ball->rim_contact = 1u;
            ball->owner = DD_NO_OWNER;
            ball->velocity_x = -ball->velocity_x;
            return 1;
        }
    }
    return 0;
}

/* $ABAB expands the original packed court target into native world axes. */
static void dd_unpack_cpu_target(uint8_t packed, int32_t *x, int32_t *depth) {
    *x = (int32_t)((((uint32_t)packed & 0x1Fu) << 4u) + 8u) << 8;
    *depth = (int32_t)((((uint32_t)packed >> 1u) & 0x70u) + 8u) << 8;
}

/* $AB72 compresses the two court axes for region and occupancy decisions. */
static uint8_t dd_pack_cpu_coordinates(int32_t court_x, int32_t court_depth) {
    uint32_t x = (uint32_t)dd_clamp(court_x >> 8, 0, 0x1FF);
    uint32_t depth = (uint32_t)dd_clamp(court_depth >> 8, 0, 0x7F);
    return (uint8_t)(((depth << 1u) & 0xE0u) | ((x >> 4u) & 0x1Fu));
}

static uint8_t dd_pack_cpu_position(const DDPlayerState *player) {
    return dd_pack_cpu_coordinates(player->court_x, player->court_depth);
}

/* $AC64 mirrors the five-bit packed court column when direction bit $40 is set. */
static uint8_t dd_mirror_packed_target(uint8_t packed) {
    return (uint8_t)((packed & 0xE0u) | (0x1Fu - (packed & 0x1Fu)));
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

static uint8_t dd_cpu_possession_region(const DDGameplayState *state, uint8_t packed) {
    if (state->possession_direction != 0u) packed = dd_mirror_packed_target(packed);
    return dd_cpu_region(packed);
}

static void dd_set_cpu_target(DDPlayerState *player, uint8_t packed) {
    player->target_zone = packed;
    dd_unpack_cpu_target(packed, &player->target_x, &player->target_depth);
}

/* $8C5B contains signed 16-bit deltas in the game's packed court space.
   Directions 0-7 are right, down-right, down, down-left, left, up-left,
   up, and up-right respectively. */
static uint16_t dd_project_packed_position(uint8_t packed, uint8_t direction,
                                           uint32_t steps) {
    static const int16_t delta[8] = {1, -33, -32, -31, -1, 31, 32, 33};
    return (uint16_t)((uint16_t)packed + delta[direction & 7u] * (int32_t)steps);
}

/* $8CF3 rejects packed-coordinate overflow, the top gutter, and positions
   outside the seven depth-band bounds at $8D0F. */
static int dd_cpu_packed_position_valid(uint16_t projected) {
    static const uint8_t bounds[16] = {
        0x02u, 0x1Du, 0x22u, 0x3Cu, 0x42u, 0x5Bu, 0x62u, 0x7Au,
        0x82u, 0x99u, 0xA2u, 0xB8u, 0xC2u, 0xD7u, 0xE2u, 0xF6u
    };
    uint8_t packed = (uint8_t)projected;
    uint8_t index;
    if ((projected >> 8u) != 0u || (packed & 0xE0u) == 0u) return 0;
    index = (uint8_t)((packed & 0xE0u) >> 4u);
    return (uint8_t)(packed - bounds[index]) < bounds[index + 1u];
}

/* Fixed bank $D99A predicts one packed step along the current facing.  Only
   a collision with the ball or the linked opponent triggers avoidance.  It
   then tries the four $8BC8 direction candidates two steps out and installs
   the first in-bounds packed target. */
static int dd_cpu_avoid_ball_or_defender(DDGameplayState *state, uint32_t player_index) {
    static const uint8_t candidates[8][4] = {
        {2u, 6u, 4u, 0u}, {0u, 0u, 0u, 0u},
        {6u, 4u, 0u, 2u}, {0u, 0u, 0u, 0u},
        {2u, 6u, 0u, 4u}, {0u, 0u, 0u, 0u},
        {2u, 4u, 0u, 6u}, {0u, 0u, 0u, 0u}
    };
    DDPlayerState *player;
    uint8_t packed;
    uint8_t predicted;
    uint8_t ball;
    uint8_t linked;
    uint8_t order[4];
    uint8_t attack_facing;
    uint32_t index;
    uint16_t projected;
    if (player_index >= DD_GAMEPLAY_PLAYER_COUNT) return 0;
    player = &state->players[player_index];
    packed = dd_pack_cpu_position(player);
    predicted = (uint8_t)dd_project_packed_position(packed, player->facing, 1u);
    /* The original owned ball keeps the carrier's packed $05B0 coordinate;
       native rendering offsets it to the hand, so exclude that visual offset
       from the lookahead comparison for the current owner. */
    ball = state->ball.owner == player_index ? packed
        : dd_pack_cpu_coordinates(state->ball.court_x, state->ball.court_depth);
    linked = dd_pack_cpu_position(&state->players[state->controlled_player]);
    if (predicted != ball && predicted != linked) return 0;
    memcpy(order, candidates[player->facing & 7u], sizeof(order));
    attack_facing = state->possession_direction == 0u ? 4u : 0u;
    if (attack_facing == (player->facing & 7u) &&
        (state->cpu_global_frame & 2u) == 0u) {
        uint8_t swap = order[0];
        order[0] = order[1];
        order[1] = swap;
    }
    for (index = 0u; index < 4u; ++index) {
        projected = dd_project_packed_position(packed, order[index], 2u);
        if (dd_cpu_packed_position_valid(projected)) {
            dd_set_cpu_target(player, (uint8_t)projected);
            return 1;
        }
    }
    return 0;
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

/* $9102 compares the current player with its $0580 paired opponent using
   two-unit half extents on both portable court axes. */
static int dd_paired_player_contact(const DDGameplayState *state, uint32_t player) {
    uint32_t opponent = player < 5u ? player + 5u : player - 5u;
    return dd_axis_boxes_overlap(state->players[player].court_depth,
                                 state->players[opponent].court_depth,
                                 0x0200, 0x0200) &&
           dd_axis_boxes_overlap(state->players[player].court_x,
                                 state->players[opponent].court_x,
                                 0x0200, 0x0200);
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
    uint8_t region = dd_cpu_possession_region(state, dd_pack_cpu_position(player));
    /* $8468 passes the current $AC2A region to $AC58. Region zero instead
       uses ($001A + 1) & 3; $AC58 reads the seven-byte table at $AC78. */
    uint8_t target_index = region != 0u
        ? region : (uint8_t)((state->cpu_global_frame + 1u) & 3u);
    uint8_t target = assets->cpu_region_targets[target_index];
    if (state->possession_direction != 0u) target = dd_mirror_packed_target(target);
    dd_set_cpu_target(player, target);
}

/* $842F indexes the 14 bytes at $8452 by region and bit two of $001A. */
static void dd_choose_regional_route_target(const DDTipoffAssetsHeader *assets,
                                            DDGameplayState *state,
                                            uint32_t player_index, uint8_t region) {
    uint8_t phase = (uint8_t)((state->cpu_global_frame >> 2u) & 1u);
    uint8_t target = assets->cpu_spacing_targets[(uint32_t)region * 2u + phase];
    if (state->possession_direction != 0u) target = dd_mirror_packed_target(target);
    dd_set_cpu_target(&state->players[player_index], target);
}

/* $9097(A=0,Y=8) finds the role-zero object on the possession-side team. */
static uint32_t dd_possession_role_zero(const DDGameplayState *state) {
    uint32_t first = state->possession_direction == 0u ? 5u : 0u;
    uint32_t player;
    for (player = first; player < first + 5u; ++player) {
        if (state->players[player].role == 0u) return player;
    }
    return first;
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

/* Bank-0 $9ABD interprets the signed height stream selected through the
   original per-object $0500/$0510 pointer.  $81 reverses the pointer and
   sign; $80 restores only the integer height byte and returns carry set. */
static int dd_step_player_height_script(const DDTipoffAssetsHeader *assets,
                                        DDPlayerState *player) {
    for (;;) {
        uint8_t value = (uint8_t)assets->height_scripts[player->height_script_index];
        if (value == 0x80u) {
            player->height = (player->height & 0x00FF) | 0x1000;
            return 1;
        }
        if (value == 0x81u) {
            player->height_script_reverse ^= 1u;
        } else {
            int delta = (int)(int8_t)value;
            uint8_t high = (uint8_t)(((uint32_t)player->height >> 8u) & 0xFFu);
            if (player->height_script_reverse != 0u) delta = -delta;
            high = (uint8_t)((int)high + delta);
            player->height = (player->height & 0x00FF) | ((int32_t)high << 8u);
            if (player->height_script_reverse == 0u) {
                ++player->height_script_index;
                return 0;
            }
            --player->height_script_index;
            return 0;
        }
        --player->height_script_index;
    }
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
    uint32_t owner_index = state->ball.owner < DD_GAMEPLAY_PLAYER_COUNT
        ? state->ball.owner : state->carrier;
    const DDPlayerState *owner;
    if (owner_index >= DD_GAMEPLAY_PLAYER_COUNT) return;
    owner = &state->players[owner_index];
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

/* $A129 scores every eligible teammate against the held direction bits.  A
   candidate receives one point for each matching horizontal/vertical half
   plane; equal nonzero scores prefer the later object slot. */
static uint32_t dd_user_pass_receiver(const DDGameplayState *state, uint32_t carrier,
                                      uint32_t input_mask) {
    uint32_t first = carrier < 5u ? 0u : 5u;
    uint32_t last = first + 5u;
    uint32_t receiver = DD_NO_OWNER;
    uint32_t best_score = 0u;
    uint8_t carrier_x;
    uint8_t carrier_y;
    uint32_t player;
    if (carrier >= DD_GAMEPLAY_PLAYER_COUNT ||
        (input_mask & (DD_INPUT_LEFT | DD_INPUT_RIGHT | DD_INPUT_UP | DD_INPUT_DOWN)) == 0u) {
        return DD_NO_OWNER;
    }
    carrier_x = (uint8_t)((state->players[carrier].court_x - state->camera_x) >> 8);
    carrier_y = (uint8_t)(0xF0 - (state->players[carrier].court_depth >> 8) -
                          (state->players[carrier].height >> 8));
    for (player = first; player < last; ++player) {
        const DDPlayerState *candidate = &state->players[player];
        int32_t projected_x = (candidate->court_x - state->camera_x) >> 8;
        uint8_t candidate_x;
        uint8_t candidate_y;
        uint32_t score = 0u;
        if (player == carrier || candidate->role == 0u ||
            projected_x < 12 || projected_x >= 244) continue;
        candidate_x = (uint8_t)projected_x;
        candidate_y = (uint8_t)(0xF0 - (candidate->court_depth >> 8) -
                                (candidate->height >> 8));
        if ((input_mask & DD_INPUT_LEFT) != 0u && candidate_x < carrier_x) ++score;
        if ((input_mask & DD_INPUT_RIGHT) != 0u && candidate_x >= carrier_x) ++score;
        if ((input_mask & DD_INPUT_UP) != 0u && candidate_y < carrier_y) ++score;
        if ((input_mask & DD_INPUT_DOWN) != 0u && candidate_y >= carrier_y) ++score;
        if (score != 0u && score >= best_score) {
            best_score = score;
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
    ball->rim_contact = 0u;
    ball->velocity_x = (state->players[receiver].court_x - ball->court_x) / 19;
    ball->velocity_depth = (state->players[receiver].court_depth - ball->court_depth) / 19;
    ball->velocity_height = 0x0300;
    if (carrier == state->controlled_player && carrier < 5u) {
        state->players[carrier].action = DD_PLAYER_USER_PASS_RECOVER;
        state->players[receiver].action = DD_PLAYER_USER_PASS_RECEIVE;
        state->players[receiver].action_age = 0u;
        state->players[receiver].velocity_x = 0;
        state->players[receiver].velocity_depth = 0;
    } else {
        state->players[carrier].action = DD_PLAYER_LIVE_SHOOTER_RECOVER;
    }
    state->players[carrier].action_age = 0u;
    state->carrier = DD_NO_OWNER;
}

/* $8FE0 decrements the $04E0 release timer installed by $9018. At four it
   launches ball state $02; below six a nonzero metasprite index moves up by
   eight; underflow replaces player state $31 with $40. */
static void dd_step_inbound_release(DDGameplayState *state, uint32_t player_index) {
    DDPlayerState *player = &state->players[player_index];
    --player->release_timer;
    if ((player->release_timer & 0x80u) != 0u) {
        player->action = DD_PLAYER_LIVE_CPU;
        player->action_age = 0u;
        return;
    }
    if (player->release_timer == 4u) {
        uint32_t receiver = state->ball.receiver;
        if (receiver < DD_GAMEPLAY_PLAYER_COUNT && receiver != player_index) {
            dd_begin_pass(state, player_index, receiver);
            player->action = DD_PLAYER_INBOUND_READY;
            player->action_age = 0u;
        } else {
            state->ball.action = DD_BALL_PASS;
            state->ball.owner = DD_NO_OWNER;
            state->carrier = DD_NO_OWNER;
        }
    }
    if (player->release_timer < 6u && player->animation != 0u) {
        player->animation = (uint8_t)(player->animation - 8u);
    }
}

static int dd_inbound_formation_ready(const DDGameplayState *state,
                                      uint32_t inbounder) {
    uint32_t player;
    for (player = 0u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
        if (player != inbounder &&
            state->players[player].action == DD_PLAYER_INBOUND_FORMATION) return 0;
    }
    return 1;
}

static void dd_start_inbound_release(DDGameplayState *state, uint32_t inbounder,
                                     uint32_t receiver) {
    DDPlayerState *player = &state->players[inbounder];
    uint32_t other_first = inbounder < 5u ? 5u : 0u;
    uint32_t teammate;
    for (teammate = other_first; teammate < other_first + 5u; ++teammate) {
        state->players[teammate].action = DD_PLAYER_LIVE_TEAMMATE;
        state->players[teammate].action_age = 0u;
    }
    state->controlled_player = (uint8_t)other_first;
    state->players[other_first].action = DD_PLAYER_LIVE_USER;
    player->action = DD_PLAYER_INBOUND_READY;
    player->action_age = 0u;
    player->release_timer = 8u;
    state->ball.action = DD_BALL_AWARDED;
    state->ball.held_height_offset = 0x08u;
    state->ball.receiver = (uint8_t)receiver;
    state->ball.action_age = 0u;
}

static void dd_start_inbound_alternate(DDGameplayState *state, uint32_t inbounder) {
    uint32_t first = inbounder < 5u ? 0u : 5u;
    uint32_t opposite = first == 0u ? 5u : 0u;
    uint32_t selected = first;
    while (selected < first + 5u && state->players[selected].role != 0u) ++selected;
    if (selected >= first + 5u) selected = inbounder;
    state->ball.action = DD_BALL_AWARDED;
    state->ball.action_age = 0u;
    state->controlled_player = (uint8_t)selected;
    state->players[selected].action = DD_PLAYER_LIVE_USER_INBOUND;
    state->players[selected].action_age = 0u;
    if (state->inbound_variant == 2u) {
        uint32_t role_zero = opposite;
        while (role_zero < opposite + 5u && state->players[role_zero].role != 0u) {
            ++role_zero;
        }
        if (role_zero < opposite + 5u) {
            state->players[role_zero].action = DD_PLAYER_LIVE_USER;
            state->players[role_zero].action_age = 0u;
        }
    }
}

static void dd_begin_shot(DDGameplayState *state, uint32_t shooter) {
    DDPlayerState *player;
    if (shooter >= DD_GAMEPLAY_PLAYER_COUNT) return;
    player = &state->players[shooter];
    state->ball.action = DD_BALL_SHOT_GATHER;
    state->ball.owner = (uint8_t)shooter;
    state->ball.receiver = DD_NO_OWNER;
    state->ball.action_age = 0u;
    state->ball.outcome = 0u;
    state->ball.rim_contact = 0u;
    state->last_shooter = (uint8_t)shooter;
    state->shot_value = 2u;
    if (shooter == state->controlled_player && shooter < 5u) {
        /* Bank-0 $AA75 is the user B-button shot initializer: install the
           $9B26/$9B27 height stream, expose dispatcher state $03, and put
           the ball in state $04.  Paired CPU defense reads that $03. */
        player->height_script_index = 11u;
        player->height_script_reverse = 0u;
        player->release_timer = 1u;
        player->action = DD_PLAYER_USER_SHOOT;
    } else {
        player->action = DD_PLAYER_LIVE_CARRIER_DECIDE;
    }
    player->action_age = 0u;
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
    int integrate_existing_velocity = 0;
    ++player->cpu_updates;
    /* Dispatcher state $3B points at $8297, a bare RTS. It bypasses even the
       common movement tail, so every portable object field remains intact. */
    if (player->action == DD_PLAYER_ROUTE_WAIT) return;
    if (player->action_age != UINT16_MAX) ++player->action_age;
    if (dd_step_possession_contact(state, player_index)) return;
    switch (player->action) {
        case DD_PLAYER_USER_PASS_RECOVER:
            speed = 0;
            player->velocity_x = 0;
            player->velocity_depth = 0;
            if (player->action_age >= 17u) {
                player->action = DD_PLAYER_LIVE_CPU;
                player->action_age = 0u;
            }
            break;
        case DD_PLAYER_USER_PASS_RECEIVE:
            speed = 0;
            player->velocity_x = 0;
            player->velocity_depth = 0;
            break;
        case DD_PLAYER_LIVE_CARRIER:
            speed = 0x0320;
            dd_cpu_avoid_ball_or_defender(state, player_index);
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
            dd_cpu_avoid_ball_or_defender(state, player_index);
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
            /* $8D9C decrements the $20 value installed in $04F0 through
               zero, changing $28->$29 only when the 33rd DEC makes it $FF.
               This player is scheduled every other rendered frame. */
            speed = 0x0180;
            if (player->action_age >= 33u) {
                player->action = DD_PLAYER_LIVE_SHOOTER_RESET;
                player->action_age = 0u;
            }
            break;
        case DD_PLAYER_LIVE_SHOOTER_RESET:
            /* $8DAB copies the ball's packed target only for rotating
               priority object $004D, then $91FB performs an immediate
               $B435 contact test and $A347 possession transfer. */
            speed = 0x0180;
            if (state->cpu_priority_player == player_index) {
                player->target_x = state->ball.court_x;
                player->target_depth = state->ball.court_depth;
                player->target_zone = dd_pack_cpu_coordinates(state->ball.court_x,
                                                               state->ball.court_depth);
            }
            if (dd_possession_ball_contact(state, player_index)) {
                dd_transfer_contact_possession(state, player_index);
                return;
            }
            break;
        case DD_PLAYER_TIP_CPU:
            /* $8DD2 waits for shared object phase $004A >= $20, then installs
               the same $9B34 height stream and advances $2A->$2B. */
            speed = 0;
            if (state->object_phase >= 0x20u) {
                player->height_script_index = 11u;
                player->height_script_reverse = 0u;
                player->action = DD_PLAYER_TIP_CPU_AIRBORNE;
                player->action_age = 0u;
            }
            break;
        case DD_PLAYER_TIP_CPU_AIRBORNE:
            /* $8DF7 runs $9ABD and gives the jumper state $25 when it owns
               the tip; otherwise its five-player side returns to state $20. */
            speed = 0;
            if (dd_step_player_height_script(assets, player)) {
                if (state->ball.owner == player_index) {
                    state->carrier = (uint8_t)player_index;
                    state->ball.action = DD_BALL_DRIBBLE;
                    player->action = DD_PLAYER_LIVE_CARRIER;
                    player->action_age = 0u;
                } else {
                    uint32_t first = player_index < 5u ? 0u : 5u;
                    uint32_t teammate;
                    for (teammate = first; teammate < first + 5u; ++teammate) {
                        state->players[teammate].action = DD_PLAYER_LIVE_TEAMMATE;
                        state->players[teammate].action_age = 0u;
                    }
                }
            }
            break;
        case DD_PLAYER_LIVE_TEAMMATE: {
            uint32_t opponent = player_index < 5u ? player_index + 5u : player_index - 5u;
            if (dd_paired_player_contact(state, player_index)) {
                player->action = DD_PLAYER_LIVE_PAIRED_DEFENDER;
                player->action_age = 0u;
                player->paired_timer = 0x10u;
                return;
            }
            if (state->players[opponent].action == 0x03u) {
                /* $9139 converts the paired defender to jump start when its
                   paired object exposes ball/action state $03. */
                player->action = DD_PLAYER_JUMP_START;
                player->action_age = 0u;
            }
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
            /* $8A3A uses packed equality through $D978, then clears $0600
               and returns to state $20. */
            speed = 0x0200;
            if (dd_pack_cpu_position(player) == player->target_zone) {
                player->action = DD_PLAYER_LIVE_TEAMMATE;
                player->action_age = 0u;
                player->route_step = 0u;
            }
            break;
        case DD_PLAYER_LIVE_PAIRED_DEFENDER: {
            uint32_t opponent = player_index < 5u ? player_index + 5u : player_index - 5u;
            speed = 0;
            if (state->players[opponent].action == DD_PLAYER_USER_SHOOT) {
                /* $8A98 shares $9139 with state $20: an already-latched
                   paired defender also converts $22->$23 for a user shot. */
                player->action = DD_PLAYER_JUMP_START;
                player->action_age = 0u;
            } else if (!dd_paired_player_contact(state, player_index)) {
                player->action = DD_PLAYER_LIVE_TEAMMATE;
                player->action_age = 0u;
            }
            player->target_x = player->court_x;
            player->target_depth = player->court_depth;
            player->velocity_x = 0;
            player->velocity_depth = 0;
            player->facing = state->players[opponent].facing;
            player->animation = state->players[opponent].animation;
            break;
        }
        case DD_PLAYER_JUMP_START:
            /* $8AF4->$B503 clears all motion, installs $9B34 (asset index 11),
               clears the $9ABD direction flag, and advances $23->$24. */
            speed = 0;
            player->velocity_x = 0;
            player->velocity_depth = 0;
            player->velocity_height = 0;
            player->height_script_index = 11u;
            player->height_script_reverse = 0u;
            player->action = DD_PLAYER_JUMP_CONTEST;
            player->action_age = 0u;
            break;
        case DD_PLAYER_JUMP_CONTEST:
            /* $8B12 tests $A6C3 contact, then runs the byte-exact $9ABD
               interpreter. Contact replaces even an owned shot with ball
               state $00; the blocker receives the full $9208 possession
               reset only after landing. */
            speed = 0;
            if (state->ball.owner != player_index &&
                dd_jump_ball_contact(state, player_index)) {
                state->ball.owner = (uint8_t)player_index;
                state->ball.receiver = DD_NO_OWNER;
                state->ball.action = DD_BALL_AWARDED;
                state->ball.action_age = 0u;
            }
            if (dd_step_player_height_script(assets, player)) {
                if (state->ball.owner == player_index) {
                    dd_transfer_contact_possession(state, player_index);
                } else {
                    player->action = DD_PLAYER_LIVE_SHOOTER_RECOVER;
                    /* $28's native age counts upward while the ROM's $04F0
                       counts $10 down through $FF: both take 17 updates. */
                    player->action_age = 16u;
                }
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
                uint8_t region = dd_cpu_possession_region(
                    state, dd_pack_cpu_position(player));
                uint8_t phase = (uint8_t)((state->cpu_global_frame >> 2u) & 1u);
                uint8_t target = assets->cpu_spacing_targets[(uint32_t)region * 2u + phase];
                if (state->possession_direction != 0u) {
                    target = dd_mirror_packed_target(target);
                }
                if (dd_cpu_target_occupied(state, player_index, target)) {
                    target = assets->cpu_spacing_targets[(uint32_t)region * 2u + (phase ^ 1u)];
                    if (state->possession_direction != 0u) {
                        target = dd_mirror_packed_target(target);
                    }
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
        case DD_PLAYER_FORMATION_CPU:
            /* $8BC5->$D98A->$A84C calls each fixed-point axis integrator
               twice. It consumes existing vectors; it does not retarget. */
            integrate_existing_velocity = 1;
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
            /* Observed $8EE2 path: clear motion, count $04F0 down, and call
               $9018 only after the remaining $36 objects have reached $37. */
            speed = 0;
            player->velocity_x = 0;
            player->velocity_depth = 0;
            if (player->hold_timer != 0u) --player->hold_timer;
            if (dd_inbound_formation_ready(state, player_index)) {
                uint32_t receiver = player_index + 1u;
                if (receiver >= DD_GAMEPLAY_PLAYER_COUNT) receiver = player_index - 1u;
                if (state->inbound_variant != 0u) {
                    dd_start_inbound_alternate(state, player_index);
                } else {
                    dd_start_inbound_release(state, player_index, receiver);
                }
            }
            break;
        case DD_PLAYER_INBOUND_READY:
            speed = 0;
            dd_step_inbound_release(state, player_index);
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
            /* $81A2: role zero returns to $32. At a packed target, enter
               $3C/$3E/$38 by region and role. While moving, compare against
               the role-zero object's region; a match enters $3A and tries the
               signed $8262 offsets before falling back to $AC78[2]. */
            speed = 0x0220;
            if (player->role == 0u) {
                player->action = DD_PLAYER_LIVE_CPU_SETUP;
                player->action_age = 0u;
            } else {
                uint8_t packed = dd_pack_cpu_position(player);
                if (packed == player->target_zone) {
                    uint8_t region = dd_cpu_possession_region(state, packed);
                    player->action = DD_PLAYER_LIVE_CPU_CUT;
                    if (region == 0u) {
                        player->action = DD_PLAYER_ROUTE_INIT;
                    } else if (player->role != 1u) {
                        player->action = DD_PLAYER_LIVE_CPU_ROUTE;
                        dd_choose_regional_route_target(assets, state, player_index, region);
                    }
                    player->action_age = 0u;
                } else {
                    uint32_t reference = dd_possession_role_zero(state);
                    uint8_t region = dd_cpu_possession_region(state, packed);
                    uint8_t reference_region = dd_cpu_possession_region(
                        state, dd_pack_cpu_position(&state->players[reference]));
                    if (region == reference_region) {
                        static const int16_t offset[2] = {-65, 95};
                        uint32_t first = (state->cpu_global_frame & 2u) != 0u ? 1u : 0u;
                        uint32_t attempt;
                        int installed = 0;
                        player->action = DD_PLAYER_ROUTE_ADJUST;
                        player->action_age = 0u;
                        for (attempt = 0u; attempt < 2u; ++attempt) {
                            uint16_t candidate = (uint16_t)((uint16_t)packed +
                                offset[first ^ attempt]);
                            if (dd_cpu_packed_position_valid(candidate)) {
                                dd_set_cpu_target(player, (uint8_t)candidate);
                                installed = 1;
                                break;
                            }
                        }
                        if (!installed) {
                            uint8_t target = assets->cpu_region_targets[2u];
                            if (state->possession_direction != 0u) {
                                target = dd_mirror_packed_target(target);
                            }
                            dd_set_cpu_target(player, target);
                        }
                    }
                }
            }
            break;
        case DD_PLAYER_ROUTE_ADJUST:
            /* $8266 is the shorter companion: role zero returns to $32;
               packed-target arrival enters $3C or loops through $38 in region zero. */
            speed = 0x0220;
            if (player->role == 0u) {
                player->action = DD_PLAYER_LIVE_CPU_SETUP;
                player->action_age = 0u;
            } else if (dd_pack_cpu_position(player) == player->target_zone) {
                player->action = dd_cpu_possession_region(
                    state, dd_pack_cpu_position(player)) == 0u
                    ? DD_PLAYER_ROUTE_INIT : DD_PLAYER_LIVE_CPU_CUT;
                player->action_age = 0u;
            }
            break;
        case DD_PLAYER_LIVE_RENDER_ONLY:
            /* $8460 calls $B503, clearing all three motion vectors, before
               the common animation/render tail. */
            speed = 0;
            player->velocity_x = 0;
            player->velocity_depth = 0;
            player->velocity_height = 0;
            break;
        case DD_PLAYER_INBOUNDER:
            /* $8C6B tests packed arrival through $D978, writes owner/carrier,
               copies the player's court coordinates to the ball, and enters $30. */
            speed = 0x0200;
            if (dd_pack_cpu_position(player) == player->target_zone) {
                dd_claim_loose_ball(state, player_index);
                state->ball.court_x = player->court_x;
                state->ball.court_depth = player->court_depth;
                player->action = DD_PLAYER_INBOUND_HOLD;
                player->action_age = 0u;
            }
            break;
        default:
            break;
    }
    if (integrate_existing_velocity) {
        player->court_x += player->velocity_x * 2;
        player->court_depth += player->velocity_depth * 2;
    } else {
        dd_move_cpu_player(player, player_index, live_frame, speed);
    }
}

static void dd_begin_live(DDGameplayState *state, uint32_t winner) {
    uint32_t player;
    state->phase = DD_GAMEPLAY_LIVE;
    state->live_frame = 0u;
    state->carrier = (uint8_t)winner;
    state->controlled_player = 0u;
    state->ball.owner = state->carrier;
    state->ball.action = DD_BALL_DRIBBLE;
    state->ball.held_height_offset = 0x08u;
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

static uint8_t dd_bcd_decrement(uint8_t value) {
    /* Bank 0 $9490 subtracts one, or seven when the low decimal digit is zero. */
    return (uint8_t)(value - ((value & 0x0Fu) == 0u ? 7u : 1u));
}

static void dd_step_game_clock(DDGameplayState *state) {
    state->match_clock_pulse = 0u;
    if (state->clock_expired || state->scene_frame < state->next_clock_frame) return;
    while (!state->clock_expired && state->scene_frame >= state->next_clock_frame) {
        state->match_clock_pulse = 1u;
        if (state->clock_seconds == 0u) state->clock_seconds = 0x60u;
        state->clock_seconds = dd_bcd_decrement(state->clock_seconds);
        if (state->clock_seconds == 0x59u && state->clock_minutes != 0u) {
            state->clock_minutes = dd_bcd_decrement(state->clock_minutes);
        }
        state->next_clock_frame += DD_CLOCK_FRAMES_PER_SECOND;
        if (state->clock_minutes == 0u && state->clock_seconds == 0u) {
            state->clock_expired = 1;
            state->clock_expired_frame = state->scene_frame;
        }
    }
}

static void dd_prepare_period_formation(DDGameplayState *state) {
    uint32_t player;
    state->sequence_frame = DD_FORMATION_VISIBLE_FRAME - 1u;
    state->phase = DD_GAMEPLAY_FORMATION;
    state->camera_x = 0x7F00;
    state->controlled_player = 0u;
    state->carrier = DD_NO_OWNER;
    state->controlled_flash_palette = 1u;
    state->ball.court_x = 0x00FF00;
    state->ball.court_depth = 0x5A00;
    state->ball.height = 0x1800;
    state->ball.animation = 1u;
    state->ball.action = DD_BALL_AWARDED;
    state->ball.held_height_offset = 0x08u;
    state->ball.owner = DD_NO_OWNER;
    state->ball.receiver = DD_NO_OWNER;
    state->ball.action_age = 0u;
    state->tip_winner = DD_NO_OWNER;
    state->tip_user_jump_frame = UINT_MAX;
    state->live_start_frame = DD_LIVE_FRAME;
    state->camera_chr_side = 1u;
    state->hud_split_y = 64u;
    state->clock_minutes = 0x05u;
    state->clock_seconds = 0u;
    state->clock_expired = 0;
    state->clock_expired_frame = UINT_MAX;
    state->next_clock_frame = state->scene_frame + 141u;
    state->last_shooter = DD_NO_OWNER;
    state->shot_value = 2u;
    state->foul_shooter = DD_NO_OWNER;
    state->foul_offender = DD_NO_OWNER;
    state->free_throw_age = 0u;
    state->game_set_age = 0u;
    state->return_to_title = 0;
    for (player = 0u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
        DDPlayerState *object = &state->players[player];
        memset(object, 0, sizeof(*object));
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
}

void dd_gameplay_reset(DDGameplayState *state) {
    if (state == NULL) return;
    memset(state, 0, sizeof(*state));
    state->scene_frame = DD_FORMATION_VISIBLE_FRAME - 1u;
    state->period = 1u;
    dd_prepare_period_formation(state);
    state->next_clock_frame = DD_FIRST_CLOCK_TICK_FRAME;
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

/* $B167 updates bounce-pass integer height with the high velocity byte and
   gravity phase $004A.  A negative trial decrements the high velocity byte;
   a still-nonnegative velocity restarts gravity at zero. */
static void dd_step_bounce_height(DDGameplayState *state) {
    DDBallState *ball = &state->ball;
    uint8_t velocity_high = (uint8_t)(((uint32_t)ball->velocity_height >> 8u) & 0xFFu);
    uint8_t height_high = (uint8_t)(((uint32_t)ball->height >> 8u) & 0xFFu);
    uint8_t trial = (uint8_t)(velocity_high - ball->vertical_phase + height_high);
    if ((trial & 0x80u) == 0u) {
        ball->height = (ball->height & 0x00FF) | ((int32_t)trial << 8u);
        if ((state->cpu_global_frame & 1u) == 0u) ++ball->vertical_phase;
    } else {
        velocity_high = (uint8_t)(velocity_high - 1u);
        ball->velocity_height = (ball->velocity_height & 0x00FF) |
            ((int32_t)velocity_high << 8u);
        if ((velocity_high & 0x80u) == 0u) ball->vertical_phase = 0u;
    }
}

static uint8_t dd_ones_complement_distance(uint8_t origin, uint8_t target) {
    uint8_t distance = (uint8_t)(origin - target);
    if ((distance & 0x80u) != 0u) distance ^= 0xFFu;
    return distance;
}

/* $A29D first keeps the two smallest screen-X distances, then compares their
   wrapped X+Y totals. $AA20 supplies the 12..243 screen-X eligibility gate. */
static uint32_t dd_user_switch_candidate(const DDGameplayState *state) {
    uint32_t first = state->controlled_player < 5u ? 0u : 5u;
    uint32_t nearest = first;
    uint32_t alternate = first;
    uint8_t nearest_x = 0x7Fu;
    uint8_t alternate_x = 0x7Fu;
    uint8_t ball_x = (uint8_t)((state->ball.court_x - state->camera_x) >> 8);
    uint8_t ball_y = (uint8_t)(0xF0 - (state->ball.court_depth >> 8) -
                               (state->ball.height >> 8));
    uint32_t player;
    for (player = first; player < first + 5u; ++player) {
        int32_t screen_x = (state->players[player].court_x - state->camera_x) >> 8;
        uint8_t distance;
        if (screen_x < 12 || screen_x >= 244) continue;
        distance = dd_ones_complement_distance(ball_x, (uint8_t)screen_x);
        if (distance < nearest_x) {
            alternate = nearest;
            alternate_x = nearest_x;
            nearest = player;
            nearest_x = distance;
        }
    }
    {
        uint8_t alternate_y = (uint8_t)(0xF0 -
            (state->players[alternate].court_depth >> 8) -
            (state->players[alternate].height >> 8));
        uint8_t nearest_y = (uint8_t)(0xF0 -
            (state->players[nearest].court_depth >> 8) -
            (state->players[nearest].height >> 8));
        uint8_t alternate_total = (uint8_t)(alternate_x +
            dd_ones_complement_distance(ball_y, alternate_y));
        uint8_t nearest_total = (uint8_t)(nearest_x +
            dd_ones_complement_distance(ball_y, nearest_y));
        if (nearest_total >= alternate_total) nearest = alternate;
    }
    return nearest;
}

static void dd_finish_ball_reception(const DDTipoffAssetsHeader *assets,
                                     DDGameplayState *state, uint32_t receiver) {
    uint32_t previous_control;
    if (receiver >= DD_GAMEPLAY_PLAYER_COUNT) return;
    previous_control = state->controlled_player;
    state->carrier = (uint8_t)receiver;
    state->ball.owner = (uint8_t)receiver;
    state->ball.receiver = DD_NO_OWNER;
    state->ball.action = DD_BALL_DRIBBLE;
    state->ball.action_age = 0u;
    state->ball.velocity_x = 0;
    state->ball.velocity_depth = 0;
    state->ball.velocity_height = 0;
    if (receiver < 5u) {
        if (previous_control < DD_GAMEPLAY_PLAYER_COUNT && previous_control != receiver) {
            state->players[previous_control].attributes = 0u;
        }
        state->controlled_player = (uint8_t)receiver;
        state->players[receiver].action = DD_PLAYER_LIVE_USER_CARRIER;
    } else {
        state->players[receiver].action = DD_PLAYER_LIVE_CARRIER;
    }
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
            /* $ACB6 attaches with table index 2, then adds 24 height units in
               tip modes $01/$03 and eight during ordinary held play. */
            if (ball->owner < DD_GAMEPLAY_PLAYER_COUNT) {
                dd_attach_ball(assets, state, 1u);
                ball->height = state->players[ball->owner].height +
                    ((int32_t)(ball->held_height_offset == 0u
                        ? 0x08u : ball->held_height_offset) << 8u);
            }
            break;
        case DD_BALL_DRIBBLE:
            dd_step_dribble(assets, state);
            break;
        case DD_BALL_PASS:
            /* The traced inbound remains in $02 for 19 frames (3553-3572).
               Keep that initializer-derived flight floor while using $B138,
               rather than elapsed time alone, to decide actual reception. */
            if (ball->action_age >= 19u && ball->receiver < DD_GAMEPLAY_PLAYER_COUNT &&
                dd_pass_receiver_contact(state, ball->receiver)) {
                dd_finish_ball_reception(assets, state, ball->receiver);
                break;
            }
            ball->court_x += ball->velocity_x;
            ball->court_depth += ball->velocity_depth;
            ball->height += ball->velocity_height;
            ball->velocity_height -= 0x0030;
            break;
        case DD_BALL_PASS_BOUNCE:
            /* $ADF2 runs rim/contact helper $B473, free-flight integrators
               $9CA0/$9CF6, and the common ball physics tail $B3E9. */
            dd_rim_sweep_contact(state);
            if (ball->velocity_height == 0) {
                ball->action = DD_BALL_HIDDEN;
                ball->action_age = 0u;
                break;
            }
            dd_step_bounce_height(state);
            ball->court_x += ball->velocity_x;
            ball->court_depth = dd_clamp(ball->court_depth + ball->velocity_depth,
                                         0x0400, 0x9800);
            break;
        case DD_BALL_SHOT_GATHER:
            if (ball->owner < DD_GAMEPLAY_PLAYER_COUNT) {
                dd_attach_ball(assets, state, 2u);
                ball->height = state->players[ball->owner].height + 0x1200;
            }
            if ((ball->owner < DD_GAMEPLAY_PLAYER_COUNT &&
                 state->players[ball->owner].action == DD_PLAYER_USER_SHOOT &&
                 ball->action_age >= 2u) || ball->action_age > 26u) {
                int32_t hoop_x = state->possession_direction == 0u ? 0x004800 : 0x01B800;
                ball->action = DD_BALL_AIRBORNE;
                ball->action_age = 0u;
                ball->velocity_x = (hoop_x - ball->court_x) / 21;
                ball->velocity_depth = (0x005800 - ball->court_depth) / 21;
                ball->height = (ball->height & 0x00FF) | 0x3800;
                ball->velocity_height = 0x0200;
                state->carrier = DD_NO_OWNER;
            }
            break;
        case DD_BALL_AIRBORNE: {
            uint8_t result = dd_basket_contact_result(state);
            if (result != 0u) {
                ball->outcome = result;
                ball->action = result == 1u ? DD_BALL_SCORE : DD_BALL_LOOSE_LAUNCH;
                ball->action_age = 0u;
                if (result == 1u) {
                    ball->height = (ball->height & 0x00FF) | 0x3200;
                    ball->velocity_x = 0;
                    ball->velocity_depth = 0;
                    state->possession_direction ^= 1u;
                }
                break;
            }
            dd_rim_sweep_contact(state);
            ball->court_x += ball->velocity_x;
            ball->court_depth += ball->velocity_depth;
            ball->velocity_height -= 0x0033;
            ball->height += ball->velocity_height;
            if (ball->height <= 0) {
                ball->action = DD_BALL_REBOUND;
                ball->action_age = 0u;
                ball->height = 0x0100;
                ball->velocity_height = 0x0290;
            }
            break;
        }
        case DD_BALL_SCORE:
            /* $AEDE starts counter $004A at $0C.  It awards the basket when
               the post-decrement value reaches $08 (fourth dispatch), lowers
               integer height only for values $05-$00, then enters $07 when
               the next decrement underflows. */
            if (ball->action_age == 4u) dd_apply_made_basket_score(state);
            if (ball->action_age >= 7u && ball->action_age <= 12u &&
                ball->height >= 0x0100) {
                ball->height -= 0x0100;
            }
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
            ball->owner = DD_NO_OWNER;
            ball->height = (ball->height & 0x00FF) | 0x3800;
            if (ball->outcome == 2u) {
                ball->velocity_x /= 2;
                ball->velocity_depth /= 2;
            } else if (ball->outcome == 3u) {
                ball->velocity_x = 0;
                ball->velocity_depth = 0;
            } else {
                ball->velocity_x = -ball->velocity_x / 2;
                ball->velocity_depth = -ball->velocity_depth / 2;
            }
            ball->velocity_height = 0x0100;
            ball->vertical_phase = 0u;
            ball->action = DD_BALL_LOOSE_AIRBORNE;
            ball->action_age = 0u;
            break;
        case DD_BALL_LOOSE_AIRBORNE:
            /* $AFDD integrates both court axes and height until its threshold,
               then switches to rebound state $07. */
            dd_rim_sweep_contact(state);
            ball->court_x = dd_clamp(ball->court_x + ball->velocity_x,
                                     0x001000, 0x01F000);
            ball->court_depth = dd_clamp(ball->court_depth + ball->velocity_depth,
                                         0x0400, 0x9800);
            ball->velocity_height -= 0x0010;
            ball->height += ball->velocity_height;
            /* $AFDD tests unsigned integer height >= $E0 after integration;
               the observed result-four arc crosses into $FF at frame 61. */
            if ((((uint32_t)ball->height >> 8u) & 0xFFu) >= 0xE0u) {
                ball->action = DD_BALL_REBOUND;
                ball->action_age = 0u;
                ball->velocity_height = 0x02E0;
                ball->rim_contact = 0u;
            }
            break;
        case DD_BALL_SHOT_LAUNCH: {
            /* Tip-toss/launch initializer $B017 writes vertical term $0305,
               curve byte $D8, and state $05 on its very next dispatch. */
            ball->velocity_height = 0x0305;
            ball->flight_curve = 0xD8u;
            ball->action = DD_BALL_AIRBORNE;
            ball->action_age = 0u;
            break;
        }
        case DD_BALL_DEAD:
        case DD_BALL_HIDDEN:
            /* $ACAB is shared by states $0B/$0C: zero height and project the
               non-live object without advancing its dispatcher state. */
            ball->height &= 0x00FF;
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
    state->ball.height = 0;
    state->ball.velocity_x = 0;
    state->ball.velocity_depth = 0;
    state->ball.velocity_height = 0;
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
        if (player != 5u &&
            state->players[player].action == DD_PLAYER_INBOUND_FORMATION &&
            dd_pack_cpu_position(&state->players[player]) ==
                state->players[player].target_zone) {
            state->players[player].action = DD_PLAYER_LIVE_SET;
            state->players[player].action_age = 0u;
        }
    }
    if (state->inbound_age == 177u) {
        state->carrier = 5u;
        state->ball.owner = 5u;
        state->ball.action = DD_BALL_DRIBBLE;
        state->ball.action_age = 0u;
        state->players[5].action = DD_PLAYER_INBOUND_HOLD;
        state->players[5].hold_timer = 0x20u;
        state->players[5].action_age = 0u;
        dd_step_dribble(assets, state);
    } else if (state->inbound_age > 229u && state->ball.action == DD_BALL_PASS) {
        dd_step_ball(assets, state);
        if (state->ball.action == DD_BALL_DRIBBLE) {
            dd_restore_post_inbound(state);
            dd_step_dribble(assets, state);
        }
    } else if (state->ball.action == DD_BALL_DRIBBLE) {
        dd_step_dribble(assets, state);
    }

    if (state->players[5].action == DD_PLAYER_INBOUND_HOLD &&
        state->inbound_age > 177u && (state->inbound_age & 1u) != 0u) {
        if (state->players[5].hold_timer != 0u) --state->players[5].hold_timer;
        if (state->players[5].hold_timer <= 0x0Au &&
            dd_inbound_formation_ready(state, 5u)) {
            dd_start_inbound_release(state, 5u, 6u);
        }
    }
    /* Original slot $07 is scheduled every other rendered frame. */
    if (state->players[5].action == DD_PLAYER_INBOUND_READY) {
        if (state->players[5].action_age != UINT16_MAX) {
            ++state->players[5].action_age;
        }
        if ((state->players[5].action_age & 1u) == 0u) {
            dd_step_inbound_release(state, 5u);
        }
    }
}

/* Controlled FCEUX frames 2608-3146 expose the foul/free-throw spine after
   $A347: dead ball $0B, shooter states $42/$4A, formation $43->$44, ball
   $01->$00, shooter $45->$46->$47, then shot states $04->$05.  The detailed
   formation walkers remain a later slice, but these timings keep the rule,
   one-point shot, and return to live play native and deterministic. */
static void dd_step_free_throw(const DDTipoffAssetsHeader *assets,
                               DDGameplayState *state) {
    uint32_t shooter = state->foul_shooter;
    DDBallState *ball = &state->ball;
    if (shooter >= DD_GAMEPLAY_PLAYER_COUNT) {
        state->phase = DD_GAMEPLAY_LIVE;
        return;
    }
    if (state->free_throw_age != UINT16_MAX) ++state->free_throw_age;
    if (state->free_throw_age == 2u) {
        state->players[shooter].action = DD_PLAYER_FREE_THROW_WALK;
    } else if (state->free_throw_age == 192u) {
        state->players[shooter].action = DD_PLAYER_FREE_THROW_SHOOTER;
    } else if (state->free_throw_age == 194u) {
        state->carrier = (uint8_t)shooter;
        ball->owner = (uint8_t)shooter;
        ball->action = DD_BALL_DRIBBLE;
        ball->action_age = 0u;
        state->players[shooter].action = DD_PLAYER_FREE_THROW_FORMATION;
        dd_step_dribble(assets, state);
    } else if (state->free_throw_age == 214u) {
        state->players[shooter].court_x = state->possession_direction == 0u
            ? 0x009200 : 0x016E00;
        state->players[shooter].court_depth = 0x005800;
        state->players[shooter].action = DD_PLAYER_FREE_THROW_READY;
        ball->action = DD_BALL_AWARDED;
        ball->action_age = 0u;
        ball->owner = (uint8_t)shooter;
    } else if (state->free_throw_age == 252u) {
        state->players[shooter].action = DD_PLAYER_FREE_THROW_SET;
    } else if (state->free_throw_age == 348u) {
        ball->action = DD_BALL_SHOT_GATHER;
        ball->action_age = 0u;
        ball->owner = (uint8_t)shooter;
        ball->receiver = DD_NO_OWNER;
        ball->outcome = 0u;
        ball->rim_contact = 0u;
        state->last_shooter = (uint8_t)shooter;
        state->shot_value = 1u;
        state->players[shooter].action = DD_PLAYER_FREE_THROW_GATHER;
    } else if (state->free_throw_age == 402u) {
        state->players[shooter].height = 0x1000;
        state->players[shooter].action = DD_PLAYER_FREE_THROW_FOLLOW;
    }
    dd_step_ball(assets, state);
    if (state->free_throw_age > 348u && ball->action == DD_BALL_DRIBBLE &&
        ball->owner < DD_GAMEPLAY_PLAYER_COUNT) {
        uint32_t winner = ball->owner;
        state->phase = DD_GAMEPLAY_LIVE;
        dd_transfer_contact_possession(state, winner);
    }
}

static void dd_step_live(const DDTipoffAssetsHeader *assets, DDGameplayState *state,
                         uint32_t input_mask) {
    DDPlayerState *controlled = &state->players[state->controlled_player];
    uint32_t player;
    uint32_t live_frame = state->live_frame + 1u;
    uint32_t pressed = input_mask & ~state->previous_input;
    uint32_t cpu_start;
    uint32_t cpu_end;
    int32_t input_x = 0;
    int32_t input_depth = 0;
    if (state->phase == DD_GAMEPLAY_FREE_THROW) {
        dd_step_free_throw(assets, state);
        dd_update_camera(state);
        state->live_frame = live_frame;
        state->previous_input = input_mask;
        return;
    }
    if (state->phase == DD_GAMEPLAY_INBOUND) {
        dd_step_inbound(assets, state, live_frame);
        dd_update_camera(state);
        state->controlled_flash_palette = (uint8_t)(((live_frame / 2u) & 1u) == 0u);
        state->players[state->controlled_player].attributes = state->controlled_flash_palette;
        state->live_frame = live_frame;
        state->previous_input = input_mask;
        return;
    }
    if (controlled->action == DD_PLAYER_LIVE_USER &&
        state->carrier != state->controlled_player &&
        (pressed & DD_INPUT_B) != 0u) {
        uint32_t selected = dd_user_switch_candidate(state);
        if (selected != state->controlled_player) {
            controlled->action = DD_PLAYER_LIVE_TEAMMATE;
            controlled->action_age = 0u;
            controlled->attributes = 0u;
            state->controlled_player = (uint8_t)selected;
            controlled = &state->players[selected];
            controlled->action = DD_PLAYER_LIVE_USER;
            controlled->action_age = 0u;
            controlled->velocity_x = 0;
            controlled->velocity_depth = 0;
        }
    }
    if (controlled->action == DD_PLAYER_LIVE_USER ||
        controlled->action == DD_PLAYER_LIVE_USER_CARRIER) {
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
    } else {
        controlled->velocity_x = 0;
        controlled->velocity_depth = 0;
    }
    if (controlled->action == DD_PLAYER_USER_PASS_RECOVER &&
        (state->cpu_global_frame & 1u) == 0u && controlled->action_age != UINT16_MAX) {
        ++controlled->action_age;
    }
    if (controlled->action == DD_PLAYER_USER_SHOOT &&
        (state->cpu_global_frame & 1u) == 0u) {
        if (controlled->action_age != UINT16_MAX) ++controlled->action_age;
        controlled->velocity_x = 0;
        controlled->velocity_depth = 0;
        if (dd_step_player_height_script(assets, controlled)) {
            controlled->action = DD_PLAYER_LIVE_USER;
            controlled->action_age = 0u;
        }
    }
    ++state->cpu_global_frame;
    if ((state->cpu_global_frame & 1u) != 0u) {
        cpu_start = 0u;
        cpu_end = 5u;
    } else {
        state->cpu_priority_player = (uint8_t)(state->cpu_priority_player + 1u);
        if (state->cpu_priority_player >= DD_GAMEPLAY_PLAYER_COUNT) state->cpu_priority_player = 5u;
        cpu_start = 5u;
        cpu_end = DD_GAMEPLAY_PLAYER_COUNT;
    }
    for (player = cpu_start; player < cpu_end; ++player) {
        if (player == state->controlled_player) continue;
        dd_update_cpu_player(assets, state, player, live_frame);
    }
    if (state->phase == DD_GAMEPLAY_FREE_THROW) {
        dd_update_camera(state);
        state->live_frame = live_frame;
        state->previous_input = input_mask;
        return;
    }
    if (state->carrier == state->controlled_player && state->ball.action == DD_BALL_DRIBBLE) {
        if ((pressed & DD_INPUT_B) != 0u) {
            dd_begin_shot(state, state->controlled_player);
        } else if ((pressed & DD_INPUT_A) != 0u) {
            uint32_t receiver = dd_user_pass_receiver(state, state->controlled_player,
                                                      input_mask);
            if (receiver < DD_GAMEPLAY_PLAYER_COUNT) {
                dd_begin_pass(state, state->controlled_player, receiver);
            }
        }
    }
    dd_step_ball(assets, state);
    if (live_frame == 447u && state->possession_count == 0u) {
        state->carrier = DD_NO_OWNER;
        state->ball.owner = DD_NO_OWNER;
        state->ball.action = DD_BALL_AWARDED;
        state->ball.held_height_offset = 0x08u;
        state->ball.action_age = 0u;
    }
    if (live_frame == 767u && state->possession_count == 0u) {
        dd_begin_inbound(state);
    }
    dd_update_camera(state);
    state->controlled_flash_palette = (uint8_t)(((live_frame / 2u) & 1u) == 0u);
    state->players[state->controlled_player].attributes = state->controlled_flash_palette;
    state->live_frame = live_frame;
    state->previous_input = input_mask;
}

int dd_gameplay_step(const DDAssetPack *pack, DDGameplayState *state, uint32_t input_mask) {
    const DDTipoffAssetsHeader *assets;
    uint32_t age;
    uint32_t sequence_frame;
    if (pack == NULL || state == NULL || pack->tipoff_assets == NULL ||
        pack->tipoff_assets_size < sizeof(DDTipoffAssetsHeader)) return 0;
    if (!state->initialized) dd_gameplay_reset(state);
    assets = (const DDTipoffAssetsHeader *)pack->tipoff_assets;
    if (state->scene_frame == UINT_MAX) return 0;
    ++state->scene_frame;
    ++state->sequence_frame;
    dd_step_game_clock(state);
    if (state->phase == DD_GAMEPLAY_GAME_SET) {
        if (state->game_set_age != UINT16_MAX) ++state->game_set_age;
        if (state->game_set_age >= DD_GAME_SET_TITLE_AGE) state->return_to_title = 1;
        return 1;
    }
    /* Original frames 45337/45338 show 00:00 for one rendered frame before
       fourth-period mode $00 installs GAME SET.  Unlike periods 1-3, this
       branch never prepares another tip formation. */
    if (state->clock_expired && state->period >= 4u &&
        state->scene_frame > state->clock_expired_frame &&
        state->players[2].action < DD_PLAYER_INBOUNDER &&
        (state->ball.action == DD_BALL_DRIBBLE ||
         state->ball.action == DD_BALL_REBOUND)) {
        state->phase = DD_GAMEPLAY_GAME_SET;
        state->game_set_age = 0u;
        state->carrier = DD_NO_OWNER;
        state->ball.owner = DD_NO_OWNER;
        return 1;
    }
    if (state->clock_expired && state->period < 4u &&
        state->scene_frame - state->clock_expired_frame >= DD_PERIOD_RESET_DELAY) {
        ++state->period;
        dd_prepare_period_formation(state);
        return 1;
    }
    sequence_frame = state->sequence_frame;
    if (sequence_frame < DD_TOSS_START_FRAME) return 1;
    if (sequence_frame == DD_TOSS_START_FRAME) {
        state->phase = DD_GAMEPLAY_TOSS;
        state->ball.action = DD_BALL_AIRBORNE;
        state->ball.owner = DD_NO_OWNER;
    }
    if (sequence_frame >= DD_TOSS_START_FRAME && sequence_frame <= DD_AWARD_FRAME &&
        state->tip_user_jump_frame == UINT_MAX && (input_mask & DD_INPUT_B) != 0u) {
        state->tip_user_jump_frame = sequence_frame;
        state->players[0].action = DD_PLAYER_TIP_USER_AIRBORNE;
        state->players[0].animation = 0x21u;
    }
    if (sequence_frame <= DD_AWARD_FRAME) {
        age = sequence_frame - DD_TOSS_START_FRAME;
        state->object_phase = (uint8_t)age;
        state->ball.height = 0x1800;
        if (age != 0u) {
            uint32_t tick;
            for (tick = 1u; tick <= age; ++tick) {
                state->ball.height += 0x0305 - (int32_t)((tick * 64u) / 3u);
            }
        }
        state->players[5].height = dd_jump_height(assets, sequence_frame);
        if (sequence_frame >= DD_JUMPER_START_FRAME) {
            state->players[5].animation = 0x21u;
            state->players[5].action = DD_PLAYER_TIP_CPU_AIRBORNE;
        }
        if (state->tip_user_jump_frame != UINT_MAX) {
            state->players[0].height = dd_scripted_jump_height(
                assets, sequence_frame, state->tip_user_jump_frame);
        }
    }
    if (sequence_frame == DD_AWARD_FRAME) {
        state->hud_split_y = 48u;
        state->phase = DD_GAMEPLAY_AWARD;
        state->carrier = 5u;
        state->tip_winner = 5u;
        state->ball.owner = state->carrier;
        state->ball.action = DD_BALL_AWARDED;
        state->ball.held_height_offset = 0x18u;
    }
    if (sequence_frame == DD_USER_AWARD_FRAME &&
        state->tip_user_jump_frame == DD_USER_JUMP_WIN_FRAME) {
        state->carrier = 0u;
        state->tip_winner = 0u;
        state->ball.owner = 0u;
        state->live_start_frame = DD_USER_LIVE_FRAME;
    }
    if (sequence_frame > DD_AWARD_FRAME && sequence_frame < state->live_start_frame) {
        if (state->tip_user_jump_frame != UINT_MAX) {
            state->players[0].height = dd_scripted_jump_height(
                assets, sequence_frame, state->tip_user_jump_frame);
        }
        state->players[5].height = dd_jump_height(assets, sequence_frame);
        dd_attach_ball(assets, state, 0u);
        state->ball.height = state->players[state->carrier].height + 0x1800;
    }
    if (sequence_frame == state->live_start_frame) {
        dd_begin_live(state, state->tip_winner);
        state->previous_input = input_mask;
    } else if (state->phase == DD_GAMEPLAY_LIVE || state->phase == DD_GAMEPLAY_INBOUND ||
               state->phase == DD_GAMEPLAY_FREE_THROW) {
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
