#include "dd_renderer.h"

#include <stdio.h>
#include <string.h>

static uint32_t dd_nes_color(uint8_t index) {
    static const uint32_t colors[64] = {
        0x00757575u, 0x0024188Cu, 0x000000AAu, 0x0044009Cu,
        0x008C0074u, 0x00AA0010u, 0x00A60000u, 0x007D0800u,
        0x00402C00u, 0x00004500u, 0x00005100u, 0x00003C14u,
        0x00183C5Du, 0x00000000u, 0x00000000u, 0x00000000u,
        0x00BEBEBEu, 0x000071EFu, 0x002038EFu, 0x008200F3u,
        0x00BC00BCu, 0x00E70059u, 0x00DB2800u, 0x00CB4D0Cu,
        0x00887000u, 0x00009400u, 0x0000AA00u, 0x00009038u,
        0x00008088u, 0x00000000u, 0x00000000u, 0x00000000u,
        0x00FFFFFFu, 0x003CBCFCu, 0x005C94FCu, 0x00CC88FCu,
        0x00F478FCu, 0x00FC74B4u, 0x00FF7561u, 0x00FF9A38u,
        0x00F0BC3Cu, 0x0082D310u, 0x004CDC48u, 0x0058F898u,
        0x0000EBDBu, 0x00787878u, 0x00000000u, 0x00000000u,
        0x00FFFFFFu, 0x00AAE7FFu, 0x00C4D4FCu, 0x00D4C8FCu,
        0x00FCC4FCu, 0x00FCC4D8u, 0x00FFBEB2u, 0x00FFDBAAu,
        0x00FCE4A0u, 0x00E0FCA0u, 0x00A8F0BCu, 0x00B0FCCCu,
        0x009CFCF0u, 0x00C4C4C4u, 0x00000000u, 0x00000000u
    };
    return colors[index & 0x3Fu];
}

static int dd_render_scene(const uint8_t *ppu, size_t ppu_size,
                           const uint8_t *oam, size_t oam_size,
                           uint32_t background_pattern_base, uint32_t nametable_base,
                           uint32_t ppu_control, uint32_t sprite_count,
                           uint32_t *pixels, uint32_t width, uint32_t height) {
    uint8_t background_opaque[256u * 240u];
    uint32_t x;
    uint32_t y;
    if (ppu == NULL || ppu_size != DD_PPU_SIZE || pixels == NULL ||
        nametable_base + 0x400u > DD_PPU_SIZE ||
        background_pattern_base + 0x1000u > DD_PPU_SIZE ||
        oam == NULL || oam_size != 256u || sprite_count > 64u ||
        width > 256u || height > 240u) {
        return 0;
    }
    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            uint32_t tile_x = x >> 3;
            uint32_t tile_y = y >> 3;
            uint32_t fine_x = x & 7u;
            uint32_t fine_y = y & 7u;
            uint32_t name_address = nametable_base + tile_y * 32u + tile_x;
            uint8_t tile = ppu[name_address];
            uint32_t pattern_address = background_pattern_base + (uint32_t)tile * 16u + fine_y;
            uint8_t low = ppu[pattern_address];
            uint8_t high = ppu[pattern_address + 8u];
            uint8_t bit = (uint8_t)(7u - fine_x);
            uint8_t color = (uint8_t)(((low >> bit) & 1u) | (((high >> bit) & 1u) << 1));
            uint8_t palette_index;
            if (color == 0u) {
                palette_index = ppu[0x3F00u];
            } else {
                uint32_t attribute_address = nametable_base + 0x3C0u + (tile_y >> 2) * 8u + (tile_x >> 2);
                uint8_t attribute = ppu[attribute_address];
                uint8_t shift = (uint8_t)(((tile_y & 2u) << 1) | (tile_x & 2u));
                uint8_t palette = (uint8_t)((attribute >> shift) & 3u);
                palette_index = ppu[0x3F00u + (uint32_t)palette * 4u + color];
            }
            pixels[y * width + x] = dd_nes_color(palette_index);
            background_opaque[y * width + x] = color != 0u;
        }
    }
    if ((ppu_control & 0x20u) != 0u) {
        int sprite_index;
        for (sprite_index = (int)sprite_count - 1; sprite_index >= 0; --sprite_index) {
            const uint8_t *sprite = oam + (size_t)sprite_index * 4u;
            uint32_t sprite_y = (uint32_t)sprite[0] + 1u;
            uint32_t sprite_x = sprite[3];
            uint8_t tile = sprite[1];
            uint8_t attributes = sprite[2];
            uint32_t sy;
            for (sy = 0; sy < 16u; ++sy) {
                uint32_t source_y = (attributes & 0x80u) != 0u ? 15u - sy : sy;
                uint32_t tile_number = (uint32_t)(tile & 0xFEu) + (source_y >> 3);
                uint32_t pattern_base = (tile & 1u) != 0u ? 0x1000u : 0u;
                uint32_t pattern_address = pattern_base + tile_number * 16u + (source_y & 7u);
                uint8_t low = ppu[pattern_address];
                uint8_t high = ppu[pattern_address + 8u];
                uint32_t sx;
                uint32_t destination_y = sprite_y + sy;
                if (destination_y >= height) continue;
                for (sx = 0; sx < 8u; ++sx) {
                    uint32_t source_x = (attributes & 0x40u) != 0u ? 7u - sx : sx;
                    uint8_t bit = (uint8_t)(7u - source_x);
                    uint8_t color = (uint8_t)(((low >> bit) & 1u) | (((high >> bit) & 1u) << 1));
                    uint32_t destination_x = sprite_x + sx;
                    uint32_t destination;
                    uint8_t palette_index;
                    if (color == 0u || destination_x >= width) continue;
                    destination = destination_y * width + destination_x;
                    if ((attributes & 0x20u) != 0u && background_opaque[destination] != 0u) continue;
                    palette_index = ppu[0x3F10u + (uint32_t)(attributes & 3u) * 4u + color];
                    pixels[destination] = dd_nes_color(palette_index);
                }
            }
        }
    }
    return 1;
}

