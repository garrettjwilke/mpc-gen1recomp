#include "ui_overlay.h"
#include "widgets.h"
#include <stdio.h>
#include <string.h>

static const char *CATEGORY_NAMES[REBIND_CAT_COUNT] = {
    "Movement",
    "Actions",
    "Turbo & Speed",
    "System / Palette",
    "Clear / Unmap"
};

static const uint16_t CAT_MOVEMENT_ITEMS[] = {
    G1_ACT_UP, G1_ACT_DOWN, G1_ACT_LEFT, G1_ACT_RIGHT, G1_ACT_NONE
};
#define NUM_CAT_MOVEMENT (sizeof(CAT_MOVEMENT_ITEMS) / sizeof(CAT_MOVEMENT_ITEMS[0]))

static const uint16_t CAT_ACTION_ITEMS[] = {
    G1_ACT_A, G1_ACT_B, G1_ACT_START, G1_ACT_SELECT, G1_ACT_NONE
};
#define NUM_CAT_ACTIONS (sizeof(CAT_ACTION_ITEMS) / sizeof(CAT_ACTION_ITEMS[0]))

static const uint16_t CAT_TURBO_ITEMS[] = {
    G1_ACT_TURBO, G1_ACT_TURBO_HOLD, G1_ACT_NONE
};
#define NUM_CAT_TURBO (sizeof(CAT_TURBO_ITEMS) / sizeof(CAT_TURBO_ITEMS[0]))

static const uint16_t CAT_SYSTEM_ITEMS[] = {
    G1_ACT_QUICK_SAVE, G1_ACT_QUICK_LOAD, G1_ACT_PALETTE, G1_ACT_SCREEN_MODE, G1_ACT_MENU, G1_ACT_NONE
};
#define NUM_CAT_SYSTEM (sizeof(CAT_SYSTEM_ITEMS) / sizeof(CAT_SYSTEM_ITEMS[0]))

static struct ui_list_state s_game_ls;
static struct ui_list_state s_settings_ls;
static struct ui_list_state s_rom_ls;

void ui_overlay_init(struct ui_overlay *o, const char *bindings_path, const char *settings_path) {
    if (!o) return;
    memset(o, 0, sizeof(*o));
    o->mode = UI_MODE_GAMEPLAY;
    o->prev_mode = UI_MODE_GAMEPLAY;
    o->rebind_selected_pad = 0;
    o->rebind_category = REBIND_CAT_MOVEMENT;
    o->rebind_item_idx = 0;
    if (bindings_path) snprintf(o->bindings_path, sizeof(o->bindings_path), "%s", bindings_path);
    if (settings_path) snprintf(o->settings_path, sizeof(o->settings_path), "%s", settings_path);
}

void ui_overlay_set_mode(struct ui_overlay *o, UiMode mode) {
    if (!o) return;
    if (o->mode != mode) {
        if (mode == UI_MODE_GAME_MENU) s_game_ls.cursor = 0;
        if (mode == UI_MODE_SETTINGS_MENU) s_settings_ls.cursor = 0;
        if (mode == UI_MODE_ROM_PICKER) s_rom_ls.cursor = 0;
    }
    o->prev_mode = o->mode;
    o->mode = mode;
}

UiMode ui_overlay_get_mode(const struct ui_overlay *o) {
    return o ? o->mode : UI_MODE_GAMEPLAY;
}

void ui_overlay_show_toast(struct ui_overlay *o, const char *msg, int64_t now_ms, int duration_ms) {
    if (!o || !msg) return;
    snprintf(o->toast_msg, sizeof(o->toast_msg), "%s", msg);
    o->toast_until_ms = now_ms + ((duration_ms > 0) ? duration_ms : 1800);
}

static void draw_app_button(struct ui *u, int x, const char *label, uint32_t color, int active) {
    ui_box(u, x, 2, 74, 18, (struct ui_paint){
        .fill_set = 1,
        .fill = active ? 0x2C384C : 0x222834,
        .border_set = 1,
        .border = active ? color : 0x3E485C,
        .border_px = active ? 2 : 1
    });
    int tw = (int)strlen(label) * 6;
    int tx = x + (74 - tw) / 2;
    ui_text(u, tx, 5, label, color, 1);
}

