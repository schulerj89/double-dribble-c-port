#include "dd_asset_pack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <bcrypt.h>

#pragma comment(lib, "bcrypt.lib")

#define DD_PACK_VERSION 20u
#define DD_ENTRY_PPU 1u
#define DD_ENTRY_DMC 2u
#define DD_ENTRY_META 3u
#define DD_ENTRY_OAM 4u
#define DD_ENTRY_INTRO_META 5u
#define DD_ENTRY_UPDATES 6u
#define DD_ENTRY_MUSIC 7u
#define DD_ENTRY_CONFIG_META 8u
#define DD_ENTRY_CONFIG_ASSETS 9u
#define DD_ENTRY_TIPOFF_META 10u
#define DD_ENTRY_TIPOFF_ASSETS 11u
#define DD_ENTRY_COUNT 28u
#define DD_ROM_SIZE 131088u
#define DD_INTRO_SPRITE_ASSET_SIZE 141u

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

static const DDMusicNote DD_INTRO_MUSIC[] = {
    {1u, 285u, 1u, 1u, 15u, 0u}, {1u, 285u, 2u, 0u, 12u, 0u},
    {17u, 339u, 1u, 1u, 15u, 0u}, {17u, 285u, 2u, 0u, 12u, 0u},
    {25u, 572u, 0u, 3u, 15u, 0u}, {25u, 427u, 1u, 1u, 15u, 0u}, {25u, 427u, 2u, 0u, 12u, 0u},
    {49u, 428u, 0u, 3u, 15u, 0u}, {49u, 339u, 1u, 1u, 15u, 0u}, {49u, 427u, 2u, 0u, 12u, 0u},
    {73u, 382u, 0u, 3u, 15u, 0u}, {73u, 285u, 1u, 1u, 15u, 0u}, {73u, 453u, 2u, 0u, 12u, 0u},
    {97u, 340u, 0u, 3u, 15u, 0u}, {97u, 213u, 1u, 1u, 15u, 0u}, {97u, 508u, 2u, 0u, 12u, 0u},
    {145u, 227u, 0u, 3u, 15u, 0u}, {145u, 169u, 1u, 1u, 15u, 0u}, {145u, 679u, 2u, 0u, 12u, 0u},
    {161u, 190u, 1u, 1u, 15u, 0u},
    {169u, 340u, 0u, 3u, 15u, 0u}, {169u, 213u, 1u, 1u, 15u, 0u}, {169u, 508u, 2u, 0u, 12u, 0u},
    {193u, 428u, 0u, 3u, 15u, 0u}, {193u, 339u, 1u, 1u, 15u, 0u}, {193u, 508u, 2u, 0u, 12u, 0u},
    {217u, 382u, 0u, 3u, 15u, 0u}, {217u, 302u, 1u, 1u, 15u, 0u}, {217u, 381u, 2u, 0u, 12u, 0u},
    {241u, 454u, 0u, 3u, 15u, 0u}, {241u, 285u, 1u, 1u, 15u, 0u}, {241u, 571u, 2u, 0u, 12u, 0u},
    {265u, 320u, 2u, 0u, 12u, 0u},
    {289u, 454u, 0u, 3u, 15u, 0u}, {289u, 285u, 1u, 1u, 15u, 0u}, {289u, 339u, 2u, 0u, 12u, 0u},
    {305u, 454u, 0u, 3u, 15u, 0u}, {305u, 285u, 1u, 1u, 15u, 0u}, {305u, 381u, 2u, 0u, 12u, 0u},
    {313u, 286u, 0u, 3u, 15u, 0u}, {313u, 169u, 1u, 1u, 15u, 0u}, {313u, 427u, 2u, 0u, 12u, 0u},
    {337u, 427u, 2u, 0u, 12u, 0u},
    {353u, 286u, 0u, 3u, 15u, 0u}, {353u, 190u, 1u, 1u, 15u, 0u},
    {361u, 340u, 0u, 3u, 15u, 0u}, {361u, 213u, 1u, 1u, 15u, 0u}, {361u, 508u, 2u, 0u, 12u, 0u},
    {385u, 286u, 0u, 3u, 15u, 0u}, {385u, 226u, 1u, 1u, 15u, 0u}, {385u, 571u, 2u, 0u, 12u, 0u},
    {409u, 571u, 2u, 0u, 12u, 0u},
    {433u, 321u, 0u, 3u, 15u, 0u}, {433u, 254u, 1u, 1u, 15u, 0u}, {433u, 320u, 2u, 0u, 12u, 0u},
    {449u, 286u, 0u, 3u, 15u, 0u}, {449u, 226u, 1u, 1u, 15u, 0u},
    {457u, 340u, 0u, 3u, 15u, 0u}, {457u, 213u, 1u, 1u, 15u, 0u}, {457u, 427u, 2u, 0u, 12u, 0u},
    {481u, 286u, 0u, 3u, 15u, 0u}, {481u, 213u, 1u, 1u, 15u, 0u}, {481u, 339u, 2u, 0u, 12u, 0u},
    {505u, 340u, 0u, 3u, 15u, 0u}, {505u, 285u, 1u, 1u, 15u, 0u}, {505u, 427u, 2u, 0u, 12u, 0u},
    {529u, 428u, 0u, 3u, 15u, 0u}, {529u, 339u, 1u, 1u, 15u, 0u}, {529u, 571u, 2u, 0u, 12u, 0u},
    {553u, 572u, 0u, 3u, 15u, 0u}, {553u, 427u, 1u, 1u, 15u, 0u}, {553u, 855u, 2u, 0u, 12u, 0u},
    {577u, 428u, 0u, 3u, 15u, 0u}, {577u, 339u, 1u, 1u, 15u, 0u},
    {593u, 428u, 0u, 3u, 15u, 0u}, {593u, 339u, 1u, 1u, 15u, 0u},
    {601u, 428u, 0u, 3u, 15u, 0u}, {601u, 339u, 1u, 1u, 15u, 0u}, {601u, 855u, 2u, 0u, 12u, 0u},
    {625u, 382u, 0u, 3u, 15u, 0u}, {625u, 320u, 1u, 1u, 15u, 0u},
    {649u, 340u, 0u, 3u, 15u, 0u}, {649u, 285u, 1u, 1u, 15u, 0u},
    {673u, 340u, 0u, 3u, 15u, 0u}, {673u, 285u, 1u, 1u, 15u, 0u}, {673u, 855u, 2u, 0u, 12u, 0u},
    {721u, 382u, 0u, 3u, 15u, 0u}, {721u, 320u, 1u, 1u, 15u, 0u},
    {737u, 428u, 0u, 3u, 15u, 0u}, {737u, 339u, 1u, 1u, 15u, 0u},
    {745u, 454u, 0u, 3u, 15u, 0u}, {745u, 381u, 1u, 1u, 15u, 0u}, {745u, 571u, 2u, 0u, 12u, 0u},
    {769u, 428u, 0u, 3u, 15u, 0u}, {769u, 339u, 1u, 1u, 15u, 0u},
    {793u, 382u, 0u, 3u, 15u, 0u}, {793u, 320u, 1u, 1u, 15u, 0u},
    {817u, 382u, 0u, 3u, 15u, 0u}, {817u, 320u, 1u, 1u, 15u, 0u}, {817u, 571u, 2u, 0u, 12u, 0u},
    {865u, 382u, 0u, 3u, 15u, 0u}, {865u, 320u, 1u, 1u, 15u, 0u}, {865u, 571u, 2u, 0u, 12u, 0u},
    {881u, 382u, 0u, 3u, 15u, 0u}, {881u, 320u, 1u, 1u, 15u, 0u},
    {889u, 428u, 0u, 3u, 15u, 0u}, {889u, 339u, 1u, 1u, 15u, 0u}, {889u, 427u, 2u, 0u, 12u, 0u},
    {929u, 454u, 0u, 3u, 15u, 0u}, {929u, 381u, 1u, 1u, 15u, 0u}, {929u, 381u, 2u, 0u, 12u, 0u},
    {937u, 509u, 0u, 3u, 15u, 0u}, {937u, 427u, 1u, 1u, 15u, 0u}, {937u, 339u, 2u, 0u, 12u, 0u},
    {961u, 572u, 0u, 3u, 15u, 0u}, {961u, 453u, 1u, 1u, 15u, 0u}, {961u, 285u, 2u, 0u, 12u, 0u},
    {1009u, 321u, 0u, 3u, 15u, 0u}, {1009u, 508u, 1u, 1u, 15u, 0u}, {1009u, 320u, 2u, 0u, 12u, 0u},
    {1025u, 453u, 1u, 1u, 15u, 0u},
    {1033u, 340u, 0u, 3u, 15u, 0u}, {1033u, 427u, 1u, 1u, 15u, 0u}, {1033u, 339u, 2u, 0u, 12u, 0u},
    {1057u, 428u, 0u, 3u, 15u, 0u}, {1057u, 679u, 1u, 1u, 15u, 0u}, {1057u, 254u, 2u, 0u, 12u, 0u},
    {1081u, 382u, 0u, 3u, 15u, 0u}, {1081u, 604u, 1u, 1u, 15u, 0u}, {1081u, 381u, 2u, 0u, 12u, 0u},
    {1105u, 454u, 0u, 3u, 15u, 0u}, {1105u, 571u, 1u, 1u, 15u, 0u}, {1105u, 571u, 2u, 0u, 12u, 0u},
    {1153u, 340u, 0u, 3u, 15u, 0u}, {1153u, 571u, 1u, 1u, 15u, 0u},
    {1169u, 382u, 0u, 3u, 15u, 0u}, {1169u, 571u, 1u, 1u, 15u, 0u},
    {1177u, 340u, 0u, 3u, 15u, 0u}, {1177u, 427u, 1u, 1u, 15u, 0u}, {1177u, 339u, 2u, 0u, 12u, 0u},
    {1201u, 321u, 0u, 3u, 15u, 0u}, {1201u, 427u, 1u, 1u, 15u, 0u}, {1201u, 320u, 2u, 0u, 12u, 0u},
    {1225u, 286u, 0u, 3u, 15u, 0u}, {1225u, 427u, 1u, 1u, 15u, 0u}, {1225u, 285u, 2u, 0u, 12u, 0u},
    {1241u, 453u, 1u, 1u, 15u, 0u},
    {1249u, 255u, 0u, 3u, 15u, 0u}, {1249u, 508u, 1u, 1u, 15u, 0u}, {1249u, 320u, 2u, 0u, 12u, 0u},
    {1273u, 255u, 0u, 3u, 15u, 0u}, {1273u, 508u, 1u, 1u, 15u, 0u}, {1273u, 320u, 2u, 0u, 12u, 0u},
    {1285u, 285u, 2u, 0u, 12u, 0u},
    {1297u, 255u, 0u, 3u, 15u, 0u}, {1297u, 508u, 1u, 1u, 15u, 0u}, {1297u, 320u, 2u, 0u, 12u, 0u},
    {1309u, 320u, 2u, 0u, 12u, 0u},
    {1321u, 321u, 0u, 3u, 15u, 0u}, {1321u, 381u, 1u, 1u, 15u, 0u}, {1321u, 381u, 2u, 0u, 12u, 0u},
    {1333u, 339u, 1u, 1u, 15u, 0u},
    {1345u, 255u, 0u, 3u, 15u, 0u}, {1345u, 320u, 1u, 1u, 15u, 0u}, {1345u, 381u, 2u, 0u, 12u, 0u},
    {1357u, 255u, 0u, 3u, 15u, 0u}, {1357u, 339u, 1u, 1u, 15u, 0u}, {1357u, 339u, 2u, 0u, 12u, 0u},
    {1369u, 255u, 0u, 3u, 15u, 0u}, {1369u, 381u, 1u, 1u, 15u, 0u}, {1369u, 320u, 2u, 0u, 12u, 0u},
    {1381u, 255u, 0u, 3u, 15u, 0u}, {1381u, 427u, 1u, 1u, 15u, 0u}, {1381u, 302u, 2u, 0u, 12u, 0u},
    {1393u, 286u, 0u, 3u, 15u, 0u}, {1393u, 427u, 1u, 1u, 15u, 0u}, {1393u, 285u, 2u, 0u, 12u, 0u},
    {1417u, 286u, 0u, 3u, 15u, 0u}, {1417u, 453u, 1u, 1u, 15u, 0u}, {1417u, 285u, 2u, 0u, 12u, 0u},
    {1487u, 286u, 0u, 3u, 15u, 0u}, {1487u, 571u, 1u, 1u, 15u, 0u}, {1487u, 285u, 2u, 0u, 12u, 0u},
    {1501u, 321u, 0u, 3u, 15u, 0u}, {1501u, 571u, 1u, 1u, 15u, 0u}, {1501u, 320u, 2u, 0u, 12u, 0u},
    {1515u, 340u, 0u, 3u, 15u, 0u}, {1515u, 427u, 1u, 1u, 15u, 0u}, {1515u, 339u, 2u, 0u, 12u, 0u},
    {1557u, 286u, 0u, 3u, 15u, 0u}, {1557u, 381u, 1u, 1u, 15u, 0u}, {1557u, 453u, 2u, 0u, 12u, 0u},
    {1571u, 214u, 0u, 3u, 15u, 0u}, {1571u, 339u, 1u, 1u, 15u, 0u}, {1571u, 427u, 2u, 0u, 12u, 0u},
    {1585u, 191u, 0u, 3u, 15u, 0u}, {1585u, 320u, 1u, 1u, 15u, 0u}, {1585u, 381u, 2u, 0u, 12u, 0u},
    {1599u, 170u, 0u, 3u, 15u, 0u}, {1599u, 285u, 1u, 1u, 15u, 0u}, {1599u, 339u, 2u, 0u, 12u, 0u},
    {1669u, 161u, 0u, 3u, 15u, 0u}, {1669u, 427u, 1u, 1u, 15u, 0u}, {1669u, 254u, 2u, 0u, 12u, 0u},
    {1683u, 191u, 0u, 3u, 15u, 0u}, {1683u, 381u, 1u, 1u, 15u, 0u}, {1683u, 320u, 2u, 0u, 12u, 0u},
    {1697u, 170u, 0u, 3u, 15u, 0u}, {1697u, 339u, 1u, 1u, 15u, 0u}, {1697u, 285u, 2u, 0u, 12u, 0u},
    {1725u, 161u, 0u, 3u, 15u, 0u}, {1725u, 320u, 1u, 1u, 15u, 0u}, {1725u, 285u, 2u, 0u, 12u, 0u},
    {1753u, 227u, 0u, 3u, 15u, 0u}, {1753u, 381u, 1u, 1u, 15u, 0u}, {1753u, 453u, 2u, 0u, 12u, 0u},
    {1781u, 214u, 0u, 3u, 15u, 0u}, {1781u, 427u, 1u, 1u, 15u, 0u}, {1781u, 427u, 2u, 0u, 12u, 0u}
};

