#include "dd_asset_pack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <bcrypt.h>

#pragma comment(lib, "bcrypt.lib")

#define DD_PACK_VERSION 1u
#define DD_ENTRY_PPU 1u
#define DD_ENTRY_DMC 2u
#define DD_ENTRY_META 3u
#define DD_ENTRY_OAM 4u
#define DD_ENTRY_COUNT 4u
#define DD_ROM_SIZE 131088u

#pragma pack(push, 1)
typedef struct DDPackHeader {
    char magic[4];
    uint32_t version;
    uint32_t header_size;
    uint32_t entry_count;
    uint32_t flags;
    uint8_t source_sha256[32];
    uint32_t directory_crc32;
    uint32_t reserved;
    uint64_t total_size;
} DDPackHeader;

typedef struct DDPackEntry {
    char id[16];
    uint32_t type;
    uint32_t version;
    uint64_t offset;
    uint64_t size;
    uint32_t crc32;
    uint32_t source_bank;
    uint32_t source_offset;
    uint32_t source_size;
    uint32_t transform;
    uint32_t reserved;
} DDPackEntry;
#pragma pack(pop)

static const uint8_t DD_EXPECTED_SHA256[32] = {
    0xBF, 0x39, 0x7E, 0xAE, 0x94, 0x86, 0x04, 0x4F,
    0xCA, 0x90, 0xA9, 0x92, 0x15, 0x33, 0x02, 0x03,
    0xD6, 0xF8, 0x5C, 0xAB, 0x63, 0xA8, 0x07, 0x2F,
    0x28, 0xCA, 0xCC, 0x13, 0x9B, 0x53, 0x88, 0xCF
};

static uint32_t dd_crc32(const uint8_t *data, size_t size) {
    uint32_t crc = 0xFFFFFFFFu;
    size_t index;
    for (index = 0; index < size; ++index) {
        uint32_t bit;
        crc ^= data[index];
        for (bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

static int dd_sha256(const uint8_t *data, size_t size, uint8_t digest[32]) {
    BCRYPT_ALG_HANDLE algorithm = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    NTSTATUS status;
    status = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, NULL, 0);
    if (status < 0) {
        return 0;
    }
    status = BCryptCreateHash(algorithm, &hash, NULL, 0, NULL, 0, 0);
    if (status >= 0) {
        status = BCryptHashData(hash, (PUCHAR)data, (ULONG)size, 0);
    }
    if (status >= 0) {
        status = BCryptFinishHash(hash, digest, 32, 0);
    }
    if (hash != NULL) {
        BCryptDestroyHash(hash);
    }
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return status >= 0;
}

static int dd_read_file(const char *path, uint8_t **data, size_t *size) {
    FILE *file;
    long length;
    uint8_t *bytes;
    *data = NULL;
    *size = 0;
    file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "Could not open %s\n", path);
        return 0;
    }
    if (fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return 0;
    }
    bytes = (uint8_t *)malloc((size_t)length);
    if (bytes == NULL || fread(bytes, 1, (size_t)length, file) != (size_t)length) {
        free(bytes);
        fclose(file);
        return 0;
    }
    fclose(file);
    *data = bytes;
    *size = (size_t)length;
    return 1;
}

static size_t dd_bank_file_offset(uint32_t bank, uint32_t cpu_address) {
    return 16u + ((size_t)bank * 0x4000u) + (cpu_address - 0x8000u);
}

static void dd_ppu_write(uint8_t ppu[DD_TITLE_PPU_SIZE], uint16_t address, uint8_t value) {
    address &= 0x3FFFu;
    ppu[address] = value;
    if (address >= 0x3F00u) {
        uint16_t palette_address = (uint16_t)(0x3F00u | (address & 0x001Fu));
        if ((palette_address & 0x0013u) == 0x0010u) {
            palette_address = (uint16_t)(palette_address & ~0x0010u);
        }
        ppu[palette_address] = value;
    }
}

