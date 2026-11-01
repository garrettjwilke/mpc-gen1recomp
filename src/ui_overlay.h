#ifndef MPC_G1_UI_OVERLAY_H
#define MPC_G1_UI_OVERLAY_H

#include "input.h"
#include "ui.h"
#include "input_map.h"
#include "config.h"
#include "rom_manager.h"
#include "g1_runner.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    UI_MODE_GAMEPLAY = 0,
    UI_MODE_GAME_MENU,
    UI_MODE_REBIND,
    UI_MODE_SETTINGS_MENU,
    UI_MODE_ROM_PICKER,
    UI_MODE_USB_STORAGE,
    UI_MODE_EXTRACTION
} UiMode;

enum rebind_category {
    REBIND_CAT_MOVEMENT = 0,
    REBIND_CAT_ACTIONS,
    REBIND_CAT_TURBO,
    REBIND_CAT_SYSTEM,
    REBIND_CAT_CLEAR,
    REBIND_CAT_COUNT
};

struct ui_overlay {
    UiMode mode;
    UiMode prev_mode;

    /* Rebind modal state */
    int rebind_selected_pad;    /* 0..15 */
    int rebind_category;        /* enum rebind_category */
    int rebind_item_idx;

    /* Toast state */
    char toast_msg[128];
    int64_t toast_until_ms;

    /* Config paths */
    char bindings_path[256];
    char settings_path[256];
};

void ui_overlay_init(struct ui_overlay *o, const char *bindings_path, const char *settings_path);
void ui_overlay_set_mode(struct ui_overlay *o, UiMode mode);
UiMode ui_overlay_get_mode(const struct ui_overlay *o);

void ui_overlay_show_toast(struct ui_overlay *o, const char *msg, int64_t now_ms, int duration_ms);

/* Main render & event dispatcher for overlay frame. Returns 1 to quit app. */
int ui_overlay_draw(struct ui_overlay *o,
                    struct ui *u,
                    const struct input *in,
                    struct g1_runner *runner,
                    struct input_map *map,
                    struct app_settings *settings,
                    struct rom_manager *rm,
                    int ticks);

#endif /* MPC_G1_UI_OVERLAY_H */

