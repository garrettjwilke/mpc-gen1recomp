#include "input_map.h"
#include <stdio.h>
#include <string.h>

static const char *s_preset_names[PRESET_COUNT] = {
    "Arcade (Default)",
    "Ergonomic (2-Hand)",
    "Game Boy Classic",
    "Speedrunner"
};

const char *input_map_preset_name(enum input_preset_id preset) {
    if (preset < 0 || preset >= PRESET_COUNT) return "Custom";
    return s_preset_names[preset];
}

void input_map_set_bank(struct input_map *map, int bank) {
    if (!map) return;
    map->active_bank = (bank != 0) ? 1 : 0;
}

int input_map_get_bank(const struct input_map *map) {
    return map ? map->active_bank : 0;
}

void input_map_toggle_bank(struct input_map *map) {
    if (!map) return;
    map->active_bank = (map->active_bank == 0) ? 1 : 0;
}

const char *input_map_action_short_name(uint16_t code) {
    switch (code) {
        case G1_ACT_UP:          return "UP";
        case G1_ACT_DOWN:        return "DOWN";
        case G1_ACT_LEFT:        return "LEFT";
        case G1_ACT_RIGHT:       return "RGHT";
        case G1_ACT_A:           return "A";
        case G1_ACT_B:           return "B";
        case G1_ACT_START:       return "STRT";
        case G1_ACT_SELECT:      return "SELC";
        case G1_ACT_TURBO:       return "TRBO";
        case G1_ACT_TURBO_HOLD:  return "HOLD";
        case G1_ACT_QUICK_SAVE:  return "SAVE";
        case G1_ACT_QUICK_LOAD:  return "LOAD";
        case G1_ACT_PALETTE:     return "PALT";
        case G1_ACT_SCREEN_MODE: return "SCRN";
        case G1_ACT_MENU:        return "MENU";
        default:                 return "-";
    }
}

const char *input_map_action_full_name(uint16_t code) {
    switch (code) {
        case G1_ACT_UP:          return "D-Pad Up";
        case G1_ACT_DOWN:        return "D-Pad Down";
        case G1_ACT_LEFT:        return "D-Pad Left";
        case G1_ACT_RIGHT:       return "D-Pad Right";
        case G1_ACT_A:           return "Button A (Confirm / Talk)";
        case G1_ACT_B:           return "Button B (Cancel / Back)";
        case G1_ACT_START:       return "Start (In-Game Menu)";
        case G1_ACT_SELECT:      return "Select (Bag / Item Move)";
        case G1_ACT_TURBO:       return "Fast-Forward Turbo (Toggle)";
        case G1_ACT_TURBO_HOLD:  return "Fast-Forward (Hold)";
        case G1_ACT_QUICK_SAVE:  return "Quick Save State";
        case G1_ACT_QUICK_LOAD:  return "Quick Load State";
        case G1_ACT_PALETTE:     return "Cycle Color Palette";
        case G1_ACT_SCREEN_MODE: return "Cycle Screen Scaling";
        case G1_ACT_MENU:        return "Open MPC Menu Overlay";
        default:                 return "None (Unmap)";
    }
}

uint16_t input_map_action_to_mask(uint16_t code) {
    switch (code) {
        case G1_ACT_UP:          return G1_BTN_UP;
        case G1_ACT_DOWN:        return G1_BTN_DOWN;
        case G1_ACT_LEFT:        return G1_BTN_LEFT;
        case G1_ACT_RIGHT:       return G1_BTN_RIGHT;
        case G1_ACT_A:           return G1_BTN_A;
        case G1_ACT_B:           return G1_BTN_B;
        case G1_ACT_START:       return G1_BTN_START;
        case G1_ACT_SELECT:      return G1_BTN_SELECT;
        case G1_ACT_TURBO:
        case G1_ACT_TURBO_HOLD:  return G1_BTN_TURBO;
        case G1_ACT_QUICK_SAVE:  return G1_BTN_QUICK_SAVE;
        case G1_ACT_QUICK_LOAD:  return G1_BTN_QUICK_LOAD;
        case G1_ACT_PALETTE:     return G1_BTN_PALETTE;
        case G1_ACT_SCREEN_MODE: return G1_BTN_SCREEN_MODE;
        case G1_ACT_MENU:        return G1_BTN_MENU;
        default:                 return 0;
    }
}

