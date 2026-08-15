#include "dd_audio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void dd_write_u16(uint8_t *target, uint16_t value) {
    target[0] = (uint8_t)value;
    target[1] = (uint8_t)(value >> 8);
}

static void dd_write_u32(uint8_t *target, uint32_t value) {
    target[0] = (uint8_t)value;
    target[1] = (uint8_t)(value >> 8);
    target[2] = (uint8_t)(value >> 16);
    target[3] = (uint8_t)(value >> 24);
}

int dd_build_title_wav(const DDAssetPack *pack, uint8_t **wav_data, size_t *wav_size) {
    static const uint16_t periods[16] = {428, 380, 340, 320, 286, 254, 226, 214, 190, 160, 142, 128, 106, 85, 72, 54};
    const uint32_t sample_rate = 44100u;
    const double cpu_clock = 1789773.0;
    double dmc_rate;
    uint32_t sample_count;
    uint32_t sample;
    uint8_t level;
    uint8_t *wav;
    if (pack == NULL || pack->dmc == NULL || pack->dmc_size == 0u ||
        pack->meta.dmc_rate_index >= 16u || wav_data == NULL || wav_size == NULL) {
        return 0;
    }
    dmc_rate = cpu_clock / periods[pack->meta.dmc_rate_index];
    sample_count = (uint32_t)(((double)pack->dmc_size * 8.0 * sample_rate) / dmc_rate + 0.5);
    if (sample_count > (UINT32_MAX - 44u) / 2u) {
        return 0;
    }
    wav = (uint8_t *)malloc(44u + (size_t)sample_count * 2u);
    if (wav == NULL) {
        return 0;
    }
    memcpy(wav, "RIFF", 4);
    dd_write_u32(wav + 4, 36u + sample_count * 2u);
    memcpy(wav + 8, "WAVEfmt ", 8);
    dd_write_u32(wav + 16, 16u);
    dd_write_u16(wav + 20, 1u);
    dd_write_u16(wav + 22, 1u);
    dd_write_u32(wav + 24, sample_rate);
    dd_write_u32(wav + 28, sample_rate * 2u);
    dd_write_u16(wav + 32, 2u);
    dd_write_u16(wav + 34, 16u);
    memcpy(wav + 36, "data", 4);
    dd_write_u32(wav + 40, sample_count * 2u);
    level = (uint8_t)pack->meta.dmc_initial_dac;
    for (sample = 0; sample < sample_count; ++sample) {
        uint32_t bit_index = (uint32_t)(((double)sample * dmc_rate) / sample_rate);
        uint8_t bit;
        int16_t pcm;
        if (bit_index >= pack->dmc_size * 8u) bit_index = (uint32_t)(pack->dmc_size * 8u - 1u);
        bit = (uint8_t)((pack->dmc[bit_index >> 3] >> (bit_index & 7u)) & 1u);
        if (bit != 0u) {
            if (level <= 125u) level = (uint8_t)(level + 2u);
        } else if (level >= 2u) {
            level = (uint8_t)(level - 2u);
        }
        pcm = (int16_t)(((int)level - 64) * 480);
        dd_write_u16(wav + 44u + (size_t)sample * 2u, (uint16_t)pcm);
    }
    *wav_data = wav;
    *wav_size = 44u + (size_t)sample_count * 2u;
    return 1;
}

int dd_write_title_wav(const DDAssetPack *pack, const char *path) {
    uint8_t *wav;
    size_t size;
    FILE *file;
    if (!dd_build_title_wav(pack, &wav, &size)) {
        return 0;
    }
    file = fopen(path, "wb");
    if (file == NULL || fwrite(wav, 1, size, file) != size) {
        if (file != NULL) fclose(file);
        free(wav);
        return 0;
    }
    fclose(file);
    free(wav);
    return 1;
}