static int dd_render_scrolled_scene(const uint8_t *ppu, size_t ppu_size,
                                    const uint8_t *oam, size_t oam_size,
                                    uint32_t background_pattern_base, uint32_t nametable_base,
                                    uint32_t ppu_control, uint32_t sprite_count, uint32_t scroll_x,
                                    uint32_t *pixels, uint32_t width, uint32_t height) {
    uint8_t background_opaque[256u * 240u];
    uint32_t x;
    uint32_t y;
    if (ppu == NULL || ppu_size != DD_PPU_SIZE || oam == NULL || oam_size != 256u ||
        pixels == NULL || width > 256u || height > 240u || sprite_count > 64u ||
        background_pattern_base + 0x1000u > DD_PPU_SIZE || nametable_base + 0x800u > DD_PPU_SIZE) return 0;
    for (y = 0u; y < height; ++y) {
        for (x = 0u; x < width; ++x) {
            /* The fixed HUD uses scroll zero; the court switches at scanline 56. */
            uint32_t world_x = x + (y < 64u ? 0u : scroll_x);
            uint32_t table = nametable_base + ((world_x >> 8u) & 1u) * 0x400u;
            uint32_t tile_x = (world_x >> 3u) & 31u;
            uint32_t tile_y = y >> 3u;
            uint32_t fine_x = world_x & 7u;
            uint32_t fine_y = y & 7u;
            uint8_t tile = ppu[table + tile_y * 32u + tile_x];
            uint32_t pattern = background_pattern_base + (uint32_t)tile * 16u + fine_y;
            uint8_t bit = (uint8_t)(7u - fine_x);
            uint8_t color = (uint8_t)(((ppu[pattern] >> bit) & 1u) | (((ppu[pattern + 8u] >> bit) & 1u) << 1u));
            uint8_t palette_index;
            if (color == 0u) {
                palette_index = ppu[0x3F00u];
            } else {
                uint8_t attribute = ppu[table + 0x3C0u + (tile_y >> 2u) * 8u + (tile_x >> 2u)];
                uint8_t shift = (uint8_t)(((tile_y & 2u) << 1u) | (tile_x & 2u));
                palette_index = ppu[0x3F00u + ((attribute >> shift) & 3u) * 4u + color];
            }
            pixels[y * width + x] = dd_nes_color(palette_index);
            background_opaque[y * width + x] = color != 0u;
        }
    }
    if ((ppu_control & 0x20u) != 0u) {
        int sprite_index;
        for (sprite_index = (int)sprite_count - 1; sprite_index >= 0; --sprite_index) {
            const uint8_t *sprite = oam + (size_t)sprite_index * 4u;
            uint32_t sprite_y = (uint32_t)sprite[0] + 1u;
            uint32_t sprite_x = sprite[3];
            uint8_t tile = sprite[1];
            uint8_t attributes = sprite[2];
            uint32_t sy;
            for (sy = 0u; sy < 16u; ++sy) {
                uint32_t source_y = (attributes & 0x80u) != 0u ? 15u - sy : sy;
                uint32_t tile_number = (uint32_t)(tile & 0xFEu) + (source_y >> 3u);
                uint32_t pattern_base = (tile & 1u) != 0u ? 0x1000u : 0u;
                uint32_t pattern = pattern_base + tile_number * 16u + (source_y & 7u);
                uint32_t sx;
                uint32_t destination_y = sprite_y + sy;
                if (destination_y >= height) continue;
                {
                    int candidate;
                    uint32_t on_scanline = 0u;
                    for (candidate = 0; candidate <= sprite_index; ++candidate) {
                        uint32_t candidate_y = (uint32_t)oam[(size_t)candidate * 4u] + 1u;
                        if (candidate_y <= destination_y && destination_y < candidate_y + 16u) ++on_scanline;
                    }
                    if (on_scanline > 8u) continue;
                }
                for (sx = 0u; sx < 8u; ++sx) {
                    uint32_t source_x = (attributes & 0x40u) != 0u ? 7u - sx : sx;
                    uint8_t bit = (uint8_t)(7u - source_x);
                    uint8_t color = (uint8_t)(((ppu[pattern] >> bit) & 1u) | (((ppu[pattern + 8u] >> bit) & 1u) << 1u));
                    uint32_t destination_x = sprite_x + sx;
                    uint32_t destination;
                    if (color == 0u || destination_x >= width) continue;
                    destination = destination_y * width + destination_x;
                    if ((attributes & 0x20u) != 0u && background_opaque[destination] != 0u) continue;
                    pixels[destination] = dd_nes_color(ppu[0x3F10u + (uint32_t)(attributes & 3u) * 4u + color]);
                }
            }
        }
    }
    return 1;
}

