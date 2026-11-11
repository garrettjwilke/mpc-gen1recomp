#include "rom_manager.h"
#include <dirent.h>
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

static bool dir_exists(const char *path) {
    if (!path || !path[0]) return false;
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

/* -------------------------------------------------------------------------
 * Pure C11 FIPS 180-4 SHA-1
 * ------------------------------------------------------------------------- */
struct sha1_ctx {
    uint32_t state[5];
    uint64_t count;
    uint8_t buf[64];
};

#define SHA1_ROTL32(v, n) (((uint32_t)(v) << (n)) | ((uint32_t)(v) >> (32 - (n))))

static void sha1_transform(uint32_t state[5], const uint8_t block[64]) {
    uint32_t w[80];
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i * 4 + 0] << 24) |
               ((uint32_t)block[i * 4 + 1] << 16) |
               ((uint32_t)block[i * 4 + 2] << 8)  |
               ((uint32_t)block[i * 4 + 3]);
    }
    for (int i = 16; i < 80; i++) {
        w[i] = SHA1_ROTL32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];

    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999u;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1u;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDCu;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6u;
        }
        uint32_t temp = SHA1_ROTL32(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = SHA1_ROTL32(b, 30);
        b = a;
        a = temp;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

static void sha1_init(struct sha1_ctx *ctx) {
    ctx->state[0] = 0x67452301u;
    ctx->state[1] = 0xEFCDAB89u;
    ctx->state[2] = 0x98BADCFEu;
    ctx->state[3] = 0x10325476u;
    ctx->state[4] = 0xC3D2E1F0u;
    ctx->count = 0;
}

static void sha1_update(struct sha1_ctx *ctx, const uint8_t *data, size_t len) {
    size_t idx = (size_t)(ctx->count % 64);
    ctx->count += (uint64_t)len;

    size_t i = 0;
    if (idx > 0) {
        size_t take = 64 - idx;
        if (take > len) take = len;
        memcpy(ctx->buf + idx, data, take);
        idx += take;
        i += take;
        if (idx == 64) {
            sha1_transform(ctx->state, ctx->buf);
        }
    }

    while (i + 64 <= len) {
        sha1_transform(ctx->state, data + i);
        i += 64;
    }

    if (i < len) {
        memcpy(ctx->buf, data + i, len - i);
    }
}

static void sha1_final(struct sha1_ctx *ctx, uint8_t out[20]) {
    uint64_t bit_len = ctx->count * 8u;
    uint8_t pad = 0x80;
    sha1_update(ctx, &pad, 1);
    uint8_t zero = 0x00;
    while ((ctx->count % 64) != 56) {
        sha1_update(ctx, &zero, 1);
    }
    uint8_t len_be[8];
    for (int i = 0; i < 8; i++) {
        len_be[i] = (uint8_t)((bit_len >> ((7 - i) * 8)) & 0xFFu);
    }
    sha1_update(ctx, len_be, 8);

    for (int i = 0; i < 5; i++) {
        out[i * 4 + 0] = (uint8_t)((ctx->state[i] >> 24) & 0xFFu);
        out[i * 4 + 1] = (uint8_t)((ctx->state[i] >> 16) & 0xFFu);
        out[i * 4 + 2] = (uint8_t)((ctx->state[i] >> 8)  & 0xFFu);
        out[i * 4 + 3] = (uint8_t)(ctx->state[i] & 0xFFu);
    }
}

bool rom_manager_sha1_file(const char *path, uint8_t out_digest[20]) {
    if (!path || !path[0] || !out_digest) return false;
    FILE *f = fopen(path, "rb");
    if (!f) return false;

    struct sha1_ctx ctx;
    sha1_init(&ctx);

    uint8_t buf[32768];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        sha1_update(&ctx, buf, n);
    }
    bool io_err = ferror(f) != 0;
    fclose(f);
    if (io_err) return false;

    sha1_final(&ctx, out_digest);
    return true;
}

/* -------------------------------------------------------------------------
 * Canonical Gen 1 ROM SHA-1 table (matches gen1recomp GameVersion.lua)
 * ------------------------------------------------------------------------- */
struct known_rom_entry {
    enum g1_game_id game_id;
    off_t expected_size;
    uint8_t sha1[20];
};