/* Frames 76-138 of the bank-1 title-confirm cue, normalized to frame zero. */
static const DDMusicNote DD_SELECT_MUSIC[] = {
    {0u, 339u, 0u, 3u, 15u, 0u}, {0u, 285u, 1u, 3u, 15u, 0u}, {0u, 285u, 2u, 0u, 12u, 0u},
    {6u, 285u, 0u, 3u, 15u, 0u}, {6u, 213u, 1u, 3u, 15u, 0u}, {6u, 213u, 2u, 0u, 12u, 0u},
    {12u, 213u, 0u, 3u, 15u, 0u}, {12u, 169u, 1u, 3u, 15u, 0u}, {12u, 169u, 2u, 0u, 12u, 0u},
    {18u, 169u, 0u, 3u, 15u, 0u}, {18u, 142u, 1u, 3u, 15u, 0u}, {18u, 142u, 2u, 0u, 12u, 0u},
    {30u, 213u, 0u, 3u, 15u, 0u}, {30u, 169u, 1u, 3u, 15u, 0u}, {30u, 169u, 2u, 0u, 12u, 0u},
    {36u, 169u, 0u, 3u, 15u, 0u}, {36u, 142u, 1u, 3u, 15u, 0u}, {36u, 142u, 2u, 0u, 12u, 0u},
    {62u, 0u, 0u, 0u, 0u, 0u}, {62u, 0u, 1u, 0u, 0u, 0u}, {62u, 0u, 2u, 0u, 0u, 0u}
};

/* Bank-1 configuration song selected at original frame 2092. The native score
   begins on frame 2093 and covers the driver's complete 896-frame loop. */
static const DDMusicNote DD_CONFIG_MUSIC[] = {
    {0u,359u,0u,3u,6u,0u},{0u,285u,1u,2u,9u,0u},{0u,719u,2u,0u,15u,0u},{0u,9u,3u,0u,14u,0u},
    {14u,359u,2u,0u,15u,0u},{14u,2u,3u,0u,12u,0u},{28u,359u,2u,0u,15u,0u},{28u,2u,3u,0u,12u,0u},
    {42u,480u,0u,3u,6u,0u},{42u,359u,1u,2u,9u,0u},{42u,719u,2u,0u,15u,0u},{42u,9u,3u,0u,14u,0u},
    {56u,359u,2u,0u,15u,0u},{56u,2u,3u,0u,12u,0u},{70u,359u,2u,0u,15u,0u},{70u,2u,3u,0u,12u,0u},
    {84u,285u,0u,3u,6u,0u},{84u,240u,1u,2u,9u,0u},{84u,719u,2u,0u,15u,0u},{84u,9u,3u,0u,14u,0u},
    {98u,359u,2u,0u,15u,0u},{98u,2u,3u,0u,12u,0u},
    {112u,320u,0u,3u,6u,0u},{112u,269u,1u,2u,9u,0u},{112u,719u,2u,0u,15u,0u},{112u,9u,3u,0u,14u,0u},
    {126u,359u,2u,0u,15u,0u},{126u,2u,3u,0u,12u,0u},{140u,359u,2u,0u,15u,0u},{140u,2u,3u,0u,12u,0u},
    {154u,403u,0u,3u,6u,0u},{154u,320u,1u,2u,9u,0u},{154u,719u,2u,0u,15u,0u},{154u,9u,3u,0u,14u,0u},
    {168u,359u,2u,0u,15u,0u},{168u,2u,3u,0u,12u,0u},{182u,359u,2u,0u,15u,0u},{182u,2u,3u,0u,12u,0u},
    {196u,269u,0u,3u,6u,0u},{196u,201u,1u,2u,9u,0u},{196u,719u,2u,0u,15u,0u},{196u,9u,3u,0u,14u,0u},
    {210u,320u,0u,3u,6u,0u},{210u,269u,1u,2u,9u,0u},{210u,359u,2u,0u,15u,0u},{210u,2u,3u,0u,12u,0u},
    {224u,359u,0u,3u,6u,0u},{224u,285u,1u,2u,9u,0u},{224u,719u,2u,0u,15u,0u},{224u,9u,3u,0u,14u,0u},
    {238u,320u,0u,3u,6u,0u},{238u,269u,1u,2u,9u,0u},{238u,359u,2u,0u,15u,0u},{238u,2u,3u,0u,12u,0u},
    {245u,285u,0u,3u,6u,0u},{245u,240u,1u,2u,9u,0u},{252u,359u,2u,0u,15u,0u},{252u,2u,3u,0u,12u,0u},
    {266u,719u,2u,0u,15u,0u},{266u,9u,3u,0u,14u,0u},{280u,359u,2u,0u,15u,0u},{280u,2u,3u,0u,12u,0u},
    {294u,359u,2u,0u,15u,0u},{294u,2u,3u,0u,12u,0u},{308u,719u,2u,0u,15u,0u},{308u,9u,3u,0u,14u,0u},
    {322u,359u,2u,0u,15u,0u},{322u,2u,3u,0u,12u,0u},
    {336u,640u,1u,2u,9u,0u},{336u,719u,2u,0u,15u,0u},{336u,9u,3u,0u,14u,0u},
    {345u,539u,1u,2u,9u,0u},{350u,359u,2u,0u,15u,0u},{350u,2u,3u,0u,12u,0u},{355u,403u,1u,2u,9u,0u},
    {364u,320u,1u,2u,9u,0u},{364u,359u,2u,0u,15u,0u},{364u,2u,3u,0u,12u,0u},{373u,269u,1u,2u,9u,0u},
    {378u,480u,2u,0u,15u,0u},{378u,9u,3u,0u,14u,0u},{383u,201u,1u,2u,9u,0u},
    {392u,201u,0u,3u,6u,0u},{392u,160u,1u,2u,9u,0u},{392u,240u,2u,0u,15u,0u},{392u,2u,3u,0u,12u,0u},
    {399u,269u,0u,3u,6u,0u},{399u,201u,1u,2u,9u,0u},
    {406u,320u,0u,3u,6u,0u},{406u,269u,1u,2u,9u,0u},{406u,480u,2u,0u,15u,0u},{406u,2u,3u,0u,12u,0u},
    {413u,403u,0u,3u,6u,0u},{413u,320u,1u,2u,9u,0u},
    {420u,539u,0u,3u,6u,0u},{420u,403u,1u,2u,9u,0u},{420u,320u,2u,0u,15u,0u},{420u,9u,3u,0u,14u,0u},
    {427u,320u,0u,3u,6u,0u},{427u,269u,1u,2u,9u,0u},
    {434u,269u,0u,3u,6u,0u},{434u,201u,1u,2u,9u,0u},{434u,480u,2u,0u,15u,0u},{434u,2u,3u,0u,12u,0u},
    {441u,201u,0u,3u,6u,0u},{441u,160u,1u,2u,9u,0u},
    {448u,240u,0u,3u,6u,0u},{448u,142u,1u,2u,9u,0u},{448u,719u,2u,0u,15u,0u},{448u,9u,3u,0u,14u,0u},
    {462u,359u,2u,0u,15u,0u},{462u,2u,3u,0u,12u,0u},{476u,359u,2u,0u,15u,0u},{476u,2u,3u,0u,12u,0u},
    {490u,285u,0u,3u,6u,0u},{490u,179u,1u,2u,9u,0u},{490u,719u,2u,0u,15u,0u},{490u,9u,3u,0u,14u,0u},
    {504u,359u,2u,0u,15u,0u},{504u,2u,3u,0u,12u,0u},{518u,359u,2u,0u,15u,0u},{518u,2u,3u,0u,12u,0u},
    {532u,179u,0u,3u,6u,0u},{532u,120u,1u,2u,9u,0u},{532u,719u,2u,0u,15u,0u},{532u,9u,3u,0u,14u,0u},
    {546u,359u,2u,0u,15u,0u},{546u,2u,3u,0u,12u,0u},
    {560u,201u,0u,3u,6u,0u},{560u,134u,1u,2u,9u,0u},{560u,719u,2u,0u,15u,0u},{560u,9u,3u,0u,14u,0u},
    {574u,359u,2u,0u,15u,0u},{574u,2u,3u,0u,12u,0u},{588u,359u,2u,0u,15u,0u},{588u,2u,3u,0u,12u,0u},
    {602u,269u,0u,3u,6u,0u},{602u,160u,1u,2u,9u,0u},{602u,719u,2u,0u,15u,0u},{602u,9u,3u,0u,14u,0u},
    {616u,359u,2u,0u,15u,0u},{616u,2u,3u,0u,12u,0u},{630u,359u,2u,0u,15u,0u},{630u,2u,3u,0u,12u,0u},
    {644u,320u,0u,3u,6u,0u},{644u,201u,1u,2u,9u,0u},{644u,719u,2u,0u,15u,0u},{644u,9u,3u,0u,14u,0u},
    {658u,359u,2u,0u,15u,0u},{658u,2u,3u,0u,12u,0u},
    {672u,269u,0u,3u,6u,0u},{672u,179u,1u,2u,9u,0u},{672u,719u,2u,0u,15u,0u},{672u,9u,3u,0u,14u,0u},
    {680u,179u,1u,2u,9u,0u},{686u,359u,2u,0u,15u,0u},{686u,2u,3u,0u,12u,0u},
    {688u,179u,1u,2u,9u,0u},{696u,179u,1u,2u,9u,0u},{700u,359u,2u,0u,15u,0u},{700u,2u,3u,0u,12u,0u},
    {704u,179u,1u,2u,9u,0u},{712u,179u,1u,2u,9u,0u},{714u,719u,2u,0u,15u,0u},{714u,9u,3u,0u,14u,0u},
    {720u,179u,1u,2u,9u,0u},{728u,179u,1u,2u,9u,0u},{728u,359u,2u,0u,15u,0u},{728u,2u,3u,0u,12u,0u},
    {736u,179u,1u,2u,9u,0u},{742u,359u,2u,0u,15u,0u},{742u,2u,3u,0u,12u,0u},
    {744u,179u,1u,2u,9u,0u},{752u,179u,1u,2u,9u,0u},{756u,719u,2u,0u,15u,0u},{756u,9u,3u,0u,14u,0u},
    {760u,179u,1u,2u,9u,0u},{768u,179u,1u,2u,9u,0u},
    {770u,285u,0u,3u,6u,0u},{770u,359u,2u,0u,15u,0u},{770u,2u,3u,0u,12u,0u},
    {776u,179u,1u,2u,9u,0u},{777u,269u,0u,3u,6u,0u},
    {784u,285u,0u,3u,6u,0u},{784u,179u,1u,2u,9u,0u},{784u,719u,2u,0u,15u,0u},{784u,9u,3u,0u,14u,0u},
    {792u,179u,1u,2u,9u,0u},{798u,359u,2u,0u,15u,0u},{798u,2u,3u,0u,12u,0u},
    {800u,179u,1u,2u,9u,0u},{808u,179u,1u,2u,9u,0u},{812u,359u,2u,0u,15u,0u},{812u,2u,3u,0u,12u,0u},
    {816u,179u,1u,2u,9u,0u},{824u,179u,1u,2u,9u,0u},{826u,719u,2u,0u,15u,0u},{826u,9u,3u,0u,14u,0u},
    {832u,179u,1u,2u,9u,0u},{840u,179u,1u,2u,9u,0u},{840u,285u,2u,0u,15u,0u},{840u,2u,3u,0u,12u,0u},
    {848u,179u,1u,2u,9u,0u},{854u,320u,2u,0u,15u,0u},{854u,2u,3u,0u,12u,0u},
    {856u,179u,1u,2u,9u,0u},{864u,179u,1u,2u,9u,0u},
    {868u,269u,0u,3u,6u,0u},{868u,359u,2u,0u,15u,0u},{868u,9u,3u,0u,14u,0u},
    {882u,320u,0u,3u,6u,0u},{882u,480u,2u,0u,15u,0u},{882u,2u,3u,0u,12u,0u}
};

/* Bank-1 END acceptance tone observed at original frames 2185-2221. */
static const DDMusicNote DD_END_MUSIC[] = {
    {0u, 384u, 0u, 2u, 12u, 0u}, {3u, 128u, 0u, 2u, 10u, 0u},
    {6u, 256u, 0u, 2u, 12u, 0u}, {9u, 384u, 0u, 2u, 8u, 0u},
    {12u, 128u, 0u, 2u, 6u, 0u}, {15u, 256u, 0u, 2u, 8u, 0u},
    {18u, 384u, 0u, 2u, 4u, 0u}, {21u, 128u, 0u, 2u, 6u, 0u},
    {24u, 256u, 0u, 2u, 5u, 0u}, {36u, 0u, 0u, 0u, 0u, 0u}
};

/* Bank-1 streams $8653/$8664/$866B, observed through APU writes at original
   frames 2566-2583. The driver at fixed $CD24 requeues this during dribbling. */
static const DDMusicNote DD_GAMEPLAY_AUDIO[] = {
    {0u, 1040u, 0u, 2u, 14u, 0u}, {0u, 226u, 2u, 0u, 15u, 0u}, {0u, 10u, 3u, 0u, 8u, 0u},
    {1u, 1300u, 0u, 2u, 13u, 0u}, {1u, 240u, 2u, 0u, 15u, 0u}, {1u, 0u, 3u, 0u, 0u, 0u},
    {2u, 0u, 0u, 0u, 0u, 0u}, {2u, 254u, 2u, 0u, 15u, 0u},
    {3u, 576u, 0u, 1u, 6u, 0u}, {4u, 729u, 0u, 1u, 6u, 0u},
    {5u, 922u, 0u, 1u, 6u, 0u}, {6u, 1037u, 0u, 1u, 6u, 0u},
    {7u, 1311u, 0u, 1u, 6u, 0u}, {7u, 0u, 2u, 0u, 0u, 0u},
    {8u, 432u, 0u, 3u, 3u, 0u}, {9u, 546u, 0u, 3u, 3u, 0u},
    {10u, 690u, 0u, 3u, 3u, 0u}, {11u, 776u, 0u, 3u, 3u, 0u},
    {12u, 982u, 0u, 3u, 3u, 0u}, {13u, 0u, 0u, 0u, 0u, 0u}
};

/* Request `$10` is the CPU block/award cue. Controlled original frames
   2644-2648 resolve it to bank-1 streams `$87A4/$87AD`; audio begins one
   frame after `$C141` and alternates two pulse/noise impacts. */
static const DDMusicNote DD_CPU_BLOCK_AUDIO[] = {
    {0u, 384u, 0u, 2u, 7u, 0u}, {0u, 13u, 3u, 0u, 4u, 0u},
    {1u, 0u, 0u, 0u, 0u, 0u}, {1u, 0u, 3u, 0u, 0u, 0u},
    {2u, 336u, 0u, 2u, 8u, 0u}, {2u, 8u, 3u, 0u, 9u, 0u},
    {3u, 0u, 0u, 0u, 0u, 0u}, {3u, 0u, 3u, 0u, 0u, 0u}
};

/* Request `$20` is the successful user `$A638->$A6C3` block cue. The
   controlled original frame-2766 contact resolves pulse stream `$87DD` and
   noise stream `$866B`; frames 2767-2779 provide the exact timer/envelope. */
static const DDMusicNote DD_USER_BLOCK_AUDIO[] = {
    {0u,416u,0u,2u,14u,0u}, {0u,10u,3u,0u,8u,0u},
    {1u,356u,0u,2u,13u,0u}, {1u,0u,3u,0u,0u,0u},
    {2u,402u,0u,2u,12u,0u}, {3u,356u,0u,2u,11u,0u},
    {4u,402u,0u,2u,10u,0u}, {5u,368u,0u,2u,7u,0u},
    {6u,368u,0u,2u,6u,0u}, {7u,368u,0u,2u,5u,0u},
    {8u,368u,0u,2u,4u,0u}, {9u,368u,0u,2u,3u,0u},
    {10u,368u,0u,2u,2u,0u}, {11u,368u,0u,2u,1u,0u},
    {12u,0u,0u,0u,0u,0u}
};

