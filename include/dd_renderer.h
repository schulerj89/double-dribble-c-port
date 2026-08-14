#ifndef DD_RENDERER_H
#define DD_RENDERER_H

#include <stdint.h>

#include "dd_asset_pack.h"
#include "dd_gameplay.h"

typedef struct DDConfigView {
    uint32_t selection;
    uint32_t time_index;
    uint32_t team_index;
    uint32_t level_index;
    uint32_t action_row;
    uint32_t action_frame;
    int action_active;
} DDConfigView;

int dd_render_title(const DDAssetPack *pack, uint32_t *pixels, uint32_t width, uint32_t height);
int dd_render_title_selection(const DDAssetPack *pack, uint32_t selection, int selection_visible,
                              uint32_t *pixels, uint32_t width, uint32_t height);
int dd_render_intro(const DDAssetPack *pack, uint32_t intro_frame,
                    uint32_t *pixels, uint32_t width, uint32_t height);
int dd_render_config(const DDAssetPack *pack, uint32_t selection,
                     uint32_t *pixels, uint32_t width, uint32_t height);
int dd_render_config_view(const DDAssetPack *pack, const DDConfigView *view,
                          uint32_t *pixels, uint32_t width, uint32_t height);
int dd_render_tipoff(const DDAssetPack *pack, uint32_t *pixels, uint32_t width, uint32_t height);
int dd_render_gameplay(const DDAssetPack *pack, const DDGameplayState *state,
                       uint32_t *pixels, uint32_t width, uint32_t height);
int dd_config_action_status(const DDAssetPack *pack, uint32_t row, uint32_t frame,
                            int *setting_applied, int *complete);
int dd_write_bmp(const char *path, const uint32_t *pixels, uint32_t width, uint32_t height);

#endif
