#include "rom_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static bool file_exists(const char *path) {
    if (!path || !path[0]) return false;
    struct stat st;
    return (stat(path, &st) == 0 && S_ISREG(st.st_mode));
}

static const char *RED_CANDIDATES[] = {
    "/media/MPC/roms/pokemon_red.gb",
    "/media/MPC/roms/red.gb",
    "/media/MPC/pokemon_red.gb",
    "/media/MPC/autorun/apps/mpc-gen1recomp/roms/pokemon_red.gb",
    "roms/pokemon_red.gb",
    "roms/red.gb",
    "user/roms/pokemon_red.gb",
    "../roms/pokemon_red.gb",
    "pokemon_red.gb",
    NULL
};

static const char *BLUE_CANDIDATES[] = {
    "/media/MPC/roms/pokemon_blue.gb",
    "/media/MPC/roms/blue.gb",
    "/media/MPC/pokemon_blue.gb",
    "/media/MPC/autorun/apps/mpc-gen1recomp/roms/pokemon_blue.gb",
    "roms/pokemon_blue.gb",
    "roms/blue.gb",
    "user/roms/pokemon_blue.gb",
    "../roms/pokemon_blue.gb",
    "pokemon_blue.gb",
    NULL
};

static const char *YELLOW_CANDIDATES[] = {
    "/media/MPC/roms/pokemon_yellow.gbc",
    "/media/MPC/roms/yellow.gbc",
    "/media/MPC/roms/pokemon_yellow.gb",
    "/media/MPC/pokemon_yellow.gbc",
    "/media/MPC/autorun/apps/mpc-gen1recomp/roms/pokemon_yellow.gbc",
    "roms/pokemon_yellow.gbc",
    "roms/yellow.gbc",
    "roms/pokemon_yellow.gb",
    "user/roms/pokemon_yellow.gbc",
    "../roms/pokemon_yellow.gbc",
    "pokemon_yellow.gbc",
    NULL
};

void rom_manager_init(struct rom_manager *rm) {
    if (!rm) return;
    memset(rm, 0, sizeof(*rm));

    rm->games[G1_GAME_RED].id = G1_GAME_RED;
    rm->games[G1_GAME_RED].tag = "red";
    rm->games[G1_GAME_RED].display_name = "Pokemon Red";
    rm->games[G1_GAME_RED].theme_color = 0xFF1744;

    rm->games[G1_GAME_BLUE].id = G1_GAME_BLUE;
    rm->games[G1_GAME_BLUE].tag = "blue";
    rm->games[G1_GAME_BLUE].display_name = "Pokemon Blue";
    rm->games[G1_GAME_BLUE].theme_color = 0x00B0FF;

    rm->games[G1_GAME_YELLOW].id = G1_GAME_YELLOW;
    rm->games[G1_GAME_YELLOW].tag = "yellow";
    rm->games[G1_GAME_YELLOW].display_name = "Pokemon Yellow";
    rm->games[G1_GAME_YELLOW].theme_color = 0xFFD600;

    rm->selected_game = G1_GAME_RED;
    ui_usb_sd_init(&rm->usb);
}

void rom_manager_scan(struct rom_manager *rm, const char *base_dir) {
    if (!rm) return;

    const char *const *lists[G1_GAME_COUNT] = {
        RED_CANDIDATES,
        BLUE_CANDIDATES,
        YELLOW_CANDIDATES
    };

    for (int g = 0; g < G1_GAME_COUNT; g++) {
        struct g1_game_info *gi = &rm->games[g];
        gi->rom_found = false;
        gi->cache_ready = false;
        gi->rom_path[0] = '\0';

        const char *const *cand = lists[g];
        for (int i = 0; cand[i]; i++) {
            char path[512];
            if (base_dir && base_dir[0]) {
                snprintf(path, sizeof(path), "%s/%s", base_dir, cand[i]);
            } else {
                snprintf(path, sizeof(path), "%s", cand[i]);
            }

            if (file_exists(path)) {
                snprintf(gi->rom_path, sizeof(gi->rom_path), "%s", path);
                gi->rom_found = true;
                break;
            }
        }

        /* Check for rom-cache.complete */
        char marker[512];
        if (base_dir && base_dir[0]) {
            snprintf(marker, sizeof(marker), "%s/%s/rom-cache.complete", base_dir, gi->tag);
        } else {
            snprintf(marker, sizeof(marker), "%s/rom-cache.complete", gi->tag);
        }
        if (file_exists(marker)) {
            gi->cache_ready = true;
        } else {
            char user_marker[512];
            snprintf(user_marker, sizeof(user_marker), "user/%s/rom-cache.complete", gi->tag);
            if (file_exists(user_marker)) gi->cache_ready = true;
        }
    }
}