static void draw_app_bar(struct ui *u, int active, const char *title) {
    ui_box(u, 0, 0, UI_W, 22, (struct ui_paint){
        .fill_set = 1, .fill = 0x141820, .border_set = 0
    });
    ui_box(u, 0, 22, UI_W, 2, (struct ui_paint){
        .fill_set = 1, .fill = 0x2A3244, .border_set = 0
    });

    draw_app_button(u, 8, "GAME", 0x00E5FF, active == 0);
    draw_app_button(u, 88, "REBIND", 0xFFD600, active == 1);
    draw_app_button(u, 238, "SETTINGS", 0x00E676, active == 2);

    if (title && title[0]) {
        int tw = (int)strlen(title) * 6;
        ui_text(u, (UI_W - tw) / 2, 5, title, 0xE8ECF0, 1);
    }
}

static void draw_toast(struct ui *u, struct ui_overlay *o, int y) {
    if (o->toast_msg[0] && u->now_ms < o->toast_until_ms) {
        int w = 240, h = 22;
        int x = (UI_W - w) / 2;
        ui_box(u, x, y, w, h, (struct ui_paint){
            .fill_set = 1, .fill = 0x1A2230,
            .border_set = 1, .border = 0x00E5FF, .border_px = 1
        });
        int tw = (int)strlen(o->toast_msg) * 6;
        ui_text(u, x + (w - tw) / 2, y + 5, o->toast_msg, 0xFFFFFF, 1);
    }
}

static void draw_dimmed_background(struct ui *u) {
    if (!u || !u->px) return;
    for (int i = 0; i < UI_W * UI_H; i++) {
        uint32_t c = u->px[i];
        uint32_t r = ((c >> 16) & 0xFF) >> 2;
        uint32_t g = ((c >> 8) & 0xFF) >> 2;
        uint32_t b = (c & 0xFF) >> 2;
        u->px[i] = (r << 16) | (g << 8) | b;
    }
}

static int handle_top_buttons(struct ui_overlay *o, const struct input *in) {
    if (in->down & (1u << IN_B1)) {
        ui_overlay_set_mode(o, o->mode == UI_MODE_GAME_MENU ? UI_MODE_GAMEPLAY : UI_MODE_GAME_MENU);
        return 1;
    }
    if (in->down & (1u << IN_B2)) {
        ui_overlay_set_mode(o, o->mode == UI_MODE_REBIND ? UI_MODE_GAMEPLAY : UI_MODE_REBIND);
        return 1;
    }
    if (in->down & (1u << IN_B3)) {
        ui_overlay_set_mode(o, o->mode == UI_MODE_SETTINGS_MENU ? UI_MODE_GAMEPLAY : UI_MODE_SETTINGS_MENU);
        return 1;
    }
    return 0;
}

static struct ui_list_style menu_style(void) {
    struct ui_list_style st = {
        .text = 0xD0D8E8,
        .highlight = 0x00E5FF,
        .row_h = 20,
        .highlight_px = 2,
        .current_fill_set = 1,
        .current_fill = 0x1E2638
    };
    return st;
}

/* -------------------------------------------------------------------------
 * GAMEPLAY MODE
 * ------------------------------------------------------------------------- */
