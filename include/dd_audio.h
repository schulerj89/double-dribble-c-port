#ifndef DD_AUDIO_H
#define DD_AUDIO_H

#include <stddef.h>
#include <stdint.h>

#include "dd_asset_pack.h"

int dd_build_title_wav(const DDAssetPack *pack, uint8_t **wav_data, size_t *wav_size);
int dd_write_title_wav(const DDAssetPack *pack, const char *path);
int dd_build_intro_music_wav(const DDAssetPack *pack, uint8_t **wav_data, size_t *wav_size);
int dd_build_select_music_wav(const DDAssetPack *pack, uint8_t **wav_data, size_t *wav_size);
int dd_build_config_music_wav(const DDAssetPack *pack, uint8_t **wav_data, size_t *wav_size);
int dd_build_end_music_wav(const DDAssetPack *pack, uint8_t **wav_data, size_t *wav_size);
int dd_build_gameplay_audio_wav(const DDAssetPack *pack, uint8_t **wav_data, size_t *wav_size);
int dd_build_whistle_audio_wav(const DDAssetPack *pack, uint8_t **wav_data, size_t *wav_size);
int dd_build_three_call_audio_wav(const DDAssetPack *pack, uint8_t **wav_data, size_t *wav_size);
int dd_build_three_score_audio_wav(const DDAssetPack *pack, uint8_t **wav_data, size_t *wav_size);
int dd_build_score_audio_wav(const DDAssetPack *pack, uint8_t **wav_data, size_t *wav_size);
int dd_build_three_basket_score_audio_wav(const DDAssetPack *pack, uint8_t **wav_data,
                                          size_t *wav_size);
int dd_build_tipoff_dmc_wav(const DDAssetPack *pack, uint8_t **wav_data, size_t *wav_size);

#endif
