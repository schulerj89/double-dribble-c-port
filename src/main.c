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
    puts("  --render-gameplay-block <input.assetpack> <contact|landing> <output.bmp>");
    puts("  --dump-title-wav <input.assetpack> <output.wav>");
    puts("  --dump-intro-wav <input.assetpack> <output.wav>");
    puts("  --dump-select-wav <input.assetpack> <output.wav>");
    puts("  --dump-config-wav <input.assetpack> <output.wav>");
    puts("  --dump-end-wav <input.assetpack> <output.wav>");
    puts("  --dump-tipoff-wav <input.assetpack> <output.wav>");
    puts("  --dump-gameplay-wav <input.assetpack> <output.wav>");
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
                      strcmp(argv[1], "--dump-whistle-wav") == 0 ||
                      strcmp(argv[1], "--dump-three-call-wav") == 0 ||
                      strcmp(argv[1], "--dump-three-score-wav") == 0)) {
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
