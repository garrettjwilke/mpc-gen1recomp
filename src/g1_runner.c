#include "g1_runner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static const uint32_t PAL_DMG[4] = {
    0x9BBC0F, 0x8BAC0F, 0x306230, 0x0F380F
};

static const uint32_t PAL_POCKET[4] = {
    0xE0E0E0, 0xA0A0A0, 0x505050, 0x101010
};

static const uint32_t PAL_SGB_RED[4] = {
    0xFFE0E0, 0xF08080, 0xC02020, 0x400000
};

static const uint32_t PAL_SGB_BLUE[4] = {
    0xE0F0FF, 0x80B0F0, 0x2050C0, 0x001040
};

static const uint32_t PAL_SGB_YELLOW[4] = {
    0xFFFFE0, 0xF0D050, 0xC08010, 0x402000
};

static const uint32_t PAL_GBC[4] = {
    0xF8F8F8, 0xA8B8D8, 0x506890, 0x181820
};

static const uint32_t *get_palette(int palette_idx, enum g1_game_id game) {
    switch (palette_idx) {
        case 1: /* SGB */
            if (game == G1_GAME_RED) return PAL_SGB_RED;
            if (game == G1_GAME_BLUE) return PAL_SGB_BLUE;
            return PAL_SGB_YELLOW;
        case 2: /* DMG */
            return PAL_DMG;
        case 3: /* Pocket */
            return PAL_POCKET;
        case 0: /* GBC / Modern */
        default:
            return PAL_GBC;
    }
}

struct g1_runner *g1_runner_create(enum g1_game_id game, int samplerate) {
    struct g1_runner *r = (struct g1_runner *)calloc(1, sizeof(*r));
    if (!r) return NULL;

    r->game_id = game;
    r->samplerate = (samplerate > 0) ? samplerate : 44100;
    r->is_running = true;
    r->is_paused = false;
    r->player_x = 80;
    r->player_y = 72;
    pthread_mutex_init(&r->audio_lock, NULL);

    g1_runner_reset(r);
    return r;
}

void g1_runner_destroy(struct g1_runner *r) {
    if (!r) return;
    pthread_mutex_destroy(&r->audio_lock);
    free(r);
}

void g1_runner_reset(struct g1_runner *r) {
    if (!r) return;
    pthread_mutex_lock(&r->audio_lock);
    r->frame_count = 0;
    r->anim_step = 0;
    r->player_x = 80;
    r->player_y = 72;
    r->audio_phase = 0;
    memset(r->game_fb, 0, sizeof(r->game_fb));
    pthread_mutex_unlock(&r->audio_lock);
}

void g1_runner_set_paused(struct g1_runner *r, bool paused) {
    if (r) r->is_paused = paused;
}

bool g1_runner_is_paused(const struct g1_runner *r) {
    return r ? r->is_paused : false;
}

void g1_runner_set_game(struct g1_runner *r, enum g1_game_id game) {
    if (!r) return;
    r->game_id = game;
    g1_runner_reset(r);
}

void g1_runner_tick(struct g1_runner *r, const struct g1_input_state *in) {
    if (!r || r->is_paused) return;

    r->frame_count++;
    r->anim_step++;

    int speed = in->turbo_active ? 2 : 1;
    if (in) {
        if (in->buttons & G1_BTN_LEFT)  r->player_x -= speed;
        if (in->buttons & G1_BTN_RIGHT) r->player_x += speed;
        if (in->buttons & G1_BTN_UP)    r->player_y -= speed;
        if (in->buttons & G1_BTN_DOWN)  r->player_y += speed;
    }

    if (r->player_x < 8) r->player_x = 8;
    if (r->player_x > GB_SCREEN_W - 8) r->player_x = GB_SCREEN_W - 8;
    if (r->player_y < 8) r->player_y = 8;
    if (r->player_y > GB_SCREEN_H - 8) r->player_y = GB_SCREEN_H - 8;

    /* Render Game Boy playfield - Pallet Town styled overworld viewport */
    for (int y = 0; y < GB_SCREEN_H; y++) {
        for (int x = 0; x < GB_SCREEN_W; x++) {
            int shade = 0;

            /* Top status border */
            if (y < 12) {
                shade = (y == 11) ? 3 : 1;
            }
            /* Bottom dialogue / HUD area */
            else if (y >= 120) {
                if (y == 120 || y == GB_SCREEN_H - 1 || x == 0 || x == GB_SCREEN_W - 1) {
                    shade = 3;
                } else {
                    shade = 0;
                }
            }
            /* Overworld field */
            else {
                /* Water pond in corner */
                if (x >= 110 && y >= 70 && y < 110) {
                    int wave = ((x + (int)(r->frame_count / 8)) % 8 < 4) ? 2 : 1;
                    shade = wave;
                }
                /* Path vs grass */
                else if ((x >= 60 && x <= 100) || (y >= 50 && y <= 75)) {
                    shade = 0; /* Light path */
                } else {
                    shade = ((x / 8 + y / 8) & 1) ? 1 : 0; /* Grass pattern */
                }

                /* Draw player trainer sprite (12x14 pixels) */
                int dx = x - r->player_x;
                int dy = y - r->player_y;
                if (dx >= -6 && dx <= 5 && dy >= -8 && dy <= 5) {
                    if (dy < -4) {
                        shade = 3; /* Cap */
                    } else if (dy < 0) {
                        shade = (dx >= -3 && dx <= 2) ? 0 : 3; /* Face */
                    } else if (dy < 3) {
                        shade = 2; /* Jacket */
                    } else {
                        shade = 3; /* Pants */
                    }
                }
            }

            r->game_fb[y * GB_SCREEN_W + x] = (uint32_t)shade;
        }
    }
}

