#ifndef DD_RENDERER_H
#define DD_RENDERER_H

#include <stdint.h>

#include "dd_asset_pack.h"

int dd_render_title(const DDAssetPack *pack, uint32_t *pixels, uint32_t width, uint32_t height);
int dd_write_bmp(const char *path, const uint32_t *pixels, uint32_t width, uint32_t height);

#endif

