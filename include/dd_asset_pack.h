#ifndef DD_ASSET_PACK_H
#define DD_ASSET_PACK_H

#include <stddef.h>
#include <stdint.h>

#define DD_TITLE_PPU_SIZE 0x4000u
#define DD_PPU_SIZE DD_TITLE_PPU_SIZE

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
    DDTitleMeta meta;
    DDIntroMeta intro_meta;
} DDAssetPack;

int dd_build_asset_pack(const char *rom_path, const char *output_path);
int dd_asset_pack_load(const char *path, DDAssetPack *pack);
void dd_asset_pack_unload(DDAssetPack *pack);
int dd_asset_pack_inspect(const char *path);

#endif
