#ifndef MPC_G1_ROM_MANAGER_H
#define MPC_G1_ROM_MANAGER_H

#include <stdbool.h>
#include <stdint.h>
#include "ui.h"
#include "input.h"
#include "usb_sd.h"

#define G1_GAME_COUNT 3

enum g1_game_id {
    G1_GAME_RED = 0,
    G1_GAME_BLUE,
    G1_GAME_YELLOW
};

struct g1_game_info {
    enum g1_game_id id;
    const char *tag;            /* "red", "blue", "yellow" */
    const char *display_name;   /* "Pokemon Red", "Pokemon Blue", "Pokemon Yellow" */
    uint32_t theme_color;       /* Red (0xFF1744), Blue (0x00B0FF), Yellow (0xFFD600) */
    char rom_path[256];         /* Path to ROM file if found */
    bool rom_found;             /* True if ROM file exists */
    bool cache_ready;           /* True if rom-cache.complete exists */
};

struct g1_extract_state {
    bool active;
    enum g1_game_id game_id;
    char stage_text[128];
    int progress;
    int total;
    float percent;
    int spinner_tick;
    bool finished;
    char error_msg[128];
};

struct rom_manager {
    struct g1_game_info games[G1_GAME_COUNT];
    int selected_game;          /* 0..2 */
    struct ui_usb_sd usb;       /* USB SD widget state */
    bool usb_active;            /* True while showing USB storage screen */
    struct g1_extract_state extract;
};

void rom_manager_init(struct rom_manager *rm);
void rom_manager_scan(struct rom_manager *rm, const char *base_dir);
void rom_manager_ensure_roms_dir(void);

int  rom_manager_count_found(const struct rom_manager *rm);
int  rom_manager_count_ready(const struct rom_manager *rm);

bool rom_manager_is_ready(const struct rom_manager *rm, enum g1_game_id game);
const char *rom_manager_game_name(enum g1_game_id game);
uint32_t    rom_manager_game_color(enum g1_game_id game);

/* USB Mass Storage helper */
int  rom_manager_draw_usb(struct rom_manager *rm, struct ui *u, const struct input *in);
void rom_manager_start_usb(struct rom_manager *rm);
void rom_manager_stop_usb(struct rom_manager *rm);

/* Extraction lifecycle */
void rom_manager_start_extraction(struct rom_manager *rm, enum g1_game_id game);
void rom_manager_step_extraction(struct rom_manager *rm, int64_t now_ms);
void rom_manager_draw_extraction(struct rom_manager *rm, struct ui *u);

#endif /* MPC_G1_ROM_MANAGER_H */