static int draw_gameplay_mode(struct ui_overlay *o, struct ui *u, const struct input *in,
                              struct g1_runner *runner, struct input_map *map,
                              struct app_settings *settings, int ticks) {
    if (handle_top_buttons(o, in)) return 0;

    struct g1_input_state g1_in;
    input_map_translate(map, in, false, &g1_in);

    if (g1_in.menu_trigger) {
        ui_overlay_set_mode(o, UI_MODE_GAME_MENU);
        return 0;
    }

    if (g1_in.palette_cycles > 0) {
        settings->palette = (settings->palette + g1_in.palette_cycles) % 4;
        static const char *pal_names[4] = { "GBC Color", "Super Game Boy", "DMG Green", "Pocket Gray" };
        ui_overlay_show_toast(o, pal_names[settings->palette], u->now_ms, 1200);
    }

    if (g1_in.screen_mode_cycles > 0) {
        settings->screen_mode = (settings->screen_mode + g1_in.screen_mode_cycles) % 3;
        static const char *scr_names[3] = { "1x Centered", "1.5x Letterbox", "Stretched" };
        ui_overlay_show_toast(o, scr_names[settings->screen_mode], u->now_ms, 1200);
    }

    if (g1_in.quick_save_trigger) {
        if (runner) g1_runner_save_sram(runner);
        ui_overlay_show_toast(o, "Game Saved", u->now_ms, 1200);
    }
    if (g1_in.quick_load_trigger) {
        ui_overlay_show_toast(o, "Quick State Loaded", u->now_ms, 1200);
    }

    /* Advance runner ONLY for the number of ticks due */
    if (runner && ticks > 0) {
        for (int i = 0; i < ticks; i++) {
            g1_runner_tick(runner, &g1_in);
        }
        g1_runner_render(runner, u->px, settings->screen_mode, settings->palette);
    }

    draw_toast(u, o, 212);
    return 0;
}

/* -------------------------------------------------------------------------
 * GAME MENU (B1)
 * ------------------------------------------------------------------------- */
static int draw_game_menu(struct ui_overlay *o, struct ui *u, const struct input *in,
                          struct g1_runner *runner, struct input_map *map,
                          struct app_settings *settings, struct rom_manager *rm) {
    if (handle_top_buttons(o, in)) return 0;

    draw_dimmed_background(u);
    draw_app_bar(u, 0, rom_manager_game_name(rm->selected_game));

    char preset_row[48];
    char bank_row[36];
    char pal_row[36];
    char scr_row[36];
    char game_row[40];

    snprintf(preset_row, sizeof(preset_row), "Preset: %s", input_map_preset_name(map->current_preset));
    snprintf(bank_row, sizeof(bank_row), "Pad Bank: %s", (input_map_get_bank(map) == 0) ? "Bank A (1-16)" : "Bank B (17-32)");
    static const char *pal_names[4] = { "GBC Color", "Super Game Boy", "DMG Green", "Pocket Gray" };
    snprintf(pal_row, sizeof(pal_row), "Palette: %s", pal_names[settings->palette % 4]);
    static const char *scr_names[3] = { "1x Centered", "1.5x Letterbox", "Stretched" };
    snprintf(scr_row, sizeof(scr_row), "Scaling: %s", scr_names[settings->screen_mode % 3]);
    snprintf(game_row, sizeof(game_row), "Select Game: %s", rom_manager_game_name(rm->selected_game));

    const char *rows[] = {
        "Resume Game",
        "Rebind Controls",
        game_row,
        preset_row,
        bank_row,
        pal_row,
        scr_row,
        "Restart Game",
        "Quit to System"
    };
    int n = 9;
    int x = 32, y = 30, w = UI_W - 64, h = 6 + n * 20;

    ui_box(u, x, y, w, h, (struct ui_paint){
        .fill_set = 1, .fill = 0x10141E,
        .border_set = 1, .border = 0x2A3448, .border_px = 1
    });

    int chosen = ui_list(u, x + 4, y + 3, w - 8, n * 20, rows, n, menu_style(), in, &s_game_ls);
    if (chosen == 0) {
        ui_overlay_set_mode(o, UI_MODE_GAMEPLAY);
    } else if (chosen == 1) {
        ui_overlay_set_mode(o, UI_MODE_REBIND);
    } else if (chosen == 2) {
        ui_overlay_set_mode(o, UI_MODE_ROM_PICKER);
    } else if (chosen == 3) {
        enum input_preset_id next_p = (enum input_preset_id)((map->current_preset + 1) % PRESET_COUNT);
        input_map_apply_preset(map, next_p);
        ui_overlay_show_toast(o, input_map_preset_name(next_p), u->now_ms, 1200);
    } else if (chosen == 4) {
        input_map_toggle_bank(map);
        ui_overlay_show_toast(o, (input_map_get_bank(map) == 0) ? "Bank A (1-16)" : "Bank B (17-32)", u->now_ms, 1200);
    } else if (chosen == 5) {
        settings->palette = (settings->palette + 1) % 4;
        ui_overlay_show_toast(o, pal_names[settings->palette], u->now_ms, 1200);
    } else if (chosen == 6) {
        settings->screen_mode = (settings->screen_mode + 1) % 3;
        ui_overlay_show_toast(o, scr_names[settings->screen_mode], u->now_ms, 1200);
    } else if (chosen == 7) {
        if (runner) g1_runner_reset(runner);
        ui_overlay_show_toast(o, "Game Restarted", u->now_ms, 1400);
        ui_overlay_set_mode(o, UI_MODE_GAMEPLAY);
    } else if (chosen == 8) {
        return 1;
    }

    draw_toast(u, o, 214);
    return 0;
}

