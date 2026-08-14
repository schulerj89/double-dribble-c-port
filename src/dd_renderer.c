#include "dd_renderer.h"

#include <stdio.h>
#include <string.h>

static uint32_t dd_nes_color(uint8_t index) {
    static const uint32_t colors[64] = {
        0x00757575u, 0x0024188Cu, 0x000000A8u, 0x0044009Cu,
        0x008C0074u, 0x00AA0010u, 0x00A60000u, 0x007D0800u,
        0x00402C00u, 0x00004500u, 0x00005100u, 0x00003C14u,
        0x00183C5Du, 0x00000000u, 0x00000000u, 0x00000000u,
        0x00BEBEBEu, 0x000071EFu, 0x002038ECu, 0x008200F3u,
        0x00BC00BCu, 0x00E70059u, 0x00DB2800u, 0x00CA4C0Du,
        0x00887000u, 0x00009400u, 0x0000AA00u, 0x00009038u,
        0x00008088u, 0x00000000u, 0x00000000u, 0x00000000u,
        0x00FFFFFFu, 0x003CBCFCu, 0x005C94FCu, 0x00CC88FCu,
        0x00F478FCu, 0x00FC74B4u, 0x00FF7561u, 0x00FF9A3Au,
        0x00F0BC3Cu, 0x0080D010u, 0x004CDC48u, 0x0058F898u,
        0x0000EBDBu, 0x00787878u, 0x00000000u, 0x00000000u,
        0x00FFFFFFu, 0x00AAE7FFu, 0x00C4D4FCu, 0x00D4C8FCu,
        0x00FCC4FCu, 0x00FCC4D8u, 0x00FFBEB0u, 0x00FFDBAAu,
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

int dd_render_title_selection(const DDAssetPack *pack, uint32_t selection, int indicator_visible,
                              uint32_t *pixels, uint32_t width, uint32_t height) {
    uint8_t oam[256];
    if (pack == NULL || width != pack->meta.width || height != pack->meta.height ||
        pack->oam == NULL || pack->oam_size != sizeof(oam)) return 0;
    memcpy(oam, pack->oam, sizeof(oam));
    oam[4] = indicator_visible ? (uint8_t)(selection == 0u ? 0x87u : 0x97u) : 0xF4u;
    return dd_render_scene(pack->ppu, pack->ppu_size, oam, sizeof(oam),
                           pack->meta.background_pattern_base, pack->meta.nametable_base,
                           pack->meta.ppu_control, pack->meta.sprite_count,
                           pixels, width, height);
}

int dd_render_title(const DDAssetPack *pack, uint32_t *pixels, uint32_t width, uint32_t height) {
    return dd_render_title_selection(pack, 0u, 1, pixels, width, height);
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
    if (position != pack->intro_updates_size) return 0;
    if (intro_frame < 1111u) {
        uint32_t shift = (intro_frame + 3u) / 8u;
        uint32_t sprite;
        for (sprite = 1u; sprite <= 16u; ++sprite) {
            uint8_t base_x = oam[sprite * 4u + 3u];
            oam[sprite * 4u + 3u] = shift > base_x ? 0u : (uint8_t)(base_x - shift);
        }
    } else {
        uint32_t sprite;
        for (sprite = 1u; sprite <= 16u; ++sprite) oam[sprite * 4u] = 0xF4u;
    }
    return dd_render_scene(ppu, sizeof(ppu), oam, sizeof(oam),
                           pack->intro_meta.background_pattern_base, pack->intro_meta.nametable_base,
                           pack->intro_meta.ppu_control, pack->intro_meta.sprite_count,
                           pixels, width, height);
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