uint32_t input_map_default_color(uint16_t code) {
    switch (code) {
        case G1_ACT_UP:
        case G1_ACT_DOWN:
        case G1_ACT_LEFT:
        case G1_ACT_RIGHT:
            return COLOR_BLUE;
        case G1_ACT_A:
            return COLOR_GREEN;
        case G1_ACT_B:
            return COLOR_RED;
        case G1_ACT_START:
            return COLOR_AMBER;
        case G1_ACT_SELECT:
            return COLOR_YELLOW;
        case G1_ACT_TURBO:
        case G1_ACT_TURBO_HOLD:
            return COLOR_PURPLE;
        case G1_ACT_QUICK_SAVE:
        case G1_ACT_QUICK_LOAD:
            return COLOR_PINK;
        case G1_ACT_PALETTE:
            return COLOR_TEAL;
        case G1_ACT_SCREEN_MODE:
        case G1_ACT_MENU:
            return COLOR_CYAN;
        default:
            return COLOR_DIM;
    }
}

void input_map_clear_binding(struct input_map *map, int pad_idx) {
    if (!map || pad_idx < 0 || pad_idx >= PAD_SLOT_COUNT) return;
    struct pad_binding *b = &map->pads[pad_idx];
    b->type = BIND_TYPE_NONE;
    b->code = G1_ACT_NONE;
    b->color = COLOR_DIM;
    snprintf(b->label, sizeof(b->label), "-");
}

void input_map_set_binding(struct input_map *map, int pad_idx,
                           enum bind_type type, uint16_t code,
                           uint32_t color, const char *label) {
    if (!map || pad_idx < 0 || pad_idx >= PAD_SLOT_COUNT) return;
    if (type == BIND_TYPE_NONE || code == G1_ACT_NONE) {
        input_map_clear_binding(map, pad_idx);
        return;
    }
    struct pad_binding *b = &map->pads[pad_idx];
    b->type = type;
    b->code = code;
    b->color = color ? color : input_map_default_color(code);
    if (label && label[0]) {
        snprintf(b->label, sizeof(b->label), "%s", label);
    } else {
        snprintf(b->label, sizeof(b->label), "%s", input_map_action_short_name(code));
    }
}

static void clear_all_pads(struct input_map *map) {
    for (int i = 0; i < PAD_SLOT_COUNT; i++) {
        input_map_clear_binding(map, i);
    }
}

static void populate_bank_b_defaults(struct input_map *map) {
    /* Bank B (slots 16..31): Utilities, Savestates, Shortcuts */
    input_map_set_binding(map, 16, BIND_TYPE_ACTION, G1_ACT_LEFT,        COLOR_BLUE,   "LEFT");
    input_map_set_binding(map, 17, BIND_TYPE_ACTION, G1_ACT_DOWN,        COLOR_BLUE,   "DOWN");
    input_map_set_binding(map, 18, BIND_TYPE_ACTION, G1_ACT_RIGHT,       COLOR_BLUE,   "RGHT");
    input_map_set_binding(map, 19, BIND_TYPE_ACTION, G1_ACT_A,           COLOR_GREEN,  "A");

    input_map_set_binding(map, 20, BIND_TYPE_ACTION, G1_ACT_B,           COLOR_RED,    "B");
    input_map_set_binding(map, 21, BIND_TYPE_ACTION, G1_ACT_UP,          COLOR_BLUE,   "UP");
    input_map_set_binding(map, 22, BIND_TYPE_ACTION, G1_ACT_A,           COLOR_GREEN,  "A");
    input_map_set_binding(map, 23, BIND_TYPE_ACTION, G1_ACT_B,           COLOR_RED,    "B");

    input_map_set_binding(map, 24, BIND_TYPE_ACTION, G1_ACT_SELECT,      COLOR_YELLOW, "SELC");
    input_map_set_binding(map, 25, BIND_TYPE_ACTION, G1_ACT_START,       COLOR_AMBER,  "STRT");
    input_map_set_binding(map, 26, BIND_TYPE_ACTION, G1_ACT_TURBO_HOLD,  COLOR_PURPLE, "HOLD");
    input_map_set_binding(map, 27, BIND_TYPE_ACTION, G1_ACT_TURBO,       COLOR_PURPLE, "TRBO");

    input_map_set_binding(map, 28, BIND_TYPE_ACTION, G1_ACT_MENU,        COLOR_CYAN,   "MENU");
    input_map_set_binding(map, 29, BIND_TYPE_ACTION, G1_ACT_SCREEN_MODE, COLOR_CYAN,   "SCRN");
    input_map_set_binding(map, 30, BIND_TYPE_ACTION, G1_ACT_QUICK_SAVE,  COLOR_PINK,   "SAVE");
    input_map_set_binding(map, 31, BIND_TYPE_ACTION, G1_ACT_QUICK_LOAD,  COLOR_PINK,   "LOAD");
}