static uint16_t dd_read_u16(const uint8_t *data) {
    return (uint16_t)(data[0] | ((uint16_t)data[1] << 8));
}

static uint32_t dd_read_u32(const uint8_t *data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static int dd_apply_intro_command(uint8_t ppu[DD_PPU_SIZE], const uint8_t *data, size_t size) {
    size_t position = 0;
    uint16_t address;
    if (size < 3u) return 0;
    address = (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
    position = 2u;
    while (position < size) {
        uint8_t value = data[position++];
        if (value == 0xFFu) return position == size;
        if (value == 0xFEu) {
            if (position + 2u > size) return 0;
            address = (uint16_t)(((uint16_t)data[position] << 8) | data[position + 1u]);
            position += 2u;
        } else {
            ppu[address & 0x3FFFu] = value;
            ++address;
        }
    }
    return 0;
}

int dd_render_title_selection(const DDAssetPack *pack, uint32_t selection, int selection_visible,
                              uint32_t *pixels, uint32_t width, uint32_t height) {
    uint8_t ppu[DD_PPU_SIZE];
    uint8_t oam[256];
    if (pack == NULL || width != pack->meta.width || height != pack->meta.height ||
        selection >= 2u || pack->ppu == NULL || pack->ppu_size != sizeof(ppu) ||
        pack->oam == NULL || pack->oam_size != sizeof(oam)) return 0;
    memcpy(ppu, pack->ppu, sizeof(ppu));
    memcpy(oam, pack->oam, sizeof(oam));
    oam[4] = (uint8_t)(selection == 0u ? 0x87u : 0x97u);
    if (!selection_visible) {
        uint32_t column;
        uint32_t row = selection == 0u ? 17u : 19u;
        uint32_t first_column = 5u;
        uint32_t tile_count = selection == 0u ? 2u : 8u;
        for (column = 0u; column < tile_count; ++column) {
            ppu[pack->meta.nametable_base + row * 32u + first_column + column] = 0xA2u;
        }
    }
    return dd_render_scene(ppu, sizeof(ppu), oam, sizeof(oam),
                           pack->meta.background_pattern_base, pack->meta.nametable_base,
                           pack->meta.ppu_control, pack->meta.sprite_count,
                           pixels, width, height);
}

int dd_title_confirmation_visible(uint32_t frame) {
    /*
     * Fixed bank $C230-$C248 tests bit 3 of the descending confirmation
     * timer. Its VRAM command is displayed one frame later, producing an
     * initial blank interval followed by alternating eight-frame bands.
     */
    if (frame == 0u) return 1;
    if (frame == 1u) return 0;
    return ((frame - 2u) & 8u) != 0u;
}

int dd_render_title(const DDAssetPack *pack, uint32_t *pixels, uint32_t width, uint32_t height) {
    return dd_render_title_selection(pack, 0u, 1, pixels, width, height);
}

#define DD_INTRO_BALLOON_FRAME 1110u
#define DD_INTRO_SPRITE_ASSET_SIZE 141u

typedef struct DDIntroObjectState {
    uint8_t state[8];
    uint8_t delay[8];
    uint8_t pass[8];
    uint8_t animation[8];
    uint8_t x[8];
    uint8_t y[8];
    uint8_t complete_count;
    uint8_t flag_started;
    uint8_t flag_animation;
    uint8_t flag_y;
} DDIntroObjectState;

static void dd_intro_init_objects(DDIntroObjectState *state, const uint8_t *assets) {
    uint32_t object;
    memset(state, 0, sizeof(*state));
    for (object = 0u; object < 8u; ++object) {
        state->state[object] = 0xFEu;
        state->delay[object] = assets[object];
        state->animation[object] = assets[8u + object];
        state->x[object] = assets[16u + object];
        state->y[object] = 0x6Cu;
    }
}

static void dd_intro_step_objects(DDIntroObjectState *state, const uint8_t *assets,
                                  uint8_t frame_counter) {
    int object;
    for (object = 7; object >= 0; --object) {
        if (state->state[object] == 0xFFu) {
            if ((frame_counter & 1u) == 0u) {
                --state->y[object];
                if (state->y[object] < 8u) {
                    ++state->pass[object];
                    if (state->pass[object] < 2u) {
                        uint8_t route = (uint8_t)(((frame_counter >> 2u) + frame_counter +
                                                  ((frame_counter >> 1u) & 1u)) & 7u);
                        --state->state[object];
                        state->delay[object] = assets[route];
                        state->animation[object] = assets[8u + route];
                        state->x[object] = assets[16u + route];
                        state->y[object] = 0x6Cu;
                    } else {
                        state->y[object] = 0xFDu;
                        state->state[object] = 0xFDu;
                        ++state->complete_count;
                    }
                }
            }
        } else if (state->state[object] != 0xFDu) {
            --state->delay[object];
            if (state->delay[object] == 0u) ++state->state[object];
        }
    }
    if (state->flag_started == 0u) {
        if (state->complete_count != 0u) {
            state->flag_started = 1u;
            state->flag_animation = 0x74u;
            state->flag_y = 0x78u;
        }
    } else {
        if ((frame_counter & 0x0Fu) == 0u) state->flag_animation ^= 0x0Eu;
        if ((frame_counter & 3u) == 0u && state->flag_y != 0x2Cu) --state->flag_y;
    }
}

static uint32_t dd_intro_emit_metasprite(uint8_t oam[256], uint32_t sprite,
                                         const uint8_t *metasprite, uint8_t base_x,
                                         uint8_t base_y, uint8_t base_attributes) {
    uint32_t record;
    size_t position = 1u;
    uint8_t attributes = base_attributes;
    for (record = 0u; record < metasprite[0] && sprite < 64u; ++record) {
        uint8_t lead = metasprite[position++];
        int8_t y_offset = (int8_t)lead;
        uint8_t tile = metasprite[position++];
        y_offset >>= 1;
        if ((lead & 1u) == 0u) attributes = (uint8_t)((base_attributes | metasprite[position++]) & 0xE3u);
        oam[sprite * 4u] = (uint8_t)(base_y + y_offset);
        oam[sprite * 4u + 1u] = tile;
        oam[sprite * 4u + 2u] = attributes;
        oam[sprite * 4u + 3u] = (uint8_t)(base_x + (int8_t)metasprite[position++]);
        ++sprite;
    }
    return sprite;
}

static void dd_intro_render_objects(uint8_t oam[256], uint32_t intro_frame,
                                    const uint8_t *assets) {
    const uint8_t *balloon = assets + 24u;
    const uint8_t *flag_a = balloon + 5u;
    const uint8_t *flag_b = flag_a + 56u;
    DDIntroObjectState state;
    uint32_t frame;
    uint32_t sprite = 1u;
    uint32_t object;
    uint32_t group;
    dd_intro_init_objects(&state, assets);
    for (frame = DD_INTRO_BALLOON_FRAME + 1u; frame < intro_frame; ++frame) {
        dd_intro_step_objects(&state, assets, (uint8_t)(frame + 149u));
    }
    /* Bank 2 splits objects at the signed-X boundary before building OAM. */
    for (group = 0u; group < 2u; ++group) {
        for (object = 0u; object < 8u; ++object) {
            if (((state.x[object] & 0x80u) != 0u) != (group != 0u)) continue;
            sprite = dd_intro_emit_metasprite(oam, sprite, balloon, state.x[object],
                                              state.y[object], 0u);
        }
    }
    if (state.flag_started != 0u) {
        const uint8_t *flag = state.flag_animation == 0x74u ? flag_a : flag_b;
        sprite = dd_intro_emit_metasprite(oam, sprite, flag, 0x80u, state.flag_y, 0u);
    }
    while (sprite < 64u) {
        oam[sprite * 4u] = 0xF4u;
        ++sprite;
    }
}

int dd_render_intro(const DDAssetPack *pack, uint32_t intro_frame,
                    uint32_t *pixels, uint32_t width, uint32_t height) {
    uint8_t ppu[DD_PPU_SIZE];
    uint8_t oam[256];
    uint32_t update_count;
    uint32_t update;
    uint32_t event_frame = 0u;
    size_t position = 4u;
    if (pack == NULL || pack->intro_ppu == NULL || pack->intro_ppu_size != sizeof(ppu) ||
        pack->intro_oam == NULL || pack->intro_oam_size != sizeof(oam) ||
        pack->intro_updates == NULL || pack->intro_updates_size < 4u ||
        width != pack->intro_meta.width || height != pack->intro_meta.height) return 0;
    memcpy(ppu, pack->intro_ppu, sizeof(ppu));
    memcpy(oam, pack->intro_oam, sizeof(oam));
    update_count = dd_read_u32(pack->intro_updates);
    if (update_count != pack->intro_meta.update_count) return 0;
    for (update = 0; update < update_count; ++update) {
        uint16_t delay;
        uint16_t command_size;
        if (position + 4u > pack->intro_updates_size) return 0;
        delay = dd_read_u16(pack->intro_updates + position);
        command_size = dd_read_u16(pack->intro_updates + position + 2u);
        position += 4u;
        if (position + command_size > pack->intro_updates_size) return 0;
        event_frame += delay;
        if (event_frame <= intro_frame && !dd_apply_intro_command(ppu, pack->intro_updates + position, command_size)) return 0;
        position += command_size;
    }
    if (position + DD_INTRO_SPRITE_ASSET_SIZE != pack->intro_updates_size) return 0;
    if (intro_frame <= DD_INTRO_BALLOON_FRAME) {
        uint32_t shift = (intro_frame + 3u) / 8u;
        uint32_t sprite;
        for (sprite = 1u; sprite <= 16u; ++sprite) {
            uint8_t base_x = oam[sprite * 4u + 3u];
            oam[sprite * 4u + 3u] = shift > base_x ? 0u : (uint8_t)(base_x - shift);
        }
    } else {
        dd_intro_render_objects(oam, intro_frame, pack->intro_updates + position);
    }
    return dd_render_scene(ppu, sizeof(ppu), oam, sizeof(oam),
                           pack->intro_meta.background_pattern_base, pack->intro_meta.nametable_base,
                           pack->intro_meta.ppu_control, pack->intro_meta.sprite_count,
                           pixels, width, height);
}

typedef struct DDConfigObjectState {
    uint8_t animation[11];
    uint8_t attributes[11];
    uint8_t x[11];
    uint8_t y[11];
} DDConfigObjectState;

static void dd_config_step_player(DDConfigObjectState *objects, uint32_t row,
                                  uint8_t *fraction, uint8_t *velocity_low,
                                  uint8_t *velocity_high, uint8_t acceleration) {
    uint16_t sum;
    uint8_t carry;
    if (row == 3u) return;
    sum = (uint16_t)*fraction + *velocity_low;
    *fraction = (uint8_t)sum;
    carry = (uint8_t)(sum > 0xFFu);
    objects->y[5] = (uint8_t)(objects->y[5] + *velocity_high + carry);
    sum = (uint16_t)*velocity_low + acceleration;
    *velocity_low = (uint8_t)sum;
    *velocity_high = (uint8_t)(*velocity_high + (sum > 0xFFu));
    if (objects->y[5] >= 0xB8u) {
        objects->y[5] = 0xB8u;
        objects->animation[5] = 0x68u;
    }
}

static int dd_config_simulate_action(const DDAssetPack *pack, uint32_t row, uint32_t frame,
                                     DDConfigObjectState *objects, int *setting_applied,
                                     int *complete) {
    const DDConfigAssetsHeader *assets;
    uint8_t phase = 0u;
    uint8_t timer = 1u;
    uint8_t ball_x_fraction = 0u;
    uint8_t ball_y_fraction = 0u;
    uint8_t ball_vx_low;
    uint8_t ball_vx_high;
    uint8_t ball_vy_low;
    uint8_t ball_vy_high;
    uint8_t ball_ax;
    uint8_t ball_ay;
    uint8_t player_fraction = 0u;
    uint8_t player_v_low;
    uint8_t player_v_high;
    uint8_t player_acceleration;
    uint8_t end_countdown = 0u;
    uint32_t step;
    if (pack == NULL || pack->config_assets == NULL ||
        pack->config_assets_size < sizeof(DDConfigAssetsHeader) || row >= 4u || objects == NULL) return 0;
    assets = (const DDConfigAssetsHeader *)pack->config_assets;
    memset(objects, 0, sizeof(*objects));
    for (step = 0u; step < 11u; ++step) {
        objects->animation[step] = assets->object_table[step * 4u];
        objects->attributes[step] = assets->object_table[step * 4u + 1u];
        objects->x[step] = assets->object_table[step * 4u + 2u];
        objects->y[step] = assets->object_table[step * 4u + 3u];
    }
    ball_vx_low = assets->ball_velocity[row * 6u];
    ball_vx_high = assets->ball_velocity[row * 6u + 1u];
    ball_vy_low = assets->ball_velocity[row * 6u + 2u];
    ball_vy_high = assets->ball_velocity[row * 6u + 3u];
    ball_ax = assets->ball_velocity[row * 6u + 4u];
    ball_ay = assets->ball_velocity[row * 6u + 5u];
    player_v_low = assets->player_velocity[row * 3u];
    player_v_high = assets->player_velocity[row * 3u + 1u];
    player_acceleration = assets->player_velocity[row * 3u + 2u];
    if (setting_applied != NULL) *setting_applied = 0;
    if (complete != NULL) *complete = 0;
    for (step = 0u; step < frame; ++step) {
        if (end_countdown != 0u) {
            --end_countdown;
            if (end_countdown == 0x60u) {
                objects->animation[5] = 0x68u;
                objects->x[6] = 0xC0u;
                objects->y[6] = 0xA2u;
            }
            if (end_countdown == 0u) {
                if (complete != NULL) *complete = 1;
                break;
            }
            continue;
        }
        if ((int8_t)phase >= 0) {
            --timer;
            if (timer == 0u) {
                uint8_t target = row == 3u ? 0x6Au : 0x6Eu;
                if (row != 3u && objects->animation[5] == 0x68u) objects->animation[5] = 0x6Au;
                ++objects->animation[5];
                if (objects->animation[5] == target) {
                    phase = 0x80u;
                    dd_config_step_player(objects, row, &player_fraction, &player_v_low,
                                          &player_v_high, player_acceleration);
                    continue;
                }
                timer = 0x1Cu;
            }
            if (row == 3u || objects->animation[5] >= 0x6Du) {
                dd_config_step_player(objects, row, &player_fraction, &player_v_low,
                                      &player_v_high, player_acceleration);
            }
            {
                uint32_t offset = (uint32_t)(objects->animation[5] - 0x68u) * 2u;
                objects->x[6] = (uint8_t)(objects->x[5] + assets->ball_offsets[offset]);
                objects->y[6] = (uint8_t)(objects->y[5] + assets->ball_offsets[offset + 1u]);
            }
            continue;
        }
        if (phase != 0xFFu) {
            uint16_t sum;
            uint8_t carry;
            sum = (uint16_t)ball_x_fraction + ball_vx_low;
            ball_x_fraction = (uint8_t)sum;
            carry = (uint8_t)(sum > 0xFFu);
            objects->x[6] = (uint8_t)(objects->x[6] + ball_vx_high + carry);
            sum = (uint16_t)ball_y_fraction + ball_vy_low;
            ball_y_fraction = (uint8_t)sum;
            carry = (uint8_t)(sum > 0xFFu);
            objects->y[6] = (uint8_t)(objects->y[6] + ball_vy_high + carry);
            if ((int8_t)ball_vy_high < 0 || objects->y[6] < assets->basket_y[row]) {
                sum = (uint16_t)ball_vx_low + ball_ax;
                ball_vx_low = (uint8_t)sum;
                ball_vx_high = (uint8_t)(ball_vx_high + (sum > 0xFFu));
                sum = (uint16_t)ball_vy_low + ball_ay;
                ball_vy_low = (uint8_t)sum;
                ball_vy_high = (uint8_t)(ball_vy_high + (sum > 0xFFu));
            } else {
                phase = 0xFFu;
                objects->y[6] = 0xF4u;
                ++objects->animation[row];
                timer = 8u;
            }
            dd_config_step_player(objects, row, &player_fraction, &player_v_low,
                                  &player_v_high, player_acceleration);
            continue;
        }
        --timer;
        if (timer != 0u) {
            dd_config_step_player(objects, row, &player_fraction, &player_v_low,
                                  &player_v_high, player_acceleration);
            continue;
        }
        dd_config_step_player(objects, row, &player_fraction, &player_v_low,
                              &player_v_high, player_acceleration);
        timer = 8u;
        if (objects->animation[row] == 0x60u) {
            if (objects->y[5] >= 0xB8u) {
                if (complete != NULL) *complete = 1;
                break;
            }
            continue;
        }
        ++objects->animation[row];
        if (objects->animation[row] >= 0x63u) {
            objects->animation[row] = 0x60u;
            if (setting_applied != NULL) *setting_applied = 1;
            if (row == 3u) end_countdown = 0x80u;
        }
    }
    return 1;
}

int dd_config_action_status(const DDAssetPack *pack, uint32_t row, uint32_t frame,
                            int *setting_applied, int *complete) {
    DDConfigObjectState objects;
    return dd_config_simulate_action(pack, row, frame, &objects, setting_applied, complete);
}

static uint32_t dd_config_emit_object(const DDAssetPack *pack, const DDConfigAssetsHeader *assets,
                                      uint8_t oam[256], uint32_t sprite, uint8_t animation,
                                      uint8_t x, uint8_t y, uint8_t attributes) {
    uint32_t index;
    if (animation < 0x60u || animation > 0x6Eu) return sprite;
    index = animation - 0x60u;
    if (assets->metasprite_offset[index] > pack->config_assets_size ||
        assets->metasprite_size[index] > pack->config_assets_size - assets->metasprite_offset[index]) return sprite;
    return dd_intro_emit_metasprite(oam, sprite,
                                    pack->config_assets + assets->metasprite_offset[index], x, y, attributes);
}

int dd_render_config_view(const DDAssetPack *pack, const DDConfigView *view,
                          uint32_t *pixels, uint32_t width, uint32_t height) {
    const DDConfigAssetsHeader *assets;
    DDConfigObjectState objects;
    uint8_t ppu[DD_PPU_SIZE];
    uint8_t oam[256];
    uint32_t object;
    uint32_t sprite = 1u;
    if (pack == NULL || view == NULL || pack->config_ppu == NULL ||
        pack->config_ppu_size != sizeof(ppu) || pack->config_assets == NULL ||
        pack->config_assets_size < sizeof(DDConfigAssetsHeader) ||
        view->selection >= 4u || view->time_index >= 4u || view->team_index >= 4u ||
        view->level_index >= 3u || width != pack->config_meta.width || height != pack->config_meta.height) return 0;
    assets = (const DDConfigAssetsHeader *)pack->config_assets;
    memcpy(ppu, pack->config_ppu, sizeof(ppu));
    memcpy(ppu + 0x2073u, assets->time_tiles + view->time_index * 8u, 4u);
    memcpy(ppu + 0x2093u, assets->time_tiles + view->time_index * 8u + 4u, 4u);
    memcpy(ppu + 0x20F3u, assets->team_tiles + view->team_index * 24u, 12u);
    memcpy(ppu + 0x2113u, assets->team_tiles + view->team_index * 24u + 12u, 12u);
    memcpy(ppu + 0x3F10u, assets->base_sprite_palette, 8u);
    memcpy(ppu + 0x3F18u, assets->team_sprite_palette + view->team_index * 4u, 4u);
    memcpy(ppu + 0x3F1Cu, assets->team_sprite_palette + 4u, 4u);
    if (view->action_active) {
        if (!dd_config_simulate_action(pack, view->action_row, view->action_frame, &objects, NULL, NULL)) return 0;
    } else {
        memset(&objects, 0, sizeof(objects));
        for (object = 0u; object < 11u; ++object) {
            objects.animation[object] = assets->object_table[object * 4u];
            objects.attributes[object] = assets->object_table[object * 4u + 1u];
            objects.x[object] = assets->object_table[object * 4u + 2u];
            objects.y[object] = assets->object_table[object * 4u + 3u];
        }
    }
    objects.x[7] = assets->level_x[view->level_index];
    objects.y[10] = assets->cursor_y[view->selection];
    memset(oam, 0, sizeof(oam));
    for (object = 0u; object < 64u; ++object) oam[object * 4u] = 0xF4u;
    oam[0] = 0x38u; oam[1] = 0xFEu; oam[2] = 0x30u; oam[3] = 0x20u;
    for (object = 0u; object < 11u; ++object) {
        sprite = dd_config_emit_object(pack, assets, oam, sprite, objects.animation[object],
                                       objects.x[object], objects.y[object], objects.attributes[object]);
    }
    return dd_render_scene(ppu, sizeof(ppu), oam, sizeof(oam),
                           pack->config_meta.background_pattern_base, pack->config_meta.nametable_base,
                           pack->config_meta.ppu_control, pack->config_meta.sprite_count,
                           pixels, width, height);
}

int dd_render_config(const DDAssetPack *pack, uint32_t selection,
                     uint32_t *pixels, uint32_t width, uint32_t height) {
    DDConfigView view;
    memset(&view, 0, sizeof(view));
    view.selection = selection;
    return dd_render_config_view(pack, &view, pixels, width, height);
}

int dd_render_tipoff(const DDAssetPack *pack, uint32_t *pixels, uint32_t width, uint32_t height) {
    if (pack == NULL || pack->tipoff_ppu == NULL || pack->tipoff_ppu_size != DD_PPU_SIZE ||
        pack->tipoff_oam == NULL || pack->tipoff_oam_size != 256u ||
        width != pack->tipoff_meta.width || height != pack->tipoff_meta.height) return 0;
    return dd_render_scrolled_scene(pack->tipoff_ppu, pack->tipoff_ppu_size,
                                    pack->tipoff_oam, pack->tipoff_oam_size,
                                    pack->tipoff_meta.background_pattern_base,
                                    pack->tipoff_meta.nametable_base,
                                    pack->tipoff_meta.ppu_control,
                                    pack->tipoff_meta.sprite_count,
                                    pack->tipoff_meta.scroll_x,
                                    pixels, width, height);
}

static uint32_t dd_gameplay_emit(const DDAssetPack *pack, const DDTipoffAssetsHeader *assets,
                                 uint8_t oam[256], uint32_t sprite, uint8_t animation,
                                 int32_t screen_x, int32_t screen_y, uint8_t attributes) {
    uint32_t offset;
    uint32_t size;
    if (animation >= DD_GAMEPLAY_METASPRITE_COUNT || screen_x < -32 || screen_x > 287 ||
        screen_y < -32 || screen_y > 255) return sprite;
    offset = assets->metasprite_offset[animation];
    size = assets->metasprite_size[animation];
    if (offset < sizeof(*assets) || offset > pack->tipoff_assets_size ||
        size == 0u || size > pack->tipoff_assets_size - offset) return sprite;
    return dd_intro_emit_metasprite(oam, sprite, pack->tipoff_assets + offset,
                                    (uint8_t)screen_x, (uint8_t)screen_y, attributes);
}

int dd_render_gameplay(const DDAssetPack *pack, const DDGameplayState *state,
                       uint32_t *pixels, uint32_t width, uint32_t height) {
    const DDTipoffAssetsHeader *assets;
    uint8_t oam[256];
    uint32_t sprite = 1u;
    uint32_t player;
    int32_t camera;
    if (pack == NULL || state == NULL || pack->tipoff_ppu == NULL ||
        pack->tipoff_ppu_size != DD_PPU_SIZE || pack->tipoff_assets == NULL ||
        pack->tipoff_assets_size < sizeof(DDTipoffAssetsHeader) ||
        width != pack->tipoff_meta.width || height != pack->tipoff_meta.height) return 0;
    if (state->scene_frame < 270u) return dd_render_tipoff(pack, pixels, width, height);
    assets = (const DDTipoffAssetsHeader *)pack->tipoff_assets;
    memset(oam, 0, sizeof(oam));
    for (player = 0u; player < 64u; ++player) oam[player * 4u] = 0xF4u;
    oam[0] = 0x38u; oam[1] = 0xFEu; oam[2] = 0x30u; oam[3] = 0x20u;
    camera = state->camera_x >> 8;
    sprite = dd_gameplay_emit(pack, assets, oam, sprite, 2u,
                              (state->ball.court_x >> 8) - camera,
                              0xF0 - ((state->ball.court_depth >> 8) + 2), 0u);
    sprite = dd_gameplay_emit(pack, assets, oam, sprite, state->ball.animation,
                              (state->ball.court_x >> 8) - camera,
                              0xF0 - (state->ball.court_depth >> 8) - (state->ball.height >> 8),
                              state->ball.attributes);
    for (player = 0u; player < DD_GAMEPLAY_PLAYER_COUNT; ++player) {
        const DDPlayerState *object = &state->players[player];
        sprite = dd_gameplay_emit(pack, assets, oam, sprite, object->animation,
                                  (object->court_x >> 8) - camera,
                                  0xF0 - (object->court_depth >> 8) - (object->height >> 8),
                                  object->attributes);
    }
    while (sprite < 64u) {
        oam[sprite * 4u] = 0xF4u;
        ++sprite;
    }
    return dd_render_scrolled_scene(pack->tipoff_ppu, pack->tipoff_ppu_size,
                                    oam, sizeof(oam), pack->tipoff_meta.background_pattern_base,
                                    pack->tipoff_meta.nametable_base, pack->tipoff_meta.ppu_control,
                                    64u, (uint32_t)camera, pixels, width, height);
}

#pragma pack(push, 1)
typedef struct DDBmpFileHeader {
    uint16_t type;
    uint32_t size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t pixel_offset;
} DDBmpFileHeader;

typedef struct DDBmpInfoHeader {
    uint32_t size;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bits_per_pixel;
    uint32_t compression;
    uint32_t image_size;
    int32_t x_pixels_per_meter;
    int32_t y_pixels_per_meter;
    uint32_t colors_used;
    uint32_t important_colors;
} DDBmpInfoHeader;
#pragma pack(pop)

int dd_write_bmp(const char *path, const uint32_t *pixels, uint32_t width, uint32_t height) {
    FILE *file;
    DDBmpFileHeader file_header;
    DDBmpInfoHeader info_header;
    uint32_t row;
    memset(&file_header, 0, sizeof(file_header));
    memset(&info_header, 0, sizeof(info_header));
    file_header.type = 0x4D42u;
    file_header.pixel_offset = sizeof(file_header) + sizeof(info_header);
    file_header.size = file_header.pixel_offset + width * height * 4u;
    info_header.size = sizeof(info_header);
    info_header.width = (int32_t)width;
    info_header.height = (int32_t)height;
    info_header.planes = 1u;
    info_header.bits_per_pixel = 32u;
    info_header.image_size = width * height * 4u;
    file = fopen(path, "wb");
    if (file == NULL || fwrite(&file_header, 1, sizeof(file_header), file) != sizeof(file_header) ||
        fwrite(&info_header, 1, sizeof(info_header), file) != sizeof(info_header)) {
        if (file != NULL) fclose(file);
        return 0;
    }
    for (row = 0; row < height; ++row) {
        const uint32_t *source = pixels + (height - 1u - row) * width;
        if (fwrite(source, sizeof(uint32_t), width, file) != width) {
            fclose(file);
            return 0;
        }
    }
    fclose(file);
    return 1;
}