static int dd_decode_stream(const uint8_t *rom, size_t rom_size, uint32_t bank,
                            uint32_t cpu_address, uint8_t ppu[DD_TITLE_PPU_SIZE],
                            uint32_t *consumed) {
    size_t position = dd_bank_file_offset(bank, cpu_address);
    const size_t bank_end = 16u + ((size_t)(bank + 1u) * 0x4000u);
    uint16_t ppu_address;
    size_t start = position;
    if (bank >= 7u || cpu_address < 0x8000u || cpu_address >= 0xC000u ||
        position + 2u > bank_end || bank_end > rom_size) {
        return 0;
    }
    ppu_address = (uint16_t)(rom[position] | ((uint16_t)rom[position + 1u] << 8));
    position += 2u;
    for (;;) {
        uint8_t command;
        uint32_t count;
        if (position >= bank_end) {
            return 0;
        }
        command = rom[position++];
        if (command == 0xFFu) {
            *consumed = (uint32_t)(position - start);
            return 1;
        }
        if (command == 0x7Fu) {
            if (position + 2u > bank_end) {
                return 0;
            }
            ppu_address = (uint16_t)(rom[position] | ((uint16_t)rom[position + 1u] << 8));
            position += 2u;
            continue;
        }
        if ((command & 0x80u) != 0u) {
            count = command & 0x7Fu;
            if (count == 0u) {
                count = 256u;
            }
            if (position + count > bank_end) {
                return 0;
            }
            while (count-- != 0u) {
                dd_ppu_write(ppu, ppu_address++, rom[position++]);
            }
        } else {
            uint8_t value;
            count = command == 0u ? 256u : command;
            if (position >= bank_end) {
                return 0;
            }
            value = rom[position++];
            while (count-- != 0u) {
                dd_ppu_write(ppu, ppu_address++, value);
            }
        }
    }
}

static void dd_set_entry(DDPackEntry *entry, const char *id, uint32_t type,
                         uint64_t offset, uint64_t size, uint32_t crc32,
                         uint32_t bank, uint32_t source_offset,
                         uint32_t source_size, uint32_t transform) {
    memset(entry, 0, sizeof(*entry));
    strncpy(entry->id, id, sizeof(entry->id) - 1u);
    entry->type = type;
    entry->version = 1u;
    entry->offset = offset;
    entry->size = size;
    entry->crc32 = crc32;
    entry->source_bank = bank;
    entry->source_offset = source_offset;
    entry->source_size = source_size;
    entry->transform = transform;
}

static void dd_build_title_oam(uint8_t oam[256]) {
    uint32_t sprite;
    uint32_t row;
    uint32_t column;
    memset(oam, 0, 256);
    for (sprite = 0; sprite < 64u; ++sprite) {
        oam[sprite * 4u] = 0xF4u;
    }
    oam[0] = 0x38u; oam[1] = 0xFEu; oam[2] = 0x30u; oam[3] = 0x20u;
    oam[4] = 0x87u; oam[5] = 0x0Eu; oam[6] = 0x01u; oam[7] = 0x1Cu;
    sprite = 2u;
    for (row = 0; row < 3u; ++row) {
        for (column = 0; column < 2u; ++column) {
            uint32_t offset = sprite * 4u;
            oam[offset] = (uint8_t)(0x2Fu + row * 0x10u);
            oam[offset + 1u] = (uint8_t)(2u + (sprite - 2u) * 2u);
            oam[offset + 3u] = (uint8_t)(0x18u + column * 8u);
            ++sprite;
        }
    }
    for (row = 0; row < 2u; ++row) {
        for (column = 0; column < 5u; ++column) {
            uint32_t offset = sprite * 4u;
            oam[offset] = (uint8_t)(0x2Fu + row * 0x10u);
            oam[offset + 1u] = (uint8_t)(0x10u + (row * 5u + column) * 2u);
            oam[offset + 3u] = (uint8_t)(0x38u + column * 8u);
            ++sprite;
        }
    }
    for (column = 0; column < 2u; ++column) {
        uint32_t offset = sprite * 4u;
        oam[offset] = 0x4Fu;
        oam[offset + 1u] = (uint8_t)(0x24u + column * 2u);
        oam[offset + 3u] = (uint8_t)(0x50u + column * 8u);
        ++sprite;
    }
}