static int dd_build_music_wav(const DDMusicNote *notes, size_t note_count,
                              uint32_t music_frames, uint32_t pulse_envelope_frames,
                              uint32_t triangle_gate_frames, uint32_t noise_envelope_frames,
                              uint8_t **wav_data, size_t *wav_size) {
    static const uint16_t noise_periods[16] = {
        4u, 8u, 16u, 32u, 64u, 96u, 128u, 160u,
        202u, 254u, 380u, 508u, 762u, 1016u, 2034u, 4068u
    };
    const uint32_t sample_rate = 44100u;
    const double cpu_clock = 1789773.0;
    const double duty_cycles[4] = {0.125, 0.25, 0.5, 0.75};
    uint32_t sample_count;
    uint32_t sample;
    size_t next_note = 0u;
    const DDMusicNote *active[4] = {NULL, NULL, NULL, NULL};
    uint32_t note_start[4] = {0u, 0u, 0u, 0u};
    double phase[4] = {0.0, 0.0, 0.0, 0.0};
    uint16_t noise_shift = 1u;
    uint8_t *wav;
    if (notes == NULL || note_count == 0u || music_frames == 0u ||
        wav_data == NULL || wav_size == NULL) return 0;
    sample_count = (uint32_t)(((uint64_t)music_frames * sample_rate) / 60u);
    if (sample_count > (UINT32_MAX - 44u) / 2u) return 0;
    wav = (uint8_t *)malloc(44u + (size_t)sample_count * 2u);
    if (wav == NULL) return 0;
    memcpy(wav, "RIFF", 4);
    dd_write_u32(wav + 4, 36u + sample_count * 2u);
    memcpy(wav + 8, "WAVEfmt ", 8);
    dd_write_u32(wav + 16, 16u);
    dd_write_u16(wav + 20, 1u);
    dd_write_u16(wav + 22, 1u);
    dd_write_u32(wav + 24, sample_rate);
    dd_write_u32(wav + 28, sample_rate * 2u);
    dd_write_u16(wav + 32, 2u);
    dd_write_u16(wav + 34, 16u);
    memcpy(wav + 36, "data", 4);
    dd_write_u32(wav + 40, sample_count * 2u);
    for (sample = 0; sample < sample_count; ++sample) {
        uint32_t frame = (uint32_t)(((uint64_t)sample * 60u) / sample_rate);
        double mix = 0.0;
        uint32_t channel;
        while (next_note < note_count && notes[next_note].frame <= frame) {
            const DDMusicNote *note = &notes[next_note++];
            if (note->channel >= 4u || note->period > 0x07FFu || note->volume > 15u || note->duty > 3u ||
                (note->channel == 3u && note->period >= 16u)) {
                free(wav);
                return 0;
            }
            active[note->channel] = note->volume == 0u ? NULL : note;
            note_start[note->channel] = note->frame;
            phase[note->channel] = 0.0;
        }
        for (channel = 0; channel < 4u; ++channel) {
            const DDMusicNote *note = active[channel];
            uint16_t period;
            double frequency;
            double wave;
            double amplitude;
            if (note == NULL) continue;
            period = note->period;
            if (note->reserved == 1u) {
                uint32_t age = frame - note_start[channel];
                period = age >= period ? 0u : (uint16_t)(period - age);
            } else if (note->reserved == 2u) {
                uint32_t swept = (uint32_t)period + frame - note_start[channel];
                period = (uint16_t)(swept > 0x07FFu ? 0x07FFu : swept);
            }
            frequency = channel == 3u
                ? cpu_clock / (2.0 * noise_periods[period])
                : cpu_clock / ((channel == 2u ? 32.0 : 16.0) * ((double)period + 1.0));
            phase[channel] += frequency / sample_rate;
            if (channel == 3u) {
                double age = (double)(frame - note_start[channel]);
                double envelope = noise_envelope_frames == 0u ? 1.0 :
                    (age >= noise_envelope_frames ? 0.0 :
                     ((double)noise_envelope_frames - age) / noise_envelope_frames);
                while (phase[channel] >= 1.0) {
                    uint16_t feedback = (uint16_t)((noise_shift ^ (noise_shift >> 1)) & 1u);
                    noise_shift = (uint16_t)((noise_shift >> 1) | (feedback << 14));
                    phase[channel] -= 1.0;
                }
                wave = (noise_shift & 1u) != 0u ? -1.0 : 1.0;
                amplitude = ((double)note->volume / 15.0) * envelope * 0.12;
            } else if (channel == 2u) {
                double age = (double)(frame - note_start[channel]);
                if (phase[channel] >= 1.0) phase[channel] -= (uint32_t)phase[channel];
                wave = phase[channel] < 0.5 ? phase[channel] * 4.0 - 1.0 : 3.0 - phase[channel] * 4.0;
                amplitude = triangle_gate_frames != 0u && age >= triangle_gate_frames ? 0.0 : 0.24;
            } else {
                double age = (double)(frame - note_start[channel]);
                double envelope = pulse_envelope_frames == 0u ? 1.0 :
                    (age >= pulse_envelope_frames
                     ? 0.0 : ((double)pulse_envelope_frames - age) / pulse_envelope_frames);
                if (phase[channel] >= 1.0) phase[channel] -= (uint32_t)phase[channel];
                wave = phase[channel] < duty_cycles[note->duty] ? 1.0 : -1.0;
                amplitude = ((double)note->volume / 15.0) * envelope * 0.20;
            }
            mix += wave * amplitude;
        }
        if (mix > 1.0) mix = 1.0;
        if (mix < -1.0) mix = -1.0;
        dd_write_u16(wav + 44u + (size_t)sample * 2u, (uint16_t)(int16_t)(mix * 28000.0));
    }
    *wav_data = wav;
    *wav_size = 44u + (size_t)sample_count * 2u;
    return 1;
}