/* -------------------------------------------------------------------------
 * REBIND CONTROLS MODAL (B2) - 4x4 Visual Pad Grid
 * ------------------------------------------------------------------------- */
static uint16_t current_rebind_action(int category, int item_idx) {
    if (item_idx < 0) item_idx = 0;
    switch (category) {
        case REBIND_CAT_MOVEMENT:
            return CAT_MOVEMENT_ITEMS[item_idx % NUM_CAT_MOVEMENT];
        case REBIND_CAT_ACTIONS:
            return CAT_ACTION_ITEMS[item_idx % NUM_CAT_ACTIONS];
        case REBIND_CAT_TURBO:
            return CAT_TURBO_ITEMS[item_idx % NUM_CAT_TURBO];
        case REBIND_CAT_SYSTEM:
            return CAT_SYSTEM_ITEMS[item_idx % NUM_CAT_SYSTEM];
        case REBIND_CAT_CLEAR:
        default:
            return G1_ACT_NONE;
    }
}

static int draw_rebind_mode(struct ui_overlay *o, struct ui *u, const struct input *in,
                            struct input_map *map, struct app_settings *settings) {
    if (in->down & (1u << IN_B1)) {
        ui_overlay_set_mode(o, UI_MODE_GAMEPLAY);
        return 0;
    }
    if (in->down & (1u << IN_B3)) {
        ui_overlay_set_mode(o, UI_MODE_SETTINGS_MENU);
        return 0;
    }

    draw_dimmed_background(u);
    draw_app_bar(u, 1, "PAD REBIND");

    /* Hitting any pad directly selects that pad slot */
    for (int p = 0; p < 16; p++) {
        if (in->pad_hit[p] >= 0) {
            o->rebind_selected_pad = p;
            break;
        }
    }

    /* Encoder or navigation changes category / action */
    if (in->encoder != 0) {
        o->rebind_item_idx += in->encoder;
        if (o->rebind_item_idx < 0) o->rebind_item_idx = 0;
    }
    if (in->down & (1u << IN_PLUS))  o->rebind_category = (o->rebind_category + 1) % REBIND_CAT_COUNT;
    if (in->down & (1u << IN_MINUS)) o->rebind_category = (o->rebind_category + REBIND_CAT_COUNT - 1) % REBIND_CAT_COUNT;

    /* Encoder press binds selected action */
    if (in->encoder_press) {
        uint16_t act = current_rebind_action(o->rebind_category, o->rebind_item_idx);
        int slot = map->active_bank * PAD_BANK_SIZE + o->rebind_selected_pad;
        if (act == G1_ACT_NONE) {
            input_map_clear_binding(map, slot);
            ui_overlay_show_toast(o, "Pad Cleared", u->now_ms, 1000);
        } else {
            input_map_set_binding(map, slot, BIND_TYPE_ACTION, act, 0, NULL);
            char toast[48];
            snprintf(toast, sizeof(toast), "Bound: %s", input_map_action_short_name(act));
            ui_overlay_show_toast(o, toast, u->now_ms, 1200);
        }
    }

    /* Draw 4x4 Pad Grid on the Left (136x136) */
    int grid_x = 16, grid_y = 32;
    int cell_w = 28, cell_h = 24, gap = 4;
    int base_slot = map->active_bank * PAD_BANK_SIZE;

    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            /* Pads visually: Row 0 is Top (12..15), Row 3 is Bottom (0..3) */
            int pad_idx = (3 - row) * 4 + col;
            int px = grid_x + col * (cell_w + gap);
            int py = grid_y + row * (cell_h + gap);
            int is_sel = (pad_idx == o->rebind_selected_pad);

            struct pad_binding *b = &map->pads[base_slot + pad_idx];
            uint32_t border_col = is_sel ? 0x00E5FF : 0x3E485C;

            ui_box(u, px, py, cell_w, cell_h, (struct ui_paint){
                .fill_set = 1, .fill = is_sel ? 0x2A3448 : 0x161C26,
                .border_set = 1, .border = border_col, .border_px = is_sel ? 2 : 1
            });

            /* Draw color dot and label */
            uint32_t dot_color = (b->type != BIND_TYPE_NONE) ? b->color : COLOR_DIM;
            ui_box(u, px + 2, py + 2, 4, 4, (struct ui_paint){
                .fill_set = 1, .fill = dot_color, .border_set = 0
            });

            int tw = (int)strlen(b->label) * 6;
            ui_text(u, px + (cell_w - tw) / 2, py + 7, b->label, is_sel ? 0xFFFFFF : 0xC0C8D8, 1);
        }
    }

    /* Right Panel (Inspector & Selector) */
    int rx = 152, ry = 32, rw = UI_W - rx - 16;
    ui_box(u, rx, ry, rw, 136, (struct ui_paint){
        .fill_set = 1, .fill = 0x10141E, .border_set = 1, .border = 0x2A3448, .border_px = 1
    });

    char slot_hdr[40];
    snprintf(slot_hdr, sizeof(slot_hdr), "Pad %d (%s)", o->rebind_selected_pad + 1,
             map->active_bank == 0 ? "Bank A" : "Bank B");
    ui_text(u, rx + 10, ry + 8, slot_hdr, 0x00E5FF, 1);

    /* Category bar */
    char cat_txt[40];
    snprintf(cat_txt, sizeof(cat_txt), "< %s >", CATEGORY_NAMES[o->rebind_category]);
    ui_text(u, rx + 10, ry + 28, cat_txt, 0xFFD600, 1);

    /* Current Action Candidate */
    uint16_t cand_act = current_rebind_action(o->rebind_category, o->rebind_item_idx);
    ui_text(u, rx + 10, ry + 50, "Action:", 0x718096, 1);
    ui_text(u, rx + 10, ry + 66, input_map_action_full_name(cand_act), 0xFFFFFF, 1);

    /* Instructions */
    ui_text(u, rx + 10, ry + 94, "Push Enc: Bind Action", 0x00E676, 1);
    ui_text(u, rx + 10, ry + 112, "Bank Btn: Switch Bank", 0xA0AEC0, 1);

    /* Bottom save shortcut */
    if (in->down & (1u << IN_ERASE)) {
        input_map_clear_binding(map, base_slot + o->rebind_selected_pad);
        ui_overlay_show_toast(o, "Pad Cleared", u->now_ms, 1200);
    }

    (void)settings;
    draw_toast(u, o, 184);
    return 0;
}