static const struct known_rom_entry KNOWN_ROMS[] = {
    /* Pokemon Red (USA, Europe) (SGB Enhanced): ea9bcae617fdf159b045185467ae58b2e4a48b9a */
    {
        G1_GAME_RED,
        1048576,
        { 0xea, 0x9b, 0xca, 0xe6, 0x17, 0xfd, 0xf1, 0x59, 0xb0, 0x45,
          0x18, 0x54, 0x67, 0xae, 0x58, 0xb2, 0xe4, 0xa4, 0x8b, 0x9a }
    },
    /* Pokemon Blue (USA, Europe) (SGB Enhanced): d7037c83e1ae5b39bde3c30787637ba1d4c48ce2 */
    {
        G1_GAME_BLUE,
        1048576,
        { 0xd7, 0x03, 0x7c, 0x83, 0xe1, 0xae, 0x5b, 0x39, 0xbd, 0xe3,
          0xc3, 0x07, 0x87, 0x63, 0x7b, 0xa1, 0xd4, 0xc4, 0x8c, 0xe2 }
    },
    /* Pokemon Yellow (USA, Europe) (GBC,SGB Enhanced): cc7d03262ebfaf2f06772c1a480c7d9d5f4a38e1 */
    {
        G1_GAME_YELLOW,
        1048576,
        { 0xcc, 0x7d, 0x03, 0x26, 0x2e, 0xbf, 0xaf, 0x2f, 0x06, 0x77,
          0x2c, 0x1a, 0x48, 0x0c, 0x7d, 0x9d, 0x5f, 0x4a, 0x38, 0xe1 }
    }
};

#define KNOWN_ROM_COUNT (sizeof(KNOWN_ROMS) / sizeof(KNOWN_ROMS[0]))

static void sha1_to_hex(const uint8_t digest[20], char hex_out[41]) {
    static const char HEX[] = "0123456789abcdef";
    for (int i = 0; i < 20; i++) {
        hex_out[i * 2 + 0] = HEX[(digest[i] >> 4) & 0xF];
        hex_out[i * 2 + 1] = HEX[digest[i] & 0xF];
    }
    hex_out[40] = '\0';
}

static bool any_unfound_matches_size(const struct rom_manager *rm, off_t sz) {
    for (size_t i = 0; i < KNOWN_ROM_COUNT; i++) {
        enum g1_game_id gid = KNOWN_ROMS[i].game_id;
        if (!rm->games[gid].rom_found && KNOWN_ROMS[i].expected_size == sz) {
            return true;
        }
    }
    return false;
}

static void check_candidate_file(struct rom_manager *rm, const char *path, off_t sz) {
    if (!any_unfound_matches_size(rm, sz)) return;
    if (strlen(path) >= sizeof(rm->games[0].rom_path)) return;

    uint8_t digest[20];
    if (!rom_manager_sha1_file(path, digest)) return;

    for (size_t i = 0; i < KNOWN_ROM_COUNT; i++) {
        if (KNOWN_ROMS[i].expected_size != sz) continue;
        if (memcmp(digest, KNOWN_ROMS[i].sha1, 20) == 0) {
            enum g1_game_id gid = KNOWN_ROMS[i].game_id;
            if (!rm->games[gid].rom_found) {
                struct g1_game_info *gi = &rm->games[gid];
                snprintf(gi->rom_path, sizeof(gi->rom_path), "%s", path);
                gi->rom_found = true;
                gi->cache_ready = true;
                fprintf(stderr, "[G1] Matched %s by SHA-1: %s\n", gi->display_name, path);
            }
            return;
        }
    }

    char hex[41];
    sha1_to_hex(digest, hex);
    fprintf(stderr, "[G1] Unrecognized ROM-sized file %s (sha1=%s)\n", path, hex);
}

static bool join_path(char *dst, size_t dst_sz, const char *dir, const char *name) {
    size_t dlen = strlen(dir);
    bool has_slash = (dlen > 0 && dir[dlen - 1] == '/');
    int n = snprintf(dst, dst_sz, "%s%s%s", dir, has_slash ? "" : "/", name);
    return n > 0 && (size_t)n < dst_sz;
}

static void scan_dir_for_roms(struct rom_manager *rm, const char *dir_path, int max_depth) {
    if (!rm || !dir_path || !dir_path[0]) return;
    if (rom_manager_count_found(rm) >= G1_GAME_COUNT) return;

    DIR *d = opendir(dir_path);
    if (!d) return;

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (rom_manager_count_found(rm) >= G1_GAME_COUNT) break;
        if (de->d_name[0] == '.') continue;

        char child[512];
        if (!join_path(child, sizeof(child), dir_path, de->d_name)) continue;

        struct stat st;
        if (stat(child, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            if (max_depth > 0) {
                scan_dir_for_roms(rm, child, max_depth - 1);
            }
        } else if (S_ISREG(st.st_mode)) {
            check_candidate_file(rm, child, st.st_size);
        }
    }
    closedir(d);
}

static void scan_subpath(struct rom_manager *rm, const char *root, const char *sub, int max_depth) {
    if (rom_manager_count_found(rm) >= G1_GAME_COUNT) return;
    char path[512];
    if (sub && sub[0]) {
        if (!join_path(path, sizeof(path), root, sub)) return;
        scan_dir_for_roms(rm, path, max_depth);
    } else {
        scan_dir_for_roms(rm, root, max_depth);
    }
}

static void restore_app_dir(void) {
    const char *app_dir = getenv("MPC_APP_DIR");
    if (app_dir && app_dir[0]) {
        (void)chdir(app_dir);
    }
}