int dd_build_intro_music_wav(const DDAssetPack *pack, uint8_t **wav_data, size_t *wav_size) {
    if (pack == NULL) return 0;
    return dd_build_music_wav(pack->intro_music, pack->intro_music_count,
                              pack->intro_meta.music_frames, 20u, 0u, 4u, wav_data, wav_size);
}

int dd_build_select_music_wav(const DDAssetPack *pack, uint8_t **wav_data, size_t *wav_size) {
    if (pack == NULL) return 0;
    return dd_build_music_wav(pack->select_music, pack->select_music_count,
                              pack->meta.select_music_frames, 20u, 0u, 4u, wav_data, wav_size);
}

int dd_build_config_music_wav(const DDAssetPack *pack, uint8_t **wav_data, size_t *wav_size) {
    if (pack == NULL) return 0;
    return dd_build_music_wav(pack->config_music, pack->config_music_count,
                              pack->config_meta.music_loop_frames, 40u, 9u, 4u, wav_data, wav_size);
}

int dd_build_end_music_wav(const DDAssetPack *pack, uint8_t **wav_data, size_t *wav_size) {
    if (pack == NULL) return 0;
    return dd_build_music_wav(pack->end_music, pack->end_music_count,
                              pack->tipoff_meta.end_music_frames, 20u, 0u, 4u, wav_data, wav_size);
}

int dd_build_gameplay_audio_wav(const DDAssetPack *pack, uint8_t **wav_data, size_t *wav_size) {
    if (pack == NULL) return 0;
    return dd_build_music_wav(pack->gameplay_audio, pack->gameplay_audio_count,
                              pack->tipoff_meta.gameplay_audio_frames, 2u, 0u, 4u,
                              wav_data, wav_size);
}

int dd_build_whistle_audio_wav(const DDAssetPack *pack, uint8_t **wav_data, size_t *wav_size) {
    if (pack == NULL) return 0;
    return dd_build_music_wav(pack->whistle_audio, pack->whistle_audio_count,
                              pack->tipoff_meta.whistle_audio_frames, 0u, 0u, 0u,
                              wav_data, wav_size);
}

int dd_build_cpu_block_audio_wav(const DDAssetPack *pack, uint8_t **wav_data, size_t *wav_size) {
    if (pack == NULL) return 0;
    return dd_build_music_wav(pack->cpu_block_audio, pack->cpu_block_audio_count,
                              pack->tipoff_meta.cpu_block_audio_frames, 0u, 0u, 0u,
                              wav_data, wav_size);
}

int dd_build_user_block_audio_wav(const DDAssetPack *pack, uint8_t **wav_data, size_t *wav_size) {
    if (pack == NULL) return 0;
    return dd_build_music_wav(pack->user_block_audio, pack->user_block_audio_count,
                              pack->tipoff_meta.user_block_audio_frames, 0u, 0u, 0u,
                              wav_data, wav_size);
}

int dd_build_three_call_audio_wav(const DDAssetPack *pack, uint8_t **wav_data, size_t *wav_size) {
    if (pack == NULL) return 0;
    return dd_build_music_wav(pack->three_call_audio, pack->three_call_audio_count,
                              pack->tipoff_meta.three_call_audio_frames, 0u, 0u, 0u,
                              wav_data, wav_size);
}

int dd_build_three_score_audio_wav(const DDAssetPack *pack, uint8_t **wav_data, size_t *wav_size) {
    if (pack == NULL) return 0;
    return dd_build_music_wav(pack->three_score_audio, pack->three_score_audio_count,
                              pack->tipoff_meta.three_score_audio_frames, 0u, 0u, 0u,
                              wav_data, wav_size);
}

int dd_build_score_audio_wav(const DDAssetPack *pack, uint8_t **wav_data, size_t *wav_size) {
    if (pack == NULL) return 0;
    return dd_build_music_wav(pack->score_audio, pack->score_audio_count,
                              pack->tipoff_meta.score_audio_frames, 0u, 0u, 0u,
                              wav_data, wav_size);
}