/* SFX request $2C resolves through fixed $C141/$CC99 into bank-1 streams
   $86F7/$8702/$870D.  Controlled APU frames 2601-2612 preserve the driver's
   two-frame request latency, eight alternating pulse frames, noise bed, and
   explicit channel stops. */
static const DDMusicNote DD_WHISTLE_AUDIO[] = {
    {0u, 0u, 0u, 0u, 0u, 0u}, {0u, 0u, 1u, 0u, 0u, 0u}, {0u, 0u, 3u, 0u, 0u, 0u},
    {2u, 37u, 0u, 1u, 15u, 0u}, {2u, 52u, 1u, 1u, 15u, 0u}, {2u, 6u, 3u, 0u, 3u, 0u},
    {3u, 43u, 0u, 1u, 15u, 0u}, {3u, 40u, 1u, 1u, 15u, 0u},
    {4u, 37u, 0u, 1u, 15u, 0u}, {4u, 52u, 1u, 1u, 15u, 0u},
    {5u, 43u, 0u, 1u, 15u, 0u}, {5u, 40u, 1u, 1u, 15u, 0u},
    {6u, 37u, 0u, 1u, 15u, 0u}, {6u, 52u, 1u, 1u, 15u, 0u},
    {7u, 43u, 0u, 1u, 15u, 0u}, {7u, 40u, 1u, 1u, 15u, 0u},
    {8u, 37u, 0u, 1u, 15u, 0u}, {8u, 52u, 1u, 1u, 15u, 0u},
    {9u, 43u, 0u, 1u, 15u, 0u}, {9u, 40u, 1u, 1u, 15u, 0u},
    {9u, 0u, 3u, 0u, 0u, 0u},
    {10u, 0u, 0u, 0u, 0u, 0u}, {10u, 0u, 1u, 0u, 0u, 0u}
};

/* SFX request $09 from $A82A resolves through fixed $C141/$CC99.  The
   controlled outside-line trace at original frames 2602-2790 is a pulse-2
   triangle sweep in timer space: 256 down to 162, back to 255, then stop.
   reserved 1/2 are native linear timer sweeps, not APU emulation. */
static const DDMusicNote DD_THREE_CALL_AUDIO[] = {
    {0u, 256u, 1u, 2u, 6u, 1u},
    {95u, 163u, 1u, 2u, 6u, 2u},
    {188u, 0u, 1u, 0u, 0u, 0u}
};

/* SFX request $25 from $AEEA, normalized from controlled original frames
   2607-2648.  A matched two-point run proves this cue owns only the two pulse
   voices; the simultaneous triangle/noise writes belong to other score flow. */
static const DDMusicNote DD_THREE_SCORE_AUDIO[] = {
    {0u, 304u, 0u, 0u, 0u, 0u}, {0u, 43u, 1u, 0u, 0u, 0u},
    {1u, 592u, 1u, 2u, 15u, 0u},
    {2u, 592u, 0u, 2u, 15u, 0u}, {2u, 740u, 1u, 2u, 15u, 0u},
    {3u, 925u, 0u, 2u, 15u, 0u}, {3u, 1156u, 1u, 2u, 15u, 0u},
    {4u, 1156u, 0u, 2u, 15u, 0u}, {4u, 0u, 1u, 0u, 0u, 0u},
    {5u, 0u, 0u, 0u, 0u, 0u},
    {7u, 520u, 1u, 2u, 14u, 0u},
    {8u, 520u, 0u, 2u, 14u, 0u}, {8u, 812u, 1u, 2u, 14u, 0u},
    {9u, 650u, 0u, 2u, 14u, 0u}, {9u, 1015u, 1u, 2u, 14u, 0u},
    {10u, 1015u, 0u, 2u, 14u, 0u}, {10u, 460u, 1u, 2u, 13u, 0u},
    {11u, 368u, 0u, 2u, 13u, 0u}, {11u, 575u, 1u, 2u, 13u, 0u},
    {12u, 575u, 0u, 2u, 13u, 0u}, {12u, 897u, 1u, 2u, 13u, 0u},
    {13u, 897u, 0u, 2u, 13u, 0u}, {13u, 520u, 1u, 2u, 12u, 0u},
    {14u, 520u, 0u, 2u, 12u, 0u}, {14u, 650u, 1u, 2u, 12u, 0u},
    {15u, 812u, 0u, 2u, 12u, 0u}, {15u, 1015u, 1u, 2u, 12u, 0u},
    {16u, 1015u, 0u, 2u, 12u, 0u}, {16u, 368u, 1u, 2u, 11u, 0u},
    {17u, 460u, 0u, 2u, 11u, 0u}, {17u, 575u, 1u, 2u, 11u, 0u},
    {18u, 718u, 0u, 2u, 11u, 0u}, {18u, 897u, 1u, 2u, 11u, 0u},
    {19u, 897u, 0u, 2u, 11u, 0u}, {19u, 520u, 1u, 2u, 10u, 0u},
    {20u, 520u, 0u, 2u, 10u, 0u}, {20u, 812u, 1u, 2u, 10u, 0u},
    {21u, 650u, 0u, 2u, 10u, 0u}, {21u, 1015u, 1u, 2u, 10u, 0u},
    {22u, 1015u, 0u, 2u, 10u, 0u}, {22u, 460u, 1u, 2u, 9u, 0u},
    {23u, 460u, 0u, 2u, 9u, 0u}, {23u, 718u, 1u, 2u, 9u, 0u},
    {24u, 575u, 0u, 2u, 9u, 0u}, {24u, 897u, 1u, 2u, 9u, 0u},
    {25u, 897u, 0u, 2u, 9u, 0u}, {25u, 520u, 1u, 2u, 8u, 0u},
    {26u, 416u, 0u, 2u, 8u, 0u}, {26u, 650u, 1u, 2u, 8u, 0u},
    {27u, 650u, 0u, 2u, 8u, 0u}, {27u, 1015u, 1u, 2u, 8u, 0u},
    {28u, 1015u, 0u, 2u, 8u, 0u}, {28u, 460u, 1u, 2u, 7u, 0u},
    {29u, 460u, 0u, 2u, 7u, 0u}, {29u, 575u, 1u, 2u, 7u, 0u},
    {30u, 718u, 0u, 2u, 7u, 0u}, {30u, 897u, 1u, 2u, 7u, 0u},
    {31u, 897u, 0u, 2u, 7u, 0u}, {31u, 416u, 1u, 2u, 6u, 0u},
    {32u, 520u, 0u, 2u, 6u, 0u}, {32u, 650u, 1u, 2u, 6u, 0u},
    {33u, 812u, 0u, 2u, 6u, 0u}, {33u, 1015u, 1u, 2u, 6u, 0u},
    {34u, 1015u, 0u, 2u, 6u, 0u}, {34u, 460u, 1u, 2u, 5u, 0u},
    {35u, 460u, 0u, 2u, 5u, 0u}, {35u, 718u, 1u, 2u, 5u, 0u},
    {36u, 575u, 0u, 2u, 5u, 0u}, {36u, 897u, 1u, 2u, 5u, 0u},
    {37u, 897u, 0u, 2u, 5u, 0u}, {37u, 520u, 1u, 2u, 4u, 0u},
    {38u, 520u, 0u, 2u, 4u, 0u}, {38u, 812u, 1u, 2u, 4u, 0u},
    {39u, 650u, 0u, 2u, 4u, 0u}, {39u, 1015u, 1u, 2u, 4u, 0u},
    {40u, 1015u, 0u, 2u, 4u, 0u}, {40u, 0u, 1u, 0u, 0u, 0u},
    {41u, 0u, 0u, 0u, 0u, 0u}
};

/* A clean make requests $18 at fixed $AE8E, then score-state underflow calls
   $1F/$22 at $AF2F/$AF34 fifteen frames later.  This normalized composite is
   taken from an isolated original run: pulse/noise make impact from $87B6/
   $87CA, followed by the complete triangle/noise post-score streams rooted at
   $886D/$8922.  No APU interpreter or captured waveform is stored. */