/* -------------------------------------------------------------------------
 * SETTINGS MENU (B3)
 * ------------------------------------------------------------------------- */
static int draw_settings_menu(struct ui_overlay *o, struct ui *u, const struct input *in,
                              struct input_map *map, struct app_settings *settings,
                              struct rom_manager *rm) {
    if (handle_top_buttons(o, in)) return 0;

    draw_dimmed_background(u);
    draw_app_bar(u, 2, "SETTINGS");

    char vol_row[40];
    char spk_row[40];
    char spd_row[40];
    snprintf(vol_row, sizeof(vol_row), "Volume: %d%%", settings ? settings->volume : 80);
    snprintf(spk_row, sizeof(spk_row), "Internal Speaker: %s", (settings && settings->speaker_on) ? "On" : "Muted");
    snprintf(spd_row, sizeof(spd_row), "Turbo Speed: %dx", settings ? settings->turbo_speed : 3);

    const char *rows[] = {
        UI_USB_SD_LABEL,
        vol_row,
        spk_row,
        spd_row,
        "Save Bindings & Settings",
        "Reset Default Controls",
        "Back to Game"
    };
    int n = 7;
    int x = 32, y = 34, w = UI_W - 64, h = 6 + n * 20;

    ui_box(u, x, y, w, h, (struct ui_paint){
        .fill_set = 1, .fill = 0x10141E,
        .border_set = 1, .border = 0x2A3448, .border_px = 1
    });

    int chosen = ui_list(u, x + 4, y + 3, w - 8, n * 20, rows, n, menu_style(), in, &s_settings_ls);
    if (chosen == 0) {
        /* USB SD Mount Mode */
        rom_manager_start_usb(rm);
        ui_overlay_set_mode(o, UI_MODE_USB_STORAGE);
    } else if (chosen == 1 && settings) {
        settings->volume = (settings->volume + 10 > 100) ? 0 : settings->volume + 10;
        if (o->settings_path[0]) config_save_settings(o->settings_path, settings);
    } else if (chosen == 2 && settings) {
        settings->speaker_on = !settings->speaker_on;
        if (o->settings_path[0]) config_save_settings(o->settings_path, settings);
        ui_overlay_show_toast(o, settings->speaker_on ? "Speaker Enabled" : "Speaker Muted", u->now_ms, 1200);
    } else if (chosen == 3 && settings) {
        settings->turbo_speed = (settings->turbo_speed >= 4) ? 2 : settings->turbo_speed + 1;
        if (o->settings_path[0]) config_save_settings(o->settings_path, settings);
    } else if (chosen == 4) {
        bool ok_b = o->bindings_path[0] ? config_save_bindings(o->bindings_path, map) : false;
        bool ok_s = (o->settings_path[0] && settings) ? config_save_settings(o->settings_path, settings) : false;
        ui_overlay_show_toast(o, (ok_b || ok_s) ? "Saved Config to SD!" : "Save Failed", u->now_ms, 1800);
    } else if (chosen == 5) {
        input_map_apply_preset(map, PRESET_ARCADE);
        ui_overlay_show_toast(o, "Reset to Default Controls", u->now_ms, 1400);
    } else if (chosen == 6) {
        ui_overlay_set_mode(o, UI_MODE_GAMEPLAY);
    }

    draw_toast(u, o, 198);
    return 0;
}

