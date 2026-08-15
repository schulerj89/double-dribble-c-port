#include "dd_asset_pack.h"
#include "dd_audio.h"
#include "dd_renderer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void dd_usage(void) {
    puts("Double Dribble native-port tools");
    puts("  --build-assetpack <rom.nes> <output.assetpack>");
    puts("  --inspect-assetpack <input.assetpack>");
    puts("  --render-title <input.assetpack> <output.bmp>");
    puts("  --render-title-state <input.assetpack> <selection> <selection-visible> <output.bmp>");
    puts("  --render-title-confirm <input.assetpack> <frame> <output.bmp>");
    puts("  --render-intro <input.assetpack> <frame> <output.bmp>");
    puts("  --render-config <input.assetpack> <selection> <output.bmp>");
    puts("  --render-config-state <input.assetpack> <selection> <time> <team> <level> <action-row> <action-frame> <output.bmp>");
    puts("  --render-tipoff <input.assetpack> <output.bmp>");
    puts("  --render-gameplay <input.assetpack> <transition-frame> <output.bmp>");
    puts("  --render-gameplay-input <input.assetpack> <transition-frame> <input-mask> <output.bmp>");
    puts("  --render-gameplay-user-contest <input.assetpack> <output.bmp>");
    puts("  --render-gameplay-switch-block <input.assetpack> <output.bmp>");
    puts("  --render-gameplay-block <input.assetpack> <contact|landing> <output.bmp>");
    puts("  --render-gameplay-shot <input.assetpack> <make|miss> <gather|release|result|chase|pickup|inbound|receive> <output.bmp>");
    puts("  --render-gameplay-moving-shot <input.assetpack> <takeoff|held|airborne> <output.bmp>");
    puts("  --render-gameplay-rule <input.assetpack> <oob|backpass> <output.bmp>");
    puts("  --render-gameplay-dunk <input.assetpack> <make|miss> <gather|airborne|result> <output.bmp>");
    puts("  --render-gameplay-team <input.assetpack> <0|2|3> <output.bmp>");
    puts("  --render-gameplay-top-edge <input.assetpack> <output.bmp>");
    puts("  --render-gameplay-tip-jump <input.assetpack> <output.bmp>");
    puts("  --dump-title-wav <input.assetpack> <output.wav>");
    puts("  --dump-intro-wav <input.assetpack> <output.wav>");
    puts("  --dump-select-wav <input.assetpack> <output.wav>");
    puts("  --dump-config-wav <input.assetpack> <output.wav>");
    puts("  --dump-end-wav <input.assetpack> <output.wav>");
    puts("  --dump-tipoff-wav <input.assetpack> <output.wav>");
    puts("  --dump-gameplay-wav <input.assetpack> <output.wav>");
    puts("  --dump-ball-bounce-wav <input.assetpack> <output.wav>");
    puts("  --dump-whistle-wav <input.assetpack> <output.wav>");
    puts("  --dump-three-call-wav <input.assetpack> <output.wav>");
    puts("  --dump-three-score-wav <input.assetpack> <output.wav>");
}