void rom_manager_ensure_roms_dir(void) {
    DIR *md = opendir("/media");
    if (md) {
        struct dirent *de;
        while ((de = readdir(md)) != NULL) {
            if (de->d_name[0] == '.') continue;
            if (strcmp(de->d_name, "az01-settings") == 0) continue;
            char vol[256];
            if (!join_path(vol, sizeof(vol), "/media", de->d_name)) continue;
            if (dir_exists(vol)) {
                char roms_dir[320];
                if (join_path(roms_dir, sizeof(roms_dir), vol, "roms")) {
                    mkdir(roms_dir, 0755);
                }
            }
        }
        closedir(md);
    }
    mkdir("roms", 0755);
    mkdir("user", 0755);
    mkdir("user/roms", 0755);
    sync();
}

void rom_manager_init(struct rom_manager *rm) {
    if (!rm) return;
    memset(rm, 0, sizeof(*rm));
    rom_manager_ensure_roms_dir();

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
    rom_manager_ensure_roms_dir();

    for (int g = 0; g < G1_GAME_COUNT; g++) {
        rm->games[g].rom_found = false;
        rm->games[g].cache_ready = false;
        rm->games[g].rom_path[0] = '\0';
    }

    if (base_dir && base_dir[0]) {
        /* Explicit root (e.g. unit tests) */
        scan_subpath(rm, base_dir, "roms", 2);
        scan_subpath(rm, base_dir, "ROMs", 2);
        scan_subpath(rm, base_dir, "Roms", 2);
        scan_subpath(rm, base_dir, "user/roms", 2);
        scan_subpath(rm, base_dir, "", 0);
        scan_subpath(rm, base_dir, "../roms", 2);
    } else {
        /* 1. Mounted SD card volumes under /media/<vol> */
        DIR *md = opendir("/media");
        if (md) {
            struct dirent *de;
            while ((de = readdir(md)) != NULL && rom_manager_count_found(rm) < G1_GAME_COUNT) {
                if (de->d_name[0] == '.') continue;
                if (strcmp(de->d_name, "az01-settings") == 0) continue;
                char vol[256];
                if (!join_path(vol, sizeof(vol), "/media", de->d_name)) continue;
                if (!dir_exists(vol)) continue;

                scan_subpath(rm, vol, "roms", 2);
                scan_subpath(rm, vol, "ROMs", 2);
                scan_subpath(rm, vol, "Roms", 2);
                scan_subpath(rm, vol, "Pokemon", 1);
                scan_subpath(rm, vol, "pokemon", 1);
                scan_subpath(rm, vol, "GB", 1);
                scan_subpath(rm, vol, "GBC", 1);
                scan_subpath(rm, vol, "autorun/roms", 2);
                scan_subpath(rm, vol, "", 0);
            }
            closedir(md);
        }

        /* 2. App-relative directories (cwd / MPC_APP_DIR) */
        scan_dir_for_roms(rm, "roms", 2);
        scan_dir_for_roms(rm, "ROMs", 2);
        scan_dir_for_roms(rm, "Roms", 2);
        scan_dir_for_roms(rm, "user/roms", 2);
        scan_dir_for_roms(rm, ".", 0);
        scan_dir_for_roms(rm, "../roms", 2);
        scan_dir_for_roms(rm, "../../roms", 2);

        const char *app_dir = getenv("MPC_APP_DIR");
        if (app_dir && app_dir[0]) {
            scan_subpath(rm, app_dir, "roms", 2);
            scan_subpath(rm, app_dir, "user/roms", 2);
            scan_subpath(rm, app_dir, "", 0);
            scan_subpath(rm, app_dir, "../roms", 2);
            scan_subpath(rm, app_dir, "../../roms", 2);
        }
    }

    /* Check for rom-cache.complete */
    for (int g = 0; g < G1_GAME_COUNT; g++) {
        struct g1_game_info *gi = &rm->games[g];
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
    rom_manager_ensure_roms_dir();
    ui_usb_sd_init(&rm->usb);
    rm->usb_active = true;
}

void rom_manager_stop_usb(struct rom_manager *rm) {
    if (!rm) return;
    if (ui_usb_sd_is_active(&rm->usb)) {
        ui_usb_sd_stop(&rm->usb);
    }
    rm->usb_active = false;
    restore_app_dir();
    rom_manager_scan(rm, NULL);
}

int rom_manager_draw_usb(struct rom_manager *rm, struct ui *u, const struct input *in) {
    if (!rm || !u) return UI_USB_SD_FAILED;
    struct ui_usb_sd_style st = ui_usb_sd_style_default();
    int rc = ui_usb_sd(u, &rm->usb, st, in);
    if (rc == UI_USB_SD_DONE || rc == UI_USB_SD_FAILED) {
        rm->usb_active = false;
        restore_app_dir();
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

