#ifndef DD_GAMEPLAY_H
#define DD_GAMEPLAY_H

#include <stdint.h>

#include "dd_asset_pack.h"

#define DD_GAMEPLAY_PLAYER_COUNT 10u
#define DD_GAME_SET_BLUE_AGE 258u
#define DD_GAME_SET_TITLE_AGE 282u

enum {
    DD_INPUT_LEFT = 1u << 0,
    DD_INPUT_RIGHT = 1u << 1,
    DD_INPUT_UP = 1u << 2,
    DD_INPUT_DOWN = 1u << 3,
    DD_INPUT_A = 1u << 4,
    DD_INPUT_B = 1u << 5
};

typedef enum DDGameplayPhase {
    DD_GAMEPLAY_FORMATION = 0,
    DD_GAMEPLAY_TOSS,
    DD_GAMEPLAY_AWARD,
    DD_GAMEPLAY_LIVE,
    DD_GAMEPLAY_INBOUND,
    DD_GAMEPLAY_FREE_THROW,
    DD_GAMEPLAY_GAME_SET
} DDGameplayPhase;

typedef enum DDBallAction {
    DD_BALL_AWARDED = 0x00,
    DD_BALL_DRIBBLE = 0x01,
    DD_BALL_PASS = 0x02,
    DD_BALL_PASS_BOUNCE = 0x03,
    DD_BALL_SHOT_GATHER = 0x04,
    DD_BALL_AIRBORNE = 0x05,
    DD_BALL_SCORE = 0x06,
    DD_BALL_REBOUND = 0x07,
    DD_BALL_LOOSE_LAUNCH = 0x08,
    DD_BALL_LOOSE_AIRBORNE = 0x09,
    DD_BALL_SHOT_LAUNCH = 0x0A,
    DD_BALL_DEAD = 0x0B,
    DD_BALL_HIDDEN = 0x0C
} DDBallAction;

typedef enum DDPlayerAction {
    DD_PLAYER_LIVE_USER_CARRIER = 0x02,
    DD_PLAYER_USER_SHOOT = 0x03,
    DD_PLAYER_USER_PASS_RECOVER = 0x05,
    DD_PLAYER_USER_PASS_RECEIVE = 0x0C,
    DD_PLAYER_LIVE_USER_INBOUND = 0x0D,
    DD_PLAYER_LIVE_USER = 0x0F,
    DD_PLAYER_USER_CONTEST_RECOVER = 0x10,
    DD_PLAYER_FORMATION_USER = 0x10,
    DD_PLAYER_USER_CONTEST = 0x11,
    DD_PLAYER_TIP_USER_AIRBORNE = 0x11,
    DD_PLAYER_LIVE_TEAMMATE = 0x20,
    DD_PLAYER_LIVE_FOLLOW_TARGET = 0x21,
    DD_PLAYER_LIVE_PAIRED_DEFENDER = 0x22,
    DD_PLAYER_JUMP_START = 0x23,
    DD_PLAYER_JUMP_CONTEST = 0x24,
    DD_PLAYER_LIVE_CARRIER = 0x25,
    DD_PLAYER_LIVE_CARRIER_ROUTE = 0x26,
    DD_PLAYER_LIVE_CARRIER_DECIDE = 0x27,
    DD_PLAYER_LIVE_SHOOTER_RECOVER = 0x28,
    DD_PLAYER_LIVE_SHOOTER_RESET = 0x29,
    DD_PLAYER_TIP_CPU = 0x2A,
    DD_PLAYER_TIP_CPU_AIRBORNE = 0x2B,
    DD_PLAYER_LIVE_CONTINUE = 0x2C,
    DD_PLAYER_REBOUND_CHASE = 0x2D,
    DD_PLAYER_REBOUND_CLAIM = 0x2E,
    DD_PLAYER_REBOUND_RETURN = 0x2F,
    DD_PLAYER_INBOUND_HOLD = 0x30,
    DD_PLAYER_INBOUND_READY = 0x31,
    DD_PLAYER_LIVE_CPU_SETUP = 0x32,
    DD_PLAYER_FORMATION_TEAMMATE = 0x32,
    DD_PLAYER_LIVE_CONTINUE_33 = 0x33,
    DD_PLAYER_LIVE_CONTINUE_34 = 0x34,
    DD_PLAYER_FORMATION_CPU = 0x35,
    DD_PLAYER_INBOUND_FORMATION = 0x36,
    DD_PLAYER_LIVE_SET = 0x37,
    DD_PLAYER_ROUTE_INIT = 0x38,
    DD_PLAYER_ROUTE_APPROACH = 0x39,
    DD_PLAYER_ROUTE_ADJUST = 0x3A,
    DD_PLAYER_ROUTE_WAIT = 0x3B,
    DD_PLAYER_LIVE_CPU_CUT = 0x3C,
    DD_PLAYER_LIVE_CPU_CUT_RUN = 0x3D,
    DD_PLAYER_LIVE_CPU_ROUTE = 0x3E,
    DD_PLAYER_LIVE_RENDER_ONLY = 0x3F,
    DD_PLAYER_LIVE_CPU = 0x40,
    DD_PLAYER_INBOUNDER = 0x41,
    DD_PLAYER_FREE_THROW_SHOOTER = 0x42,
    DD_PLAYER_FREE_THROW_FORMATION = 0x43,
    DD_PLAYER_FREE_THROW_READY = 0x45,
    DD_PLAYER_FREE_THROW_SET = 0x46,
    DD_PLAYER_FREE_THROW_GATHER = 0x47,
    DD_PLAYER_FREE_THROW_FOLLOW = 0x48,
    DD_PLAYER_FREE_THROW_RECOVER = 0x49,
    DD_PLAYER_FREE_THROW_WALK = 0x4A
} DDPlayerAction;