void input_map_apply_preset(struct input_map *map, enum input_preset_id preset) {
    if (!map) return;
    clear_all_pads(map);
    map->current_preset = preset;

    switch (preset) {
        case PRESET_ARCADE:
        default:
            /* Row 1 (Bottom: 0..3): D-Pad Left, Down, Right, Button A */
            input_map_set_binding(map, 0,  BIND_TYPE_ACTION, G1_ACT_LEFT,        COLOR_BLUE,   "LEFT");
            input_map_set_binding(map, 1,  BIND_TYPE_ACTION, G1_ACT_DOWN,        COLOR_BLUE,   "DOWN");
            input_map_set_binding(map, 2,  BIND_TYPE_ACTION, G1_ACT_RIGHT,       COLOR_BLUE,   "RGHT");
            input_map_set_binding(map, 3,  BIND_TYPE_ACTION, G1_ACT_A,           COLOR_GREEN,  "A");

            /* Row 2 (4..7): Button B, Up, Button A, Button B */
            input_map_set_binding(map, 4,  BIND_TYPE_ACTION, G1_ACT_B,           COLOR_RED,    "B");
            input_map_set_binding(map, 5,  BIND_TYPE_ACTION, G1_ACT_UP,          COLOR_BLUE,   "UP");
            input_map_set_binding(map, 6,  BIND_TYPE_ACTION, G1_ACT_A,           COLOR_GREEN,  "A");
            input_map_set_binding(map, 7,  BIND_TYPE_ACTION, G1_ACT_B,           COLOR_RED,    "B");

            /* Row 3 (8..11): Select, Start, Turbo, Palette */
            input_map_set_binding(map, 8,  BIND_TYPE_ACTION, G1_ACT_SELECT,      COLOR_YELLOW, "SELC");
            input_map_set_binding(map, 9,  BIND_TYPE_ACTION, G1_ACT_START,       COLOR_AMBER,  "STRT");
            input_map_set_binding(map, 10, BIND_TYPE_ACTION, G1_ACT_TURBO,       COLOR_PURPLE, "TRBO");
            input_map_set_binding(map, 11, BIND_TYPE_ACTION, G1_ACT_PALETTE,     COLOR_TEAL,   "PALT");

            /* Row 4 (Top: 12..15): Menu, Screen Mode, Quick Save, Quick Load */
            input_map_set_binding(map, 12, BIND_TYPE_ACTION, G1_ACT_MENU,        COLOR_CYAN,   "MENU");
            input_map_set_binding(map, 13, BIND_TYPE_ACTION, G1_ACT_SCREEN_MODE, COLOR_CYAN,   "SCRN");
            input_map_set_binding(map, 14, BIND_TYPE_ACTION, G1_ACT_QUICK_SAVE,  COLOR_PINK,   "SAVE");
            input_map_set_binding(map, 15, BIND_TYPE_ACTION, G1_ACT_QUICK_LOAD,  COLOR_PINK,   "LOAD");
            populate_bank_b_defaults(map);
            break;

        case PRESET_ERGONOMIC:
            /* Left Hand (cols 1-2: 0, 1, 4, 5): Left, Down, Up, Right */
            input_map_set_binding(map, 0,  BIND_TYPE_ACTION, G1_ACT_LEFT,        COLOR_BLUE,   "LEFT");
            input_map_set_binding(map, 1,  BIND_TYPE_ACTION, G1_ACT_DOWN,        COLOR_BLUE,   "DOWN");
            input_map_set_binding(map, 4,  BIND_TYPE_ACTION, G1_ACT_UP,          COLOR_BLUE,   "UP");
            input_map_set_binding(map, 5,  BIND_TYPE_ACTION, G1_ACT_RIGHT,       COLOR_BLUE,   "RGHT");

            /* Right Hand (cols 3-4: 2, 3, 6, 7): B, A, Select, Start */
            input_map_set_binding(map, 2,  BIND_TYPE_ACTION, G1_ACT_B,           COLOR_RED,    "B");
            input_map_set_binding(map, 3,  BIND_TYPE_ACTION, G1_ACT_A,           COLOR_GREEN,  "A");
            input_map_set_binding(map, 6,  BIND_TYPE_ACTION, G1_ACT_SELECT,      COLOR_YELLOW, "SELC");
            input_map_set_binding(map, 7,  BIND_TYPE_ACTION, G1_ACT_START,       COLOR_AMBER,  "STRT");

            /* Row 3 (8..11): Turbo, Hold, Palette, Screen Mode */
            input_map_set_binding(map, 8,  BIND_TYPE_ACTION, G1_ACT_TURBO,       COLOR_PURPLE, "TRBO");
            input_map_set_binding(map, 9,  BIND_TYPE_ACTION, G1_ACT_TURBO_HOLD,  COLOR_PURPLE, "HOLD");
            input_map_set_binding(map, 10, BIND_TYPE_ACTION, G1_ACT_PALETTE,     COLOR_TEAL,   "PALT");
            input_map_set_binding(map, 11, BIND_TYPE_ACTION, G1_ACT_SCREEN_MODE, COLOR_CYAN,   "SCRN");

            /* Row 4 (12..15): Menu, Quick Save, Quick Load, A */
            input_map_set_binding(map, 12, BIND_TYPE_ACTION, G1_ACT_MENU,        COLOR_CYAN,   "MENU");
            input_map_set_binding(map, 13, BIND_TYPE_ACTION, G1_ACT_QUICK_SAVE,  COLOR_PINK,   "SAVE");
            input_map_set_binding(map, 14, BIND_TYPE_ACTION, G1_ACT_QUICK_LOAD,  COLOR_PINK,   "LOAD");
            input_map_set_binding(map, 15, BIND_TYPE_ACTION, G1_ACT_A,           COLOR_GREEN,  "A");
            populate_bank_b_defaults(map);
            break;

        case PRESET_GB_CLASSIC:
            /* Row 1 (Bottom: 0..3): Select, Start, B, A */
            input_map_set_binding(map, 0,  BIND_TYPE_ACTION, G1_ACT_SELECT,      COLOR_YELLOW, "SELC");
            input_map_set_binding(map, 1,  BIND_TYPE_ACTION, G1_ACT_START,       COLOR_AMBER,  "STRT");
            input_map_set_binding(map, 2,  BIND_TYPE_ACTION, G1_ACT_B,           COLOR_RED,    "B");
            input_map_set_binding(map, 3,  BIND_TYPE_ACTION, G1_ACT_A,           COLOR_GREEN,  "A");

            /* Row 2 (4..7): Left, Down, Right, Up */
            input_map_set_binding(map, 4,  BIND_TYPE_ACTION, G1_ACT_LEFT,        COLOR_BLUE,   "LEFT");
            input_map_set_binding(map, 5,  BIND_TYPE_ACTION, G1_ACT_DOWN,        COLOR_BLUE,   "DOWN");
            input_map_set_binding(map, 6,  BIND_TYPE_ACTION, G1_ACT_RIGHT,       COLOR_BLUE,   "RGHT");
            input_map_set_binding(map, 7,  BIND_TYPE_ACTION, G1_ACT_UP,          COLOR_BLUE,   "UP");

            /* Row 3 (8..11): D-Pad Cross replica & Action B/A */
            input_map_set_binding(map, 8,  BIND_TYPE_ACTION, G1_ACT_LEFT,        COLOR_BLUE,   "LEFT");
            input_map_set_binding(map, 9,  BIND_TYPE_ACTION, G1_ACT_UP,          COLOR_BLUE,   "UP");
            input_map_set_binding(map, 10, BIND_TYPE_ACTION, G1_ACT_B,           COLOR_RED,    "B");
            input_map_set_binding(map, 11, BIND_TYPE_ACTION, G1_ACT_A,           COLOR_GREEN,  "A");

            /* Row 4 (12..15): Menu, Palette, Turbo, Hold */
            input_map_set_binding(map, 12, BIND_TYPE_ACTION, G1_ACT_MENU,        COLOR_CYAN,   "MENU");
            input_map_set_binding(map, 13, BIND_TYPE_ACTION, G1_ACT_PALETTE,     COLOR_TEAL,   "PALT");
            input_map_set_binding(map, 14, BIND_TYPE_ACTION, G1_ACT_TURBO,       COLOR_PURPLE, "TRBO");
            input_map_set_binding(map, 15, BIND_TYPE_ACTION, G1_ACT_TURBO_HOLD,  COLOR_PURPLE, "HOLD");
            populate_bank_b_defaults(map);
            break;

        case PRESET_SPEEDRUNNER:
            /* Optimised for mashing A and fast-forward */
            input_map_set_binding(map, 0,  BIND_TYPE_ACTION, G1_ACT_LEFT,        COLOR_BLUE,   "LEFT");
            input_map_set_binding(map, 1,  BIND_TYPE_ACTION, G1_ACT_DOWN,        COLOR_BLUE,   "DOWN");
            input_map_set_binding(map, 2,  BIND_TYPE_ACTION, G1_ACT_RIGHT,       COLOR_BLUE,   "RGHT");
            input_map_set_binding(map, 3,  BIND_TYPE_ACTION, G1_ACT_UP,          COLOR_BLUE,   "UP");

            input_map_set_binding(map, 4,  BIND_TYPE_ACTION, G1_ACT_A,           COLOR_GREEN,  "A");
            input_map_set_binding(map, 5,  BIND_TYPE_ACTION, G1_ACT_B,           COLOR_RED,    "B");
            input_map_set_binding(map, 6,  BIND_TYPE_ACTION, G1_ACT_A,           COLOR_GREEN,  "A");
            input_map_set_binding(map, 7,  BIND_TYPE_ACTION, G1_ACT_B,           COLOR_RED,    "B");

            input_map_set_binding(map, 8,  BIND_TYPE_ACTION, G1_ACT_TURBO_HOLD,  COLOR_PURPLE, "HOLD");
            input_map_set_binding(map, 9,  BIND_TYPE_ACTION, G1_ACT_TURBO,       COLOR_PURPLE, "TRBO");
            input_map_set_binding(map, 10, BIND_TYPE_ACTION, G1_ACT_A,           COLOR_GREEN,  "A");
            input_map_set_binding(map, 11, BIND_TYPE_ACTION, G1_ACT_START,       COLOR_AMBER,  "STRT");

            input_map_set_binding(map, 12, BIND_TYPE_ACTION, G1_ACT_QUICK_SAVE,  COLOR_PINK,   "SAVE");
            input_map_set_binding(map, 13, BIND_TYPE_ACTION, G1_ACT_QUICK_LOAD,  COLOR_PINK,   "LOAD");
            input_map_set_binding(map, 14, BIND_TYPE_ACTION, G1_ACT_PALETTE,     COLOR_TEAL,   "PALT");
            input_map_set_binding(map, 15, BIND_TYPE_ACTION, G1_ACT_MENU,        COLOR_CYAN,   "MENU");
            populate_bank_b_defaults(map);
            break;
    }
}