int dd_build_three_basket_score_audio_wav(const DDAssetPack *pack, uint8_t **wav_data,
                                          size_t *wav_size) {
    DDMusicNote *notes;
    size_t score_index = 0u;
    size_t three_index = 0u;
    size_t output_index = 0u;
    int ok;
    if (pack == NULL || wav_data == NULL || wav_size == NULL) return 0;
    notes = (DDMusicNote *)malloc((pack->score_audio_count +
                                   pack->three_score_audio_count) * sizeof(*notes));
    if (notes == NULL) return 0;
    /* `$25` is requested six frames after `$18`.  On equal frames retain the
       basket channel write first, then let `$25` replace the pulse voice just
       as the original bank-1 driver does. */
    while (score_index < pack->score_audio_count ||
           three_index < pack->three_score_audio_count) {
        uint32_t score_frame = score_index < pack->score_audio_count
            ? pack->score_audio[score_index].frame : UINT32_MAX;
        uint32_t three_frame = three_index < pack->three_score_audio_count
            ? pack->three_score_audio[three_index].frame + 6u : UINT32_MAX;
        if (score_frame <= three_frame) {
            notes[output_index++] = pack->score_audio[score_index++];
        } else {
            notes[output_index] = pack->three_score_audio[three_index++];
            notes[output_index++].frame = three_frame;
        }
    }
    ok = dd_build_music_wav(notes, output_index,
                            pack->tipoff_meta.score_audio_frames, 0u, 0u, 0u,
                            wav_data, wav_size);
    free(notes);
    return ok;
}

int dd_build_tipoff_dmc_wav(const DDAssetPack *pack, uint8_t **wav_data, size_t *wav_size) {
    static const uint16_t periods[16] = {428, 380, 340, 320, 286, 254, 226, 214, 190, 160, 142, 128, 106, 85, 72, 54};
    const uint32_t sample_rate = 44100u;
    const double cpu_clock = 1789773.0;
    double dmc_rate;
    uint32_t sample_count;
    uint32_t sample;
    uint8_t level;
    uint8_t *wav;
    if (pack == NULL || pack->tipoff_dmc == NULL || pack->tipoff_dmc_size == 0u ||
        pack->tipoff_meta.dmc_rate_index >= 16u || wav_data == NULL || wav_size == NULL) return 0;
    dmc_rate = cpu_clock / periods[pack->tipoff_meta.dmc_rate_index];
    sample_count = (uint32_t)(((double)pack->tipoff_dmc_size * 8.0 * sample_rate) / dmc_rate + 0.5);
    if (sample_count > (UINT32_MAX - 44u) / 2u) return 0;
    wav = (uint8_t *)malloc(44u + (size_t)sample_count * 2u);
    if (wav == NULL) return 0;
    memcpy(wav, "RIFF", 4);
    dd_write_u32(wav + 4, 36u + sample_count * 2u);
    memcpy(wav + 8, "WAVEfmt ", 8);
    dd_write_u32(wav + 16, 16u);
    dd_write_u16(wav + 20, 1u);
    dd_write_u16(wav + 22, 1u);
    dd_write_u32(wav + 24, sample_rate);
    dd_write_u32(wav + 28, sample_rate * 2u);
    dd_write_u16(wav + 32, 2u);
    dd_write_u16(wav + 34, 16u);
    memcpy(wav + 36, "data", 4);
    dd_write_u32(wav + 40, sample_count * 2u);
    level = (uint8_t)pack->tipoff_meta.dmc_initial_dac;
    for (sample = 0u; sample < sample_count; ++sample) {
        uint32_t bit_index = (uint32_t)(((double)sample * dmc_rate) / sample_rate);
        uint8_t bit;
        int16_t pcm;
        if (bit_index >= pack->tipoff_dmc_size * 8u) bit_index = (uint32_t)(pack->tipoff_dmc_size * 8u - 1u);
        bit = (uint8_t)((pack->tipoff_dmc[bit_index >> 3] >> (bit_index & 7u)) & 1u);
        if (bit != 0u) {
            if (level <= 125u) level = (uint8_t)(level + 2u);
        } else if (level >= 2u) {
            level = (uint8_t)(level - 2u);
        }
        pcm = (int16_t)(((int)level - 64) * 480);
        dd_write_u16(wav + 44u + (size_t)sample * 2u, (uint16_t)pcm);
    }
    *wav_data = wav;
    *wav_size = 44u + (size_t)sample_count * 2u;
    return 1;
}