typedef struct DDPlayerState {
    int32_t court_x;
    int32_t court_depth;
    int32_t height;
    int32_t velocity_x;
    int32_t velocity_depth;
    int32_t velocity_height;
    int32_t route_velocity_x;
    int32_t route_velocity_depth;
    int32_t target_x;
    int32_t target_depth;
    uint32_t cpu_updates;
    uint16_t action_age;
    uint8_t facing;
    uint8_t route_facing;
    uint8_t animation;
    uint8_t attributes;
    uint8_t action;
    uint8_t role;
    uint8_t target_zone;
    uint8_t route_step;
    uint8_t contact_age;
    uint8_t height_script_index;
    uint8_t height_script_reverse;
    uint8_t release_timer;
    uint8_t decision_timer;
    uint8_t hold_timer;
    uint8_t paired_timer;
    uint8_t paired_player;
} DDPlayerState;

typedef struct DDBallState {
    int32_t court_x;
    int32_t court_depth;
    int32_t height;
    uint8_t animation;
    uint8_t attributes;
    uint8_t action;
    uint8_t owner;
    uint8_t receiver;
    uint8_t outcome;
    uint8_t rim_contact;
    uint8_t flight_angle;
    uint8_t flight_curve;
    uint8_t flight_duration;
    uint8_t held_height_offset;
    uint8_t vertical_phase;
    uint16_t action_age;
    int32_t velocity_x;
    int32_t velocity_depth;
    int32_t velocity_height;
} DDBallState;

typedef struct DDGameplayState {
    DDPlayerState players[DD_GAMEPLAY_PLAYER_COUNT];
    DDBallState ball;
    int32_t camera_x;
    uint32_t scene_frame;
    uint32_t sequence_frame;
    uint32_t live_frame;
    uint8_t controlled_player;
    uint8_t carrier;
    uint8_t phase;
    uint8_t controlled_flash_palette;
    uint8_t cpu_global_frame;
    uint8_t object_phase;
    uint8_t cpu_priority_player;
    uint8_t cpu_pass_cooldown;
    uint8_t cpu_entropy;
    uint8_t possession_direction;
    uint8_t possession_count;
    uint8_t last_touch_player;
    uint8_t inbound_variant;
    uint8_t inbound_reason;
    uint16_t rule_message_age;
    uint16_t rebound_formation_pending;
    uint8_t dead_ball_latch;
    uint8_t audio_event;
    uint32_t audio_event_serial;
    uint8_t backcourt_latched;
    uint8_t camera_chr_side;
    uint8_t hud_split_y;
    uint8_t tip_winner;
    uint16_t inbound_age;
    uint16_t possession_rule_age;
    uint32_t tip_user_jump_frame;
    uint32_t live_start_frame;
    uint32_t next_clock_frame;
    uint32_t clock_expired_frame;
    uint32_t previous_input;
    uint16_t score[2];
    uint8_t clock_minutes;
    uint8_t clock_seconds;
    uint8_t period;
    uint8_t match_time_index;
    uint8_t match_time_bcd;
    uint8_t match_team_index;
    uint8_t match_level_index;
    uint8_t last_shooter;
    uint8_t shot_value;
    uint8_t net_animation_phase;
    uint8_t net_basket_side;
    uint8_t dunk_active;
    uint8_t dunk_outcome;
    uint16_t dunk_age;
    uint8_t foul_shooter;
    uint8_t foul_offender;
    uint8_t match_clock_pulse;
    uint16_t free_throw_age;
    uint16_t game_set_age;
    int clock_expired;
    int return_to_title;
    int initialized;
} DDGameplayState;

void dd_gameplay_reset(DDGameplayState *state);
int dd_gameplay_configure(const DDAssetPack *pack, DDGameplayState *state,
                          uint32_t time_index, uint32_t team_index,
                          uint32_t level_index);
int dd_gameplay_step(const DDAssetPack *pack, DDGameplayState *state, uint32_t input_mask);
int dd_gameplay_advance_to(const DDAssetPack *pack, DDGameplayState *state,
                           uint32_t scene_frame, uint32_t input_mask);

#endif