void input_map_init(struct input_map *map) {
    if (!map) return;
    memset(map, 0, sizeof(*map));
    map->active_bank = 0;
    map->turbo_toggle_state = false;
    map->prev_buttons = 0;
    input_map_apply_preset(map, PRESET_ARCADE);
}

void input_map_translate(struct input_map *map,
                         const struct input *in,
                         bool in_menu,
                         struct g1_input_state *out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!map || !in) return;

    /* Handle PAD_BANK button toggle edge */
    if (in->down & (1u << IN_PAD_BANK)) {
        input_map_toggle_bank(map);
    }

    uint16_t cur_buttons = 0;
    int base_slot = map->active_bank * PAD_BANK_SIZE;
    bool hold_turbo = false;

    for (int i = 0; i < PAD_BANK_SIZE; i++) {
        int slot = base_slot + i;
        if (slot >= PAD_SLOT_COUNT) break;
        const struct pad_binding *b = &map->pads[slot];
        if (b->type == BIND_TYPE_NONE || b->code == G1_ACT_NONE) continue;

        if (in->pad_down[i]) {
            uint16_t mask = input_map_action_to_mask(b->code);
            cur_buttons |= mask;

            if (b->code == G1_ACT_TURBO_HOLD) {
                hold_turbo = true;
            }
        }

        /* Check pad hit edges for toggle / one-shot actions */
        if (in->pad_hit[i] >= 0) {
            if (b->code == G1_ACT_TURBO) {
                map->turbo_toggle_state = !map->turbo_toggle_state;
            } else if (b->code == G1_ACT_PALETTE) {
                out->palette_cycles++;
            } else if (b->code == G1_ACT_SCREEN_MODE) {
                out->screen_mode_cycles++;
            } else if (b->code == G1_ACT_QUICK_SAVE) {
                out->quick_save_trigger = true;
            } else if (b->code == G1_ACT_QUICK_LOAD) {
                out->quick_load_trigger = true;
            } else if (b->code == G1_ACT_MENU) {
                out->menu_trigger = true;
            }
        }
    }

    /* Hardware Buttons mapping */
    if (in->held & (1u << IN_PLAY)) cur_buttons |= G1_BTN_START;
    if (in->held & (1u << IN_STOP)) cur_buttons |= G1_BTN_SELECT;

    /* Rotary encoder in menu vs gameplay */
    if (in_menu) {
        if (in->encoder > 0) cur_buttons |= G1_BTN_DOWN;
        if (in->encoder < 0) cur_buttons |= G1_BTN_UP;
        if (in->encoder_press) cur_buttons |= G1_BTN_A;
    }

    out->buttons = cur_buttons;
    out->pressed = cur_buttons & ~map->prev_buttons;
    map->prev_buttons = cur_buttons;

    out->turbo_active = map->turbo_toggle_state || hold_turbo;
}