int dd_build_asset_pack(const char *rom_path, const char *output_path) {
    uint8_t *rom = NULL;
    size_t rom_size = 0;
    uint8_t digest[32];
    uint8_t ppu[DD_TITLE_PPU_SIZE] = {0};
    uint8_t oam[256];
    uint32_t consumed[3] = {0};
    const uint32_t dmc_file_offset = 0x1EAD0u;
    const uint32_t dmc_size = 3073u;
    const uint32_t title_palette_file_offset = 0x1C956u;
    DDTitleMeta meta = {256u, 240u, 0x1000u, 0x2000u, 10u, 15u, 0u, 0xEAC0u, 3073u, 0xB0u, 20u};
    DDPackHeader header;
    DDPackEntry entries[DD_ENTRY_COUNT];
    uint64_t payload_offset = sizeof(header) + sizeof(entries);
    FILE *output;

    if (!dd_read_file(rom_path, &rom, &rom_size)) {
        return 0;
    }
    if (rom_size != DD_ROM_SIZE || memcmp(rom, "NES\x1A", 4) != 0 || rom[4] != 8u ||
        rom[5] != 0u || ((rom[6] >> 4) & 0x0Fu) != 2u ||
        !dd_sha256(rom, rom_size, digest) || memcmp(digest, DD_EXPECTED_SHA256, 32) != 0) {
        fprintf(stderr, "Unsupported ROM. Expected Double Dribble (USA) (Rev 1), SHA-256 BF397E...88CF.\n");
        free(rom);
        return 0;
    }
    if (!dd_decode_stream(rom, rom_size, 5u, 0xAFB2u, ppu, &consumed[0]) ||
        !dd_decode_stream(rom, rom_size, 6u, 0xB0A0u, ppu, &consumed[1]) ||
        !dd_decode_stream(rom, rom_size, 2u, 0xA7C2u, ppu, &consumed[2]) ||
        title_palette_file_offset + 32u > rom_size || dmc_file_offset + dmc_size > rom_size) {
        fprintf(stderr, "The title asset streams were malformed.\n");
        free(rom);
        return 0;
    }
    memcpy(ppu + 0x3F00u, rom + title_palette_file_offset, 32u);
    dd_build_title_oam(oam);

    memset(&header, 0, sizeof(header));
    memcpy(header.magic, "DDAP", 4);
    header.version = DD_PACK_VERSION;
    header.header_size = (uint32_t)sizeof(header);
    header.entry_count = DD_ENTRY_COUNT;
    memcpy(header.source_sha256, digest, 32);

    dd_set_entry(&entries[0], "title.meta", DD_ENTRY_META, payload_offset,
                 sizeof(meta), dd_crc32((const uint8_t *)&meta, sizeof(meta)),
                 0xFFFFFFFFu, 0u, 0u, 0u);
    payload_offset += sizeof(meta);
    dd_set_entry(&entries[1], "title.ppu", DD_ENTRY_PPU, payload_offset,
                 sizeof(ppu), dd_crc32(ppu, sizeof(ppu)),
                 0xFFFFFFFFu, 0u, consumed[0] + consumed[1] + consumed[2], 1u);
    payload_offset += sizeof(ppu);
    dd_set_entry(&entries[2], "title.dmc", DD_ENTRY_DMC, payload_offset,
                 dmc_size, dd_crc32(rom + dmc_file_offset, dmc_size),
                 7u, dmc_file_offset, dmc_size, 2u);
    payload_offset += dmc_size;
    dd_set_entry(&entries[3], "title.oam", DD_ENTRY_OAM, payload_offset,
                 sizeof(oam), dd_crc32(oam, sizeof(oam)),
                 0xFFFFFFFFu, 0u, 0u, 3u);
    payload_offset += sizeof(oam);
    header.directory_crc32 = dd_crc32((const uint8_t *)entries, sizeof(entries));
    header.total_size = payload_offset;

    output = fopen(output_path, "wb");
    if (output == NULL || fwrite(&header, 1, sizeof(header), output) != sizeof(header) ||
        fwrite(entries, 1, sizeof(entries), output) != sizeof(entries) ||
        fwrite(&meta, 1, sizeof(meta), output) != sizeof(meta) ||
        fwrite(ppu, 1, sizeof(ppu), output) != sizeof(ppu) ||
        fwrite(rom + dmc_file_offset, 1, dmc_size, output) != dmc_size ||
        fwrite(oam, 1, sizeof(oam), output) != sizeof(oam)) {
        if (output != NULL) {
            fclose(output);
        }
        free(rom);
        return 0;
    }
    fclose(output);
    free(rom);
    printf("Built %s (title streams: %u/%u/%u bytes, DMC: %u bytes).\n",
           output_path, consumed[0], consumed[1], consumed[2], dmc_size);
    return 1;
}

static const DDPackEntry *dd_find_entry(const DDPackEntry *entries, uint32_t count,
                                        uint32_t type, const char *id) {
    uint32_t index;
    for (index = 0; index < count; ++index) {
        if (entries[index].type == type && strncmp(entries[index].id, id, sizeof(entries[index].id)) == 0) {
            return &entries[index];
        }
    }
    return NULL;
}

static int dd_entry_in_bounds(const DDPackEntry *entry, size_t file_size) {
    return entry->offset <= file_size && entry->size <= file_size - (size_t)entry->offset;
}

