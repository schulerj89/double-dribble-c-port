#ifndef DD_ASSET_PACK_H
#define DD_ASSET_PACK_H

#include <stddef.h>
#include <stdint.h>

#define DD_TITLE_PPU_SIZE 0x4000u
#define DD_PPU_SIZE DD_TITLE_PPU_SIZE
#define DD_GAMEPLAY_METASPRITE_COUNT 42u

typedef struct DDTitleMeta {
    uint32_t width;
    uint32_t height;
    uint32_t background_pattern_base;
    uint32_t nametable_base;
    uint32_t spoken_frame;
    uint32_t dmc_rate_index;
    uint32_t dmc_initial_dac;
    uint32_t dmc_source_cpu;
    uint32_t dmc_length;
    uint32_t ppu_control;
    uint32_t sprite_count;
    uint32_t select_music_frames;
} DDTitleMeta;

typedef struct DDIntroMeta {
    uint32_t width;
    uint32_t height;
    uint32_t background_pattern_base;
    uint32_t nametable_base;
    uint32_t ppu_control;
    uint32_t sprite_count;
    uint32_t visible_frame;
    uint32_t update_count;
    uint32_t music_frames;
} DDIntroMeta;

typedef struct DDConfigMeta {
    uint32_t width;
    uint32_t height;
    uint32_t background_pattern_base;
    uint32_t nametable_base;
    uint32_t ppu_control;
    uint32_t sprite_count;
    uint32_t option_count;
    uint32_t original_visible_frame;
    uint32_t music_start_frame;
    uint32_t music_loop_frames;
} DDConfigMeta;

typedef struct DDTipoffMeta {
    uint32_t width;
    uint32_t height;
    uint32_t background_pattern_base;
    uint32_t nametable_base;
    uint32_t ppu_control;
    uint32_t sprite_count;
    uint32_t black_frame;
    uint32_t blue_frame;
    uint32_t dmc_frame;
    uint32_t visible_frame;
    uint32_t dmc_rate_index;
    uint32_t dmc_initial_dac;
    uint32_t dmc_source_cpu;
    uint32_t dmc_length;
    uint32_t end_music_frames;
    uint32_t scroll_x;
} DDTipoffMeta;

#pragma pack(push, 1)
typedef struct DDConfigAssetsHeader {
    uint32_t metasprite_offset[15];
    uint32_t metasprite_size[15];
    uint8_t object_table[44];
    uint8_t cursor_y[4];
    uint8_t time_values[4];
    uint8_t time_tiles[32];
    uint8_t team_tiles[96];
    uint8_t base_sprite_palette[8];
    uint8_t team_sprite_palette[16];
    uint8_t level_x[3];
    uint8_t ball_velocity[24];
    uint8_t player_velocity[12];
    uint8_t ball_offsets[14];
    uint8_t basket_y[4];
} DDConfigAssetsHeader;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct DDTipoffAssetsHeader {
    uint32_t metasprite_offset[DD_GAMEPLAY_METASPRITE_COUNT];
    uint32_t metasprite_size[DD_GAMEPLAY_METASPRITE_COUNT];
    int8_t held_ball_offsets[48];
    int8_t height_scripts[32];
} DDTipoffAssetsHeader;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct DDMusicNote {
    uint32_t frame;
    uint16_t period;
    uint8_t channel;
    uint8_t duty;
    uint8_t volume;
    uint8_t reserved;
} DDMusicNote;
#pragma pack(pop)

typedef struct DDAssetPack {
    uint8_t *ppu;
    size_t ppu_size;
    uint8_t *dmc;
    size_t dmc_size;
    uint8_t *oam;
    size_t oam_size;
    uint8_t *intro_ppu;
    size_t intro_ppu_size;
    uint8_t *intro_oam;
    size_t intro_oam_size;
    uint8_t *intro_updates;
    size_t intro_updates_size;
    DDMusicNote *intro_music;
    size_t intro_music_count;
    DDMusicNote *select_music;
    size_t select_music_count;
    DDMusicNote *config_music;
    size_t config_music_count;
    uint8_t *config_ppu;
    size_t config_ppu_size;
    uint8_t *config_oam;
    size_t config_oam_size;
    uint8_t *config_assets;
    size_t config_assets_size;
    uint8_t *tipoff_ppu;
    size_t tipoff_ppu_size;
    uint8_t *tipoff_oam;
    size_t tipoff_oam_size;
    uint8_t *tipoff_assets;
    size_t tipoff_assets_size;
    uint8_t *tipoff_dmc;
    size_t tipoff_dmc_size;
    DDMusicNote *end_music;
    size_t end_music_count;
    DDTitleMeta meta;
    DDIntroMeta intro_meta;
    DDConfigMeta config_meta;
    DDTipoffMeta tipoff_meta;
} DDAssetPack;

int dd_build_asset_pack(const char *rom_path, const char *output_path);
int dd_asset_pack_load(const char *path, DDAssetPack *pack);
void dd_asset_pack_unload(DDAssetPack *pack);
int dd_asset_pack_inspect(const char *path);

#endif
