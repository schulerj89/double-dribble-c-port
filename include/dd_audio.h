#ifndef DD_AUDIO_H
#define DD_AUDIO_H

#include <stddef.h>
#include <stdint.h>

#include "dd_asset_pack.h"

int dd_build_title_wav(const DDAssetPack *pack, uint8_t **wav_data, size_t *wav_size);
int dd_write_title_wav(const DDAssetPack *pack, const char *path);

#endif