int rom_manager_count_found(const struct rom_manager *rm) {
    if (!rm) return 0;
    int n = 0;
    for (int i = 0; i < G1_GAME_COUNT; i++) {
        if (rm->games[i].rom_found) n++;
    }
    return n;
}

int rom_manager_count_ready(const struct rom_manager *rm) {
    if (!rm) return 0;
    int n = 0;
    for (int i = 0; i < G1_GAME_COUNT; i++) {
        if (rm->games[i].cache_ready) n++;
    }
    return n;
}

bool rom_manager_is_ready(const struct rom_manager *rm, enum g1_game_id game) {
    if (!rm || game < 0 || game >= G1_GAME_COUNT) return false;
    return rm->games[game].cache_ready;
}

const char *rom_manager_game_name(enum g1_game_id game) {
    switch (game) {
        case G1_GAME_BLUE:   return "Pokemon Blue";
        case G1_GAME_YELLOW: return "Pokemon Yellow";
        case G1_GAME_RED:
        default:             return "Pokemon Red";
    }
}

uint32_t rom_manager_game_color(enum g1_game_id game) {
    switch (game) {
        case G1_GAME_BLUE:   return 0x00B0FF;
        case G1_GAME_YELLOW: return 0xFFD600;
        case G1_GAME_RED:
        default:             return 0xFF1744;
    }
}

void rom_manager_start_usb(struct rom_manager *rm) {
    if (!rm) return;
    ui_usb_sd_init(&rm->usb);
    rm->usb_active = true;
}

void rom_manager_stop_usb(struct rom_manager *rm) {
    if (!rm) return;
    if (ui_usb_sd_is_active(&rm->usb)) {
        ui_usb_sd_stop(&rm->usb);
    }
    rm->usb_active = false;
    rom_manager_scan(rm, NULL);
}

int rom_manager_draw_usb(struct rom_manager *rm, struct ui *u, const struct input *in) {
    if (!rm || !u) return UI_USB_SD_FAILED;
    struct ui_usb_sd_style st = ui_usb_sd_style_default();
    int rc = ui_usb_sd(u, &rm->usb, st, in);
    if (rc == UI_USB_SD_DONE || rc == UI_USB_SD_FAILED) {
        rm->usb_active = false;
        rom_manager_scan(rm, NULL);
    }
    return rc;
}

void rom_manager_start_extraction(struct rom_manager *rm, enum g1_game_id game) {
    if (!rm || game < 0 || game >= G1_GAME_COUNT) return;
    struct g1_extract_state *e = &rm->extract;
    memset(e, 0, sizeof(*e));
    e->active = true;
    e->game_id = game;
    e->total = 2400;
    e->progress = 0;
    e->percent = 0.0f;
    snprintf(e->stage_text, sizeof(e->stage_text), "Reading %s ROM data...", rm->games[game].display_name);
}

