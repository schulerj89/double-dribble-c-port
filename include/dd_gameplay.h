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
    DD_GAMEPLAY_LIVE
} DDGameplayPhase;

typedef enum DDBallAction {
    DD_BALL_AWARDED = 0x00,
    DD_BALL_DRIBBLE = 0x01,
    DD_BALL_AIRBORNE = 0x05
} DDBallAction;

typedef enum DDPlayerAction {
    DD_PLAYER_LIVE_USER = 0x0F,
    DD_PLAYER_FORMATION_USER = 0x10,
    DD_PLAYER_LIVE_TEAMMATE = 0x20,
    DD_PLAYER_LIVE_CARRIER = 0x25,
    DD_PLAYER_TIP_CPU = 0x2A,
    DD_PLAYER_TIP_CPU_AIRBORNE = 0x2B,
    DD_PLAYER_FORMATION_TEAMMATE = 0x32,
    DD_PLAYER_FORMATION_CPU = 0x35,
    DD_PLAYER_LIVE_CPU_CUT = 0x3C,
    DD_PLAYER_LIVE_CPU_ROUTE = 0x3E,
    DD_PLAYER_LIVE_CPU = 0x40
} DDPlayerAction;

typedef struct DDPlayerState {
    int32_t court_x;
    int32_t court_depth;
    int32_t height;
    int32_t velocity_x;
    int32_t velocity_depth;
    uint8_t facing;
    uint8_t animation;
    uint8_t attributes;
    uint8_t action;
} DDPlayerState;

typedef struct DDBallState {
    int32_t court_x;
    int32_t court_depth;
    int32_t height;
    uint8_t animation;
    uint8_t attributes;
    uint8_t action;
    uint8_t owner;
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
    int initialized;
} DDGameplayState;

void dd_gameplay_reset(DDGameplayState *state);
int dd_gameplay_step(const DDAssetPack *pack, DDGameplayState *state, uint32_t input_mask);
int dd_gameplay_advance_to(const DDAssetPack *pack, DDGameplayState *state,
                           uint32_t scene_frame, uint32_t input_mask);

#endif
