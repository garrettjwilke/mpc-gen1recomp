#ifndef MPC_G1_INPUT_MAP_H
#define MPC_G1_INPUT_MAP_H

#include "input.h"
#include "ui.h"
#include <stdbool.h>
#include <stdint.h>

#define PAD_BANK_SIZE  16
#define PAD_BANK_COUNT 2
#define PAD_SLOT_COUNT (PAD_BANK_SIZE * PAD_BANK_COUNT)

/* Pad & UI color palette (0x00RRGGBB) */
#define COLOR_DIM    0x202634
#define COLOR_BLUE   0x00B0FF
#define COLOR_GREEN  0x00E676
#define COLOR_RED    0xFF1744
#define COLOR_AMBER  0xFFB300
#define COLOR_PURPLE 0xD500F9
#define COLOR_TEAL   0x00BFA5
#define COLOR_CYAN   0x00E5FF
#define COLOR_YELLOW 0xFFD600
#define COLOR_PINK   0xFF4081
#define COLOR_WHITE  0xE8ECF0

enum bind_type {
    BIND_TYPE_NONE = 0,
    BIND_TYPE_ACTION
};

enum g1_action_id {
    G1_ACT_NONE = 0,
    G1_ACT_UP,          /* D-Pad Up */
    G1_ACT_DOWN,        /* D-Pad Down */
    G1_ACT_LEFT,        /* D-Pad Left */
    G1_ACT_RIGHT,       /* D-Pad Right */
    G1_ACT_A,           /* Button A (Confirm / Talk) */
    G1_ACT_B,           /* Button B (Cancel / Back) */
    G1_ACT_START,       /* Start Menu */
    G1_ACT_SELECT,      /* Select / Item Switch */
    G1_ACT_TURBO,       /* Turbo Toggle */
    G1_ACT_TURBO_HOLD,  /* Turbo while held */
    G1_ACT_QUICK_SAVE,  /* Quick Save State */
    G1_ACT_QUICK_LOAD,  /* Quick Load State */
    G1_ACT_PALETTE,     /* Cycle Color Palette */
    G1_ACT_SCREEN_MODE, /* Cycle Screen Aspect */
    G1_ACT_MENU,        /* Open In-Game Menu */
    G1_ACT_COUNT
};

/* Button bitmasks */
#define G1_BTN_UP          (1u << 0)
#define G1_BTN_DOWN        (1u << 1)
#define G1_BTN_LEFT        (1u << 2)
#define G1_BTN_RIGHT       (1u << 3)
#define G1_BTN_A           (1u << 4)
#define G1_BTN_B           (1u << 5)
#define G1_BTN_START       (1u << 6)
#define G1_BTN_SELECT      (1u << 7)
#define G1_BTN_TURBO       (1u << 8)
#define G1_BTN_QUICK_SAVE  (1u << 9)
#define G1_BTN_QUICK_LOAD  (1u << 10)
#define G1_BTN_PALETTE     (1u << 11)
#define G1_BTN_SCREEN_MODE (1u << 12)
#define G1_BTN_MENU        (1u << 13)

struct g1_input_state {
    uint16_t buttons;
    uint16_t pressed;
    bool turbo_active;
    int palette_cycles;
    int screen_mode_cycles;
    bool quick_save_trigger;
    bool quick_load_trigger;
    bool menu_trigger;
};

struct pad_binding {
    enum bind_type type;
    uint16_t code;      /* enum g1_action_id */
    uint32_t color;     /* 0x00RRGGBB LED & UI color */
    char label[12];     /* Short display label */
};

enum input_preset_id {
    PRESET_ARCADE = 0,
    PRESET_ERGONOMIC,
    PRESET_GB_CLASSIC,
    PRESET_SPEEDRUNNER,
    PRESET_COUNT
};

struct input_map {
    struct pad_binding pads[PAD_SLOT_COUNT];
    int active_bank;    /* 0 = Bank A (0..15), 1 = Bank B (16..31) */
    enum input_preset_id current_preset;
    bool turbo_toggle_state;
    uint16_t prev_buttons;
};

void input_map_init(struct input_map *map);
void input_map_apply_preset(struct input_map *map, enum input_preset_id preset);
const char *input_map_preset_name(enum input_preset_id preset);

void input_map_set_bank(struct input_map *map, int bank);
int  input_map_get_bank(const struct input_map *map);
void input_map_toggle_bank(struct input_map *map);

void input_map_set_binding(struct input_map *map, int pad_idx,
                           enum bind_type type, uint16_t code,
                           uint32_t color, const char *label);
void input_map_clear_binding(struct input_map *map, int pad_idx);

/* Build g1_input_state from current held pads & encoder */
void input_map_translate(struct input_map *map,
                         const struct input *in,
                         bool in_menu,
                         struct g1_input_state *out);

/* Compute RGB color for physical pad LED (0..127 per channel) */
void input_map_get_pad_led(const struct input_map *map,
                           const struct input *in,
                           int pad,
                           uint8_t *r, uint8_t *g, uint8_t *b);

/* Helpers for Rebind UI */
const char *input_map_action_short_name(uint16_t code);
const char *input_map_action_full_name(uint16_t code);
uint16_t    input_map_action_to_mask(uint16_t code);
uint32_t    input_map_default_color(uint16_t code);

#endif /* MPC_G1_INPUT_MAP_H */