int main(int argc, char **argv) {
    DDAssetPack pack;
    if (argc == 4 && strcmp(argv[1], "--build-assetpack") == 0) {
        return dd_build_asset_pack(argv[2], argv[3]) ? 0 : 1;
    }
    if (argc == 3 && strcmp(argv[1], "--inspect-assetpack") == 0) {
        return dd_asset_pack_inspect(argv[2]) ? 0 : 1;
    }
    if (argc == 4 && strcmp(argv[1], "--render-title") == 0) {
        uint32_t *pixels;
        int ok;
        if (!dd_asset_pack_load(argv[2], &pack)) return 1;
        pixels = (uint32_t *)malloc((size_t)pack.meta.width * pack.meta.height * sizeof(uint32_t));
        ok = pixels != NULL && dd_render_title(&pack, pixels, pack.meta.width, pack.meta.height) &&
             dd_write_bmp(argv[3], pixels, pack.meta.width, pack.meta.height);
        free(pixels);
        dd_asset_pack_unload(&pack);
        return ok ? 0 : 1;
    }
    if (argc == 6 && strcmp(argv[1], "--render-title-state") == 0) {
        uint32_t *pixels;
        unsigned long selection;
        unsigned long selection_visible;
        char *end;
        int ok;
        selection = strtoul(argv[3], &end, 10);
        if (*argv[3] == '\0' || *end != '\0' || selection > 1u) return 1;
        selection_visible = strtoul(argv[4], &end, 10);
        if (*argv[4] == '\0' || *end != '\0' || selection_visible > 1u ||
            !dd_asset_pack_load(argv[2], &pack)) return 1;
        pixels = (uint32_t *)malloc((size_t)pack.meta.width * pack.meta.height * sizeof(uint32_t));
        ok = pixels != NULL && dd_render_title_selection(&pack, (uint32_t)selection,
                                                         selection_visible != 0u, pixels,
                                                         pack.meta.width, pack.meta.height) &&
             dd_write_bmp(argv[5], pixels, pack.meta.width, pack.meta.height);
        free(pixels);
        dd_asset_pack_unload(&pack);
        return ok ? 0 : 1;
    }
    if (argc == 5 && strcmp(argv[1], "--render-title-confirm") == 0) {
        uint32_t *pixels;
        unsigned long frame;
        char *end;
        int ok;
        frame = strtoul(argv[3], &end, 10);
        if (*argv[3] == '\0' || *end != '\0' || frame > UINT32_MAX ||
            !dd_asset_pack_load(argv[2], &pack)) return 1;
        pixels = (uint32_t *)malloc((size_t)pack.meta.width * pack.meta.height * sizeof(uint32_t));
        ok = pixels != NULL && dd_render_title_selection(&pack, 0u,
                                                         dd_title_confirmation_visible((uint32_t)frame),
                                                         pixels, pack.meta.width, pack.meta.height) &&
             dd_write_bmp(argv[4], pixels, pack.meta.width, pack.meta.height);
        free(pixels);
        dd_asset_pack_unload(&pack);
        return ok ? 0 : 1;
    }
    if (argc == 4 && strcmp(argv[1], "--dump-title-wav") == 0) {
        int ok;
        if (!dd_asset_pack_load(argv[2], &pack)) return 1;
        ok = dd_write_title_wav(&pack, argv[3]);
        dd_asset_pack_unload(&pack);
        return ok ? 0 : 1;
    }
    if (argc == 5 && strcmp(argv[1], "--render-intro") == 0) {
        uint32_t *pixels;
        unsigned long frame;
        char *end;
        int ok;
        frame = strtoul(argv[3], &end, 10);
        if (*argv[3] == '\0' || *end != '\0' || frame > UINT32_MAX || !dd_asset_pack_load(argv[2], &pack)) return 1;
        pixels = (uint32_t *)malloc((size_t)pack.intro_meta.width * pack.intro_meta.height * sizeof(uint32_t));
        ok = pixels != NULL && dd_render_intro(&pack, (uint32_t)frame, pixels,
                                               pack.intro_meta.width, pack.intro_meta.height) &&
             dd_write_bmp(argv[4], pixels, pack.intro_meta.width, pack.intro_meta.height);
        free(pixels);
        dd_asset_pack_unload(&pack);
        return ok ? 0 : 1;
    }
    if (argc == 4 && strcmp(argv[1], "--dump-intro-wav") == 0) {
        uint8_t *wav;
        size_t size;
        FILE *file;
        int ok;
        if (!dd_asset_pack_load(argv[2], &pack)) return 1;
        ok = dd_build_intro_music_wav(&pack, &wav, &size);
        if (ok) {
            file = fopen(argv[3], "wb");
            ok = file != NULL && fwrite(wav, 1, size, file) == size;
            if (file != NULL) fclose(file);
            free(wav);
        }
        dd_asset_pack_unload(&pack);
        return ok ? 0 : 1;
    }
    if (argc == 4 && (strcmp(argv[1], "--dump-select-wav") == 0 ||
                      strcmp(argv[1], "--dump-config-wav") == 0)) {
        uint8_t *wav;
        size_t size;
        FILE *file;
        int ok;
        if (!dd_asset_pack_load(argv[2], &pack)) return 1;
        ok = strcmp(argv[1], "--dump-select-wav") == 0
            ? dd_build_select_music_wav(&pack, &wav, &size)
            : dd_build_config_music_wav(&pack, &wav, &size);
        if (ok) {
            file = fopen(argv[3], "wb");
            ok = file != NULL && fwrite(wav, 1, size, file) == size;
            if (file != NULL) fclose(file);
            free(wav);
        }
        dd_asset_pack_unload(&pack);
        return ok ? 0 : 1;
    }
    if (argc == 4 && (strcmp(argv[1], "--dump-end-wav") == 0 ||
                      strcmp(argv[1], "--dump-tipoff-wav") == 0 ||
                      strcmp(argv[1], "--dump-gameplay-wav") == 0 ||
                      strcmp(argv[1], "--dump-ball-bounce-wav") == 0 ||
                      strcmp(argv[1], "--dump-whistle-wav") == 0 ||
                      strcmp(argv[1], "--dump-three-call-wav") == 0 ||
                      strcmp(argv[1], "--dump-three-score-wav") == 0 ||
                      strcmp(argv[1], "--dump-score-wav") == 0)) {
        uint8_t *wav;
        size_t size;
        FILE *file;
        int ok;
        if (!dd_asset_pack_load(argv[2], &pack)) return 1;
        if (strcmp(argv[1], "--dump-end-wav") == 0) {
            ok = dd_build_end_music_wav(&pack, &wav, &size);
        } else if (strcmp(argv[1], "--dump-tipoff-wav") == 0) {
            ok = dd_build_tipoff_dmc_wav(&pack, &wav, &size);
        } else if (strcmp(argv[1], "--dump-whistle-wav") == 0) {
            ok = dd_build_whistle_audio_wav(&pack, &wav, &size);
        } else if (strcmp(argv[1], "--dump-three-call-wav") == 0) {
            ok = dd_build_three_call_audio_wav(&pack, &wav, &size);
        } else if (strcmp(argv[1], "--dump-three-score-wav") == 0) {
            ok = dd_build_three_score_audio_wav(&pack, &wav, &size);
        } else if (strcmp(argv[1], "--dump-score-wav") == 0) {
            ok = dd_build_score_audio_wav(&pack, &wav, &size);
        } else {
            ok = dd_build_gameplay_audio_wav(&pack, &wav, &size);
        }
        if (ok) {
            file = fopen(argv[3], "wb");
            ok = file != NULL && fwrite(wav, 1, size, file) == size;
            if (file != NULL) fclose(file);
            free(wav);
        }
        dd_asset_pack_unload(&pack);
        return ok ? 0 : 1;
    }
    if (argc == 4 && strcmp(argv[1], "--render-tipoff") == 0) {
        uint32_t *pixels;
        int ok;
        if (!dd_asset_pack_load(argv[2], &pack)) return 1;
        pixels = (uint32_t *)malloc((size_t)pack.tipoff_meta.width * pack.tipoff_meta.height * sizeof(uint32_t));
        ok = pixels != NULL && dd_render_tipoff(&pack, pixels, pack.tipoff_meta.width, pack.tipoff_meta.height) &&
             dd_write_bmp(argv[3], pixels, pack.tipoff_meta.width, pack.tipoff_meta.height);
        free(pixels);
        dd_asset_pack_unload(&pack);
        return ok ? 0 : 1;
    }
    if ((argc == 5 && strcmp(argv[1], "--render-gameplay") == 0) ||
        (argc == 6 && strcmp(argv[1], "--render-gameplay-input") == 0)) {
        DDGameplayState state;
        uint32_t *pixels;
        uint32_t input_mask = 0u;
        unsigned long frame;
        char *end;
        int ok;
        int output_argument = 4;
        frame = strtoul(argv[3], &end, 10);
        if (*argv[3] == '\0' || *end != '\0' || frame > UINT32_MAX) return 1;
        if (argc == 6) {
            unsigned long parsed_mask = strtoul(argv[4], &end, 0);
            if (*argv[4] == '\0' || *end != '\0' || parsed_mask > 0x3Fu) return 1;
            input_mask = (uint32_t)parsed_mask;
            output_argument = 5;
        }
        if (!dd_asset_pack_load(argv[2], &pack)) return 1;
        memset(&state, 0, sizeof(state));
        pixels = (uint32_t *)malloc((size_t)pack.tipoff_meta.width * pack.tipoff_meta.height * sizeof(uint32_t));
        ok = pixels != NULL && dd_gameplay_advance_to(&pack, &state, (uint32_t)frame, input_mask) &&
             dd_render_gameplay(&pack, &state, pixels, pack.tipoff_meta.width, pack.tipoff_meta.height) &&
             dd_write_bmp(argv[output_argument], pixels, pack.tipoff_meta.width, pack.tipoff_meta.height);
        free(pixels);
        dd_asset_pack_unload(&pack);
        return ok ? 0 : 1;
    }
    if (argc == 5 && strcmp(argv[1], "--render-gameplay-team") == 0) {
        DDGameplayState state;
        uint32_t *pixels;
        unsigned long team;
        char *end;
        int ok;
        team = strtoul(argv[3], &end, 10);
        if (*argv[3] == '\0' || *end != '\0' ||
            (team != 0u && team != 2u && team != 3u) ||
            !dd_asset_pack_load(argv[2], &pack)) return 1;
        memset(&state, 0, sizeof(state));
        ok = dd_gameplay_configure(&pack, &state, 0u, (uint32_t)team, 0u) &&
            dd_gameplay_advance_to(&pack, &state, 144u, 0u);
        pixels = (uint32_t *)malloc((size_t)pack.tipoff_meta.width *
                                    pack.tipoff_meta.height * sizeof(uint32_t));
        ok = ok && pixels != NULL &&
            dd_render_gameplay(&pack, &state, pixels,
                               pack.tipoff_meta.width, pack.tipoff_meta.height) &&
            dd_write_bmp(argv[4], pixels, pack.tipoff_meta.width,
                         pack.tipoff_meta.height);
        free(pixels);
        dd_asset_pack_unload(&pack);
        return ok ? 0 : 1;
    }
    if (argc == 4 && (strcmp(argv[1], "--render-gameplay-top-edge") == 0 ||
                      strcmp(argv[1], "--render-gameplay-tip-jump") == 0)) {
        DDGameplayState state;
        uint32_t *pixels;
        uint32_t player;
        int top_edge = strcmp(argv[1], "--render-gameplay-top-edge") == 0;
        int ok;
        if (!dd_asset_pack_load(argv[2], &pack)) return 1;
        memset(&state, 0, sizeof(state));
        if (top_edge) {
            ok = dd_gameplay_advance_to(&pack, &state, 356u, 0u);
            if (ok) {
                state.ball.animation = 0xFFu;
                for (player = 0u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
                    state.players[player].animation = 0xFFu;
                }
                state.players[0].animation = 0x1Bu;
                state.players[0].court_x = state.camera_x + 0x008000;
                state.players[0].court_depth = 0x009000;
                state.players[0].height = 0x1000;
            }
        } else {
            ok = dd_gameplay_advance_to(&pack, &state, 300u, 0u) &&
                dd_gameplay_step(&pack, &state, DD_INPUT_B) &&
                dd_gameplay_advance_to(&pack, &state, 304u, 0u);
        }
        pixels = (uint32_t *)malloc((size_t)pack.tipoff_meta.width *
                                    pack.tipoff_meta.height * sizeof(uint32_t));
        ok = ok && pixels != NULL &&
            dd_render_gameplay(&pack, &state, pixels,
                               pack.tipoff_meta.width, pack.tipoff_meta.height) &&
            dd_write_bmp(argv[3], pixels, pack.tipoff_meta.width,
                         pack.tipoff_meta.height);
        free(pixels);
        dd_asset_pack_unload(&pack);
        return ok ? 0 : 1;
    }
    if (argc == 4 && strcmp(argv[1], "--render-gameplay-user-contest") == 0) {
        DDGameplayState state;
        uint32_t *pixels;
        int ok;
        if (!dd_asset_pack_load(argv[2], &pack)) return 1;
        memset(&state, 0, sizeof(state));
        /* Original frame 2748 presses A while user slot $02 is paired with
           shooter $07 in ball state $04.  Native scene 547 is the matching
           dispatcher boundary; scene 548 is original frame 2749's first
           airborne-shot/user-state-$11 screenshot checkpoint. */
        ok = dd_gameplay_advance_to(&pack, &state, 546u, 0u) &&
             dd_gameplay_step(&pack, &state, DD_INPUT_A) &&
             dd_gameplay_step(&pack, &state, 0u) &&
             state.players[0].action == DD_PLAYER_USER_CONTEST &&
             state.ball.action == DD_BALL_AIRBORNE &&
             state.ball.flight_angle == 0x62u;
        pixels = (uint32_t *)malloc((size_t)pack.tipoff_meta.width *
                                    pack.tipoff_meta.height * sizeof(uint32_t));
        ok = ok && pixels != NULL &&
             dd_render_gameplay(&pack, &state, pixels,
                                pack.tipoff_meta.width, pack.tipoff_meta.height) &&
             dd_write_bmp(argv[3], pixels, pack.tipoff_meta.width,
                          pack.tipoff_meta.height);
        free(pixels);
        dd_asset_pack_unload(&pack);
        return ok ? 0 : 1;
    }
    if (argc == 4 && strcmp(argv[1], "--render-gameplay-switch-block") == 0) {
        DDGameplayState state;
        uint32_t *pixels;
        uint32_t player;
        int ok;
        if (!dd_asset_pack_load(argv[2], &pack)) return 1;
        memset(&state, 0, sizeof(state));
        ok = dd_gameplay_advance_to(&pack, &state, 356u, 0u);
        if (ok) {
            state.phase = DD_GAMEPLAY_LIVE;
            state.controlled_player = 0u;
            state.carrier = 5u;
            state.ball.action = DD_BALL_DRIBBLE;
            state.ball.owner = 5u;
            state.ball.court_x = 0x010000;
            state.ball.court_depth = 0x005800;
            state.ball.height = 0x1800;
            for (player = 0u; player < 5u; ++player) {
                state.players[player].action = DD_PLAYER_LIVE_TEAMMATE;
                state.players[player].court_depth = 0x005800;
            }
            state.players[0].action = DD_PLAYER_LIVE_USER;
            state.players[0].court_x = 0x004000;
            state.players[1].court_x = 0x018000;
            state.players[2].court_x = 0x012000;
            state.players[3].court_x = 0x01C000;
            state.players[4].court_x = 0x000000;
            ok = dd_gameplay_step(&pack, &state, DD_INPUT_B) &&
                 state.controlled_player == 2u &&
                 state.players[2].role == 0u &&
                 state.players[2].paired_player == 5u;
            state.ball.action = DD_BALL_SHOT_GATHER;
            state.ball.owner = 5u;
            state.players[5].action = DD_PLAYER_LIVE_CARRIER_DECIDE;
            state.players[5].court_x = state.players[2].court_x + 0x0600;
            state.players[5].court_depth = state.players[2].court_depth;
            ok = ok && dd_gameplay_step(&pack, &state, DD_INPUT_A) &&
                 state.players[2].action == DD_PLAYER_USER_CONTEST;
            for (player = 0u; ok && player < 8u; ++player) {
                ok = dd_gameplay_step(&pack, &state, 0u);
            }
        }
        pixels = (uint32_t *)malloc((size_t)pack.tipoff_meta.width *
                                    pack.tipoff_meta.height * sizeof(uint32_t));
        ok = ok && pixels != NULL &&
             dd_render_gameplay(&pack, &state, pixels,
                                pack.tipoff_meta.width, pack.tipoff_meta.height) &&
             dd_write_bmp(argv[3], pixels, pack.tipoff_meta.width,
                          pack.tipoff_meta.height);
        free(pixels);
        dd_asset_pack_unload(&pack);
        return ok ? 0 : 1;
    }
    if (argc == 5 && strcmp(argv[1], "--render-gameplay-block") == 0) {
        DDGameplayState state;
        uint32_t *pixels;
        uint32_t player;
        uint32_t steps = 0u;
        int capture_landing;
        int ok;
        if (strcmp(argv[3], "contact") == 0) {
            capture_landing = 0;
        } else if (strcmp(argv[3], "landing") == 0) {
            capture_landing = 1;
        } else {
            return 1;
        }
        if (!dd_asset_pack_load(argv[2], &pack)) return 1;
        memset(&state, 0, sizeof(state));
        ok = dd_gameplay_advance_to(&pack, &state, 356u, 0u);
        if (ok) {
            /* Deterministic native counterpart to Capture-TipoffGameplay's
               opt-in frame-2606 $8B12->$A6C3 block probe.  It enters through
               dd_gameplay_step, not through a recorded state or renderer
               shortcut, so contact and landing exercise the shipping loop. */
            state.phase = DD_GAMEPLAY_LIVE;
            state.controlled_player = 0u;
            state.carrier = 0xFFu;
            state.cpu_global_frame = 1u;
            state.ball.action = DD_BALL_AIRBORNE;
            state.ball.owner = 0u;
            state.ball.receiver = 0xFFu;
            state.players[0].action = DD_PLAYER_USER_SHOOT;
            state.players[0].court_x = 0x00FA00;
            state.players[0].court_depth = 0x005800;
            state.players[0].height = 0x1800;
            state.players[5].action = DD_PLAYER_JUMP_CONTEST;
            state.players[5].court_x = 0x010000;
            state.players[5].court_depth = 0x005800;
            state.players[5].height = 0x1000;
            state.players[5].height_script_index = 11u;
            state.players[5].height_script_reverse = 0u;
            state.ball.court_x = 0x00FA00;
            state.ball.court_depth = 0x005800;
            state.ball.height = 0x1800;
            for (player = 1u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
                if (player != 5u) state.players[player].action = DD_PLAYER_ROUTE_WAIT;
            }
            ok = dd_gameplay_step(&pack, &state, 0u);
            while (ok && capture_landing &&
                   !(state.carrier == 5u && state.ball.action == DD_BALL_DRIBBLE) &&
                   steps < 80u) {
                ok = dd_gameplay_step(&pack, &state, 0u);
                ++steps;
            }
            if (capture_landing &&
                !(state.carrier == 5u && state.ball.action == DD_BALL_DRIBBLE)) ok = 0;
        }
        pixels = (uint32_t *)malloc((size_t)pack.tipoff_meta.width *
                                    pack.tipoff_meta.height * sizeof(uint32_t));
        ok = ok && pixels != NULL &&
             dd_render_gameplay(&pack, &state, pixels,
                                pack.tipoff_meta.width, pack.tipoff_meta.height) &&
             dd_write_bmp(argv[4], pixels, pack.tipoff_meta.width,
                          pack.tipoff_meta.height);
        free(pixels);
        dd_asset_pack_unload(&pack);
        return ok ? 0 : 1;
    }
    if (argc == 5 && strcmp(argv[1], "--render-gameplay-moving-shot") == 0) {
        DDGameplayState state;
        uint32_t *pixels;
        uint32_t player;
        uint32_t start_x;
        int checkpoint;
        int ok;
        if (strcmp(argv[3], "takeoff") == 0) checkpoint = 0;
        else if (strcmp(argv[3], "held") == 0) checkpoint = 1;
        else if (strcmp(argv[3], "airborne") == 0) checkpoint = 2;
        else return 1;
        if (!dd_asset_pack_load(argv[2], &pack)) return 1;
        memset(&state, 0, sizeof(state));
        ok = dd_gameplay_advance_to(&pack, &state, 356u, 0u);
        if (ok) {
            state.phase = DD_GAMEPLAY_LIVE;
            state.possession_direction = 1u;
            state.controlled_player = 0u;
            state.carrier = 0u;
            state.cpu_global_frame = 0u;
            state.ball.action = DD_BALL_DRIBBLE;
            state.ball.owner = 0u;
            state.ball.receiver = 0xFFu;
            state.players[0].action = DD_PLAYER_LIVE_USER_CARRIER;
            state.players[0].facing = 0u;
            state.players[0].court_x = 0x00D000;
            state.players[0].court_depth = 0x006100;
            state.players[0].height = 0x1000;
            for (player = 1u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
                state.players[player].action = DD_PLAYER_ROUTE_WAIT;
            }
            ok = dd_gameplay_step(&pack, &state, DD_INPUT_RIGHT | DD_INPUT_B);
            start_x = (uint32_t)state.players[0].court_x;
            for (player = 0u; ok && checkpoint != 0 && player < 8u; ++player) {
                ok = dd_gameplay_step(&pack, &state,
                                      checkpoint == 1 ? DD_INPUT_B : 0u);
            }
            ok = ok && state.players[0].action == DD_PLAYER_USER_SHOOT &&
                (checkpoint != 1 ||
                 (state.ball.action == DD_BALL_SHOT_GATHER &&
                  state.ball.owner == 0u &&
                  state.ball.height == state.players[0].height + 0x1200)) &&
                (checkpoint == 0 || (uint32_t)state.players[0].court_x > start_x);
        }
        pixels = (uint32_t *)malloc((size_t)pack.tipoff_meta.width *
                                    pack.tipoff_meta.height * sizeof(uint32_t));
        ok = ok && pixels != NULL &&
             dd_render_gameplay(&pack, &state, pixels,
                                pack.tipoff_meta.width, pack.tipoff_meta.height) &&
             dd_write_bmp(argv[4], pixels, pack.tipoff_meta.width,
                          pack.tipoff_meta.height);
        free(pixels);
        dd_asset_pack_unload(&pack);
        return ok ? 0 : 1;
    }
    if (argc == 5 && strcmp(argv[1], "--render-gameplay-rule") == 0) {
        DDGameplayState state;
        uint32_t *pixels;
        uint32_t player;
        int backpass = strcmp(argv[3], "backpass") == 0;
        int ok = backpass || strcmp(argv[3], "oob") == 0;
        if (!ok || !dd_asset_pack_load(argv[2], &pack)) return 1;
        memset(&state, 0, sizeof(state));
        ok = dd_gameplay_advance_to(&pack, &state, 356u, 0u);
        if (ok) {
            state.phase = DD_GAMEPLAY_LIVE;
            state.possession_direction = 1u;
            state.controlled_player = 0u;
            state.carrier = 0u;
            state.ball.owner = 0u;
            state.last_touch_player = 0u;
            state.ball.action = backpass ? DD_BALL_DRIBBLE : DD_BALL_HIDDEN;
            state.players[0].action = DD_PLAYER_LIVE_USER_CARRIER;
            if (backpass) {
                state.players[0].court_x = 0x00E000;
                state.players[0].court_depth = 0x005800;
            }
            state.ball.court_x = backpass ? 0x00F000 : 0x010000;
            state.ball.court_depth = backpass ? 0x005800 : 0x001000;
            state.backcourt_latched = (uint8_t)backpass;
            for (player = 1u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
                state.players[player].action = DD_PLAYER_ROUTE_WAIT;
            }
            ok = dd_gameplay_step(&pack, &state, 0u) &&
                 state.phase == DD_GAMEPLAY_INBOUND &&
                 state.inbound_reason == (backpass ? 0x15u : 0x16u);
        }
        pixels = (uint32_t *)malloc((size_t)pack.tipoff_meta.width *
                                    pack.tipoff_meta.height * sizeof(uint32_t));
        ok = ok && pixels != NULL &&
             dd_render_gameplay(&pack, &state, pixels,
                                pack.tipoff_meta.width, pack.tipoff_meta.height) &&
             dd_write_bmp(argv[4], pixels, pack.tipoff_meta.width,
                          pack.tipoff_meta.height);
        free(pixels);
        dd_asset_pack_unload(&pack);
        return ok ? 0 : 1;
    }
    if (argc == 6 && strcmp(argv[1], "--render-gameplay-dunk") == 0) {
        DDGameplayState state;
        uint32_t *pixels;
        uint32_t player;
        uint32_t steps;
        int make = strcmp(argv[3], "make") == 0;
        int checkpoint;
        int ok = make || strcmp(argv[3], "miss") == 0;
        if (strcmp(argv[4], "gather") == 0) checkpoint = 0;
        else if (strcmp(argv[4], "airborne") == 0) checkpoint = 1;
        else if (strcmp(argv[4], "result") == 0) checkpoint = 2;
        else return 1;
        if (!ok || !dd_asset_pack_load(argv[2], &pack)) return 1;
        memset(&state, 0, sizeof(state));
        ok = dd_gameplay_advance_to(&pack, &state, 356u, 0u);
        if (ok) {
            state.phase = DD_GAMEPLAY_LIVE;
            state.possession_direction = 1u;
            state.controlled_player = 0u;
            state.carrier = 0u;
            state.ball.action = DD_BALL_DRIBBLE;
            state.ball.owner = 0u;
            state.players[0].action = DD_PLAYER_LIVE_USER_CARRIER;
            state.players[0].facing = 0u;
            /* Packed cell `$BA` is accepted by original `$B189` but lay
               outside the port's former narrow radius gate. */
            state.players[0].court_x = 0x01A400;
            state.players[0].court_depth = 0x005800;
            state.players[0].height = 0x1000;
            for (player = 1u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
                state.players[player].action = DD_PLAYER_ROUTE_WAIT;
            }
            ok = dd_gameplay_step(&pack, &state, DD_INPUT_B) &&
                 state.dunk_active != 0u;
            state.dunk_outcome = make ? 1u : 4u;
            steps = checkpoint == 0 ? 0u : checkpoint == 1 ? 9u : 20u;
            for (player = 0u; ok && player < steps &&
                 (checkpoint != 2 || state.dunk_active != 0u); ++player) {
                ok = dd_gameplay_step(&pack, &state, 0u);
            }
            if (checkpoint == 2) {
                ok = ok && state.dunk_active == 0u &&
                    state.ball.action == (make ? DD_BALL_SCORE : DD_BALL_LOOSE_LAUNCH);
            }
        }
        pixels = (uint32_t *)malloc((size_t)pack.tipoff_meta.width *
                                    pack.tipoff_meta.height * sizeof(uint32_t));
        ok = ok && pixels != NULL &&
             dd_render_gameplay(&pack, &state, pixels,
                                pack.tipoff_meta.width, pack.tipoff_meta.height) &&
             dd_write_bmp(argv[5], pixels, pack.tipoff_meta.width,
                          pack.tipoff_meta.height);
        free(pixels);
        dd_asset_pack_unload(&pack);
        return ok ? 0 : 1;
    }
    if (argc == 6 && strcmp(argv[1], "--render-gameplay-shot") == 0) {
        DDGameplayState state;
        uint32_t *pixels;
        uint32_t player;
        uint32_t steps = 0u;
        int make;
        int checkpoint;
        int ok;
        if (strcmp(argv[3], "make") == 0) make = 1;
        else if (strcmp(argv[3], "miss") == 0) make = 0;
        else return 1;
        if (strcmp(argv[4], "gather") == 0) checkpoint = 0;
        else if (strcmp(argv[4], "release") == 0) checkpoint = 1;
        else if (strcmp(argv[4], "result") == 0) checkpoint = 2;
        else if (strcmp(argv[4], "inbound") == 0) checkpoint = 3;
        else if (strcmp(argv[4], "pickup") == 0) checkpoint = 4;
        else if (strcmp(argv[4], "chase") == 0) checkpoint = 5;
        else if (strcmp(argv[4], "receive") == 0) checkpoint = 6;
        else return 1;
        if (!dd_asset_pack_load(argv[2], &pack)) return 1;
        memset(&state, 0, sizeof(state));
        ok = dd_gameplay_advance_to(&pack, &state, 356u, 0u);
        if (ok) {
            /* Deterministic counterparts to the controlled FCEUX probes.
               Both enter through the real B edge and shipping dispatcher;
               only the pre-shot carrier coordinates differ. */
            state.phase = DD_GAMEPLAY_LIVE;
            state.possession_direction = 1u;
            state.controlled_player = 0u;
            state.carrier = 0u;
            state.cpu_global_frame = 0u;
            state.ball.action = DD_BALL_DRIBBLE;
            state.ball.owner = 0u;
            state.ball.receiver = 0xFFu;
            state.players[0].action = DD_PLAYER_LIVE_USER_CARRIER;
            state.players[0].facing = 0u;
            state.players[0].court_x = make ? 0x00F700 : 0x00F600;
            state.players[0].court_depth = make ? 0x006100 : 0x005A00;
            state.players[0].height = 0x1000;
            for (player = 1u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
                state.players[player].action = DD_PLAYER_ROUTE_WAIT;
            }
            ok = dd_gameplay_step(&pack, &state, DD_INPUT_B);
        }
        if (ok && checkpoint >= 1) {
            while (state.ball.action != DD_BALL_AIRBORNE && steps < 8u) {
                ok = dd_gameplay_step(&pack, &state, 0u);
                ++steps;
            }
            if (state.ball.action != DD_BALL_AIRBORNE) ok = 0;
        }
        if (ok && checkpoint >= 2) {
            while (state.ball.action != DD_BALL_SCORE &&
                   state.ball.action != DD_BALL_LOOSE_LAUNCH &&
                   state.ball.action != DD_BALL_REBOUND && steps < 420u) {
                ok = dd_gameplay_step(&pack, &state, 0u);
                ++steps;
            }
            if (make) ok = ok && state.ball.action == DD_BALL_SCORE &&
                state.ball.outcome == 1u;
            else ok = ok && state.ball.action != DD_BALL_SCORE;
        }
        if (ok && checkpoint == 5) {
            int running = 0;
            while (ok && !running && steps < 900u) {
                for (player = 0u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
                    if (state.players[player].action == DD_PLAYER_REBOUND_CHASE &&
                        state.players[player].action_age >= 2u &&
                        (state.players[player].velocity_x != 0 ||
                         state.players[player].velocity_depth != 0)) {
                        running = 1;
                        break;
                    }
                }
                if (!running) {
                    ok = dd_gameplay_step(&pack, &state, 0u);
                    ++steps;
                }
            }
            ok = ok && running;
        } else if (ok && checkpoint == 4) {
            int holding = 0;
            while (ok && !holding && steps < 900u) {
                for (player = 0u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
                    if (state.players[player].action == DD_PLAYER_INBOUND_HOLD &&
                        state.ball.action == DD_BALL_DRIBBLE &&
                        state.ball.owner == player) {
                        holding = 1;
                        break;
                    }
                }
                if (!holding) {
                    ok = dd_gameplay_step(&pack, &state, 0u);
                    ++steps;
                }
            }
            ok = ok && holding;
        } else if (ok && (checkpoint == 3 || checkpoint == 6)) {
            if (make) {
                while (state.ball.action != DD_BALL_PASS && steps < 900u) {
                    ok = dd_gameplay_step(&pack, &state, 0u);
                    ++steps;
                }
                ok = ok && state.ball.action == DD_BALL_PASS &&
                    state.ball.receiver >= 5u &&
                    state.ball.receiver < DD_GAMEPLAY_PLAYER_COUNT;
                if (ok && checkpoint == 6) {
                    uint32_t receiver = state.ball.receiver;
                    while (ok && steps < 1100u &&
                           (state.ball.action != DD_BALL_DRIBBLE ||
                            state.carrier != receiver || state.inbound_variant != 0u)) {
                        ok = dd_gameplay_step(&pack, &state, 0u);
                        ++steps;
                    }
                    ok = ok && state.ball.action == DD_BALL_DRIBBLE &&
                        state.carrier == receiver && state.inbound_variant == 0u;
                }
            } else {
                int moving = 1;
                while (state.phase != DD_GAMEPLAY_INBOUND && steps < 900u) {
                    ok = dd_gameplay_step(&pack, &state, 0u);
                    ++steps;
                }
                while (ok && state.phase == DD_GAMEPLAY_INBOUND && moving &&
                       steps < 900u) {
                    int inbounder_holding = 0;
                    moving = 0;
                    for (player = 0u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
                        if (state.players[player].action == DD_PLAYER_INBOUND_FORMATION) {
                            moving = 1;
                        }
                        if (state.players[player].action == DD_PLAYER_INBOUND_HOLD ||
                            state.players[player].action == DD_PLAYER_INBOUND_READY) {
                            inbounder_holding = 1;
                        }
                    }
                    if (!inbounder_holding) moving = 1;
                    if (moving) {
                        ok = dd_gameplay_step(&pack, &state, 0u);
                        ++steps;
                    }
                }
                ok = ok && state.phase == DD_GAMEPLAY_INBOUND &&
                    state.inbound_reason == 0x16u && !moving;
            }
        }
        pixels = (uint32_t *)malloc((size_t)pack.tipoff_meta.width *
                                    pack.tipoff_meta.height * sizeof(uint32_t));
        ok = ok && pixels != NULL &&
             dd_render_gameplay(&pack, &state, pixels,
                                pack.tipoff_meta.width, pack.tipoff_meta.height) &&
             dd_write_bmp(argv[5], pixels, pack.tipoff_meta.width,
                          pack.tipoff_meta.height);
        free(pixels);
        dd_asset_pack_unload(&pack);
        return ok ? 0 : 1;
    }
    if (argc == 5 && strcmp(argv[1], "--render-config") == 0) {
        uint32_t *pixels;
        unsigned long selection;
        char *end;
        int ok;
        selection = strtoul(argv[3], &end, 10);
        if (*argv[3] == '\0' || *end != '\0' || selection > UINT32_MAX ||
            !dd_asset_pack_load(argv[2], &pack)) return 1;
        pixels = (uint32_t *)malloc((size_t)pack.config_meta.width * pack.config_meta.height * sizeof(uint32_t));
        ok = pixels != NULL && dd_render_config(&pack, (uint32_t)selection, pixels,
                                                pack.config_meta.width, pack.config_meta.height) &&
             dd_write_bmp(argv[4], pixels, pack.config_meta.width, pack.config_meta.height);
        free(pixels);
        dd_asset_pack_unload(&pack);
        return ok ? 0 : 1;
    }
    if (argc == 10 && strcmp(argv[1], "--render-config-state") == 0) {
        DDConfigView view;
        uint32_t *pixels;
        unsigned long values[6];
        uint32_t index;
        int ok;
        for (index = 0u; index < 6u; ++index) {
            char *end;
            values[index] = strtoul(argv[index + 3u], &end, 10);
            if (*argv[index + 3u] == '\0' || *end != '\0' || values[index] > UINT32_MAX) return 1;
        }
        if (!dd_asset_pack_load(argv[2], &pack)) return 1;
        memset(&view, 0, sizeof(view));
        view.selection = (uint32_t)values[0];
        view.time_index = (uint32_t)values[1];
        view.team_index = (uint32_t)values[2];
        view.level_index = (uint32_t)values[3];
        view.action_row = (uint32_t)values[4];
        view.action_frame = (uint32_t)values[5];
        view.action_active = 1;
        pixels = (uint32_t *)malloc((size_t)pack.config_meta.width * pack.config_meta.height * sizeof(uint32_t));
        ok = pixels != NULL && dd_render_config_view(&pack, &view, pixels,
                                                     pack.config_meta.width, pack.config_meta.height) &&
             dd_write_bmp(argv[9], pixels, pack.config_meta.width, pack.config_meta.height);
        free(pixels);
        dd_asset_pack_unload(&pack);
        return ok ? 0 : 1;
    }
    dd_usage();
    return 2;
}
