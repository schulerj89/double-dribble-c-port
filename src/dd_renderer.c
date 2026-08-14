#include "dd_renderer.h"

#include <stdio.h>
#include <string.h>

static uint32_t dd_nes_color(uint8_t index) {
    switch (index & 0x3Fu) {
        case 0x06: return 0x00A60000u;
        case 0x07: return 0x007D0800u;
        case 0x0C: return 0x00183C5Du;
        case 0x0F: return 0x00000000u;
        case 0x10: return 0x00BEBEBEu;
        case 0x11: return 0x000071EFu;
        case 0x15: return 0x00E70059u;
        case 0x16: return 0x00DB2800u;
        case 0x20: return 0x00FFFFFFu;
        case 0x26: return 0x00FF7561u;
        default: return 0x00FF00FFu;
    }
}

int dd_render_title(const DDAssetPack *pack, uint32_t *pixels, uint32_t width, uint32_t height) {
    uint8_t background_opaque[256u * 240u];
    uint32_t x;
    uint32_t y;
    if (pack == NULL || pack->ppu == NULL || pack->ppu_size != DD_TITLE_PPU_SIZE ||
        pixels == NULL || width != pack->meta.width || height != pack->meta.height ||
        pack->meta.nametable_base + 0x400u > DD_TITLE_PPU_SIZE ||
        pack->meta.background_pattern_base + 0x1000u > DD_TITLE_PPU_SIZE ||
        pack->oam == NULL || pack->oam_size != 256u || width > 256u || height > 240u) {
        return 0;
    }
    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            uint32_t tile_x = x >> 3;
            uint32_t tile_y = y >> 3;
            uint32_t fine_x = x & 7u;
            uint32_t fine_y = y & 7u;
            uint32_t name_address = pack->meta.nametable_base + tile_y * 32u + tile_x;
            uint8_t tile = pack->ppu[name_address];
            uint32_t pattern_address = pack->meta.background_pattern_base + (uint32_t)tile * 16u + fine_y;
            uint8_t low = pack->ppu[pattern_address];
            uint8_t high = pack->ppu[pattern_address + 8u];
            uint8_t bit = (uint8_t)(7u - fine_x);
            uint8_t color = (uint8_t)(((low >> bit) & 1u) | (((high >> bit) & 1u) << 1));
            uint8_t palette_index;
            if (color == 0u) {
                palette_index = pack->ppu[0x3F00u];
            } else {
                uint32_t attribute_address = pack->meta.nametable_base + 0x3C0u + (tile_y >> 2) * 8u + (tile_x >> 2);
                uint8_t attribute = pack->ppu[attribute_address];
                uint8_t shift = (uint8_t)(((tile_y & 2u) << 1) | (tile_x & 2u));
                uint8_t palette = (uint8_t)((attribute >> shift) & 3u);
                palette_index = pack->ppu[0x3F00u + (uint32_t)palette * 4u + color];
            }
            pixels[y * width + x] = dd_nes_color(palette_index);
            background_opaque[y * width + x] = color != 0u;
        }
    }
    if ((pack->meta.ppu_control & 0x20u) != 0u) {
        int sprite_index;
        for (sprite_index = (int)pack->meta.sprite_count - 1; sprite_index >= 0; --sprite_index) {
            const uint8_t *sprite = pack->oam + (size_t)sprite_index * 4u;
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
                uint8_t low = pack->ppu[pattern_address];
                uint8_t high = pack->ppu[pattern_address + 8u];
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
                    palette_index = pack->ppu[0x3F10u + (uint32_t)(attributes & 3u) * 4u + color];
                    pixels[destination] = dd_nes_color(palette_index);
                }
            }
        }
    }
    return 1;
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