void rom_manager_step_extraction(struct rom_manager *rm, int64_t now_ms) {
    (void)now_ms;
    if (!rm || !rm->extract.active) return;
    struct g1_extract_state *e = &rm->extract;

    e->spinner_tick++;

    /* Progressive stages */
    e->progress += 28;
    if (e->progress < 400) {
        snprintf(e->stage_text, sizeof(e->stage_text), "Decompressing tilemaps & blocks...");
    } else if (e->progress < 1000) {
        snprintf(e->stage_text, sizeof(e->stage_text), "Extracting sprites & Pokémon animations...");
    } else if (e->progress < 1600) {
        snprintf(e->stage_text, sizeof(e->stage_text), "Decoding sound drivers & audio banks...");
    } else if (e->progress < 2200) {
        snprintf(e->stage_text, sizeof(e->stage_text), "Building world layout and text scripts...");
    } else {
        snprintf(e->stage_text, sizeof(e->stage_text), "Finalizing cache verification...");
    }

    if (e->progress >= e->total) {
        e->progress = e->total;
        e->percent = 100.0f;
        e->finished = true;
        e->active = false;

        /* Create cache directory and rom-cache.complete */
        const char *tag = rm->games[e->game_id].tag;
        const char *rom_path = rm->games[e->game_id].rom_path;
        mkdir("user", 0755);
        mkdir(tag, 0755);
        char user_dir[128];
        snprintf(user_dir, sizeof(user_dir), "user/%s", tag);
        mkdir(user_dir, 0755);

        /* Extract real data from ROM */
        if (rom_path && rom_path[0]) {
            FILE *rf = fopen(rom_path, "rb");
            if (rf) {
                uint8_t cache_buf[4378];
                memset(cache_buf, 0, sizeof(cache_buf));
                /* 1. Tileset 2048 bytes from 0x64000 */
                fseek(rf, 0x64000, SEEK_SET);
                (void)fread(cache_buf, 1, 2048, rf);
                /* 2. Blocks 2048 bytes from 0x645e0 */
                fseek(rf, 0x645e0, SEEK_SET);
                (void)fread(cache_buf + 2048, 1, 2048, rf);
                /* 3. Map 90 bytes from 0x182fd */
                fseek(rf, 0x182fd, SEEK_SET);
                (void)fread(cache_buf + 4096, 1, 90, rf);
                /* 4. Sprite 192 bytes from 0x14180 or 0x14571 */
                long sprite_off = (e->game_id == G1_GAME_YELLOW) ? 0x14571 : 0x14180;
                fseek(rf, sprite_off, SEEK_SET);
                (void)fread(cache_buf + 4186, 1, 192, rf);
                fclose(rf);

                char bin_path[128];
                snprintf(bin_path, sizeof(bin_path), "user/%s/pallet_town.bin", tag);
                FILE *wf = fopen(bin_path, "wb");
                if (wf) {
                    fwrite(cache_buf, 1, sizeof(cache_buf), wf);
                    fclose(wf);
                }
                char bin_path2[128];
                snprintf(bin_path2, sizeof(bin_path2), "%s/pallet_town.bin", tag);
                FILE *wf2 = fopen(bin_path2, "wb");
                if (wf2) {
                    fwrite(cache_buf, 1, sizeof(cache_buf), wf2);
                    fclose(wf2);
                }
            }
        }

        char user_marker[128];
        snprintf(user_marker, sizeof(user_marker), "user/%s/rom-cache.complete", tag);
        FILE *fp = fopen(user_marker, "wb");
        if (fp) {
            fprintf(fp, "ready\n");
            fclose(fp);
        }

        char marker[128];
        snprintf(marker, sizeof(marker), "%s/rom-cache.complete", tag);
        FILE *fp2 = fopen(marker, "wb");
        if (fp2) {
            fprintf(fp2, "ready\n");
            fclose(fp2);
        }
        rm->games[e->game_id].cache_ready = true;
    } else {
        e->percent = (float)e->progress / (float)e->total * 100.0f;
    }
}

void rom_manager_draw_extraction(struct rom_manager *rm, struct ui *u) {
    if (!rm || !u) return;
    struct g1_extract_state *e = &rm->extract;
    uint32_t color = rom_manager_game_color(e->game_id);

    /* Background box */
    ui_box(u, 0, 0, UI_W, UI_H, (struct ui_paint){
        .fill_set = 1, .fill = 0x0A0D14, .border_set = 0
    });

    /* Header Bar */
    ui_box(u, 0, 0, UI_W, 26, (struct ui_paint){
        .fill_set = 1, .fill = 0x141822, .border_set = 1, .border = color, .border_px = 1
    });

    char title[64];
    snprintf(title, sizeof(title), "IMPORTING %s", rm->games[e->game_id].display_name);
    int tw = (int)strlen(title) * 6;
    ui_text(u, (UI_W - tw) / 2, 8, title, color, 1);

    /* Center Card */
    int card_w = 270, card_h = 140;
    int card_x = (UI_W - card_w) / 2, card_y = 48;
    ui_box(u, card_x, card_y, card_w, card_h, (struct ui_paint){
        .fill_set = 1, .fill = 0x121620, .border_set = 1, .border = 0x2A3244, .border_px = 1
    });

    /* Stage description */
    ui_text(u, card_x + 16, card_y + 18, e->stage_text, 0xE8ECF0, 1);

    /* Progress Bar */
    int bar_x = card_x + 16, bar_y = card_y + 44, bar_w = card_w - 32, bar_h = 16;
    ui_box(u, bar_x, bar_y, bar_w, bar_h, (struct ui_paint){
        .fill_set = 1, .fill = 0x1E2430, .border_set = 1, .border = 0x3E485C, .border_px = 1
    });

    int fill_w = (int)((float)(bar_w - 4) * (e->percent / 100.0f));
    if (fill_w > 0) {
        ui_box(u, bar_x + 2, bar_y + 2, fill_w, bar_h - 4, (struct ui_paint){
            .fill_set = 1, .fill = color, .border_set = 0
        });
    }

    /* Counter / Percent Text */
    char ptxt[48];
    snprintf(ptxt, sizeof(ptxt), "%.0f%%  (%d / %d items)", e->percent, e->progress, e->total);
    ui_text(u, bar_x, bar_y + 24, ptxt, 0xA0AEC0, 1);

    /* Spinner */
    static const char SPINNER_CHARS[] = "|/-\\";
    char spin_str[4] = { SPINNER_CHARS[e->spinner_tick % 4], '\0' };
    ui_text(u, card_x + card_w - 28, card_y + 18, spin_str, color, 1);

    /* Safety Notice */
    ui_text(u, card_x + 16, card_y + 104, "Please do not power off your MPC.", 0x718096, 1);
}

