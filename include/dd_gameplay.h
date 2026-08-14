#ifndef DD_GAMEPLAY_H
#define DD_GAMEPLAY_H

#include <stdint.h>

#include "dd_asset_pack.h"

#define DD_GAMEPLAY_PLAYER_COUNT 10u

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
    DD_GAMEPLAY_INBOUND
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
    DD_BALL_DEAD = 0x0B
} DDBallAction;

typedef enum DDPlayerAction {
    DD_PLAYER_LIVE_USER_CARRIER = 0x02,
    DD_PLAYER_LIVE_USER = 0x0F,
    DD_PLAYER_FORMATION_USER = 0x10,
    DD_PLAYER_TIP_USER_AIRBORNE = 0x11,
    DD_PLAYER_LIVE_TEAMMATE = 0x20,
    DD_PLAYER_LIVE_CARRIER = 0x25,
    DD_PLAYER_LIVE_CARRIER_ROUTE = 0x26,
    DD_PLAYER_LIVE_CARRIER_DECIDE = 0x27,
    DD_PLAYER_LIVE_SHOOTER_RECOVER = 0x28,
    DD_PLAYER_LIVE_SHOOTER_RESET = 0x29,
    DD_PLAYER_TIP_CPU = 0x2A,
    DD_PLAYER_TIP_CPU_AIRBORNE = 0x2B,
    DD_PLAYER_INBOUND_HOLD = 0x30,
    DD_PLAYER_INBOUND_READY = 0x31,
    DD_PLAYER_LIVE_CPU_SETUP = 0x32,
    DD_PLAYER_FORMATION_TEAMMATE = 0x32,
    DD_PLAYER_FORMATION_CPU = 0x35,
    DD_PLAYER_LIVE_CPU_CUT = 0x3C,
    DD_PLAYER_LIVE_CPU_CUT_RUN = 0x3D,
    DD_PLAYER_LIVE_CPU_ROUTE = 0x3E,
    DD_PLAYER_INBOUND_FORMATION = 0x36,
    DD_PLAYER_LIVE_CPU = 0x40,
    DD_PLAYER_INBOUNDER = 0x41
} DDPlayerAction;

typedef struct DDPlayerState {
    int32_t court_x;
    int32_t court_depth;
    int32_t height;
    int32_t velocity_x;
    int32_t velocity_depth;
    int32_t target_x;
    int32_t target_depth;
    uint32_t cpu_updates;
    uint16_t action_age;
    uint8_t facing;
    uint8_t animation;
    uint8_t attributes;
    uint8_t action;
    uint8_t role;
    uint8_t target_zone;
    uint8_t route_step;
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
    uint32_t live_frame;
    uint8_t controlled_player;
    uint8_t carrier;
    uint8_t phase;
    uint8_t controlled_flash_palette;
    uint8_t cpu_global_frame;
    uint8_t cpu_priority_player;
    uint8_t possession_direction;
    uint8_t possession_count;
    uint8_t camera_chr_side;
    uint8_t tip_winner;
    uint16_t inbound_age;
    uint32_t tip_user_jump_frame;
    uint32_t live_start_frame;
    uint32_t previous_input;
    int initialized;
} DDGameplayState;

void dd_gameplay_reset(DDGameplayState *state);
int dd_gameplay_step(const DDAssetPack *pack, DDGameplayState *state, uint32_t input_mask);
int dd_gameplay_advance_to(const DDAssetPack *pack, DDGameplayState *state,
                           uint32_t scene_frame, uint32_t input_mask);

#endif
