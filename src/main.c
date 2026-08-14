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
    puts("  --dump-title-wav <input.assetpack> <output.wav>");
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
    if (argc == 4 && strcmp(argv[1], "--dump-title-wav") == 0) {
        int ok;
        if (!dd_asset_pack_load(argv[2], &pack)) return 1;
        ok = dd_write_title_wav(&pack, argv[3]);
        dd_asset_pack_unload(&pack);
        return ok ? 0 : 1;
    }
    dd_usage();
    return 2;
}