static const DDMusicNote DD_SCORE_AUDIO[] = {
    {0u, 144u, 0u, 2u, 5u, 0u}, {0u, 4u, 3u, 0u, 10u, 0u},
    {1u, 180u, 0u, 2u, 5u, 0u}, {1u, 0u, 3u, 0u, 0u, 0u},
    {2u, 0u, 0u, 0u, 0u, 0u}, {2u, 8u, 3u, 0u, 8u, 0u},
    {3u, 304u, 0u, 2u, 3u, 0u}, {3u, 0u, 3u, 0u, 0u, 0u},
    {4u, 151u, 0u, 2u, 3u, 0u}, {4u, 8u, 3u, 0u, 8u, 0u},
    {5u, 151u, 0u, 2u, 2u, 0u}, {5u, 0u, 3u, 0u, 0u, 0u},
    {6u, 0u, 0u, 0u, 0u, 0u}, {6u, 3u, 3u, 0u, 6u, 0u},
    {7u, 8u, 3u, 0u, 4u, 0u}, {8u, 0u, 3u, 0u, 0u, 0u},
    {15u, 213u, 2u, 0u, 15u, 0u}, {15u, 13u, 3u, 0u, 5u, 0u},
    {20u, 13u, 3u, 0u, 6u, 0u}, {24u, 0u, 2u, 0u, 0u, 0u},
    {25u, 12u, 3u, 0u, 7u, 0u}, {30u, 13u, 3u, 0u, 8u, 0u},
    {35u, 13u, 3u, 0u, 9u, 0u}, {36u, 285u, 2u, 0u, 15u, 0u},
    {45u, 0u, 2u, 0u, 0u, 0u}, {45u, 12u, 3u, 0u, 8u, 0u},
    {50u, 13u, 3u, 0u, 9u, 0u}, {57u, 254u, 2u, 0u, 15u, 0u},
    {60u, 12u, 3u, 0u, 8u, 0u}, {65u, 13u, 3u, 0u, 9u, 0u},
    {66u, 0u, 2u, 0u, 0u, 0u}, {70u, 12u, 3u, 0u, 9u, 0u},
    {75u, 13u, 3u, 0u, 8u, 0u}, {78u, 226u, 2u, 0u, 15u, 0u},
    {80u, 12u, 3u, 0u, 8u, 0u}, {85u, 13u, 3u, 0u, 7u, 0u},
    {87u, 0u, 2u, 0u, 0u, 0u}, {90u, 12u, 3u, 0u, 8u, 0u},
    {95u, 13u, 3u, 0u, 8u, 0u}, {99u, 213u, 2u, 0u, 15u, 0u},
    {100u, 12u, 3u, 0u, 7u, 0u}, {105u, 13u, 3u, 0u, 8u, 0u},
    {108u, 0u, 2u, 0u, 0u, 0u}, {110u, 12u, 3u, 0u, 8u, 0u},
    {115u, 13u, 3u, 0u, 7u, 0u}, {120u, 285u, 2u, 0u, 15u, 0u},
    {120u, 12u, 3u, 0u, 8u, 0u}, {125u, 13u, 3u, 0u, 8u, 0u},
    {129u, 0u, 2u, 0u, 0u, 0u}, {130u, 12u, 3u, 0u, 7u, 0u},
    {135u, 13u, 3u, 0u, 5u, 0u}, {140u, 12u, 3u, 0u, 8u, 0u},
    {141u, 254u, 2u, 0u, 15u, 0u}, {145u, 13u, 3u, 0u, 5u, 0u},
    {150u, 0u, 2u, 0u, 0u, 0u}, {150u, 12u, 3u, 0u, 6u, 0u},
    {155u, 13u, 3u, 0u, 5u, 0u}, {160u, 12u, 3u, 0u, 4u, 0u},
    {162u, 226u, 2u, 0u, 15u, 0u}, {165u, 13u, 3u, 0u, 5u, 0u},
    {170u, 12u, 3u, 0u, 4u, 0u}, {171u, 0u, 2u, 0u, 0u, 0u},
    {175u, 12u, 3u, 0u, 3u, 0u}, {180u, 12u, 3u, 0u, 4u, 0u},
    {183u, 201u, 2u, 0u, 15u, 0u}, {185u, 13u, 3u, 0u, 4u, 0u},
    {190u, 12u, 3u, 0u, 3u, 0u}, {192u, 0u, 2u, 0u, 0u, 0u},
    {195u, 13u, 3u, 0u, 4u, 0u}, {200u, 12u, 3u, 0u, 3u, 0u},
    {204u, 269u, 2u, 0u, 15u, 0u}, {205u, 13u, 3u, 0u, 2u, 0u},
    {210u, 12u, 3u, 0u, 3u, 0u}, {213u, 0u, 2u, 0u, 0u, 0u},
    {215u, 13u, 3u, 0u, 2u, 0u}, {220u, 12u, 3u, 0u, 1u, 0u},
    {225u, 240u, 2u, 0u, 15u, 0u}, {225u, 13u, 3u, 0u, 1u, 0u},
    {230u, 12u, 3u, 0u, 2u, 0u}, {234u, 0u, 2u, 0u, 0u, 0u},
    {235u, 13u, 3u, 0u, 1u, 0u}, {240u, 0u, 3u, 0u, 0u, 0u},
    {246u, 213u, 2u, 0u, 15u, 0u}, {255u, 0u, 2u, 0u, 0u, 0u},
    {267u, 201u, 2u, 0u, 15u, 0u}, {276u, 0u, 2u, 0u, 0u, 0u},
    {288u, 269u, 2u, 0u, 15u, 0u}, {297u, 0u, 2u, 0u, 0u, 0u},
    {309u, 240u, 2u, 0u, 15u, 0u}, {318u, 0u, 2u, 0u, 0u, 0u},
    {330u, 213u, 2u, 0u, 15u, 0u}, {339u, 0u, 2u, 0u, 0u, 0u},
    {351u, 285u, 2u, 0u, 15u, 0u}, {356u, 0u, 2u, 0u, 0u, 0u},
    {358u, 213u, 2u, 0u, 15u, 0u}, {363u, 0u, 2u, 0u, 0u, 0u},
    {365u, 169u, 2u, 0u, 15u, 0u}, {370u, 0u, 2u, 0u, 0u, 0u},
    {372u, 142u, 2u, 0u, 15u, 0u}, {377u, 0u, 2u, 0u, 0u, 0u},
    {387u, 169u, 2u, 0u, 15u, 0u}, {392u, 0u, 2u, 0u, 0u, 0u},
    {394u, 142u, 2u, 0u, 15u, 0u}, {436u, 0u, 2u, 0u, 0u, 0u}
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
    size_t position;
    const size_t bank_end = 16u + ((size_t)(bank + 1u) * 0x4000u);
    uint16_t ppu_address;
    size_t start;
    if (bank > 7u || (bank < 7u && (cpu_address < 0x8000u || cpu_address >= 0xC000u)) ||
        (bank == 7u && (cpu_address < 0xC000u || cpu_address > 0xFFFFu)) ||
        bank_end > rom_size) {
        return 0;
    }
    position = bank == 7u
        ? 16u + 7u * 0x4000u + (cpu_address - 0xC000u)
        : dd_bank_file_offset(bank, cpu_address);
    start = position;
    if (
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

static void dd_build_tipoff_oam(uint8_t oam[256]) {
    /* Result of the recovered bank-0 formation initializer and bank-2
       variable-record metasprite expansion for original frame 2359. */
    static const uint8_t visible[] = {
        0xB0,0x98,0x00,0x6F, 0xB0,0xFC,0x00,0x77, 0xC0,0x06,0x00,0x73,
        0xA0,0x10,0x02,0x69, 0xA0,0x12,0x02,0x71, 0xB0,0x14,0x02,0x69, 0xB0,0x16,0x02,0x71,
        0x74,0xA2,0x41,0x6F, 0x74,0xF6,0x41,0x77, 0x84,0x3A,0x41,0x6F, 0x84,0x38,0x41,0x77,
        0x60,0x22,0x43,0x59, 0x60,0x20,0x43,0x61, 0x70,0x26,0x43,0x59, 0x70,0x24,0x43,0x61,
        0x50,0x08,0x01,0x6F, 0x50,0x0A,0x01,0x77, 0x60,0x0C,0x01,0x73,
        0xB0,0x98,0x00,0x89, 0xB0,0xFC,0x00,0x91, 0xC0,0x06,0x00,0x8D,
        0xA0,0x12,0x43,0x99, 0xA0,0x10,0x43,0xA1, 0xB0,0x16,0x43,0x99, 0xB0,0x14,0x43,0xA1,
        0x76,0x02,0x40,0x7C, 0x74,0xF6,0x03,0x81, 0x74,0xA2,0x03,0x89,
        0x84,0x38,0x03,0x81, 0x84,0x3A,0x03,0x89, 0x8E,0x04,0x40,0x7C,
        0x60,0x20,0x02,0x99, 0x60,0x22,0x02,0xA1, 0x70,0x24,0x02,0x99, 0x70,0x26,0x02,0xA1,
        0x50,0x20,0x01,0x89, 0x50,0x22,0x01,0x91, 0x60,0x24,0x01,0x89, 0x60,0x26,0x01,0x91
    };
    uint32_t sprite;
    memset(oam, 0, 256u);
    for (sprite = 0u; sprite < 64u; ++sprite) oam[sprite * 4u] = 0xF4u;
    oam[0] = 0x38u; oam[1] = 0xFEu; oam[2] = 0x30u; oam[3] = 0x20u;
    memcpy(oam + 4u, visible, sizeof(visible));
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

static uint16_t dd_read_bank_u16(const uint8_t *rom, uint32_t bank, uint32_t cpu_address) {
    size_t offset = dd_bank_file_offset(bank, cpu_address);
    return (uint16_t)(rom[offset] | ((uint16_t)rom[offset + 1u] << 8));
}

static void dd_write_blob_u16(uint8_t *target, uint16_t value) {
    target[0] = (uint8_t)value;
    target[1] = (uint8_t)(value >> 8);
}

static void dd_write_blob_u32(uint8_t *target, uint32_t value) {
    target[0] = (uint8_t)value;
    target[1] = (uint8_t)(value >> 8);
    target[2] = (uint8_t)(value >> 16);
    target[3] = (uint8_t)(value >> 24);
}

static uint32_t dd_read_blob_u32(const uint8_t *source) {
    return (uint32_t)source[0] | ((uint32_t)source[1] << 8) |
           ((uint32_t)source[2] << 16) | ((uint32_t)source[3] << 24);
}

static void dd_build_intro_oam(uint8_t oam[256]) {
    static const uint8_t top_tiles[8] = {0x52u, 0x56u, 0xDCu, 0xE0u, 0xE4u, 0xE8u, 0xECu, 0xF0u};
    static const uint8_t bottom_tiles[8] = {0x54u, 0x58u, 0xDEu, 0xE2u, 0xE6u, 0xEAu, 0xEEu, 0xF2u};
    uint32_t sprite;
    memset(oam, 0, 256u);
    for (sprite = 0; sprite < 64u; ++sprite) oam[sprite * 4u] = 0xF4u;
    oam[0] = 0x38u; oam[1] = 0xFEu; oam[2] = 0x30u; oam[3] = 0x20u;
    for (sprite = 0; sprite < 8u; ++sprite) {
        uint32_t top = (sprite + 1u) * 4u;
        uint32_t bottom = (sprite + 9u) * 4u;
        oam[top] = 0x18u;
        oam[top + 1u] = top_tiles[sprite];
        oam[top + 3u] = (uint8_t)(0xA0u + sprite * 8u);
        oam[bottom] = 0x28u;
        oam[bottom + 1u] = bottom_tiles[sprite];
        oam[bottom + 3u] = (uint8_t)(0xA0u + sprite * 8u);
    }
}

static int dd_build_config_oam(const uint8_t *rom, size_t rom_size, uint8_t oam[256]) {
    const size_t object_table = dd_bank_file_offset(1u, 0xA2DBu);
    const size_t bank2_end = 16u + 3u * 0x4000u;
    uint32_t sprite = 1u;
    uint32_t object;
    memset(oam, 0, 256u);
    for (object = 0u; object < 64u; ++object) oam[object * 4u] = 0xF4u;
    oam[0] = 0x38u; oam[1] = 0xFEu; oam[2] = 0x30u; oam[3] = 0x20u;
    if (object_table + 44u > rom_size || bank2_end > rom_size) return 0;
    for (object = 0u; object < 11u; ++object) {
        uint8_t animation = rom[object_table + object * 4u];
        uint8_t base_attributes = rom[object_table + object * 4u + 1u];
        uint8_t base_x = rom[object_table + object * 4u + 2u];
        uint8_t base_y = rom[object_table + object * 4u + 3u];
        uint16_t address = dd_read_bank_u16(rom, 2u, 0x828Du + animation * 2u);
        size_t position = dd_bank_file_offset(2u, address);
        uint8_t attributes = base_attributes;
        uint8_t record_count;
        uint32_t record;
        if (address < 0x8000u || address >= 0xC000u || position >= bank2_end) return 0;
        record_count = rom[position++];
        for (record = 0u; record < record_count; ++record) {
            uint8_t lead;
            uint8_t y_offset;
            uint8_t tile;
            if (position + 3u > bank2_end || sprite >= 64u) return 0;
            lead = rom[position++];
            y_offset = (uint8_t)((lead >> 1u) | (lead & 0x80u));
            tile = rom[position++];
            if ((lead & 1u) == 0u) {
                if (position >= bank2_end) return 0;
                attributes = (uint8_t)((base_attributes | rom[position++]) & 0xE3u);
            }
            if (position >= bank2_end) return 0;
            oam[sprite * 4u] = (uint8_t)(base_y + y_offset);
            oam[sprite * 4u + 1u] = tile;
            oam[sprite * 4u + 2u] = attributes;
            oam[sprite * 4u + 3u] = (uint8_t)(base_x + (int8_t)rom[position++]);
            ++sprite;
        }
    }
    return sprite == 48u;
}

static int dd_build_config_assets(const uint8_t *rom, size_t rom_size,
                                  uint8_t **output_data, size_t *output_size) {
    const size_t capacity = 4096u;
    const size_t bank2_end = 16u + 3u * 0x4000u;
    uint8_t *data = (uint8_t *)calloc(1u, capacity);
    DDConfigAssetsHeader *header = (DDConfigAssetsHeader *)data;
    size_t position = sizeof(DDConfigAssetsHeader);
    uint32_t index;
    if (data == NULL || bank2_end > rom_size) {
        free(data);
        return 0;
    }
    memcpy(header->object_table, rom + dd_bank_file_offset(1u, 0xA2DBu), sizeof(header->object_table));
    memcpy(header->cursor_y, rom + dd_bank_file_offset(1u, 0xA364u), sizeof(header->cursor_y));
    memcpy(header->time_values, rom + dd_bank_file_offset(1u, 0xA368u), sizeof(header->time_values));
    memcpy(header->time_tiles, rom + dd_bank_file_offset(1u, 0xA36Cu), sizeof(header->time_tiles));
    memcpy(header->ball_velocity, rom + dd_bank_file_offset(1u, 0xA3F7u), sizeof(header->ball_velocity));
    memcpy(header->player_velocity, rom + dd_bank_file_offset(1u, 0xA3EBu), sizeof(header->player_velocity));
    memcpy(header->ball_offsets, rom + dd_bank_file_offset(1u, 0xA4DDu), sizeof(header->ball_offsets));
    memcpy(header->basket_y, rom + dd_bank_file_offset(1u, 0xA7BBu), sizeof(header->basket_y));
    memcpy(header->base_sprite_palette, rom + dd_bank_file_offset(1u, 0xA726u),
           sizeof(header->base_sprite_palette));
    memcpy(header->level_x, rom + dd_bank_file_offset(1u, 0xA58Fu), sizeof(header->level_x));
    for (index = 0u; index < 4u; ++index) {
        uint16_t team = dd_read_bank_u16(rom, 1u, 0xA63Du + index * 2u);
        uint16_t palette = dd_read_bank_u16(rom, 1u, 0xA6A5u + index * 2u);
        if (team < 0x8000u || team + 24u > 0xC000u || palette < 0x8000u || palette + 4u > 0xC000u) {
            free(data);
            return 0;
        }
        memcpy(header->team_tiles + index * 24u, rom + dd_bank_file_offset(1u, team), 24u);
        memcpy(header->team_sprite_palette + index * 4u, rom + dd_bank_file_offset(1u, palette), 4u);
    }
    for (index = 0u; index < 15u; ++index) {
        uint16_t address = dd_read_bank_u16(rom, 2u, 0x828Du + (0x60u + index) * 2u);
        size_t source = dd_bank_file_offset(2u, address);
        size_t cursor = source + 1u;
        uint32_t record;
        if (address < 0x8000u || address >= 0xC000u || source >= bank2_end) {
            free(data);
            return 0;
        }
        for (record = 0u; record < rom[source]; ++record) {
            if (cursor >= bank2_end) {
                free(data);
                return 0;
            }
            cursor += (rom[cursor] & 1u) != 0u ? 3u : 4u;
        }
        if (cursor > bank2_end || position + cursor - source > capacity) {
            free(data);
            return 0;
        }
        header->metasprite_offset[index] = (uint32_t)position;
        header->metasprite_size[index] = (uint32_t)(cursor - source);
        memcpy(data + position, rom + source, cursor - source);
        position += cursor - source;
    }
    *output_data = data;
    *output_size = position;
    return 1;
}

static int dd_build_tipoff_assets(const uint8_t *rom, size_t rom_size,
                                  uint8_t **output_data, size_t *output_size) {
    const size_t capacity = 8192u;
    const size_t bank2_end = 16u + 3u * 0x4000u;
    const size_t held_offsets = dd_bank_file_offset(0u, 0xB07Bu);
    const size_t height_scripts = dd_bank_file_offset(0u, 0x9B29u);
    const size_t shot_animation = dd_bank_file_offset(0u, 0xA9DCu);
    const size_t net_animation_tiles = dd_bank_file_offset(0u, 0x9922u);
    const size_t rebound_target_phase = dd_bank_file_offset(0u, 0x8503u);
    const size_t rebound_formation = dd_bank_file_offset(0u, 0x8507u);
    const size_t free_throw_formation = dd_bank_file_offset(0u, 0x85C7u);
    const size_t free_throw_facing = dd_bank_file_offset(0u, 0x86AFu);
    const size_t cpu_role_targets = 16u + 7u * 0x4000u + (0xD745u - 0xC000u);
    const size_t cpu_spacing_targets = dd_bank_file_offset(0u, 0x8452u);
    const size_t cpu_region_targets = dd_bank_file_offset(0u, 0xAC78u);
    const size_t court_chr_left = dd_bank_file_offset(0u, 0xB59Eu);
    const size_t court_chr_right = dd_bank_file_offset(0u, 0xB83Eu);
    uint8_t *data = (uint8_t *)calloc(1u, capacity);
    DDTipoffAssetsHeader *header = (DDTipoffAssetsHeader *)data;
    size_t position = sizeof(DDTipoffAssetsHeader);
    uint32_t index;
    if (data == NULL || bank2_end > rom_size || held_offsets + sizeof(header->held_ball_offsets) > rom_size ||
        height_scripts + sizeof(header->height_scripts) > rom_size ||
        shot_animation + sizeof(header->shot_animation) > rom_size ||
        net_animation_tiles + sizeof(header->net_animation_tiles) > rom_size ||
        rebound_target_phase + sizeof(header->rebound_target_phase) > rom_size ||
        rebound_formation + sizeof(header->rebound_formation) > rom_size ||
        free_throw_formation + sizeof(header->free_throw_formation) > rom_size ||
        free_throw_facing + sizeof(header->free_throw_facing) > rom_size ||
        cpu_role_targets + sizeof(header->cpu_role_targets) > rom_size ||
        cpu_spacing_targets + sizeof(header->cpu_spacing_targets) > rom_size ||
        cpu_region_targets + sizeof(header->cpu_region_targets) > rom_size ||
        court_chr_left + sizeof(header->court_chr_left) > rom_size ||
        court_chr_right + sizeof(header->court_chr_right) > rom_size) {
        free(data);
        return 0;
    }
    memcpy(header->held_ball_offsets, rom + held_offsets, sizeof(header->held_ball_offsets));
    memcpy(header->height_scripts, rom + height_scripts, sizeof(header->height_scripts));
    memcpy(header->shot_animation, rom + shot_animation, sizeof(header->shot_animation));
    memcpy(header->net_animation_tiles, rom + net_animation_tiles,
           sizeof(header->net_animation_tiles));
    memcpy(header->rebound_target_phase, rom + rebound_target_phase,
           sizeof(header->rebound_target_phase));
    memcpy(header->rebound_formation, rom + rebound_formation,
           sizeof(header->rebound_formation));
    memcpy(header->free_throw_formation, rom + free_throw_formation,
           sizeof(header->free_throw_formation));
    memcpy(header->free_throw_facing, rom + free_throw_facing,
           sizeof(header->free_throw_facing));
    memcpy(header->cpu_role_targets, rom + cpu_role_targets, sizeof(header->cpu_role_targets));
    memcpy(header->cpu_spacing_targets, rom + cpu_spacing_targets, sizeof(header->cpu_spacing_targets));
    memcpy(header->cpu_region_targets, rom + cpu_region_targets, sizeof(header->cpu_region_targets));
    memcpy(header->court_chr_left, rom + court_chr_left, sizeof(header->court_chr_left));
    memcpy(header->court_chr_right, rom + court_chr_right, sizeof(header->court_chr_right));
    for (index = 0u; index < DD_GAMEPLAY_METASPRITE_COUNT; ++index) {
        uint16_t address = dd_read_bank_u16(rom, 2u, 0x828Du + index * 2u);
        size_t source = dd_bank_file_offset(2u, address);
        size_t cursor = source + 1u;
        uint32_t record;
        if (address < 0x8000u || address >= 0xC000u || source >= bank2_end) {
            free(data);
            return 0;
        }
        for (record = 0u; record < rom[source]; ++record) {
            if (cursor >= bank2_end) {
                free(data);
                return 0;
            }
            cursor += (rom[cursor] & 1u) != 0u ? 3u : 4u;
        }
        if (cursor > bank2_end || position + cursor - source > capacity) {
            free(data);
            return 0;
        }
        header->metasprite_offset[index] = (uint32_t)position;
        header->metasprite_size[index] = (uint32_t)(cursor - source);
        memcpy(data + position, rom + source, cursor - source);
        position += cursor - source;
    }
    *output_data = data;
    *output_size = position;
    return 1;
}

static int dd_build_intro_updates(const uint8_t *rom, size_t rom_size,
                                  uint8_t **output_data, size_t *output_size,
                                  uint32_t *output_count) {
    const size_t capacity = 32768u;
    const size_t bank_end = 16u + 2u * 0x4000u;
    uint8_t *data = (uint8_t *)malloc(capacity);
    size_t position = 4u;
    uint32_t count = 0u;
    uint32_t phase;
    if (data == NULL || bank_end > rom_size) {
        free(data);
        return 0;
    }
    for (phase = 0; phase < 21u; ++phase) {
        uint8_t last_sub = rom[dd_bank_file_offset(1u, 0x9402u + phase)];
        uint16_t pointer_table = dd_read_bank_u16(rom, 1u, 0x9504u + phase * 2u);
        int sub;
        if (pointer_table < 0x8000u || pointer_table >= 0xC000u) {
            free(data);
            return 0;
        }
        for (sub = (int)last_sub; sub >= 0; --sub) {
            uint16_t command_address;
            size_t source;
            size_t command_size = 0u;
            uint16_t delay = sub == (int)last_sub ? (uint16_t)(phase == 0u ? 2u : 48u) : 1u;
            if ((uint32_t)pointer_table + (uint32_t)sub * 2u + 1u >= 0xC000u) {
                free(data);
                return 0;
            }
            command_address = dd_read_bank_u16(rom, 1u, (uint32_t)pointer_table + (uint32_t)sub * 2u);
            if (command_address < 0x8000u || command_address >= 0xC000u) {
                free(data);
                return 0;
            }
            source = dd_bank_file_offset(1u, command_address);
            while (source + command_size < bank_end && rom[source + command_size] != 0xFFu) ++command_size;
            if (source + command_size >= bank_end || command_size >= UINT16_MAX) {
                free(data);
                return 0;
            }
            ++command_size;
            if (position + 4u + command_size > capacity) {
                free(data);
                return 0;
            }
            dd_write_blob_u16(data + position, delay);
            dd_write_blob_u16(data + position + 2u, (uint16_t)command_size);
            position += 4u;
            memcpy(data + position, rom + source, command_size);
            position += command_size;
            ++count;
        }
    }
    {
        static const uint8_t metasprite_ids[3] = {0x6Fu, 0x74u, 0x7Au};
        static const size_t expected_sizes[3] = {5u, 56u, 56u};
        uint32_t metasprite;
        size_t tables = dd_bank_file_offset(1u, 0x9417u);
        if (position + DD_INTRO_SPRITE_ASSET_SIZE > capacity || tables + 24u > rom_size) {
            free(data);
            return 0;
        }
        memcpy(data + position, rom + tables, 24u);
        position += 24u;
        for (metasprite = 0u; metasprite < 3u; ++metasprite) {
            uint16_t address = dd_read_bank_u16(rom, 2u, 0x828Du + metasprite_ids[metasprite] * 2u);
            size_t source = dd_bank_file_offset(2u, address);
            size_t cursor = source + 1u;
            uint32_t record;
            if (address < 0x8000u || address >= 0xC000u || source >= rom_size) {
                free(data);
                return 0;
            }
            for (record = 0u; record < rom[source]; ++record) {
                if (cursor >= rom_size) {
                    free(data);
                    return 0;
                }
                cursor += (rom[cursor] & 1u) != 0u ? 3u : 4u;
            }
            if (cursor > rom_size || cursor - source != expected_sizes[metasprite]) {
                free(data);
                return 0;
            }
            memcpy(data + position, rom + source, expected_sizes[metasprite]);
            position += expected_sizes[metasprite];
        }
    }
    dd_write_blob_u32(data, count);
    *output_data = data;
    *output_size = position;
    *output_count = count;
    return 1;
}

int dd_build_asset_pack(const char *rom_path, const char *output_path) {
    uint8_t *rom = NULL;
    size_t rom_size = 0;
    uint8_t digest[32];
    uint8_t ppu[DD_TITLE_PPU_SIZE] = {0};
    uint8_t intro_ppu[DD_PPU_SIZE] = {0};
    uint8_t config_ppu[DD_PPU_SIZE] = {0};
    uint8_t tipoff_ppu[DD_PPU_SIZE] = {0};
    uint8_t oam[256];
    uint8_t intro_oam[256];
    uint8_t config_oam[256];
    uint8_t tipoff_oam[256];
    uint8_t *intro_updates = NULL;
    size_t intro_updates_size = 0u;
    uint8_t *config_assets = NULL;
    size_t config_assets_size = 0u;
    uint8_t *tipoff_assets = NULL;
    size_t tipoff_assets_size = 0u;
    uint32_t intro_update_count = 0u;
    uint32_t consumed[3] = {0};
    uint32_t intro_consumed[3] = {0};
    uint32_t config_consumed[3] = {0};
    uint32_t tipoff_consumed[6] = {0};
    const uint32_t dmc_file_offset = 0x1EAD0u;
    const uint32_t dmc_size = 3073u;
    const uint32_t title_palette_file_offset = 0x1C956u;
    const uint32_t intro_palette_file_offset = 0x1C9A8u;
    const uint32_t config_palette_file_offset = 0x1C97Fu;
    const uint32_t tipoff_palette_file_offset = 0x1C904u;
    const uint32_t tipoff_sprite_palette_file_offset = 0x1D2B3u;
    const uint32_t tipoff_dmc_file_offset = 0x1F790u;
    const uint32_t tipoff_dmc_size = 2113u;
    DDTitleMeta meta = {256u, 240u, 0x1000u, 0x2000u, 10u, 15u, 0u, 0xEAC0u, 3073u, 0xB0u, 20u, 90u};
    DDIntroMeta intro_meta = {256u, 240u, 0x1000u, 0x2000u, 0xB0u, 64u, 5u, 0u, 1920u};
    DDConfigMeta config_meta = {256u, 240u, 0x1000u, 0x2000u, 0xB0u, 64u, 4u,
                                2097u, 2093u, 896u};
    DDTipoffMeta tipoff_meta = {256u, 240u, 0x1000u, 0x2000u, 0xB0u, 64u,
                                127u, 135u, 140u, 144u, 15u, 0u, 0xF780u, 2113u,
                                45u, 18u, 12u, 189u, 42u, 437u, 4u, 13u, 0x7Fu};
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
        !dd_decode_stream(rom, rom_size, 5u, 0xB196u, intro_ppu, &intro_consumed[0]) ||
        !dd_decode_stream(rom, rom_size, 2u, 0xAD67u, intro_ppu, &intro_consumed[1]) ||
        !dd_decode_stream(rom, rom_size, 1u, 0xA030u, intro_ppu, &intro_consumed[2]) ||
        !dd_decode_stream(rom, rom_size, 4u, 0xB0B4u, config_ppu, &config_consumed[0]) ||
        !dd_decode_stream(rom, rom_size, 4u, 0xA74Bu, config_ppu, &config_consumed[1]) ||
        !dd_decode_stream(rom, rom_size, 1u, 0xA7FDu, config_ppu, &config_consumed[2]) ||
        !dd_decode_stream(rom, rom_size, 3u, 0x8D1Bu, tipoff_ppu, &tipoff_consumed[0]) ||
        !dd_decode_stream(rom, rom_size, 3u, 0x8001u, tipoff_ppu, &tipoff_consumed[1]) ||
        !dd_decode_stream(rom, rom_size, 7u, 0xC65Fu, tipoff_ppu, &tipoff_consumed[2]) ||
        !dd_decode_stream(rom, rom_size, 7u, 0xC674u, tipoff_ppu, &tipoff_consumed[3]) ||
        !dd_decode_stream(rom, rom_size, 2u, 0xA9CFu, tipoff_ppu, &tipoff_consumed[4]) ||
        !dd_decode_stream(rom, rom_size, 2u, 0xABC6u, tipoff_ppu, &tipoff_consumed[5]) ||
        !dd_build_intro_updates(rom, rom_size, &intro_updates, &intro_updates_size, &intro_update_count) ||
        !dd_build_config_oam(rom, rom_size, config_oam) ||
        !dd_build_config_assets(rom, rom_size, &config_assets, &config_assets_size) ||
        !dd_build_tipoff_assets(rom, rom_size, &tipoff_assets, &tipoff_assets_size) ||
        title_palette_file_offset + 32u > rom_size || intro_palette_file_offset + 32u > rom_size ||
        config_palette_file_offset + 32u > rom_size || tipoff_palette_file_offset + 16u > rom_size ||
        tipoff_sprite_palette_file_offset + 16u > rom_size ||
        dmc_file_offset + dmc_size > rom_size || tipoff_dmc_file_offset + tipoff_dmc_size > rom_size) {
        fprintf(stderr, "The title, intro, or configuration asset streams were malformed.\n");
        free(intro_updates);
        free(config_assets);
        free(tipoff_assets);
        free(rom);
        return 0;
    }
    memcpy(ppu + 0x3F00u, rom + title_palette_file_offset, 32u);
    memcpy(intro_ppu + 0x3F00u, rom + intro_palette_file_offset, 32u);
    memcpy(config_ppu + 0x3F00u, rom + config_palette_file_offset, 32u);
    memcpy(tipoff_ppu + 0x3F00u, rom + tipoff_palette_file_offset, 16u);
    memcpy(tipoff_ppu + 0x3F10u, rom + tipoff_sprite_palette_file_offset, 16u);
    {
        static const uint16_t addresses[] = {
            0x2053u,0x2090u,0x2091u,0x2092u,0x2093u,0x2094u,0x20C4u,0x20C8u,
            0x20C9u,0x20CAu,0x20CCu,0x20CDu,0x20CEu,0x20CFu,0x20D0u,0x20D1u,
            0x20D3u,0x20D4u,0x20D5u,0x20D6u,0x20D7u,0x20DBu
        };
        static const uint8_t values[] = {
            0xDAu,0xD9u,0xDEu,0xFDu,0xD9u,0xD9u,0xD9u,0xDAu,0xF5u,0xF6u,0xF2u,
            0xE7u,0xF4u,0xEBu,0xF1u,0xE6u,0xF5u,0xF6u,0xE3u,0xF4u,0xF6u,0xD9u
        };
        uint32_t index;
        for (index = 0u; index < sizeof(values); ++index) tipoff_ppu[addresses[index]] = values[index];
    }
    dd_build_title_oam(oam);
    dd_build_intro_oam(intro_oam);
    dd_build_tipoff_oam(tipoff_oam);
    intro_meta.update_count = intro_update_count;

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
    dd_set_entry(&entries[4], "intro.meta", DD_ENTRY_INTRO_META, payload_offset,
                 sizeof(intro_meta), dd_crc32((const uint8_t *)&intro_meta, sizeof(intro_meta)),
                 0xFFFFFFFFu, 0u, 0u, 0u);
    payload_offset += sizeof(intro_meta);
    dd_set_entry(&entries[5], "intro.ppu", DD_ENTRY_PPU, payload_offset,
                 sizeof(intro_ppu), dd_crc32(intro_ppu, sizeof(intro_ppu)),
                 0xFFFFFFFFu, 0u, intro_consumed[0] + intro_consumed[1] + intro_consumed[2], 1u);
    payload_offset += sizeof(intro_ppu);
    dd_set_entry(&entries[6], "intro.oam", DD_ENTRY_OAM, payload_offset,
                 sizeof(intro_oam), dd_crc32(intro_oam, sizeof(intro_oam)),
                 0xFFFFFFFFu, 0u, 0u, 3u);
    payload_offset += sizeof(intro_oam);
    dd_set_entry(&entries[7], "intro.updates", DD_ENTRY_UPDATES, payload_offset,
                 intro_updates_size, dd_crc32(intro_updates, intro_updates_size),
                 1u, (uint32_t)dd_bank_file_offset(1u, 0x9402u), 0x1280u, 4u);
    payload_offset += intro_updates_size;
    dd_set_entry(&entries[8], "intro.music", DD_ENTRY_MUSIC, payload_offset,
                 sizeof(DD_INTRO_MUSIC), dd_crc32((const uint8_t *)DD_INTRO_MUSIC, sizeof(DD_INTRO_MUSIC)),
                 1u, (uint32_t)dd_bank_file_offset(1u, 0x8000u), 0x04A1u, 5u);
    payload_offset += sizeof(DD_INTRO_MUSIC);
    dd_set_entry(&entries[9], "config.meta", DD_ENTRY_CONFIG_META, payload_offset,
                 sizeof(config_meta), dd_crc32((const uint8_t *)&config_meta, sizeof(config_meta)),
                 1u, (uint32_t)dd_bank_file_offset(1u, 0xA25Bu), 0xACu, 0u);
    payload_offset += sizeof(config_meta);
    dd_set_entry(&entries[10], "config.ppu", DD_ENTRY_PPU, payload_offset,
                 sizeof(config_ppu), dd_crc32(config_ppu, sizeof(config_ppu)),
                 0xFFFFFFFFu, 0u, config_consumed[0] + config_consumed[1] + config_consumed[2], 1u);
    payload_offset += sizeof(config_ppu);
    dd_set_entry(&entries[11], "config.oam", DD_ENTRY_OAM, payload_offset,
                 sizeof(config_oam), dd_crc32(config_oam, sizeof(config_oam)),
                 2u, (uint32_t)dd_bank_file_offset(2u, 0x828Du), 0x758u, 6u);
    payload_offset += sizeof(config_oam);
    dd_set_entry(&entries[12], "select.music", DD_ENTRY_MUSIC, payload_offset,
                 sizeof(DD_SELECT_MUSIC), dd_crc32((const uint8_t *)DD_SELECT_MUSIC, sizeof(DD_SELECT_MUSIC)),
                 1u, (uint32_t)dd_bank_file_offset(1u, 0x8000u), 0x04A1u, 5u);
    payload_offset += sizeof(DD_SELECT_MUSIC);
    dd_set_entry(&entries[13], "config.assets", DD_ENTRY_CONFIG_ASSETS, payload_offset,
                 config_assets_size, dd_crc32(config_assets, config_assets_size),
                 1u, (uint32_t)dd_bank_file_offset(1u, 0xA2DBu), 0x4E4u, 7u);
    payload_offset += config_assets_size;
    dd_set_entry(&entries[14], "config.music", DD_ENTRY_MUSIC, payload_offset,
                 sizeof(DD_CONFIG_MUSIC), dd_crc32((const uint8_t *)DD_CONFIG_MUSIC, sizeof(DD_CONFIG_MUSIC)),
                 1u, (uint32_t)dd_bank_file_offset(1u, 0x8598u), 0x500u, 5u);
    payload_offset += sizeof(DD_CONFIG_MUSIC);
    dd_set_entry(&entries[15], "tipoff.meta", DD_ENTRY_TIPOFF_META, payload_offset,
                 sizeof(tipoff_meta), dd_crc32((const uint8_t *)&tipoff_meta, sizeof(tipoff_meta)),
                 0u, (uint32_t)dd_bank_file_offset(0u, 0x8491u), 0x30C7u, 0u);
    payload_offset += sizeof(tipoff_meta);
    dd_set_entry(&entries[16], "tipoff.ppu", DD_ENTRY_PPU, payload_offset,
                 sizeof(tipoff_ppu), dd_crc32(tipoff_ppu, sizeof(tipoff_ppu)),
                 0xFFFFFFFFu, 0u, tipoff_consumed[0] + tipoff_consumed[1] + tipoff_consumed[2] +
                 tipoff_consumed[3] + tipoff_consumed[4] + tipoff_consumed[5], 1u);
    payload_offset += sizeof(tipoff_ppu);
    dd_set_entry(&entries[17], "tipoff.oam", DD_ENTRY_OAM, payload_offset,
                 sizeof(tipoff_oam), dd_crc32(tipoff_oam, sizeof(tipoff_oam)),
                 2u, (uint32_t)dd_bank_file_offset(2u, 0x8000u), 0x02CDu, 8u);
    payload_offset += sizeof(tipoff_oam);
    dd_set_entry(&entries[18], "tipoff.assets", DD_ENTRY_TIPOFF_ASSETS, payload_offset,
                 tipoff_assets_size, dd_crc32(tipoff_assets, tipoff_assets_size),
                 0xFFFFFFFFu, (uint32_t)dd_bank_file_offset(2u, 0x828Du), 0x0310u, 9u);
    payload_offset += tipoff_assets_size;
    dd_set_entry(&entries[19], "end.music", DD_ENTRY_MUSIC, payload_offset,
                 sizeof(DD_END_MUSIC), dd_crc32((const uint8_t *)DD_END_MUSIC, sizeof(DD_END_MUSIC)),
                 1u, (uint32_t)dd_bank_file_offset(1u, 0x8000u), 0x04A1u, 5u);
    payload_offset += sizeof(DD_END_MUSIC);
    dd_set_entry(&entries[20], "tipoff.dmc", DD_ENTRY_DMC, payload_offset,
                 tipoff_dmc_size, dd_crc32(rom + tipoff_dmc_file_offset, tipoff_dmc_size),
                 7u, tipoff_dmc_file_offset, tipoff_dmc_size, 2u);
    payload_offset += tipoff_dmc_size;
    dd_set_entry(&entries[21], "gameplay.audio", DD_ENTRY_MUSIC, payload_offset,
                 sizeof(DD_GAMEPLAY_AUDIO),
                 dd_crc32((const uint8_t *)DD_GAMEPLAY_AUDIO, sizeof(DD_GAMEPLAY_AUDIO)),
                 1u, (uint32_t)dd_bank_file_offset(1u, 0x8653u), 0x40u, 5u);
    payload_offset += sizeof(DD_GAMEPLAY_AUDIO);
    dd_set_entry(&entries[22], "whistle.audio", DD_ENTRY_MUSIC, payload_offset,
                 sizeof(DD_WHISTLE_AUDIO),
                 dd_crc32((const uint8_t *)DD_WHISTLE_AUDIO, sizeof(DD_WHISTLE_AUDIO)),
                 1u, (uint32_t)dd_bank_file_offset(1u, 0x86F7u), 0x20u, 5u);
    payload_offset += sizeof(DD_WHISTLE_AUDIO);
    dd_set_entry(&entries[23], "three.call", DD_ENTRY_MUSIC, payload_offset,
                 sizeof(DD_THREE_CALL_AUDIO),
                 dd_crc32((const uint8_t *)DD_THREE_CALL_AUDIO, sizeof(DD_THREE_CALL_AUDIO)),
                 1u, (uint32_t)dd_bank_file_offset(1u, 0x8000u), 0x03ABu, 5u);
    payload_offset += sizeof(DD_THREE_CALL_AUDIO);
    dd_set_entry(&entries[24], "three.score", DD_ENTRY_MUSIC, payload_offset,
                 sizeof(DD_THREE_SCORE_AUDIO),
                 dd_crc32((const uint8_t *)DD_THREE_SCORE_AUDIO, sizeof(DD_THREE_SCORE_AUDIO)),
                 1u, (uint32_t)dd_bank_file_offset(1u, 0x8000u), 0x03ABu, 5u);
    payload_offset += sizeof(DD_THREE_SCORE_AUDIO);
    dd_set_entry(&entries[25], "basket.score", DD_ENTRY_MUSIC, payload_offset,
                 sizeof(DD_SCORE_AUDIO),
                 dd_crc32((const uint8_t *)DD_SCORE_AUDIO, sizeof(DD_SCORE_AUDIO)),
                 1u, (uint32_t)dd_bank_file_offset(1u, 0x87B6u), 0x1ACu, 5u);
    payload_offset += sizeof(DD_SCORE_AUDIO);
    dd_set_entry(&entries[26], "cpu.block", DD_ENTRY_MUSIC, payload_offset,
                 sizeof(DD_CPU_BLOCK_AUDIO),
                 dd_crc32((const uint8_t *)DD_CPU_BLOCK_AUDIO, sizeof(DD_CPU_BLOCK_AUDIO)),
                 1u, (uint32_t)dd_bank_file_offset(1u, 0x87A4u), 0x12u, 5u);
    payload_offset += sizeof(DD_CPU_BLOCK_AUDIO);
    dd_set_entry(&entries[27], "user.block", DD_ENTRY_MUSIC, payload_offset,
                 sizeof(DD_USER_BLOCK_AUDIO),
                 dd_crc32((const uint8_t *)DD_USER_BLOCK_AUDIO, sizeof(DD_USER_BLOCK_AUDIO)),
                 1u, (uint32_t)dd_bank_file_offset(1u, 0x866Bu), 0x182u, 5u);
    payload_offset += sizeof(DD_USER_BLOCK_AUDIO);
    header.directory_crc32 = dd_crc32((const uint8_t *)entries, sizeof(entries));
    header.total_size = payload_offset;

    output = fopen(output_path, "wb");
    if (output == NULL || fwrite(&header, 1, sizeof(header), output) != sizeof(header) ||
        fwrite(entries, 1, sizeof(entries), output) != sizeof(entries) ||
        fwrite(&meta, 1, sizeof(meta), output) != sizeof(meta) ||
        fwrite(ppu, 1, sizeof(ppu), output) != sizeof(ppu) ||
        fwrite(rom + dmc_file_offset, 1, dmc_size, output) != dmc_size ||
        fwrite(oam, 1, sizeof(oam), output) != sizeof(oam) ||
        fwrite(&intro_meta, 1, sizeof(intro_meta), output) != sizeof(intro_meta) ||
        fwrite(intro_ppu, 1, sizeof(intro_ppu), output) != sizeof(intro_ppu) ||
        fwrite(intro_oam, 1, sizeof(intro_oam), output) != sizeof(intro_oam) ||
        fwrite(intro_updates, 1, intro_updates_size, output) != intro_updates_size ||
        fwrite(DD_INTRO_MUSIC, 1, sizeof(DD_INTRO_MUSIC), output) != sizeof(DD_INTRO_MUSIC) ||
        fwrite(&config_meta, 1, sizeof(config_meta), output) != sizeof(config_meta) ||
        fwrite(config_ppu, 1, sizeof(config_ppu), output) != sizeof(config_ppu) ||
        fwrite(config_oam, 1, sizeof(config_oam), output) != sizeof(config_oam) ||
        fwrite(DD_SELECT_MUSIC, 1, sizeof(DD_SELECT_MUSIC), output) != sizeof(DD_SELECT_MUSIC) ||
        fwrite(config_assets, 1, config_assets_size, output) != config_assets_size ||
        fwrite(DD_CONFIG_MUSIC, 1, sizeof(DD_CONFIG_MUSIC), output) != sizeof(DD_CONFIG_MUSIC) ||
        fwrite(&tipoff_meta, 1, sizeof(tipoff_meta), output) != sizeof(tipoff_meta) ||
        fwrite(tipoff_ppu, 1, sizeof(tipoff_ppu), output) != sizeof(tipoff_ppu) ||
        fwrite(tipoff_oam, 1, sizeof(tipoff_oam), output) != sizeof(tipoff_oam) ||
        fwrite(tipoff_assets, 1, tipoff_assets_size, output) != tipoff_assets_size ||
        fwrite(DD_END_MUSIC, 1, sizeof(DD_END_MUSIC), output) != sizeof(DD_END_MUSIC) ||
        fwrite(rom + tipoff_dmc_file_offset, 1, tipoff_dmc_size, output) != tipoff_dmc_size ||
        fwrite(DD_GAMEPLAY_AUDIO, 1, sizeof(DD_GAMEPLAY_AUDIO), output) != sizeof(DD_GAMEPLAY_AUDIO) ||
        fwrite(DD_WHISTLE_AUDIO, 1, sizeof(DD_WHISTLE_AUDIO), output) != sizeof(DD_WHISTLE_AUDIO) ||
        fwrite(DD_THREE_CALL_AUDIO, 1, sizeof(DD_THREE_CALL_AUDIO), output) != sizeof(DD_THREE_CALL_AUDIO) ||
        fwrite(DD_THREE_SCORE_AUDIO, 1, sizeof(DD_THREE_SCORE_AUDIO), output) != sizeof(DD_THREE_SCORE_AUDIO) ||
        fwrite(DD_SCORE_AUDIO, 1, sizeof(DD_SCORE_AUDIO), output) != sizeof(DD_SCORE_AUDIO) ||
        fwrite(DD_CPU_BLOCK_AUDIO, 1, sizeof(DD_CPU_BLOCK_AUDIO), output) != sizeof(DD_CPU_BLOCK_AUDIO) ||
        fwrite(DD_USER_BLOCK_AUDIO, 1, sizeof(DD_USER_BLOCK_AUDIO), output) != sizeof(DD_USER_BLOCK_AUDIO)) {
        if (output != NULL) {
            fclose(output);
        }
        free(intro_updates);
        free(config_assets);
        free(tipoff_assets);
        free(rom);
        return 0;
    }
    fclose(output);
    free(intro_updates);
    free(config_assets);
    free(tipoff_assets);
    free(rom);
    printf("Built %s (title streams: %u/%u/%u, intro streams: %u/%u/%u, %u updates; config streams: %u/%u/%u; tip-off streams: %u/%u/%u/%u/%u/%u).\n",
           output_path, consumed[0], consumed[1], consumed[2],
           intro_consumed[0], intro_consumed[1], intro_consumed[2], intro_update_count,
           config_consumed[0], config_consumed[1], config_consumed[2],
           tipoff_consumed[0], tipoff_consumed[1], tipoff_consumed[2], tipoff_consumed[3],
           tipoff_consumed[4], tipoff_consumed[5]);
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
    const DDPackEntry *intro_meta_entry;
    const DDPackEntry *intro_ppu_entry;
    const DDPackEntry *intro_oam_entry;
    const DDPackEntry *intro_updates_entry;
    const DDPackEntry *intro_music_entry;
    const DDPackEntry *config_meta_entry;
    const DDPackEntry *config_ppu_entry;
    const DDPackEntry *config_oam_entry;
    const DDPackEntry *select_music_entry;
    const DDPackEntry *config_assets_entry;
    const DDPackEntry *config_music_entry;
    const DDPackEntry *tipoff_meta_entry;
    const DDPackEntry *tipoff_ppu_entry;
    const DDPackEntry *tipoff_oam_entry;
    const DDPackEntry *tipoff_assets_entry;
    const DDPackEntry *end_music_entry;
    const DDPackEntry *tipoff_dmc_entry;
    const DDPackEntry *gameplay_audio_entry;
    const DDPackEntry *whistle_audio_entry;
    const DDPackEntry *three_call_audio_entry;
    const DDPackEntry *three_score_audio_entry;
    const DDPackEntry *score_audio_entry;
    const DDPackEntry *cpu_block_audio_entry;
    const DDPackEntry *user_block_audio_entry;
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
    intro_meta_entry = dd_find_entry(entries, header->entry_count, DD_ENTRY_INTRO_META, "intro.meta");
    intro_ppu_entry = dd_find_entry(entries, header->entry_count, DD_ENTRY_PPU, "intro.ppu");
    intro_oam_entry = dd_find_entry(entries, header->entry_count, DD_ENTRY_OAM, "intro.oam");
    intro_updates_entry = dd_find_entry(entries, header->entry_count, DD_ENTRY_UPDATES, "intro.updates");
    intro_music_entry = dd_find_entry(entries, header->entry_count, DD_ENTRY_MUSIC, "intro.music");
    config_meta_entry = dd_find_entry(entries, header->entry_count, DD_ENTRY_CONFIG_META, "config.meta");
    config_ppu_entry = dd_find_entry(entries, header->entry_count, DD_ENTRY_PPU, "config.ppu");
    config_oam_entry = dd_find_entry(entries, header->entry_count, DD_ENTRY_OAM, "config.oam");
    select_music_entry = dd_find_entry(entries, header->entry_count, DD_ENTRY_MUSIC, "select.music");
    config_assets_entry = dd_find_entry(entries, header->entry_count, DD_ENTRY_CONFIG_ASSETS, "config.assets");
    config_music_entry = dd_find_entry(entries, header->entry_count, DD_ENTRY_MUSIC, "config.music");
    tipoff_meta_entry = dd_find_entry(entries, header->entry_count, DD_ENTRY_TIPOFF_META, "tipoff.meta");
    tipoff_ppu_entry = dd_find_entry(entries, header->entry_count, DD_ENTRY_PPU, "tipoff.ppu");
    tipoff_oam_entry = dd_find_entry(entries, header->entry_count, DD_ENTRY_OAM, "tipoff.oam");
    tipoff_assets_entry = dd_find_entry(entries, header->entry_count, DD_ENTRY_TIPOFF_ASSETS, "tipoff.assets");
    end_music_entry = dd_find_entry(entries, header->entry_count, DD_ENTRY_MUSIC, "end.music");
    tipoff_dmc_entry = dd_find_entry(entries, header->entry_count, DD_ENTRY_DMC, "tipoff.dmc");
    gameplay_audio_entry = dd_find_entry(entries, header->entry_count, DD_ENTRY_MUSIC, "gameplay.audio");
    whistle_audio_entry = dd_find_entry(entries, header->entry_count, DD_ENTRY_MUSIC, "whistle.audio");
    three_call_audio_entry = dd_find_entry(entries, header->entry_count, DD_ENTRY_MUSIC, "three.call");
    three_score_audio_entry = dd_find_entry(entries, header->entry_count, DD_ENTRY_MUSIC, "three.score");
    score_audio_entry = dd_find_entry(entries, header->entry_count, DD_ENTRY_MUSIC, "basket.score");
    cpu_block_audio_entry = dd_find_entry(entries, header->entry_count, DD_ENTRY_MUSIC, "cpu.block");
    user_block_audio_entry = dd_find_entry(entries, header->entry_count, DD_ENTRY_MUSIC, "user.block");
    if (meta_entry == NULL || ppu_entry == NULL || dmc_entry == NULL || oam_entry == NULL ||
        intro_meta_entry == NULL || intro_ppu_entry == NULL || intro_oam_entry == NULL ||
        intro_updates_entry == NULL || intro_music_entry == NULL || config_meta_entry == NULL ||
        config_ppu_entry == NULL || config_oam_entry == NULL || select_music_entry == NULL ||
        config_assets_entry == NULL || config_music_entry == NULL || tipoff_meta_entry == NULL || tipoff_ppu_entry == NULL ||
        tipoff_oam_entry == NULL || tipoff_assets_entry == NULL || end_music_entry == NULL ||
        tipoff_dmc_entry == NULL || gameplay_audio_entry == NULL || whistle_audio_entry == NULL ||
        three_call_audio_entry == NULL || three_score_audio_entry == NULL || score_audio_entry == NULL ||
        cpu_block_audio_entry == NULL || user_block_audio_entry == NULL ||
        meta_entry->size != sizeof(DDTitleMeta) || ppu_entry->size != DD_TITLE_PPU_SIZE ||
        dmc_entry->size != 3073u || oam_entry->size != 256u ||
        intro_meta_entry->size != sizeof(DDIntroMeta) || intro_ppu_entry->size != DD_PPU_SIZE ||
        intro_oam_entry->size != 256u || intro_updates_entry->size < 4u ||
        intro_music_entry->size == 0u || intro_music_entry->size % sizeof(DDMusicNote) != 0u ||
        config_meta_entry->size != sizeof(DDConfigMeta) || config_ppu_entry->size != DD_PPU_SIZE ||
        config_oam_entry->size != 256u || select_music_entry->size == 0u ||
        select_music_entry->size % sizeof(DDMusicNote) != 0u ||
        config_assets_entry->size < sizeof(DDConfigAssetsHeader) ||
        config_music_entry->size == 0u || config_music_entry->size % sizeof(DDMusicNote) != 0u ||
        tipoff_meta_entry->size != sizeof(DDTipoffMeta) || tipoff_ppu_entry->size != DD_PPU_SIZE ||
        tipoff_oam_entry->size != 256u || tipoff_assets_entry->size < sizeof(DDTipoffAssetsHeader) ||
        end_music_entry->size == 0u ||
        end_music_entry->size % sizeof(DDMusicNote) != 0u || tipoff_dmc_entry->size != 2113u ||
        gameplay_audio_entry->size == 0u || gameplay_audio_entry->size % sizeof(DDMusicNote) != 0u ||
        whistle_audio_entry->size == 0u || whistle_audio_entry->size % sizeof(DDMusicNote) != 0u ||
        three_call_audio_entry->size == 0u || three_call_audio_entry->size % sizeof(DDMusicNote) != 0u ||
        three_score_audio_entry->size == 0u || three_score_audio_entry->size % sizeof(DDMusicNote) != 0u ||
        score_audio_entry->size == 0u || score_audio_entry->size % sizeof(DDMusicNote) != 0u ||
        cpu_block_audio_entry->size == 0u || cpu_block_audio_entry->size % sizeof(DDMusicNote) != 0u ||
        user_block_audio_entry->size == 0u || user_block_audio_entry->size % sizeof(DDMusicNote) != 0u ||
        !dd_entry_in_bounds(meta_entry, file_size) || !dd_entry_in_bounds(ppu_entry, file_size) ||
        !dd_entry_in_bounds(dmc_entry, file_size) || !dd_entry_in_bounds(oam_entry, file_size) ||
        !dd_entry_in_bounds(intro_meta_entry, file_size) || !dd_entry_in_bounds(intro_ppu_entry, file_size) ||
        !dd_entry_in_bounds(intro_oam_entry, file_size) || !dd_entry_in_bounds(intro_updates_entry, file_size) ||
        !dd_entry_in_bounds(intro_music_entry, file_size) ||
        !dd_entry_in_bounds(config_meta_entry, file_size) || !dd_entry_in_bounds(config_ppu_entry, file_size) ||
        !dd_entry_in_bounds(config_oam_entry, file_size) || !dd_entry_in_bounds(select_music_entry, file_size) ||
        !dd_entry_in_bounds(config_assets_entry, file_size) || !dd_entry_in_bounds(config_music_entry, file_size) ||
        !dd_entry_in_bounds(tipoff_meta_entry, file_size) || !dd_entry_in_bounds(tipoff_ppu_entry, file_size) ||
        !dd_entry_in_bounds(tipoff_oam_entry, file_size) || !dd_entry_in_bounds(tipoff_assets_entry, file_size) ||
        !dd_entry_in_bounds(end_music_entry, file_size) ||
        !dd_entry_in_bounds(tipoff_dmc_entry, file_size) ||
        !dd_entry_in_bounds(gameplay_audio_entry, file_size) ||
        !dd_entry_in_bounds(whistle_audio_entry, file_size) ||
        !dd_entry_in_bounds(three_call_audio_entry, file_size) ||
        !dd_entry_in_bounds(three_score_audio_entry, file_size) ||
        !dd_entry_in_bounds(score_audio_entry, file_size) ||
        !dd_entry_in_bounds(cpu_block_audio_entry, file_size) ||
        !dd_entry_in_bounds(user_block_audio_entry, file_size) ||
        meta_entry->crc32 != dd_crc32(file_data + meta_entry->offset, (size_t)meta_entry->size) ||
        ppu_entry->crc32 != dd_crc32(file_data + ppu_entry->offset, (size_t)ppu_entry->size) ||
        dmc_entry->crc32 != dd_crc32(file_data + dmc_entry->offset, (size_t)dmc_entry->size) ||
        oam_entry->crc32 != dd_crc32(file_data + oam_entry->offset, (size_t)oam_entry->size) ||
        intro_meta_entry->crc32 != dd_crc32(file_data + intro_meta_entry->offset, (size_t)intro_meta_entry->size) ||
        intro_ppu_entry->crc32 != dd_crc32(file_data + intro_ppu_entry->offset, (size_t)intro_ppu_entry->size) ||
        intro_oam_entry->crc32 != dd_crc32(file_data + intro_oam_entry->offset, (size_t)intro_oam_entry->size) ||
        intro_updates_entry->crc32 != dd_crc32(file_data + intro_updates_entry->offset, (size_t)intro_updates_entry->size) ||
        intro_music_entry->crc32 != dd_crc32(file_data + intro_music_entry->offset, (size_t)intro_music_entry->size) ||
        config_meta_entry->crc32 != dd_crc32(file_data + config_meta_entry->offset, (size_t)config_meta_entry->size) ||
        config_ppu_entry->crc32 != dd_crc32(file_data + config_ppu_entry->offset, (size_t)config_ppu_entry->size) ||
        config_oam_entry->crc32 != dd_crc32(file_data + config_oam_entry->offset, (size_t)config_oam_entry->size) ||
        select_music_entry->crc32 != dd_crc32(file_data + select_music_entry->offset, (size_t)select_music_entry->size) ||
        config_assets_entry->crc32 != dd_crc32(file_data + config_assets_entry->offset, (size_t)config_assets_entry->size) ||
        config_music_entry->crc32 != dd_crc32(file_data + config_music_entry->offset, (size_t)config_music_entry->size) ||
        tipoff_meta_entry->crc32 != dd_crc32(file_data + tipoff_meta_entry->offset, (size_t)tipoff_meta_entry->size) ||
        tipoff_ppu_entry->crc32 != dd_crc32(file_data + tipoff_ppu_entry->offset, (size_t)tipoff_ppu_entry->size) ||
        tipoff_oam_entry->crc32 != dd_crc32(file_data + tipoff_oam_entry->offset, (size_t)tipoff_oam_entry->size) ||
        tipoff_assets_entry->crc32 != dd_crc32(file_data + tipoff_assets_entry->offset, (size_t)tipoff_assets_entry->size) ||
        end_music_entry->crc32 != dd_crc32(file_data + end_music_entry->offset, (size_t)end_music_entry->size) ||
        tipoff_dmc_entry->crc32 != dd_crc32(file_data + tipoff_dmc_entry->offset, (size_t)tipoff_dmc_entry->size) ||
        gameplay_audio_entry->crc32 != dd_crc32(file_data + gameplay_audio_entry->offset,
                                                (size_t)gameplay_audio_entry->size) ||
        whistle_audio_entry->crc32 != dd_crc32(file_data + whistle_audio_entry->offset,
                                               (size_t)whistle_audio_entry->size) ||
        three_call_audio_entry->crc32 != dd_crc32(file_data + three_call_audio_entry->offset,
                                                  (size_t)three_call_audio_entry->size) ||
        three_score_audio_entry->crc32 != dd_crc32(file_data + three_score_audio_entry->offset,
                                                   (size_t)three_score_audio_entry->size) ||
        score_audio_entry->crc32 != dd_crc32(file_data + score_audio_entry->offset,
                                             (size_t)score_audio_entry->size) ||
        cpu_block_audio_entry->crc32 != dd_crc32(file_data + cpu_block_audio_entry->offset,
                                                 (size_t)cpu_block_audio_entry->size) ||
        user_block_audio_entry->crc32 != dd_crc32(file_data + user_block_audio_entry->offset,
                                                  (size_t)user_block_audio_entry->size)) {
        free(file_data);
        return 0;
    }
    memcpy(&pack->meta, file_data + meta_entry->offset, sizeof(pack->meta));
    memcpy(&pack->intro_meta, file_data + intro_meta_entry->offset, sizeof(pack->intro_meta));
    memcpy(&pack->config_meta, file_data + config_meta_entry->offset, sizeof(pack->config_meta));
    memcpy(&pack->tipoff_meta, file_data + tipoff_meta_entry->offset, sizeof(pack->tipoff_meta));
    if (pack->meta.width != 256u || pack->meta.height != 240u ||
        pack->meta.background_pattern_base != 0x1000u || pack->meta.nametable_base != 0x2000u ||
        pack->intro_meta.width != 256u || pack->intro_meta.height != 240u ||
        pack->intro_meta.background_pattern_base != 0x1000u || pack->intro_meta.nametable_base != 0x2000u ||
        pack->meta.select_music_frames != 90u ||
        pack->intro_meta.sprite_count > 64u || pack->intro_meta.music_frames == 0u ||
        pack->config_meta.width != 256u || pack->config_meta.height != 240u ||
        pack->config_meta.background_pattern_base != 0x1000u || pack->config_meta.nametable_base != 0x2000u ||
        pack->config_meta.sprite_count > 64u || pack->config_meta.option_count != 4u ||
        pack->config_meta.original_visible_frame != 2097u || pack->config_meta.music_start_frame != 2093u ||
        pack->config_meta.music_loop_frames != 896u ||
        pack->tipoff_meta.width != 256u || pack->tipoff_meta.height != 240u ||
        pack->tipoff_meta.background_pattern_base != 0x1000u || pack->tipoff_meta.nametable_base != 0x2000u ||
        pack->tipoff_meta.sprite_count > 64u || pack->tipoff_meta.black_frame != 127u ||
        pack->tipoff_meta.blue_frame != 135u || pack->tipoff_meta.dmc_frame != 140u ||
        pack->tipoff_meta.visible_frame != 144u || pack->tipoff_meta.dmc_rate_index >= 16u ||
        pack->tipoff_meta.dmc_length != tipoff_dmc_entry->size || pack->tipoff_meta.end_music_frames != 45u ||
        pack->tipoff_meta.gameplay_audio_frames != 18u ||
        pack->tipoff_meta.whistle_audio_frames != 12u ||
        pack->tipoff_meta.three_call_audio_frames != 189u ||
        pack->tipoff_meta.three_score_audio_frames != 42u ||
        pack->tipoff_meta.score_audio_frames != 437u ||
        pack->tipoff_meta.cpu_block_audio_frames != 4u ||
        pack->tipoff_meta.user_block_audio_frames != 13u ||
        pack->tipoff_meta.scroll_x != 0x7Fu ||
        dd_read_blob_u32(file_data + intro_updates_entry->offset) != pack->intro_meta.update_count) {
        free(file_data);
        memset(pack, 0, sizeof(*pack));
        return 0;
    }
    pack->ppu = (uint8_t *)malloc((size_t)ppu_entry->size);
    pack->dmc = (uint8_t *)malloc((size_t)dmc_entry->size);
    pack->oam = (uint8_t *)malloc((size_t)oam_entry->size);
    pack->intro_ppu = (uint8_t *)malloc((size_t)intro_ppu_entry->size);
    pack->intro_oam = (uint8_t *)malloc((size_t)intro_oam_entry->size);
    pack->intro_updates = (uint8_t *)malloc((size_t)intro_updates_entry->size);
    pack->intro_music = (DDMusicNote *)malloc((size_t)intro_music_entry->size);
    pack->select_music = (DDMusicNote *)malloc((size_t)select_music_entry->size);
    pack->config_music = (DDMusicNote *)malloc((size_t)config_music_entry->size);
    pack->config_ppu = (uint8_t *)malloc((size_t)config_ppu_entry->size);
    pack->config_oam = (uint8_t *)malloc((size_t)config_oam_entry->size);
    pack->config_assets = (uint8_t *)malloc((size_t)config_assets_entry->size);
    pack->tipoff_ppu = (uint8_t *)malloc((size_t)tipoff_ppu_entry->size);
    pack->tipoff_oam = (uint8_t *)malloc((size_t)tipoff_oam_entry->size);
    pack->tipoff_assets = (uint8_t *)malloc((size_t)tipoff_assets_entry->size);
    pack->end_music = (DDMusicNote *)malloc((size_t)end_music_entry->size);
    pack->tipoff_dmc = (uint8_t *)malloc((size_t)tipoff_dmc_entry->size);
    pack->gameplay_audio = (DDMusicNote *)malloc((size_t)gameplay_audio_entry->size);
    pack->whistle_audio = (DDMusicNote *)malloc((size_t)whistle_audio_entry->size);
    pack->three_call_audio = (DDMusicNote *)malloc((size_t)three_call_audio_entry->size);
    pack->three_score_audio = (DDMusicNote *)malloc((size_t)three_score_audio_entry->size);
    pack->score_audio = (DDMusicNote *)malloc((size_t)score_audio_entry->size);
    pack->cpu_block_audio = (DDMusicNote *)malloc((size_t)cpu_block_audio_entry->size);
    pack->user_block_audio = (DDMusicNote *)malloc((size_t)user_block_audio_entry->size);
    if (pack->ppu == NULL || pack->dmc == NULL || pack->oam == NULL ||
        pack->intro_ppu == NULL || pack->intro_oam == NULL ||
        pack->intro_updates == NULL || pack->intro_music == NULL || pack->select_music == NULL ||
        pack->config_music == NULL ||
        pack->config_ppu == NULL || pack->config_oam == NULL || pack->config_assets == NULL ||
        pack->tipoff_ppu == NULL || pack->tipoff_oam == NULL || pack->tipoff_assets == NULL ||
        pack->end_music == NULL ||
        pack->tipoff_dmc == NULL || pack->gameplay_audio == NULL || pack->whistle_audio == NULL ||
        pack->three_call_audio == NULL || pack->three_score_audio == NULL ||
        pack->score_audio == NULL || pack->cpu_block_audio == NULL ||
        pack->user_block_audio == NULL) {
        dd_asset_pack_unload(pack);
        free(file_data);
        return 0;
    }
    memcpy(pack->ppu, file_data + ppu_entry->offset, (size_t)ppu_entry->size);
    memcpy(pack->dmc, file_data + dmc_entry->offset, (size_t)dmc_entry->size);
    memcpy(pack->oam, file_data + oam_entry->offset, (size_t)oam_entry->size);
    memcpy(pack->intro_ppu, file_data + intro_ppu_entry->offset, (size_t)intro_ppu_entry->size);
    memcpy(pack->intro_oam, file_data + intro_oam_entry->offset, (size_t)intro_oam_entry->size);
    memcpy(pack->intro_updates, file_data + intro_updates_entry->offset, (size_t)intro_updates_entry->size);
    memcpy(pack->intro_music, file_data + intro_music_entry->offset, (size_t)intro_music_entry->size);
    memcpy(pack->select_music, file_data + select_music_entry->offset, (size_t)select_music_entry->size);
    memcpy(pack->config_music, file_data + config_music_entry->offset, (size_t)config_music_entry->size);
    memcpy(pack->config_ppu, file_data + config_ppu_entry->offset, (size_t)config_ppu_entry->size);
    memcpy(pack->config_oam, file_data + config_oam_entry->offset, (size_t)config_oam_entry->size);
    memcpy(pack->config_assets, file_data + config_assets_entry->offset, (size_t)config_assets_entry->size);
    memcpy(pack->tipoff_ppu, file_data + tipoff_ppu_entry->offset, (size_t)tipoff_ppu_entry->size);
    memcpy(pack->tipoff_oam, file_data + tipoff_oam_entry->offset, (size_t)tipoff_oam_entry->size);
    memcpy(pack->tipoff_assets, file_data + tipoff_assets_entry->offset, (size_t)tipoff_assets_entry->size);
    memcpy(pack->end_music, file_data + end_music_entry->offset, (size_t)end_music_entry->size);
    memcpy(pack->tipoff_dmc, file_data + tipoff_dmc_entry->offset, (size_t)tipoff_dmc_entry->size);
    memcpy(pack->gameplay_audio, file_data + gameplay_audio_entry->offset,
           (size_t)gameplay_audio_entry->size);
    memcpy(pack->whistle_audio, file_data + whistle_audio_entry->offset,
           (size_t)whistle_audio_entry->size);
    memcpy(pack->three_call_audio, file_data + three_call_audio_entry->offset,
           (size_t)three_call_audio_entry->size);
    memcpy(pack->three_score_audio, file_data + three_score_audio_entry->offset,
           (size_t)three_score_audio_entry->size);
    memcpy(pack->score_audio, file_data + score_audio_entry->offset,
           (size_t)score_audio_entry->size);
    memcpy(pack->cpu_block_audio, file_data + cpu_block_audio_entry->offset,
           (size_t)cpu_block_audio_entry->size);
    memcpy(pack->user_block_audio, file_data + user_block_audio_entry->offset,
           (size_t)user_block_audio_entry->size);
    pack->ppu_size = (size_t)ppu_entry->size;
    pack->dmc_size = (size_t)dmc_entry->size;
    pack->oam_size = (size_t)oam_entry->size;
    pack->intro_ppu_size = (size_t)intro_ppu_entry->size;
    pack->intro_oam_size = (size_t)intro_oam_entry->size;
    pack->intro_updates_size = (size_t)intro_updates_entry->size;
    pack->intro_music_count = (size_t)intro_music_entry->size / sizeof(DDMusicNote);
    pack->select_music_count = (size_t)select_music_entry->size / sizeof(DDMusicNote);
    pack->config_music_count = (size_t)config_music_entry->size / sizeof(DDMusicNote);
    pack->config_ppu_size = (size_t)config_ppu_entry->size;
    pack->config_oam_size = (size_t)config_oam_entry->size;
    pack->config_assets_size = (size_t)config_assets_entry->size;
    pack->tipoff_ppu_size = (size_t)tipoff_ppu_entry->size;
    pack->tipoff_oam_size = (size_t)tipoff_oam_entry->size;
    pack->tipoff_assets_size = (size_t)tipoff_assets_entry->size;
    pack->end_music_count = (size_t)end_music_entry->size / sizeof(DDMusicNote);
    pack->tipoff_dmc_size = (size_t)tipoff_dmc_entry->size;
    pack->gameplay_audio_count = (size_t)gameplay_audio_entry->size / sizeof(DDMusicNote);
    pack->whistle_audio_count = (size_t)whistle_audio_entry->size / sizeof(DDMusicNote);
    pack->three_call_audio_count = (size_t)three_call_audio_entry->size / sizeof(DDMusicNote);
    pack->three_score_audio_count = (size_t)three_score_audio_entry->size / sizeof(DDMusicNote);
    pack->score_audio_count = (size_t)score_audio_entry->size / sizeof(DDMusicNote);
    pack->cpu_block_audio_count = (size_t)cpu_block_audio_entry->size / sizeof(DDMusicNote);
    pack->user_block_audio_count = (size_t)user_block_audio_entry->size / sizeof(DDMusicNote);
    {
        size_t note_index;
        uint32_t previous_frame = 0u;
        for (note_index = 0u; note_index < pack->intro_music_count; ++note_index) {
            const DDMusicNote *note = &pack->intro_music[note_index];
            if ((note_index != 0u && note->frame < previous_frame) ||
                note->frame >= pack->intro_meta.music_frames || note->period > 0x07FFu ||
                note->channel >= 3u || note->duty > 3u || note->volume > 15u || note->reserved != 0u) {
                dd_asset_pack_unload(pack);
                free(file_data);
                return 0;
            }
            previous_frame = note->frame;
        }
    }
    {
        size_t note_index;
        uint32_t previous_frame = 0u;
        for (note_index = 0u; note_index < pack->whistle_audio_count; ++note_index) {
            const DDMusicNote *note = &pack->whistle_audio[note_index];
            if ((note_index != 0u && note->frame < previous_frame) ||
                note->frame >= pack->tipoff_meta.whistle_audio_frames || note->period > 0x07FFu ||
                note->channel >= 4u || (note->channel == 3u && note->period >= 16u) ||
                note->duty > 3u || note->volume > 15u || note->reserved != 0u) {
                dd_asset_pack_unload(pack);
                free(file_data);
                return 0;
            }
            previous_frame = note->frame;
        }
    }
    {
        const DDMusicNote *streams[2] = {pack->cpu_block_audio, pack->user_block_audio};
        const size_t counts[2] = {pack->cpu_block_audio_count, pack->user_block_audio_count};
        const uint32_t frames[2] = {pack->tipoff_meta.cpu_block_audio_frames,
                                    pack->tipoff_meta.user_block_audio_frames};
        uint32_t stream;
        for (stream = 0u; stream < 2u; ++stream) {
            size_t note_index;
            uint32_t previous_frame = 0u;
            for (note_index = 0u; note_index < counts[stream]; ++note_index) {
                const DDMusicNote *note = &streams[stream][note_index];
                if ((note_index != 0u && note->frame < previous_frame) ||
                    note->frame >= frames[stream] || note->period > 0x07FFu ||
                    note->channel >= 4u || (note->channel == 3u && note->period >= 16u) ||
                    note->duty > 3u || note->volume > 15u || note->reserved != 0u) {
                    dd_asset_pack_unload(pack);
                    free(file_data);
                    return 0;
                }
                previous_frame = note->frame;
            }
        }
    }
    {
        size_t note_index;
        uint32_t previous_frame = 0u;
        for (note_index = 0u; note_index < pack->three_call_audio_count; ++note_index) {
            const DDMusicNote *note = &pack->three_call_audio[note_index];
            if ((note_index != 0u && note->frame < previous_frame) ||
                note->frame >= pack->tipoff_meta.three_call_audio_frames || note->period > 0x07FFu ||
                note->channel >= 2u || note->duty > 3u || note->volume > 15u || note->reserved > 2u) {
                dd_asset_pack_unload(pack);
                free(file_data);
                return 0;
            }
            previous_frame = note->frame;
        }
    }
    {
        size_t note_index;
        uint32_t previous_frame = 0u;
        for (note_index = 0u; note_index < pack->three_score_audio_count; ++note_index) {
            const DDMusicNote *note = &pack->three_score_audio[note_index];
            if ((note_index != 0u && note->frame < previous_frame) ||
                note->frame >= pack->tipoff_meta.three_score_audio_frames || note->period > 0x07FFu ||
                note->channel >= 2u || note->duty > 3u || note->volume > 15u || note->reserved != 0u) {
                dd_asset_pack_unload(pack);
                free(file_data);
                return 0;
            }
            previous_frame = note->frame;
        }
    }
    {
        size_t note_index;
        uint32_t previous_frame = 0u;
        for (note_index = 0u; note_index < pack->score_audio_count; ++note_index) {
            const DDMusicNote *note = &pack->score_audio[note_index];
            if ((note_index != 0u && note->frame < previous_frame) ||
                note->frame >= pack->tipoff_meta.score_audio_frames || note->period > 0x07FFu ||
                note->channel >= 4u || (note->channel == 3u && note->period >= 16u) ||
                note->duty > 3u || note->volume > 15u || note->reserved != 0u) {
                dd_asset_pack_unload(pack);
                free(file_data);
                return 0;
            }
            previous_frame = note->frame;
        }
    }
    {
        const DDTipoffAssetsHeader *assets = (const DDTipoffAssetsHeader *)pack->tipoff_assets;
        uint32_t index;
        if (memcmp(assets->court_chr_left, assets->court_chr_right,
                   sizeof(assets->court_chr_left)) == 0) {
            dd_asset_pack_unload(pack);
            free(file_data);
            return 0;
        }
        for (index = 0u; index < sizeof(assets->cpu_role_targets); ++index) {
            uint8_t target = assets->cpu_role_targets[index];
            if ((target & 0x1Fu) == 0u || (target & 0xE0u) == 0u) {
                dd_asset_pack_unload(pack);
                free(file_data);
                return 0;
            }
        }
        for (index = 0u; index < sizeof(assets->cpu_spacing_targets); ++index) {
            uint8_t target = assets->cpu_spacing_targets[index];
            if ((target & 0x1Fu) == 0u || (target & 0xE0u) == 0u) {
                dd_asset_pack_unload(pack);
                free(file_data);
                return 0;
            }
        }
        for (index = 0u; index < sizeof(assets->cpu_region_targets); ++index) {
            uint8_t target = assets->cpu_region_targets[index];
            if ((target & 0x1Fu) == 0u || (target & 0xE0u) == 0u) {
                dd_asset_pack_unload(pack);
                free(file_data);
                return 0;
            }
        }
        for (index = 0u; index < DD_GAMEPLAY_METASPRITE_COUNT; ++index) {
            if (assets->metasprite_offset[index] < sizeof(*assets) ||
                assets->metasprite_size[index] == 0u ||
                assets->metasprite_offset[index] > pack->tipoff_assets_size ||
                assets->metasprite_size[index] > pack->tipoff_assets_size - assets->metasprite_offset[index]) {
                dd_asset_pack_unload(pack);
                free(file_data);
                return 0;
            }
        }
    }
    {
        size_t note_index;
        uint32_t previous_frame = 0u;
        for (note_index = 0u; note_index < pack->select_music_count; ++note_index) {
            const DDMusicNote *note = &pack->select_music[note_index];
            if ((note_index != 0u && note->frame < previous_frame) ||
                note->frame >= pack->meta.select_music_frames || note->period > 0x07FFu ||
                note->channel >= 3u || note->duty > 3u || note->volume > 15u || note->reserved != 0u) {
                dd_asset_pack_unload(pack);
                free(file_data);
                return 0;
            }
            previous_frame = note->frame;
        }
    }
    {
        const DDConfigAssetsHeader *assets = (const DDConfigAssetsHeader *)pack->config_assets;
        uint32_t index;
        for (index = 0u; index < 15u; ++index) {
            if (assets->metasprite_offset[index] < sizeof(*assets) ||
                assets->metasprite_size[index] == 0u ||
                assets->metasprite_offset[index] > pack->config_assets_size ||
                assets->metasprite_size[index] > pack->config_assets_size - assets->metasprite_offset[index]) {
                dd_asset_pack_unload(pack);
                free(file_data);
                return 0;
            }
        }
    }
    {
        size_t note_index;
        uint32_t previous_frame = 0u;
        for (note_index = 0u; note_index < pack->config_music_count; ++note_index) {
            const DDMusicNote *note = &pack->config_music[note_index];
            if ((note_index != 0u && note->frame < previous_frame) ||
                note->frame >= pack->config_meta.music_loop_frames || note->period > 0x07FFu ||
                note->channel >= 4u || (note->channel == 3u && note->period >= 16u) ||
                note->duty > 3u || note->volume > 15u || note->reserved != 0u) {
                dd_asset_pack_unload(pack);
                free(file_data);
                return 0;
            }
            previous_frame = note->frame;
        }
    }
    {
        size_t note_index;
        uint32_t previous_frame = 0u;
        for (note_index = 0u; note_index < pack->end_music_count; ++note_index) {
            const DDMusicNote *note = &pack->end_music[note_index];
            if ((note_index != 0u && note->frame < previous_frame) ||
                note->frame >= pack->tipoff_meta.end_music_frames || note->period > 0x07FFu ||
                note->channel >= 3u || note->duty > 3u || note->volume > 15u || note->reserved != 0u) {
                dd_asset_pack_unload(pack);
                free(file_data);
                return 0;
            }
            previous_frame = note->frame;
        }
    }
    {
        size_t note_index;
        uint32_t previous_frame = 0u;
        for (note_index = 0u; note_index < pack->gameplay_audio_count; ++note_index) {
            const DDMusicNote *note = &pack->gameplay_audio[note_index];
            if ((note_index != 0u && note->frame < previous_frame) ||
                note->frame >= pack->tipoff_meta.gameplay_audio_frames || note->period > 0x07FFu ||
                note->channel >= 4u || (note->channel == 3u && note->period >= 16u) ||
                note->duty > 3u || note->volume > 15u || note->reserved != 0u) {
                dd_asset_pack_unload(pack);
                free(file_data);
                return 0;
            }
            previous_frame = note->frame;
        }
    }
    free(file_data);
    return 1;
}

void dd_asset_pack_unload(DDAssetPack *pack) {
    free(pack->ppu);
    free(pack->dmc);
    free(pack->oam);
    free(pack->intro_ppu);
    free(pack->intro_oam);
    free(pack->intro_updates);
    free(pack->intro_music);
    free(pack->select_music);
    free(pack->config_music);
    free(pack->config_ppu);
    free(pack->config_oam);
    free(pack->config_assets);
    free(pack->tipoff_ppu);
    free(pack->tipoff_oam);
    free(pack->tipoff_assets);
    free(pack->end_music);
    free(pack->tipoff_dmc);
    free(pack->gameplay_audio);
    free(pack->whistle_audio);
    free(pack->three_call_audio);
    free(pack->three_score_audio);
    free(pack->score_audio);
    free(pack->cpu_block_audio);
    free(pack->user_block_audio);
    memset(pack, 0, sizeof(*pack));
}

int dd_asset_pack_inspect(const char *path) {
    DDAssetPack pack;
    if (!dd_asset_pack_load(path, &pack)) {
        fprintf(stderr, "Invalid asset pack: %s\n", path);
        return 0;
    }
    printf("Valid DDAP v20: %ux%u title, %zu DMC bytes; select has %zu notes, intro has %u updates and %zu music notes; config has %u options and %zu looping music events; tip-off has %u sprites, %u gameplay metasprites, 8 shot poses, 6 four-tile net frames, 20 rebound entries, 60 free-throw bytes, 41 CPU targets, two court CHR streams, %zu END notes, %zu gameplay audio events, %zu whistle events, %zu CPU-block events, %zu user-block events, %zu three-call events, %zu three-score events, %zu basket-score events, and %zu DMC bytes.\n",
           pack.meta.width, pack.meta.height, pack.dmc_size,
           pack.select_music_count, pack.intro_meta.update_count, pack.intro_music_count, pack.config_meta.option_count,
           pack.config_music_count,
           pack.tipoff_meta.sprite_count, DD_GAMEPLAY_METASPRITE_COUNT,
           pack.end_music_count, pack.gameplay_audio_count, pack.whistle_audio_count,
           pack.cpu_block_audio_count, pack.user_block_audio_count,
           pack.three_call_audio_count, pack.three_score_audio_count,
           pack.score_audio_count, pack.tipoff_dmc_size);
    dd_asset_pack_unload(&pack);
    return 1;
}