void g1_runner_render(struct g1_runner *r, uint32_t *dst_fb, int screen_mode, int palette_idx) {
    if (!r || !dst_fb) return;

    const uint32_t *pal = get_palette(palette_idx, r->game_id);

    /* Clear screen to border color */
    uint32_t bg_col = (r->game_id == G1_GAME_RED) ? 0x1A080C :
                      (r->game_id == G1_GAME_BLUE) ? 0x08121E : 0x1C1808;
    for (int i = 0; i < UI_W * UI_H; i++) dst_fb[i] = bg_col;

    if (screen_mode == 0) {
        /* 1x Centered (160x144 placed at 80, 48) */
        int ox = (UI_W - GB_SCREEN_W) / 2;
        int oy = (UI_H - GB_SCREEN_H) / 2;
        for (int y = 0; y < GB_SCREEN_H; y++) {
            for (int x = 0; x < GB_SCREEN_W; x++) {
                uint32_t shade = r->game_fb[y * GB_SCREEN_W + x] & 3;
                dst_fb[(oy + y) * UI_W + (ox + x)] = pal[shade];
            }
        }
    } else if (screen_mode == 1) {
        /* 1.5x Scaled Aspect-Preserved (240x216 centered at x=40, y=12) */
        int out_w = 240, out_h = 216;
        int ox = (UI_W - out_w) / 2;
        int oy = (UI_H - out_h) / 2;
        for (int y = 0; y < out_h; y++) {
            int src_y = (y * 2) / 3;
            if (src_y >= GB_SCREEN_H) src_y = GB_SCREEN_H - 1;
            for (int x = 0; x < out_w; x++) {
                int src_x = (x * 2) / 3;
                if (src_x >= GB_SCREEN_W) src_x = GB_SCREEN_W - 1;
                uint32_t shade = r->game_fb[src_y * GB_SCREEN_W + src_x] & 3;
                dst_fb[(oy + y) * UI_W + (ox + x)] = pal[shade];
            }
        }
    } else {
        /* 2x width (320) by 1.66x height (240) full stretch */
        for (int y = 0; y < UI_H; y++) {
            int src_y = (y * GB_SCREEN_H) / UI_H;
            if (src_y >= GB_SCREEN_H) src_y = GB_SCREEN_H - 1;
            for (int x = 0; x < UI_W; x++) {
                int src_x = (x * GB_SCREEN_W) / UI_W;
                if (src_x >= GB_SCREEN_W) src_x = GB_SCREEN_W - 1;
                uint32_t shade = r->game_fb[src_y * GB_SCREEN_W + src_x] & 3;
                dst_fb[y * UI_W + x] = pal[shade];
            }
        }
    }
}

void g1_runner_fill_audio(struct g1_runner *r, int16_t *dst_stereo, int frames) {
    if (!dst_stereo || frames <= 0) return;
    /* Clean silence: no loud placeholder square wave */
    memset(dst_stereo, 0, (size_t)frames * 2 * sizeof(int16_t));
    (void)r;
}