/* -------------------------------------------------------------------------
 * ROM PICKER MODAL
 * ------------------------------------------------------------------------- */
static int draw_rom_picker(struct ui_overlay *o, struct ui *u, const struct input *in,
                           struct g1_runner *runner, struct rom_manager *rm) {
    if (handle_top_buttons(o, in)) return 0;

    draw_dimmed_background(u);
    draw_app_bar(u, 0, "SELECT POKEMON GAME");

    char r_row[64], b_row[64], y_row[64];
    snprintf(r_row, sizeof(r_row), "Pokemon Red    [%s]",
             rm->games[G1_GAME_RED].rom_found ? "READY" : "MISSING");
    snprintf(b_row, sizeof(b_row), "Pokemon Blue   [%s]",
             rm->games[G1_GAME_BLUE].rom_found ? "READY" : "MISSING");
    snprintf(y_row, sizeof(y_row), "Pokemon Yellow [%s]",
             rm->games[G1_GAME_YELLOW].rom_found ? "READY" : "MISSING");

    bool has_running = (runner && g1_runner_is_ready(runner));
    const char *rows[] = {
        r_row,
        b_row,
        y_row,
        "USB SD Card Mode (Transfer ROMs)",
        has_running ? "Resume Current Game" : "Quit to System"
    };
    int n = 5;
    int x = 20, y = 34, w = UI_W - 40, h = 6 + n * 22;

    ui_box(u, x, y, w, h, (struct ui_paint){
        .fill_set = 1, .fill = 0x10141E,
        .border_set = 1, .border = 0x2A3448, .border_px = 1
    });

    int chosen = ui_list(u, x + 4, y + 3, w - 8, n * 22, rows, n, menu_style(), in, &s_rom_ls);
    if (chosen >= 0 && chosen < G1_GAME_COUNT) {
        enum g1_game_id g = (enum g1_game_id)chosen;
        if (rm->games[g].rom_found) {
            rm->selected_game = g;
            if (runner) {
                g1_runner_set_game(runner, g);
                g1_runner_load_rom(runner, rm->games[g].rom_path);
            }
            ui_overlay_show_toast(o, rom_manager_game_name(g), u->now_ms, 1200);
            ui_overlay_set_mode(o, UI_MODE_GAMEPLAY);
        } else {
            ui_overlay_show_toast(o, "ROM Missing! Use USB SD Mode to add.", u->now_ms, 2000);
        }
    } else if (chosen == 3) {
        rom_manager_ensure_roms_dir();
        rom_manager_start_usb(rm);
        ui_overlay_set_mode(o, UI_MODE_USB_STORAGE);
    } else if (chosen == 4) {
        if (has_running) {
            ui_overlay_set_mode(o, UI_MODE_GAMEPLAY);
        } else {
            return 1; /* Clean exit back to system / boot menu */
        }
    }

    /* Helpful footer text */
    ui_text(u, 24, y + h + 10, "SD Card ROM folder: /media/MPC/roms/", 0x8898B0, 1);
    ui_text(u, 24, y + h + 24, "Put: pokemon_red.gb, blue.gb, yellow.gbc", 0x607088, 1);

    draw_toast(u, o, 186);
    return 0;
}