int dd_asset_pack_load(const char *path, DDAssetPack *pack) {
    uint8_t *file_data = NULL;
    size_t file_size = 0;
    const DDPackHeader *header;
    const DDPackEntry *entries;
    const DDPackEntry *meta_entry;
    const DDPackEntry *ppu_entry;
    const DDPackEntry *dmc_entry;
    const DDPackEntry *oam_entry;
    memset(pack, 0, sizeof(*pack));
    if (!dd_read_file(path, &file_data, &file_size) || file_size < sizeof(DDPackHeader) + sizeof(DDPackEntry) * DD_ENTRY_COUNT) {
        free(file_data);
        return 0;
    }
    header = (const DDPackHeader *)file_data;
    entries = (const DDPackEntry *)(file_data + sizeof(*header));
    if (memcmp(header->magic, "DDAP", 4) != 0 || header->version != DD_PACK_VERSION ||
        header->header_size != sizeof(*header) || header->entry_count != DD_ENTRY_COUNT ||
        header->total_size != file_size || memcmp(header->source_sha256, DD_EXPECTED_SHA256, 32) != 0 ||
        header->directory_crc32 != dd_crc32((const uint8_t *)entries, sizeof(DDPackEntry) * DD_ENTRY_COUNT)) {
        free(file_data);
        return 0;
    }
    meta_entry = dd_find_entry(entries, header->entry_count, DD_ENTRY_META, "title.meta");
    ppu_entry = dd_find_entry(entries, header->entry_count, DD_ENTRY_PPU, "title.ppu");
    dmc_entry = dd_find_entry(entries, header->entry_count, DD_ENTRY_DMC, "title.dmc");
    oam_entry = dd_find_entry(entries, header->entry_count, DD_ENTRY_OAM, "title.oam");
    if (meta_entry == NULL || ppu_entry == NULL || dmc_entry == NULL || oam_entry == NULL ||
        meta_entry->size != sizeof(DDTitleMeta) || ppu_entry->size != DD_TITLE_PPU_SIZE ||
        dmc_entry->size != 3073u || oam_entry->size != 256u ||
        !dd_entry_in_bounds(meta_entry, file_size) || !dd_entry_in_bounds(ppu_entry, file_size) ||
        !dd_entry_in_bounds(dmc_entry, file_size) || !dd_entry_in_bounds(oam_entry, file_size) ||
        meta_entry->crc32 != dd_crc32(file_data + meta_entry->offset, (size_t)meta_entry->size) ||
        ppu_entry->crc32 != dd_crc32(file_data + ppu_entry->offset, (size_t)ppu_entry->size) ||
        dmc_entry->crc32 != dd_crc32(file_data + dmc_entry->offset, (size_t)dmc_entry->size) ||
        oam_entry->crc32 != dd_crc32(file_data + oam_entry->offset, (size_t)oam_entry->size)) {
        free(file_data);
        return 0;
    }
    memcpy(&pack->meta, file_data + meta_entry->offset, sizeof(pack->meta));
    pack->ppu = (uint8_t *)malloc((size_t)ppu_entry->size);
    pack->dmc = (uint8_t *)malloc((size_t)dmc_entry->size);
    pack->oam = (uint8_t *)malloc((size_t)oam_entry->size);
    if (pack->ppu == NULL || pack->dmc == NULL || pack->oam == NULL) {
        dd_asset_pack_unload(pack);
        free(file_data);
        return 0;
    }
    memcpy(pack->ppu, file_data + ppu_entry->offset, (size_t)ppu_entry->size);
    memcpy(pack->dmc, file_data + dmc_entry->offset, (size_t)dmc_entry->size);
    memcpy(pack->oam, file_data + oam_entry->offset, (size_t)oam_entry->size);
    pack->ppu_size = (size_t)ppu_entry->size;
    pack->dmc_size = (size_t)dmc_entry->size;
    pack->oam_size = (size_t)oam_entry->size;
    free(file_data);
    return 1;
}

void dd_asset_pack_unload(DDAssetPack *pack) {
    free(pack->ppu);
    free(pack->dmc);
    free(pack->oam);
    memset(pack, 0, sizeof(*pack));
}

int dd_asset_pack_inspect(const char *path) {
    DDAssetPack pack;
    if (!dd_asset_pack_load(path, &pack)) {
        fprintf(stderr, "Invalid asset pack: %s\n", path);
        return 0;
    }
    printf("Valid DDAP v1: %ux%u title, %zu PPU bytes, %zu DMC bytes, spoken frame %u.\n",
           pack.meta.width, pack.meta.height, pack.ppu_size, pack.dmc_size, pack.meta.spoken_frame);
    dd_asset_pack_unload(&pack);
    return 1;
}
