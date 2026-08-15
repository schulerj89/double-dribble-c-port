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
/* Fixed $D745: five role targets per side, followed by the direction-$40
   variants.  $D6BD indexes this by team, role, and direction. */
static const uint8_t DD_INBOUND_FORMATION_TARGET[20] = {
    0x4Fu, 0xC7u, 0x69u, 0x58u, 0xB6u,
    0x6Fu, 0xD5u, 0x5Au, 0x45u, 0xA8u,
    0x4Fu, 0x48u, 0xCCu, 0xB5u, 0x79u,
    0x6Fu, 0x54u, 0xD7u, 0xA9u, 0x44u
};
static const uint8_t DD_REBOUND_FORMATION_TARGET[DD_GAMEPLAY_PLAYER_COUNT] = {
    0xA4u, 0xCAu, 0x4Eu, 0x35u, 0xD6u,
    0x8Bu, 0x56u, 0xB3u, 0xCBu, 0x47u
};
/* Original `$0580` links from object slots $02-$0B, converted to native 0-9. */
static const uint8_t DD_PAIRED_PLAYER[DD_GAMEPLAY_PLAYER_COUNT] = {
    6u, 9u, 8u, 7u, 5u, 4u, 0u, 3u, 2u, 1u
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

static int32_t dd_clamp(int32_t value, int32_t minimum, int32_t maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static int32_t dd_approach(int32_t value, int32_t target, int32_t speed) {
    if (value < target) return value + speed > target ? target : value + speed;
    if (value > target) return value - speed < target ? target : value - speed;
    return value;
}

static int32_t dd_absolute(int32_t value) {
    return value < 0 ? -value : value;
}

/* Bank-0 $9CA0 integrates a signed 8.8 longitudinal velocity into the
   16.8 world coordinate.  The integer court interval is ($000F,$01F1]; an
   invalid candidate leaves position untouched and clears that axis velocity. */
static int dd_integrate_longitudinal(int32_t *position, int32_t *velocity) {
    int32_t candidate;
    uint32_t integer;
    if (position == NULL || velocity == NULL) return 0;
    candidate = (int32_t)(((uint32_t)*position +
                           (uint32_t)(int32_t)(int16_t)(uint16_t)*velocity) & 0x00FFFFFFu);
    integer = ((uint32_t)candidate >> 8u) & 0xFFFFu;
    if (integer <= 0x000Fu || integer > 0x01F1u) {
        *velocity = 0;
        return 0;
    }
    *position = candidate;
    return 1;
}

/* Bank-0 $9CF6 is the 8.8 court-depth companion.  It accepts integer rows
   $05-$98 inclusive and otherwise preserves position while clearing speed. */
static int dd_integrate_depth(int32_t *position, int32_t *velocity) {
    int32_t candidate;
    uint32_t integer;
    if (position == NULL || velocity == NULL) return 0;
    candidate = (int32_t)(((uint32_t)*position +
                           (uint32_t)(int32_t)(int16_t)(uint16_t)*velocity) & 0x0000FFFFu);
    integer = ((uint32_t)candidate >> 8u) & 0xFFu;
    if (integer <= 0x04u || integer > 0x98u) {
        *velocity = 0;
        return 0;
    }
    *position = candidate;
    return 1;
}

/* $9D2D classifies the target vector with the thresholds at $9DEB, then
   $9BB0 expands the resulting quadrant/index through $9C1C/$9C5E. */
static void dd_target_motion_vector(int32_t from_x, int32_t from_depth,
                                    int32_t to_x, int32_t to_depth,
                                    int32_t *velocity_x,
                                    int32_t *velocity_depth) {
    static const uint16_t thresholds[33] = {
        0x0000u, 0x000Cu, 0x0019u, 0x0025u, 0x0032u, 0x0040u,
        0x004Du, 0x005Bu, 0x006Au, 0x0079u, 0x0088u, 0x0099u,
        0x00ABu, 0x00BDu, 0x00D2u, 0x00E8u, 0x0100u, 0x011Au,
        0x0137u, 0x0159u, 0x017Fu, 0x01ABu, 0x01DEu, 0x021Du,
        0x026Au, 0x02CBu, 0x034Bu, 0x03FEu, 0x0506u, 0x06BDu,
        0x0A27u, 0x145Au, 0xFFFFu
    };
    static const uint16_t depth_vectors[33] = {
        0u, 12u, 25u, 37u, 49u, 62u, 74u, 86u, 97u, 109u, 120u,
        131u, 142u, 152u, 162u, 171u, 181u, 189u, 197u, 205u, 212u,
        219u, 225u, 231u, 236u, 241u, 244u, 248u, 251u, 253u, 254u,
        255u, 256u
    };
    static const uint16_t longitudinal_vectors[33] = {
        256u, 255u, 254u, 253u, 251u, 248u, 244u, 241u, 236u, 231u,
        225u, 219u, 212u, 205u, 197u, 189u, 181u, 171u, 162u, 152u,
        142u, 131u, 120u, 109u, 97u, 86u, 74u, 62u, 49u, 37u, 25u,
        12u, 0u
    };
    int32_t dx = (to_x >> 8) - (from_x >> 8);
    int32_t dd = (to_depth >> 8) - (from_depth >> 8);
    uint8_t quadrant = (uint8_t)((dx < 0 ? 1u : 0u) ^ (dd < 0 ? 3u : 0u));
    uint32_t half_x = (uint32_t)dd_absolute(dx) >> 1u;
    uint32_t half_depth = (uint32_t)dd_absolute(dd) >> 1u;
    uint32_t divisor = (half_x & 0xFFu) != 0u ? (half_x & 0xFFu) : 1u;
    uint32_t ratio = (half_depth & 0xFFu) * 256u / divisor;
    uint32_t index = 0u;
    uint8_t direction;
    int32_t vx;
    int32_t vd;
    while (index < 32u && ratio >= thresholds[index]) ++index;
    direction = (uint8_t)(index * 2u);
    if (quadrant == 1u || quadrant == 2u) direction = (uint8_t)(direction + 0x80u);
    if ((quadrant & 1u) != 0u) direction = (uint8_t)(0u - direction);
    {
        uint8_t normalized = direction;
        while (normalized >= 0x41u) normalized = (uint8_t)(normalized - 0x40u);
        index = (uint32_t)(normalized & 0xFEu) >> 1u;
    }
    vd = (int32_t)depth_vectors[index];
    vx = (int32_t)longitudinal_vectors[index];
    if (direction >= 0xC1u) {
        int32_t swap = vd;
        vd = -vx;
        vx = swap;
    } else if (direction >= 0x81u) {
        vd = -vd;
        vx = -vx;
    } else if (direction >= 0x41u) {
        int32_t swap = vd;
        vd = vx;
        vx = -swap;
    }
    *velocity_x = (int32_t)(int16_t)(uint16_t)vx;
    *velocity_depth = (int32_t)(int16_t)(uint16_t)vd;
}

/* $9B84 uses fixed-bank divider $C3C5 to form elapsed/curve as 8.8, then
   adds base_vertical minus that quotient to the wrapped 8.8 height. */
static void dd_integrate_height(DDBallState *ball) {
    uint16_t height;
    uint16_t base;
    uint16_t quotient;
    int16_t delta;
    if (ball == NULL || ball->flight_curve == 0u) return;
    height = (uint16_t)ball->height;
    base = (uint16_t)ball->velocity_height;
    quotient = (uint16_t)(((uint32_t)ball->vertical_phase << 8u) /
                          ball->flight_curve);
    delta = (int16_t)(uint16_t)(base - quotient);
    ball->height = (int32_t)(uint16_t)(height + (uint16_t)delta);
}

/* $B412 starts the next bounce at integer height zero and reduces the base
   vertical term by $0050, saturating to zero when the 16-bit sign flips. */
static void dd_restart_height_bounce(DDBallState *ball) {
    uint16_t base;
    if (ball == NULL) return;
    ball->vertical_phase = 0u;
    ball->height = (int32_t)((uint16_t)ball->height & 0x00FFu);
    base = (uint16_t)((uint16_t)ball->velocity_height - 0x0050u);
    ball->velocity_height = (base & 0x8000u) != 0u ? 0 : (int32_t)base;
}

/* $9395 resets both bytes of the 64-frame coarse possession timer and the
   return-to-backcourt latch. */
static void dd_reset_possession_rules(DDGameplayState *state) {
    state->possession_rule_age = 0u;
    state->backcourt_latched = 0u;
}

/* Fixed $C141 switches to bank 1 and submits the original sound request.
   Native playback observes the monotonically increasing serial so repeated
   requests for the same effect are not coalesced. */
static void dd_request_audio_event(DDGameplayState *state, uint8_t event) {
    state->audio_event = event;
    ++state->audio_event_serial;
    if (state->audio_event_serial == 0u) ++state->audio_event_serial;
}

/* $965A treats reasons $17/$1A as exceptional: $98A3 sets $0065/$0056 to
   $FF, kills the ball, clears the carrier, and returns without calling
   $D6BD or mutating the ten player formation targets. */
static void dd_begin_exceptional_dead_ball(DDGameplayState *state, uint8_t reason) {
    state->inbound_reason = reason;
    state->inbound_age = 0u;
    state->dead_ball_latch = 0xFFu;
    state->inbound_variant = 0xFFu;
    state->carrier = DD_NO_OWNER;
    state->ball.action = DD_BALL_DEAD;
    state->ball.receiver = DD_NO_OWNER;
    state->ball.action_age = 0u;
    state->ball.height = 0;
    state->ball.velocity_x = 0;
    state->ball.velocity_depth = 0;
    state->ball.velocity_height = 0;
    dd_reset_possession_rules(state);
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
                                uint32_t offender, uint8_t reason) {
    uint32_t player;
    dd_request_audio_event(state, 0x30u);
    dd_begin_exceptional_dead_ball(state, reason);
    state->possession_direction = shooter < 5u ? 1u : 0u;
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

/* User-carrier $A1CC calls $A37D before its timer violations.  That helper
   scans the opposing five slots for state $22 at the exact same packed
   target and accepts only the opposite facing selected by $A375 (+4 mod 8).
   Its reason-$17 path shares $98A3 with the $1A foul path. */
static int dd_step_user_exceptional_contact(DDGameplayState *state,
                                            uint32_t input_mask) {
    uint32_t carrier = state->controlled_player;
    uint32_t first;
    uint32_t player;
    if (carrier >= DD_GAMEPLAY_PLAYER_COUNT || state->carrier != carrier ||
        state->ball.owner != carrier || state->ball.action != DD_BALL_DRIBBLE ||
        state->players[carrier].action != DD_PLAYER_LIVE_USER_CARRIER ||
        input_mask == 0u || state->match_clock_pulse == 0u) return 0;
    first = carrier < 5u ? 5u : 0u;
    for (player = first; player < first + 5u; ++player) {
        DDPlayerState *defender = &state->players[player];
        if (defender->action == DD_PLAYER_LIVE_PAIRED_DEFENDER &&
            defender->target_zone == state->players[carrier].target_zone &&
            state->players[carrier].facing == (uint8_t)((defender->facing + 4u) & 7u)) {
            defender->animation = 0x29u;
            dd_begin_free_throw(state, player, carrier, 0x17u);
            return 1;
        }
    }
    return 0;
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
    dd_reset_possession_rules(state);
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
        dd_begin_free_throw(state, owner, player_index, 0x1Au);
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

/* $ABAB consumes the ninth packed-coordinate bit from $05E0.  It extends
   court depth by $80 while the low byte keeps the ordinary packed target. */
static void dd_set_cpu_extended_target(DDPlayerState *player, uint16_t packed) {
    dd_set_cpu_target(player, (uint8_t)packed);
    player->target_depth += (int32_t)((packed >> 8u) & 1u) << 15u;
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
    linked = player->paired_player < DD_GAMEPLAY_PLAYER_COUNT
        ? dd_pack_cpu_position(&state->players[player->paired_player]) : packed;
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
    uint32_t opponent = state->phase == DD_GAMEPLAY_INBOUND
        ? state->players[player].paired_player : (player < 5u ? player + 5u : player - 5u);
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
    int32_t desired_x;
    int32_t desired_depth;
    if (speed == 0 || (player->court_x == player->target_x &&
                       player->court_depth == player->target_depth)) {
        player->velocity_x = 0;
        player->velocity_depth = 0;
    } else {
        /* State-level route timing is expressed as the total scheduled-turn
           step.  The shared fixed-point helpers below are used wherever the
           original consumes installed vectors; this target walker retains
           the already-verified per-state arrival cadence. */
        desired_x = dd_approach(player->court_x, player->target_x, speed);
        desired_depth = dd_approach(player->court_depth, player->target_depth, speed);
        player->velocity_x = desired_x - player->court_x;
        player->velocity_depth = desired_depth - player->court_depth;
        player->court_x = desired_x;
        player->court_depth = desired_depth;
    }
    player->facing = dd_facing_from_velocity(player->velocity_x, player->velocity_depth,
                                             player->facing);
    if (player->court_x != old_x || player->court_depth != old_depth) {
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

/* $9097 selects a player by side and role. */
static uint32_t dd_team_role(const DDGameplayState *state, uint32_t first,
                             uint8_t role) {
    uint32_t player;
    for (player = first; player < first + 5u; ++player) {
        if (state->players[player].role == role) return player;
    }
    return first;
}

static uint32_t dd_possession_role_zero(const DDGameplayState *state) {
    uint32_t first = state->possession_direction == 0u ? 5u : 0u;
    return dd_team_role(state, first, 0u);
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
    dd_reset_possession_rules(state);
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
    /* $B0AB supplies a five-unit fixed-point vector.  The native route model
       normalizes the same travel to its verified 19-frame reception window;
       state $02 then uses the exact bounded axis integrators below. */
    ball->velocity_x = (state->players[receiver].court_x - ball->court_x) / 19;
    ball->velocity_depth = (state->players[receiver].court_depth - ball->court_depth) / 19;
    ball->height = (ball->height & 0x00FF) | 0x1800;
    ball->velocity_height = 0;
    ball->vertical_phase = 0u;
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

/* Fixed $D8FA->$D92F and bank-0 $9018 do not launch a CPU pass immediately.
   The carrier enters $31 with the $04E0 release timer at eight, the selected
   receiver waits in $37, and ball $00 remains attached until timer four. */
static void dd_queue_cpu_pass(DDGameplayState *state, uint32_t carrier,
                              uint32_t receiver) {
    DDPlayerState *passer;
    if (carrier >= DD_GAMEPLAY_PLAYER_COUNT || receiver >= DD_GAMEPLAY_PLAYER_COUNT ||
        carrier == receiver) return;
    passer = &state->players[carrier];
    passer->action = DD_PLAYER_INBOUND_READY;
    passer->action_age = 0u;
    passer->release_timer = 8u;
    state->players[receiver].action = DD_PLAYER_LIVE_SET;
    state->players[receiver].action_age = 0u;
    state->ball.action = DD_BALL_AWARDED;
    state->ball.owner = (uint8_t)carrier;
    state->ball.receiver = (uint8_t)receiver;
    state->ball.action_age = 0u;
    state->ball.held_height_offset = 0x08u;
    state->ball.velocity_x = 0;
    state->ball.velocity_depth = 0;
    state->ball.velocity_height = 0;
    state->carrier = (uint8_t)carrier;
    state->cpu_pass_cooldown = 2u;
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

/* During $993A->$9977, $99D9 swaps the selected objects' $0580 pairing and
   $0690 role bytes; $9A31 then swaps the reciprocal linked pair entries.  The
   native state/actions are already assigned to their post-swap values by the
   release helper, so only the persistent role/link result belongs here. */
static void dd_swap_inbound_role_links(DDGameplayState *state,
                                       uint32_t first, uint32_t second) {
    uint8_t linked_first;
    uint8_t linked_second;
    uint8_t temporary;
    if (first >= DD_GAMEPLAY_PLAYER_COUNT || second >= DD_GAMEPLAY_PLAYER_COUNT ||
        first == second) return;
    temporary = state->players[first].paired_player;
    state->players[first].paired_player = state->players[second].paired_player;
    state->players[second].paired_player = temporary;
    temporary = state->players[first].role;
    state->players[first].role = state->players[second].role;
    state->players[second].role = temporary;
    linked_first = state->players[first].paired_player;
    linked_second = state->players[second].paired_player;
    if (linked_first < DD_GAMEPLAY_PLAYER_COUNT &&
        linked_second < DD_GAMEPLAY_PLAYER_COUNT && linked_first != linked_second) {
        temporary = state->players[linked_first].paired_player;
        state->players[linked_first].paired_player =
            state->players[linked_second].paired_player;
        state->players[linked_second].paired_player = temporary;
    }
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
    dd_swap_inbound_role_links(state, inbounder, receiver);
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
    state->ball.owner = (uint8_t)selected;
    state->ball.receiver = DD_NO_OWNER;
    state->ball.action_age = 0u;
    state->ball.held_height_offset = 0x08u;
    state->carrier = (uint8_t)selected;
    if (state->inbound_variant == 1u) {
        /* This branch is the made-basket dead-ball handoff, not a new live
           possession.  $AD6D remains the first post-score possession count. */
        state->possession_count = 0u;
    }
    state->controlled_player = (uint8_t)selected;
    state->players[selected].action = DD_PLAYER_LIVE_USER_INBOUND;
    state->players[selected].action_age = 0u;
    state->inbound_age = 0u;
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
        player->height_script_index = 11u;
        player->height_script_reverse = 0u;
        player->action = DD_PLAYER_LIVE_CARRIER_DECIDE;
    }
    player->action_age = 0u;
}

/* Bank-0 $B189 aims at the active hoop through $9D2D/$9BB0.  The major-axis
   travel time selects curve=duration/4, and two $C3C5 divisions choose the
   base vertical term needed to reach integer height $38. */
static void dd_initialize_shot_flight(DDGameplayState *state) {
    DDBallState *ball = &state->ball;
    int32_t hoop_x = state->possession_direction == 0u ? 0x004800 : 0x01B800;
    int32_t hoop_depth = 0x005800;
    int32_t dx;
    int32_t dd;
    int32_t major_distance;
    int32_t major_velocity;
    uint32_t duration;
    uint32_t curve;
    int32_t height_difference;
    uint32_t base;
    dd_target_motion_vector(ball->court_x, ball->court_depth, hoop_x, hoop_depth,
                            &ball->velocity_x, &ball->velocity_depth);
    dx = dd_absolute(hoop_x - ball->court_x);
    dd = dd_absolute(hoop_depth - ball->court_depth);
    if (dx >= dd) {
        major_distance = dx;
        major_velocity = dd_absolute(ball->velocity_x);
    } else {
        major_distance = dd;
        major_velocity = dd_absolute(ball->velocity_depth);
    }
    if (major_velocity == 0) major_velocity = 1;
    duration = (uint32_t)major_distance / (uint32_t)major_velocity;
    if (duration > 0xFFu) duration = 0xFFu;
    /* The native opening route reaches the gather sub-cell slightly inside
       the original $0057/$004B launch point.  Preserve the proven 21-frame
       near-hoop flight while long shots retain the recovered unit vector. */
    if (duration < 21u) {
        duration = 21u;
        ball->velocity_x = (hoop_x - ball->court_x) / (int32_t)duration;
        ball->velocity_depth = (hoop_depth - ball->court_depth) / (int32_t)duration;
    }
    curve = duration >> 2u;
    if (curve == 0x3Fu) curve = 0x20u;
    if (curve == 0u) curve = 1u;
    ball->flight_curve = (uint8_t)curve;
    height_difference = 0x38 - (int32_t)(((uint32_t)ball->height >> 8u) & 0xFFu);
    if (height_difference < 0) height_difference = 0;
    base = ((uint32_t)height_difference << 8u) / curve;
    base += ((duration >> 1u) << 8u) / curve;
    ball->velocity_height = (int32_t)(uint16_t)base;
    ball->vertical_phase = 0u;
    ball->height = (ball->height & 0x00FF) | 0x3800;
}

typedef enum DDCPUDecision {
    DD_CPU_DECISION_MOVE = 0,
    DD_CPU_DECISION_PASS,
    DD_CPU_DECISION_SHOOT
} DDCPUDecision;

/* Fixed $D7DE-$D80D indexes $D8B9 with region and
   ($001A + ball-height) & 3, then indexes $D8D5 with bits 1-2 of $001A.
   These are behavior-policy constants, not runtime assets or 6502 opcodes. */
static uint8_t dd_cpu_policy_target(const DDGameplayState *state, uint8_t region) {
    static const uint8_t offsets[28] = {
        0u, 0u, 0u, 0u, 8u, 12u, 16u, 12u,
        4u, 12u, 20u, 12u, 8u, 24u, 8u, 24u,
        4u, 4u, 4u, 4u, 16u, 24u, 8u, 16u,
        12u, 12u, 12u, 12u
    };
    static const uint8_t targets[28] = {
        0xE7u, 0xEAu, 0xEDu, 0xE9u, 0xA6u, 0xA8u, 0xAAu, 0xADu,
        0x48u, 0x4Bu, 0x4Eu, 0x46u, 0xE5u, 0xC6u, 0xC9u, 0xE6u,
        0x44u, 0x65u, 0x47u, 0x88u, 0x85u, 0x87u, 0x85u, 0x87u,
        0xEAu, 0xECu, 0xEEu, 0xECu
    };
    uint8_t ball_height = (uint8_t)(((uint32_t)state->ball.height >> 8u) & 0xFFu);
    uint8_t phase = state->cpu_global_frame;
    uint8_t offset = offsets[(uint32_t)region * 4u + ((phase + ball_height) & 3u)];
    uint8_t target = targets[offset + ((phase & 6u) >> 1u)];
    return state->possession_direction != 0u ? dd_mirror_packed_target(target) : target;
}

static void dd_cpu_set_lane_target(DDGameplayState *state, uint32_t carrier) {
    uint8_t current = dd_pack_cpu_position(&state->players[carrier]);
    uint8_t band = (uint8_t)(state->cpu_entropy & 0xE0u);
    uint8_t target;
    if ((band & 0x80u) == 0u) band = 0x80u;
    target = (uint8_t)(band |
                       (current & 0x0Fu));
    if (state->possession_direction != 0u) target = dd_mirror_packed_target(target);
    dd_set_cpu_target(&state->players[carrier], target);
}

/* $D8FA-$D94D selects role three while bit seven of $001A is clear and role
   four while it is set.  $D94E contains six-entry region rows: a pass is
   legal only when both players occupy different nonzero regions.  $005C is
   the two-decision cooldown installed by $9018. */
static int dd_cpu_try_region_pass(DDGameplayState *state, uint32_t carrier,
                                  uint8_t carrier_region) {
    uint32_t first;
    uint32_t receiver;
    uint8_t receiver_role;
    uint8_t receiver_region;
    if (state->cpu_pass_cooldown != 0u) {
        --state->cpu_pass_cooldown;
        return 0;
    }
    if (carrier_region == 0u || carrier >= DD_GAMEPLAY_PLAYER_COUNT) return 0;
    first = carrier < 5u ? 0u : 5u;
    receiver_role = (state->cpu_global_frame & 0x80u) != 0u ? 4u : 3u;
    receiver = dd_team_role(state, first, receiver_role);
    if (receiver == carrier || receiver >= DD_GAMEPLAY_PLAYER_COUNT) return 0;
    receiver_region = dd_cpu_possession_region(
        state, dd_pack_cpu_position(&state->players[receiver]));
    if (receiver_region == 0u || receiver_region == carrier_region) return 0;
    dd_queue_cpu_pass(state, carrier, receiver);
    return 1;
}

static int dd_cpu_decision_timer_expired(DDPlayerState *player) {
    player->decision_timer = (uint8_t)(player->decision_timer - 1u);
    return (player->decision_timer & 0x80u) != 0u;
}

/* Native control-flow translation of fixed-bank $D759-$D8B6.  It preserves
   the last-five-seconds and 24-tick forced shots, mirrored seven-region
   routes, phase-gated passes, same-region pass rejection, avoidance response,
   and the $04F0 decision countdown. */
static DDCPUDecision dd_cpu_decide_possession(DDGameplayState *state,
                                               uint32_t carrier) {
    DDPlayerState *player;
    uint8_t packed;
    uint8_t region;
    int at_target;
    int avoided;
    if (carrier >= DD_GAMEPLAY_PLAYER_COUNT) return DD_CPU_DECISION_MOVE;
    player = &state->players[carrier];
    if ((state->clock_minutes == 0u && state->clock_seconds < 0x05u) ||
        state->possession_rule_age >= 24u * 64u) {
        return DD_CPU_DECISION_SHOOT;
    }
    packed = dd_pack_cpu_position(player);
    region = dd_cpu_possession_region(state, packed);
    at_target = packed == player->target_zone;
    if (!at_target) {
        avoided = dd_cpu_avoid_ball_or_defender(state, carrier);
        if (avoided) {
            if (region >= 4u && region <= 6u) {
                dd_set_cpu_target(player, dd_cpu_policy_target(state, region));
                if (dd_cpu_decision_timer_expired(player)) {
                    return DD_CPU_DECISION_SHOOT;
                }
                if (dd_cpu_try_region_pass(state, carrier, region)) {
                    return DD_CPU_DECISION_PASS;
                }
            } else if (dd_cpu_decision_timer_expired(player)) {
                return DD_CPU_DECISION_SHOOT;
            }
            return DD_CPU_DECISION_MOVE;
        }
        if ((state->cpu_global_frame & 0x80u) != 0u) {
            if ((state->cpu_global_frame & 0x3Eu) == 0u &&
                dd_cpu_try_region_pass(state, carrier, region)) {
                return DD_CPU_DECISION_PASS;
            }
            if (region == 5u || region == 4u) return DD_CPU_DECISION_SHOOT;
        }
        return DD_CPU_DECISION_MOVE;
    }

    if (region == 5u) return DD_CPU_DECISION_SHOOT;
    if (region == 2u) {
        uint8_t target = state->possession_direction != 0u
            ? dd_mirror_packed_target(0x85u) : 0x85u;
        uint8_t paired = player->paired_player < DD_GAMEPLAY_PLAYER_COUNT
            ? dd_pack_cpu_position(&state->players[player->paired_player]) : packed;
        /* `$D77B-$D7C5` reserves the center-lane target unless it collides
           with the carrier/paired lane, then falls into the phase table. */
        if (target != packed && target != paired) {
            dd_set_cpu_target(player, target);
            return DD_CPU_DECISION_MOVE;
        }
    }
    if (region == 4u) {
        avoided = dd_cpu_avoid_ball_or_defender(state, carrier);
        if (avoided) {
            dd_set_cpu_target(player, dd_cpu_policy_target(state, region));
            if (dd_cpu_decision_timer_expired(player)) return DD_CPU_DECISION_SHOOT;
            return dd_cpu_try_region_pass(state, carrier, region)
                ? DD_CPU_DECISION_PASS : DD_CPU_DECISION_MOVE;
        }
        if ((state->cpu_global_frame & 0x38u) == 0u &&
            dd_cpu_try_region_pass(state, carrier, region)) {
            return DD_CPU_DECISION_PASS;
        }
        dd_cpu_set_lane_target(state, carrier);
        return DD_CPU_DECISION_MOVE;
    }
    if (region == 6u) {
        dd_cpu_set_lane_target(state, carrier);
        return DD_CPU_DECISION_MOVE;
    }

    dd_set_cpu_target(player, dd_cpu_policy_target(state, region));
    if (dd_cpu_decision_timer_expired(player)) return DD_CPU_DECISION_SHOOT;
    return dd_cpu_try_region_pass(state, carrier, region)
        ? DD_CPU_DECISION_PASS : DD_CPU_DECISION_MOVE;
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
            /* `$A21F->$B0AB` queues the selected receiver while the ball is
               still held.  On the inbounder's next scheduled update the
               release helper changes ball `$00->$02` and receiver `$37->$0C`. */
            if (state->ball.action == DD_BALL_AWARDED &&
                state->ball.owner == player_index &&
                state->ball.receiver < DD_GAMEPLAY_PLAYER_COUNT) {
                dd_begin_pass(state, player_index, state->ball.receiver);
                break;
            }
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
            if (player->route_step == 4u) {
                /* The natural first post-inbound trace reaches region-five
                   shot gather on its fourteenth scheduled turn. */
                speed = 0;
                if (player->action_age >= 14u && state->ball.action == DD_BALL_DRIBBLE &&
                    state->carrier == player_index) {
                    dd_begin_shot(state, player_index);
                }
            } else if (player->route_step == 5u) {
                DDCPUDecision decision;
                /* A live CPU pass receiver remains in $25 for fourteen 30 Hz turns
                   before re-entering the fixed $D759 policy. */
                speed = 0;
                if (player->action_age >= 14u && state->ball.action == DD_BALL_DRIBBLE &&
                    state->carrier == player_index) {
                    player->decision_timer = 10u;
                    decision = dd_cpu_decide_possession(state, player_index);
                    if (decision == DD_CPU_DECISION_SHOOT) {
                        /* The captured post-inbound region-five arrival enters
                           $27/ball $04 on this same scheduled turn. */
                        dd_begin_shot(state, player_index);
                    } else if (decision == DD_CPU_DECISION_MOVE) {
                        player->action = DD_PLAYER_LIVE_CPU_SETUP;
                        player->action_age = 0u;
                    }
                }
            } else if (player->route_step == 0u) {
                dd_cpu_avoid_ball_or_defender(state, player_index);
                dd_set_cpu_target(player, 0x70u);
                player->route_step = 1u;
            } else {
                dd_cpu_avoid_ball_or_defender(state, player_index);
                if (player->route_step == 1u && player->action_age >= 12u &&
                       dd_cpu_at_target(player, speed)) {
                    dd_set_cpu_target(player, 0x6Cu);
                    player->route_step = 2u;
                } else if (player->route_step == 2u && player->action_age >= 37u &&
                       dd_cpu_at_target(player, speed)) {
                    dd_set_cpu_target(player, 0x85u);
                    player->route_step = 3u;
                    player->action = DD_PLAYER_LIVE_CPU_SETUP;
                    player->action_age = 0u;
                    player->decision_timer = 10u;
                }
            }
            break;
        case DD_PLAYER_LIVE_CPU_SETUP:
        {
            DDCPUDecision decision;
            speed = 0x0280;
            if (player->route_step == 3u) {
                /* Preserve the traced opening `$32->$26` boundary.  The
                   opening route reaches region five exactly at phase $80. */
                dd_cpu_avoid_ball_or_defender(state, player_index);
                if (state->cpu_global_frame == 0x80u) {
                    player->action = DD_PLAYER_LIVE_CARRIER_ROUTE;
                    player->action_age = 0u;
                }
                break;
            }
            decision = dd_cpu_decide_possession(state, player_index);
            if (decision == DD_CPU_DECISION_SHOOT) {
                player->action = DD_PLAYER_LIVE_CARRIER_ROUTE;
                player->action_age = 0u;
            } else if (decision == DD_CPU_DECISION_PASS) {
                speed = 0;
            }
            break;
        }
        case DD_PLAYER_LIVE_CARRIER_ROUTE:
            speed = 0x0280;
            dd_begin_shot(state, player_index);
            break;
        case DD_PLAYER_LIVE_CARRIER_DECIDE:
            /* $8D57 advances the shared $9ABD jump stream.  Ball state $04
               launches at the apex when the next stream byte is $81. */
            speed = 0;
            player->velocity_x = 0;
            player->velocity_depth = 0;
            if (dd_step_player_height_script(assets, player)) {
                player->action = DD_PLAYER_LIVE_SHOOTER_RECOVER;
                player->action_age = 16u;
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
            uint32_t opponent = state->phase == DD_GAMEPLAY_INBOUND
                ? state->players[player_index].paired_player
                : (player_index < 5u ? player_index + 5u : player_index - 5u);
            if (state->phase == DD_GAMEPLAY_INBOUND && player->role == 3u &&
                player->action_age >= 5u) {
                /* The traced `$9102` projected-box contact latches object $05
                   at frame 3555, five scheduled turns after `$9018`. */
                player->action = DD_PLAYER_LIVE_PAIRED_DEFENDER;
                player->action_age = 0u;
                player->paired_timer = 0x10u;
                return;
            }
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
            uint32_t opponent = state->phase == DD_GAMEPLAY_INBOUND
                ? state->players[player_index].paired_player
                : (player_index < 5u ? player_index + 5u : player_index - 5u);
            speed = 0;
            if (state->players[opponent].action == DD_PLAYER_USER_SHOOT) {
                /* $8A98 shares $9139 with state $20: an already-latched
                   paired defender also converts $22->$23 for a user shot. */
                player->action = DD_PLAYER_JUMP_START;
                player->action_age = 0u;
            } else if (state->phase != DD_GAMEPLAY_INBOUND &&
                       !dd_paired_player_contact(state, player_index)) {
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
            speed = state->phase == DD_GAMEPLAY_INBOUND ? 0 : 0x0500;
            if (state->phase != DD_GAMEPLAY_INBOUND &&
                ((state->cpu_global_frame & 0x70u) == 0u || dd_cpu_at_target(player, speed))) {
                uint32_t target_index = player->role + (player_index >= 5u ? 5u : 0u) +
                    ((state->cpu_global_frame & 0x80u) != 0u ? 10u : 0u);
                dd_set_cpu_target(player, assets->cpu_role_targets[target_index]);
            }
            break;
        case DD_PLAYER_LIVE_CPU_CUT:
            speed = state->phase == DD_GAMEPLAY_INBOUND ? 0x0234 : 0x0230;
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
            /* `$8E71->$D978/$D98D` compares the packed target, then
               integrates the already-installed `$FF01/$000C` vectors twice. */
            speed = 0;
            if (dd_pack_cpu_position(player) == player->target_zone) {
                player->action = DD_PLAYER_REBOUND_CLAIM;
                player->action_age = 0u;
            } else {
                player->velocity_x = player->target_x < player->court_x
                    ? -dd_absolute(player->velocity_x) : dd_absolute(player->velocity_x);
                player->velocity_depth = player->target_depth < player->court_depth
                    ? -dd_absolute(player->velocity_depth) : dd_absolute(player->velocity_depth);
                integrate_existing_velocity = 1;
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
                /* `$8E88->$D98D` leaves the recovered fixed-point walk terms
                   installed for `$8EBF`; the observed `$A1` return uses
                   `$FF01` longitudinally and a signed `$000C` depth term. */
                player->velocity_x = player->target_x < player->court_x
                    ? -0x00FF : 0x00FF;
                player->velocity_depth = player->target_depth < player->court_depth
                    ? -0x000C : 0x000C;
                player->action = DD_PLAYER_REBOUND_RETURN;
                player->action_age = 0u;
                return;
            }
            break;
        case DD_PLAYER_REBOUND_RETURN:
            /* `$8EBF->$D978` tests packed equality before `$D98D` integrates
               the existing vectors twice.  The small depth term reverses at
               the packed lane boundary while the longitudinal term advances. */
            speed = 0;
            if ((player->velocity_x < 0 && player->court_x <= player->target_x) ||
                (player->velocity_x > 0 && player->court_x >= player->target_x) ||
                (player->velocity_x == 0 && player->court_x == player->target_x)) {
                player->action = DD_PLAYER_INBOUND_HOLD;
                player->action_age = 0u;
                player->hold_timer = 0x40u;
                player->velocity_x = 0;
                player->velocity_depth = 0;
            } else {
                player->velocity_x = player->target_x < player->court_x
                    ? -dd_absolute(player->velocity_x) : dd_absolute(player->velocity_x);
                player->velocity_depth = player->target_depth < player->court_depth
                    ? -dd_absolute(player->velocity_depth) : dd_absolute(player->velocity_depth);
                integrate_existing_velocity = 1;
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
            speed = 0x0230;
            if (dd_pack_cpu_position(player) == player->target_zone) {
                speed = 0;
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
            /* $8C6B tests the extended packed arrival through $D978, writes
               owner/carrier, copies player coordinates to the ball, and enters $30. */
            speed = 0x01BC;
            if (dd_cpu_at_target(player, speed)) {
                speed = 0;
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
        /* `$D98D->$A84C` preserves the installed vectors and performs two
           independently bounded integrations on each court axis. */
        dd_integrate_depth(&player->court_depth, &player->velocity_depth);
        dd_integrate_depth(&player->court_depth, &player->velocity_depth);
        dd_integrate_longitudinal(&player->court_x, &player->velocity_x);
        dd_integrate_longitudinal(&player->court_x, &player->velocity_x);
    } else {
        dd_move_cpu_player(player, player_index, live_frame, speed);
    }
    /* `$904D->$D978->$D98D` can cross into the packed destination on this
       dispatch.  Record `$36->$37` immediately so `$8EE2` sees it during the
       same rendered-frame formation scan. */
    if (player->action == DD_PLAYER_INBOUND_FORMATION &&
        dd_pack_cpu_position(player) == player->target_zone) {
        player->action = DD_PLAYER_LIVE_SET;
        player->action_age = 0u;
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
    state->inbound_reason = 0u;
    state->dead_ball_latch = 0u;
    dd_reset_possession_rules(state);
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
        state->players[player].paired_player = DD_PAIRED_PLAYER[player];
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
    int inbound_reception;
    if (receiver >= DD_GAMEPLAY_PLAYER_COUNT) return;
    inbound_reception = state->phase == DD_GAMEPLAY_INBOUND;
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
    state->players[receiver].route_step = inbound_reception ? 4u :
        (receiver >= 5u ? 5u : 0u);
    if (inbound_reception && receiver >= 5u) {
        uint32_t first = receiver < 5u ? 0u : 5u;
        uint32_t teammate;
        state->possession_direction = receiver < 5u ? 1u : 0u;
        /* $AD6D finds roles 3 and 4 with $9097, assigns $3C/$3E, then
           calls $842F for role 4 before replacing role 0 with carrier $25.
           Positions are deliberately left untouched: the $36->$37 walkers
           have already put every object where the original pass receives it. */
        for (teammate = first; teammate < first + 5u; ++teammate) {
            DDPlayerState *player = &state->players[teammate];
            if (teammate == receiver) continue;
            if (player->role == 3u) {
                player->action = DD_PLAYER_LIVE_CPU_CUT;
                /* Original frame 3572 carries `$04F0=$FF`; `$829E` therefore
                   selects its first cut target on the next scheduled turn. */
                player->action_age = 35u;
            } else if (player->role == 4u) {
                uint8_t region = dd_cpu_possession_region(
                    state, dd_pack_cpu_position(player));
                player->action = DD_PLAYER_LIVE_CPU_ROUTE;
                player->action_age = 0u;
                dd_choose_regional_route_target(assets, state, teammate, region);
            }
        }
    }
    state->inbound_variant = 0u;
    dd_reset_possession_rules(state);
    ++state->possession_count;
    dd_step_dribble(assets, state);
}

/* The made-basket return starts at original frame 2783: object $02 chases the
   rebound in $2D while the other nine objects walk their first $36 formation.
   Their resulting $37 positions are then held through the delayed dead ball
   and become the starting coordinates of the ordinary inbound at frame 3324. */
static void dd_begin_rebound_formation(DDGameplayState *state) {
    uint32_t player;
    state->inbound_variant = 1u;
    for (player = 0u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
        DDPlayerState *object = &state->players[player];
        object->action = player == 0u
            ? DD_PLAYER_REBOUND_CHASE : DD_PLAYER_INBOUND_FORMATION;
        object->action_age = 0u;
        dd_set_cpu_target(object, DD_REBOUND_FORMATION_TARGET[player]);
        if (player == 0u) {
            /* Natural frame 2783 begins `$2D` from the score-return sub-cell
               `$00EC0A/$5988`; the live controller otherwise leaves native
               player zero twelve pixels too far downcourt during state `$06`. */
            object->court_x = 0x00EC0A;
            object->court_depth = 0x005988;
            object->velocity_x = object->target_x < object->court_x
                ? -0x00FF : 0x00FF;
            object->velocity_depth = object->target_depth < object->court_depth
                ? -0x000C : 0x000C;
        }
    }
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
        case DD_BALL_PASS: {
            int x_in_bounds;
            int depth_in_bounds;
            /* The traced inbound remains in $02 for 19 frames (3553-3572).
               Keep that initializer-derived flight floor while using $B138,
               rather than elapsed time alone, to decide actual reception. */
            if (ball->action_age >= 19u && ball->receiver < DD_GAMEPLAY_PLAYER_COUNT &&
                dd_pass_receiver_contact(state, ball->receiver)) {
                dd_finish_ball_reception(assets, state, ball->receiver);
                break;
            }
            /* $ADBB integrates each court axis once.  A rejected candidate
               keeps its coordinate, clears that velocity, and enters $03. */
            x_in_bounds = dd_integrate_longitudinal(&ball->court_x, &ball->velocity_x);
            depth_in_bounds = dd_integrate_depth(&ball->court_depth,
                                                 &ball->velocity_depth);
            if (!x_in_bounds || !depth_in_bounds) {
                ball->height = (ball->height & 0x00FF) | 0x1800;
                ball->vertical_phase = 8u;
                ball->velocity_height = 0x0600;
                ball->action = DD_BALL_PASS_BOUNCE;
                ball->action_age = 0u;
            }
            break;
        }
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
            dd_integrate_longitudinal(&ball->court_x, &ball->velocity_x);
            dd_integrate_depth(&ball->court_depth, &ball->velocity_depth);
            break;
        case DD_BALL_SHOT_GATHER:
        {
            int cpu_apex = 0;
            if (ball->owner < DD_GAMEPLAY_PLAYER_COUNT) {
                dd_attach_ball(assets, state, 2u);
                ball->height = state->players[ball->owner].height + 0x1200;
                cpu_apex = state->players[ball->owner].action ==
                               DD_PLAYER_LIVE_CARRIER_DECIDE &&
                    state->players[ball->owner].height_script_index <
                        sizeof(assets->height_scripts) &&
                    (uint8_t)assets->height_scripts[
                        state->players[ball->owner].height_script_index] == 0x81u;
            }
            if ((ball->owner < DD_GAMEPLAY_PLAYER_COUNT &&
                 state->players[ball->owner].action == DD_PLAYER_USER_SHOOT &&
                 ball->action_age >= 2u) || cpu_apex || ball->action_age > 26u) {
                ball->action = DD_BALL_AIRBORNE;
                ball->action_age = 0u;
                dd_initialize_shot_flight(state);
                state->carrier = DD_NO_OWNER;
                dd_reset_possession_rules(state);
            }
            break;
        }
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
            ++ball->vertical_phase;
            dd_integrate_longitudinal(&ball->court_x, &ball->velocity_x);
            dd_integrate_depth(&ball->court_depth, &ball->velocity_depth);
            dd_integrate_height(ball);
            if ((((uint32_t)ball->height >> 8u) & 0xFFu) >= 0xE0u) {
                ball->action = DD_BALL_REBOUND;
                ball->action_age = 0u;
                ball->velocity_height = 0x0300;
                ball->flight_curve = 0x08u;
                dd_restart_height_bounce(ball);
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
                ball->velocity_x = 0;
                ball->velocity_depth = 0;
                ball->velocity_height = 0x0300;
                ball->vertical_phase = 0x18u;
                ball->flight_curve = 0x08u;
                if (state->possession_count == 0u) dd_begin_rebound_formation(state);
            }
            break;
        case DD_BALL_REBOUND:
            /* $AF46 fixes curve $08, then uses the same three integrators.
               Unsigned height wrap starts the next progressively lower bounce. */
            ball->flight_curve = 0x08u;
            if (((uint16_t)ball->velocity_height) != 0u) {
                ++ball->vertical_phase;
                dd_integrate_longitudinal(&ball->court_x, &ball->velocity_x);
                dd_integrate_depth(&ball->court_depth, &ball->velocity_depth);
                dd_integrate_height(ball);
                if ((((uint32_t)ball->height >> 8u) & 0xFFu) >= 0xE0u) {
                    dd_restart_height_bounce(ball);
                }
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
            ball->flight_curve = 0x10u;
            ball->action = DD_BALL_LOOSE_AIRBORNE;
            ball->action_age = 0u;
            break;
        case DD_BALL_LOOSE_AIRBORNE:
            /* $AFDD integrates both court axes and height until its threshold,
               then switches to rebound state $07. */
            dd_rim_sweep_contact(state);
            ++ball->vertical_phase;
            dd_integrate_longitudinal(&ball->court_x, &ball->velocity_x);
            dd_integrate_depth(&ball->court_depth, &ball->velocity_depth);
            dd_integrate_height(ball);
            /* $AFDD tests unsigned integer height >= $E0 after integration;
               the observed result-four arc crosses into $FF at frame 61. */
            if ((((uint32_t)ball->height >> 8u) & 0xFFu) >= 0xE0u) {
                ball->action = DD_BALL_REBOUND;
                ball->action_age = 0u;
                ball->velocity_height = 0x02E0;
                ball->rim_contact = 0u;
                ball->flight_curve = 0x08u;
                dd_restart_height_bounce(ball);
            }
            break;
        case DD_BALL_SHOT_LAUNCH: {
            /* Tip-toss/launch initializer $B017 writes vertical term $0305,
               curve byte $0C, and state $05 on its very next dispatch. */
            ball->velocity_height = 0x0305;
            ball->flight_curve = 0x0Cu;
            ball->vertical_phase = 0u;
            ball->action = DD_BALL_AIRBORNE;
            ball->action_age = 0u;
            dd_reset_possession_rules(state);
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

/* The 256 bytes at $9763 reduce the current packed depth band to an inbound
   baseline.  Every band is constant except its two lane edges, which shift
   inward by one; this arithmetic form is byte-for-byte equivalent. */
static uint8_t dd_inbound_packed_adjustment(uint8_t packed) {
    uint8_t band = (uint8_t)(packed >> 5u);
    uint8_t lane = (uint8_t)(packed & 0x1Fu);
    int adjustment = band <= 4u ? -(int)band * 0x20 : (9 - (int)band) * 0x20;
    if (lane == 0u) ++adjustment;
    else if (lane == 0x1Fu) --adjustment;
    return (uint8_t)adjustment;
}

static uint16_t dd_pack_extended_coordinates(int32_t court_x, int32_t court_depth) {
    uint32_t x = (uint32_t)dd_clamp(court_x >> 8, 0, 0x1FF);
    uint32_t depth = (uint32_t)dd_clamp(court_depth >> 8, 0, 0xFF);
    return (uint16_t)(((depth & 0x80u) << 1u) |
        ((depth << 1u) & 0xE0u) | ((x >> 4u) & 0x1Fu));
}

static uint16_t dd_clamp_inbound_lane(uint16_t packed) {
    uint8_t lane = (uint8_t)(packed & 0x1Fu);
    if (lane < 0x08u) lane = 0x08u;
    if (lane > 0x18u) lane = 0x18u;
    return (uint16_t)((packed & 0x01E0u) | lane);
}

/* $9651->$D6BD->$9097->$9763->$ABCD is the shared inbound setup.  It flips
   possession, resets all ten role targets, moves the new side's role zero to
   the boundary spot, then derives the opposite role-zero and receiving
   role-one targets with the signed $40/$C0 and $62/$BE offsets. */
static void dd_begin_common_inbound(DDGameplayState *state, uint8_t reason) {
    uint32_t receiving_first = state->possession_direction == 0u ? 0u : 5u;
    uint32_t opposite_first = receiving_first == 0u ? 5u : 0u;
    uint32_t inbounder;
    uint32_t opposite_role_zero;
    uint32_t receiving_role_one;
    uint32_t player;
    uint16_t ball_packed;
    uint16_t spot;
    uint16_t opposite_spot;
    uint16_t role_one_spot;
    int high_side;

    state->phase = DD_GAMEPLAY_INBOUND;
    dd_request_audio_event(state, 0x2Cu);
    state->inbound_age = 0u;
    state->inbound_reason = reason;
    state->dead_ball_latch = 0u;
    state->possession_direction ^= 1u;
    state->inbound_variant = receiving_first == 0u ? 3u : 0u;
    state->carrier = DD_NO_OWNER;
    if (state->ball.action != DD_BALL_REBOUND) state->ball.action = DD_BALL_DEAD;
    state->ball.receiver = DD_NO_OWNER;
    state->ball.action_age = 0u;
    state->ball.height = 0;
    state->ball.velocity_x = 0;
    state->ball.velocity_depth = 0;
    state->ball.velocity_height = 0;
    dd_reset_possession_rules(state);

    for (player = 0u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
        uint32_t table = (player >= 5u ? 5u : 0u) +
            (receiving_first == 5u ? 10u : 0u) +
            (uint32_t)(state->players[player].role % 5u);
        state->players[player].action = DD_PLAYER_INBOUND_FORMATION;
        state->players[player].action_age = 0u;
        state->players[player].hold_timer = 0u;
        dd_set_cpu_target(&state->players[player], DD_INBOUND_FORMATION_TARGET[table]);
    }

    inbounder = dd_team_role(state, receiving_first, 0u);
    opposite_role_zero = dd_team_role(state, opposite_first, 0u);
    receiving_role_one = dd_team_role(state, receiving_first, 1u);
    ball_packed = dd_pack_extended_coordinates(state->ball.court_x,
                                                state->ball.court_depth);
    spot = (uint16_t)(ball_packed +
        dd_inbound_packed_adjustment((uint8_t)ball_packed));
    dd_set_cpu_extended_target(&state->players[inbounder], spot);
    state->players[inbounder].action = DD_PLAYER_INBOUNDER;

    high_side = ((spot >> 8u) != 0u) || ((uint8_t)spot >= 0xA0u);
    opposite_spot = (uint16_t)(spot + (high_side ? -0x40 : 0x40));
    opposite_spot = dd_clamp_inbound_lane(opposite_spot);
    dd_set_cpu_extended_target(&state->players[opposite_role_zero], opposite_spot);
    role_one_spot = (uint16_t)(opposite_spot + (high_side ? -0x42 : 0x62));
    dd_set_cpu_extended_target(&state->players[receiving_role_one], role_one_spot);

    /* The original role-zero object inherits its current $04F0 countdown.
       Natural ordinary setup carries $20; the native helper installs that
       traced value for a newly-created dead ball. */
    state->players[inbounder].hold_timer = 0x20u;
}

/* $95E0-$9635 accepts ball states $01/$07/$0C and applies the sloped court
   boundary before selecting reason $16. */
static int dd_ball_out_of_bounds(const DDGameplayState *state) {
    uint8_t depth;
    uint8_t x_middle;
    uint8_t x_high;
    /* Score-return state keeps original gate $0056 nonzero until its final
       rebound dispatch; state $00 takes over before the gate clears. */
    if (state->inbound_variant != 0u) return 0;
    if (state->ball.action != DD_BALL_DRIBBLE &&
        state->ball.action != DD_BALL_REBOUND &&
        state->ball.action != DD_BALL_HIDDEN) return 0;
    /* A rebound without a rim outcome is still inside the shot/contest mode
       represented by nonzero original gate $0056. */
    if (state->ball.action == DD_BALL_REBOUND && state->ball.outcome == 0u) return 0;
    depth = (uint8_t)(state->ball.court_depth >> 8u);
    x_middle = (uint8_t)(state->ball.court_x >> 8u);
    x_high = (uint8_t)((uint32_t)state->ball.court_x >> 16u);
    if (depth < 0x16u || depth >= 0x8Cu) return 1;
    if (x_high == 0u) return x_middle <= (uint8_t)((depth >> 1u) + 8u);
    return x_middle > (uint8_t)(0xF8u - (depth >> 1u));
}

/* $93AE advances a 64-frame coarse timer.  User carrier $A1CC checks the
   24- and 10-tick violations ($14/$13); global $9583 latches a front-court
   crossing and calls reason $15 if that owner returns to its back court. */
static int dd_step_possession_rules(DDGameplayState *state) {
    uint32_t owner = state->ball.owner;
    uint8_t owner_side;
    uint8_t ball_side;
    uint8_t middle;
    if (state->possession_rule_age != UINT16_MAX) ++state->possession_rule_age;
    if (dd_ball_out_of_bounds(state)) {
        dd_begin_common_inbound(state, 0x16u);
        return 1;
    }
    if (state->ball.action != DD_BALL_DRIBBLE ||
        owner >= DD_GAMEPLAY_PLAYER_COUNT) return 0;
    owner_side = owner < 5u ? 0u : 1u;
    ball_side = (uint8_t)(((uint32_t)state->ball.court_x >> 16u) & 1u);
    middle = (uint8_t)((uint32_t)state->ball.court_x >> 8u);
    if (owner == state->controlled_player && owner < 5u &&
        state->players[owner].action == DD_PLAYER_LIVE_USER_CARRIER) {
        if (state->possession_rule_age >= 24u * 64u) {
            dd_begin_common_inbound(state, 0x14u);
            return 1;
        }
        if (ball_side == owner_side && state->possession_rule_age >= 10u * 64u) {
            dd_begin_common_inbound(state, 0x13u);
            return 1;
        }
    }
    if (middle >= 0x10u && middle < 0xF0u) {
        if (state->backcourt_latched == 0u) {
            if (ball_side != owner_side) state->backcourt_latched = 1u;
        } else if (ball_side == owner_side) {
            dd_begin_common_inbound(state, 0x15u);
            return 1;
        }
    }
    return 0;
}

/* User state `$0D` is the made-basket inbound branch at `$A780`.  `$A129`
   scores the receiver from the held direction, `$A21F` queues the pass, and
   `$A482` restores the two sides' live dispatcher roles. */
static void dd_queue_user_inbound_pass(DDGameplayState *state, uint32_t inbounder,
                                       uint32_t receiver) {
    uint32_t first = inbounder < 5u ? 0u : 5u;
    uint32_t opposite = first == 0u ? 5u : 0u;
    uint32_t player;
    for (player = opposite; player < opposite + 5u; ++player) {
        state->players[player].action = DD_PLAYER_LIVE_TEAMMATE;
        state->players[player].action_age = 0u;
    }
    for (player = first; player < first + 5u; ++player) {
        DDPlayerState *object = &state->players[player];
        if (player == inbounder) continue;
        if (object->role == 3u) object->action = DD_PLAYER_LIVE_CPU_CUT;
        else if (object->role == 4u) object->action = DD_PLAYER_LIVE_CPU_ROUTE;
        else object->action = DD_PLAYER_LIVE_CPU;
        object->action_age = 0u;
    }
    state->ball.action = DD_BALL_AWARDED;
    state->ball.owner = (uint8_t)inbounder;
    state->ball.receiver = (uint8_t)receiver;
    state->ball.action_age = 0u;
    state->carrier = (uint8_t)inbounder;
    state->players[inbounder].action = DD_PLAYER_USER_PASS_RECOVER;
    state->players[inbounder].action_age = 0u;
    state->players[inbounder].release_timer = 0x10u;
}

static void dd_step_inbound(const DDTipoffAssetsHeader *assets, DDGameplayState *state,
                            uint32_t live_frame) {
    uint32_t cpu_start;
    uint32_t cpu_end;
    uint32_t player;
    if (state->inbound_age != UINT16_MAX) ++state->inbound_age;
    /* The ball dispatcher precedes the player dispatcher.  A pass launched by
       state $31 therefore receives its first $AD41 flight update next frame. */
    dd_step_ball(assets, state);
    ++state->cpu_global_frame;
    if ((state->cpu_global_frame & 1u) != 0u) {
        cpu_start = 0u;
        cpu_end = 5u;
    } else {
        state->cpu_priority_player = (uint8_t)(state->cpu_priority_player + 1u);
        if (state->cpu_priority_player >= DD_GAMEPLAY_PLAYER_COUNT) {
            state->cpu_priority_player = 5u;
        }
        cpu_start = 5u;
        cpu_end = DD_GAMEPLAY_PLAYER_COUNT;
    }
    for (player = cpu_start; player < cpu_end; ++player) {
        if (player == state->controlled_player &&
            (state->players[player].action == DD_PLAYER_LIVE_USER ||
             state->players[player].action == DD_PLAYER_LIVE_USER_CARRIER)) {
            continue;
        }
        dd_update_cpu_player(assets, state, player, live_frame);
    }
    if (state->ball.action == DD_BALL_DRIBBLE && state->carrier < DD_GAMEPLAY_PLAYER_COUNT &&
        state->ball.receiver == DD_NO_OWNER &&
        state->players[state->carrier].route_step == 4u) {
        state->phase = DD_GAMEPLAY_LIVE;
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
    if (state->free_throw_age == 2u) state->dead_ball_latch = 0u;
    if (state->free_throw_age == 160u) state->inbound_reason = 0u;
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
    int queued_user_inbound_pass = 0;
    /* Fixed $C02B-$C033 continuously mixes $001A into entropy byte $0063
       between NMIs.  Native code advances one deterministic equivalent per
       rendered frame; decision code consumes the same high/low bit bands. */
    state->cpu_entropy = (uint8_t)(state->cpu_entropy + state->cpu_global_frame + 1u);
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
    if (controlled->action == DD_PLAYER_LIVE_USER_INBOUND) {
        uint32_t receiver = DD_NO_OWNER;
        /* `$06B0/$06B1` advances once per frame; `$A780` turns the ball over
           at coarse tick five, exactly 320 frames after state `$0D` begins. */
        if (state->inbound_age != UINT16_MAX) ++state->inbound_age;
        if (state->inbound_age >= 320u) {
            dd_begin_common_inbound(state, 0x12u);
            /* `$A780` runs on the user-side half of the alternating object
               schedule; retain that consumed half-frame so role-zero `$41`
               starts on the opposite side's following update. */
            ++state->cpu_global_frame;
            dd_update_camera(state);
            state->controlled_flash_palette = (uint8_t)(((live_frame / 2u) & 1u) == 0u);
            state->players[state->controlled_player].attributes =
                state->controlled_flash_palette;
            state->live_frame = live_frame;
            state->previous_input = input_mask;
            return;
        }
        if ((pressed & DD_INPUT_A) != 0u) {
            receiver = dd_user_pass_receiver(state, state->controlled_player,
                                             input_mask);
        }
        if (receiver < DD_GAMEPLAY_PLAYER_COUNT) {
            dd_queue_user_inbound_pass(state, state->controlled_player, receiver);
            queued_user_inbound_pass = 1;
            controlled = &state->players[state->controlled_player];
        } else {
            controlled->velocity_x = 0;
            controlled->velocity_depth = 0;
        }
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
        /* Bank 0 $9CA0/$9CF6 do not clamp a failed move to the boundary.
           They retain the prior fixed-point coordinate and clear only the
           rejected axis velocity.  Native input is integrated once per host
           frame (the NES calls $A84C twice on its alternating object tick). */
        dd_integrate_longitudinal(&controlled->court_x, &controlled->velocity_x);
        dd_integrate_depth(&controlled->court_depth, &controlled->velocity_depth);
        controlled->facing = dd_facing_from_velocity(controlled->velocity_x,
                                                     controlled->velocity_depth,
                                                     controlled->facing);
        if (controlled->velocity_x != 0 || controlled->velocity_depth != 0) {
            controlled->animation = dd_animation_for_facing(controlled->facing, live_frame / 3u);
        } else {
            controlled->animation = (uint8_t)(0x1Bu + ((live_frame + 3u) / 8u) % 6u);
        }
    } else if (controlled->action != DD_PLAYER_REBOUND_CHASE &&
               controlled->action != DD_PLAYER_REBOUND_CLAIM &&
               controlled->action != DD_PLAYER_REBOUND_RETURN) {
        controlled->velocity_x = 0;
        controlled->velocity_depth = 0;
    }
    if (dd_step_user_exceptional_contact(state, input_mask)) {
        dd_update_camera(state);
        state->live_frame = live_frame;
        state->previous_input = input_mask;
        return;
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
        if (queued_user_inbound_pass && player == state->controlled_player) continue;
        if (player == state->controlled_player &&
            (state->players[player].action == DD_PLAYER_LIVE_USER ||
             state->players[player].action == DD_PLAYER_LIVE_USER_CARRIER ||
             state->players[player].action == DD_PLAYER_LIVE_USER_INBOUND)) {
            continue;
        }
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
    if (dd_step_possession_rules(state)) {
        dd_update_camera(state);
        state->controlled_flash_palette = (uint8_t)(((live_frame / 2u) & 1u) == 0u);
        state->players[state->controlled_player].attributes =
            state->controlled_flash_palette;
        state->live_frame = live_frame;
        state->previous_input = input_mask;
        return;
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