/* -------------------------------------------------------------------------
 * MAIN DISPATCHER
 * ------------------------------------------------------------------------- */
int ui_overlay_draw(struct ui_overlay *o,
                    struct ui *u,
                    const struct input *in,
                    struct g1_runner *runner,
                    struct input_map *map,
                    struct app_settings *settings,
                    struct rom_manager *rm,
                    int ticks) {
    if (!o || !u) return 0;

    switch (o->mode) {
        case UI_MODE_GAMEPLAY:
            return draw_gameplay_mode(o, u, in, runner, map, settings, ticks);

        case UI_MODE_GAME_MENU:
            return draw_game_menu(o, u, in, runner, map, settings, rm);

        case UI_MODE_REBIND:
            return draw_rebind_mode(o, u, in, map, settings);

        case UI_MODE_SETTINGS_MENU:
            return draw_settings_menu(o, u, in, map, settings, rm);

        case UI_MODE_ROM_PICKER:
            return draw_rom_picker(o, u, in, runner, rm);

        case UI_MODE_USB_STORAGE: {
            int rc = rom_manager_draw_usb(rm, u, in);
            if (rc == UI_USB_SD_DONE || rc == UI_USB_SD_FAILED) {
                rom_manager_ensure_roms_dir();
                rom_manager_scan(rm, NULL);
                ui_overlay_set_mode(o, UI_MODE_ROM_PICKER);
            }
            return 0;
        }

        case UI_MODE_EXTRACTION: {
            rom_manager_step_extraction(rm, u->now_ms);
            rom_manager_draw_extraction(rm, u);
            if (rm->extract.finished) {
                rm->selected_game = rm->extract.game_id;
                if (runner) {
                    g1_runner_set_game(runner, rm->extract.game_id);
                    g1_runner_load_assets(runner, rm->games[rm->extract.game_id].rom_path);
                }
                ui_overlay_show_toast(o, "Extraction Finished!", u->now_ms, 1500);
                ui_overlay_set_mode(o, UI_MODE_GAMEPLAY);
            }
            return 0;
        }

        default:
            return 0;
    }
}