void input_map_get_pad_led(const struct input_map *map,
                           const struct input *in,
                           int pad,
                           uint8_t *r, uint8_t *g, uint8_t *b) {
    if (!r || !g || !b) return;
    *r = *g = *b = 0;
    if (!map || pad < 0 || pad >= PAD_BANK_SIZE) return;

    int slot = map->active_bank * PAD_BANK_SIZE + pad;
    if (slot >= PAD_SLOT_COUNT) return;

    const struct pad_binding *p = &map->pads[slot];
    if (p->type == BIND_TYPE_NONE || p->code == G1_ACT_NONE) {
        *r = 2; *g = 2; *b = 3;
        return;
    }

    uint32_t col = p->color;
    uint8_t cr = (uint8_t)((col >> 16) & 0xFF);
    uint8_t cg = (uint8_t)((col >> 8) & 0xFF);
    uint8_t cb = (uint8_t)(col & 0xFF);

    int is_down = (in && pad < 16) ? in->pad_down[pad] : 0;
    if (is_down) {
        /* Full brightness when physically pressed */
        *r = (uint8_t)(cr >> 1);
        *g = (uint8_t)(cg >> 1);
        *b = (uint8_t)(cb >> 1);
    } else {
        /* Dim backlight when idle */
        *r = (uint8_t)(cr >> 3);
        *g = (uint8_t)(cg >> 3);
        *b = (uint8_t)(cb >> 3);
    }
}

