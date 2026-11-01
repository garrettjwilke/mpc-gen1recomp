#include "g1_runner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>

#define ENABLE_SOUND 1
#define ENABLE_LCD 1
#define PEANUT_GB_12_COLOUR 0
#define PEANUT_GB_HIGH_LCD_ACCURACY 1
#define PEANUT_GB_USE_INTRINSICS 1

#define MINIGB_APU_AUDIO_FORMAT_S16SYS 1
#ifndef AUDIO_SAMPLE_RATE
#define AUDIO_SAMPLE_RATE 44100
#endif

/* Forward declarations required by peanut_gb.h when ENABLE_SOUND is 1 */
uint8_t audio_read(const uint16_t addr);
void audio_write(const uint16_t addr, const uint8_t val);

#include "minigb_apu.h"
#include "peanut_gb.h"

#define AUDIO_RING_FRAMES 8192
#define MAX_AUDIO_LATENCY_FRAMES 2048
#define CART_RAM_MAX_SIZE (32 * 1024) /* 32KB covers all Gen1 MBC1/MBC3 carts */

struct g1_runner {
    enum g1_game_id game_id;
    char rom_path[512];
    char save_path[512];
    bool is_running;
    bool is_paused;
    int samplerate;
    int64_t frame_count;
    uint32_t game_fb[GB_SCREEN_W * GB_SCREEN_H];

    /* Peanut-GB context */
    struct gb_s gb;
    bool gb_initialized;
    uint8_t *rom_data;
    size_t rom_size;

    /* Cartridge battery RAM for persistent game saves */
    uint8_t *cart_ram;
    size_t cart_ram_size;
    bool cart_ram_dirty;
    int64_t last_save_frame;

    /* minigb_apu context */
    struct minigb_apu_ctx apu;

    /* Audio ring buffer (interleaved 16-bit stereo PCM) */
    pthread_mutex_t audio_lock;
    int16_t audio_ring[AUDIO_RING_FRAMES * 2];
    int audio_ring_read;
    int audio_ring_write;
    int audio_ring_count;
};

static struct g1_runner *s_active_runner = NULL;

/* -------------------------------------------------------------------------
 * Audio Callbacks for Peanut-GB
 * ------------------------------------------------------------------------- */
uint8_t audio_read(const uint16_t addr) {
    if (s_active_runner) {
        return minigb_apu_audio_read(&s_active_runner->apu, addr);
    }
    return 0xFF;
}

void audio_write(const uint16_t addr, const uint8_t val) {
    if (s_active_runner) {
        minigb_apu_audio_write(&s_active_runner->apu, addr, val);
    }
}

/* -------------------------------------------------------------------------
 * Memory & LCD Callbacks for Peanut-GB
 * ------------------------------------------------------------------------- */
static uint8_t gb_rom_read(struct gb_s *gb, const uint_fast32_t addr) {
    struct g1_runner *r = (struct g1_runner *)gb->direct.priv;
    if (r && r->rom_data && addr < r->rom_size) {
        return r->rom_data[addr];
    }
    return 0xFF;
}

static uint8_t gb_cart_ram_read(struct gb_s *gb, const uint_fast32_t addr) {
    struct g1_runner *r = (struct g1_runner *)gb->direct.priv;
    if (r && r->cart_ram && addr < r->cart_ram_size) {
        return r->cart_ram[addr];
    }
    return 0xFF;
}

static void gb_cart_ram_write(struct gb_s *gb, const uint_fast32_t addr, const uint8_t val) {
    struct g1_runner *r = (struct g1_runner *)gb->direct.priv;
    if (r && r->cart_ram && addr < r->cart_ram_size) {
        r->cart_ram[addr] = val;
        r->cart_ram_dirty = true;
    }
}

static void gb_error(struct gb_s *gb, const enum gb_error_e err, const uint16_t val) {
    (void)gb;
    fprintf(stderr, "[peanut_gb] error code %d at 0x%04X\n", (int)err, val);
}

static void lcd_draw_line(struct gb_s *gb, const uint8_t *pixels, const uint_fast8_t line) {
    struct g1_runner *r = (struct g1_runner *)gb->direct.priv;
    if (!r || line >= GB_SCREEN_H) return;
    uint32_t *dst = &r->game_fb[line * GB_SCREEN_W];
    for (int x = 0; x < GB_SCREEN_W; x++) {
        dst[x] = pixels[x] & 0x03; /* 2-bit shade: 0=lightest, 3=darkest */
    }
}

/* -------------------------------------------------------------------------
 * Color Palettes (RGB888)
 * ------------------------------------------------------------------------- */
static const uint32_t PAL_DMG[4] = {
    0x9BBC0F, 0x8BAC0F, 0x306230, 0x0F380F
};

static const uint32_t PAL_POCKET[4] = {
    0xFFFFFF, 0xAAAAAA, 0x555555, 0x000000
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
        case 1: /* SGB / Game-specific tint */
            if (game == G1_GAME_RED) return PAL_SGB_RED;
            if (game == G1_GAME_BLUE) return PAL_SGB_BLUE;
            return PAL_SGB_YELLOW;
        case 2: /* DMG Classic */
            return PAL_DMG;
        case 3: /* Pocket B&W */
            return PAL_POCKET;
        case 0: /* Modern / GBC */
        default:
            return PAL_GBC;
    }
}

/* -------------------------------------------------------------------------
 * SRAM Save & Persistence
 * ------------------------------------------------------------------------- */
bool g1_runner_save_sram(struct g1_runner *r) {
    if (!r || !r->cart_ram || !r->cart_ram_dirty || !r->save_path[0]) return false;

    FILE *f = fopen(r->save_path, "wb");
    if (!f) return false;

    size_t written = fwrite(r->cart_ram, 1, r->cart_ram_size, f);
    fclose(f);
    if (written == r->cart_ram_size) {
        r->cart_ram_dirty = false;
        fprintf(stderr, "[G1] Saved cartridge SRAM (%zu bytes) to %s\n", written, r->save_path);
        return true;
    }
    return false;
}

static void load_sram_if_exists(struct g1_runner *r) {
    if (!r || !r->cart_ram || !r->save_path[0]) return;

    FILE *f = fopen(r->save_path, "rb");
    if (f) {
        size_t n = fread(r->cart_ram, 1, r->cart_ram_size, f);
        fclose(f);
        r->cart_ram_dirty = false;
        fprintf(stderr, "[G1] Restored cartridge SRAM (%zu bytes) from %s\n", n, r->save_path);
    }
}

/* -------------------------------------------------------------------------
 * Public Runner Lifecycle
 * ------------------------------------------------------------------------- */
struct g1_runner *g1_runner_create(enum g1_game_id game, int samplerate) {
    struct g1_runner *r = calloc(1, sizeof(*r));
    if (!r) return NULL;

    r->game_id = game;
    r->samplerate = (samplerate > 0) ? samplerate : 44100;
    r->is_running = true;
    r->is_paused = false;
    r->frame_count = 0;
    r->last_save_frame = 0;

    pthread_mutex_init(&r->audio_lock, NULL);
    r->audio_ring_read = 0;
    r->audio_ring_write = 1024;
    r->audio_ring_count = 1024;
    memset(r->audio_ring, 0, 1024 * 2 * sizeof(int16_t));

    s_active_runner = r;
    return r;
}

void g1_runner_destroy(struct g1_runner *r) {
    if (!r) return;

    g1_runner_save_sram(r);

    if (s_active_runner == r) {
        s_active_runner = NULL;
    }

    pthread_mutex_destroy(&r->audio_lock);

    if (r->rom_data) {
        free(r->rom_data);
        r->rom_data = NULL;
    }
    if (r->cart_ram) {
        free(r->cart_ram);
        r->cart_ram = NULL;
    }

    free(r);
}

void g1_runner_reset(struct g1_runner *r) {
    if (!r || !r->gb_initialized) return;

    g1_runner_save_sram(r);
    gb_reset(&r->gb);
    minigb_apu_audio_init(&r->apu);

    pthread_mutex_lock(&r->audio_lock);
    r->audio_ring_read = 0;
    r->audio_ring_write = 1024;
    r->audio_ring_count = 1024;
    memset(r->audio_ring, 0, 1024 * 2 * sizeof(int16_t));
    pthread_mutex_unlock(&r->audio_lock);
}

void g1_runner_set_paused(struct g1_runner *r, bool paused) {
    if (r) r->is_paused = paused;
}

bool g1_runner_is_paused(const struct g1_runner *r) {
    return r ? r->is_paused : false;
}

int64_t g1_runner_get_frame_count(const struct g1_runner *r) {
    return r ? r->frame_count : 0;
}

bool g1_runner_is_ready(const struct g1_runner *r) {
    return r ? r->gb_initialized : false;
}

void g1_runner_set_game(struct g1_runner *r, enum g1_game_id game) {
    if (!r) return;
    if (r->game_id != game) {
        g1_runner_save_sram(r);
        r->game_id = game;
        r->gb_initialized = false;
        if (r->rom_data) {
            free(r->rom_data);
            r->rom_data = NULL;
            r->rom_size = 0;
        }
        if (r->cart_ram) {
            free(r->cart_ram);
            r->cart_ram = NULL;
            r->cart_ram_size = 0;
        }
        r->cart_ram_dirty = false;
    }
}

bool g1_runner_load_rom(struct g1_runner *r, const char *rom_path) {
    if (!r || !rom_path || !rom_path[0]) return false;

    FILE *f = fopen(rom_path, "rb");
    if (!f) {
        fprintf(stderr, "[G1] Cannot open ROM file: %s\n", rom_path);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz < 32768) {
        fprintf(stderr, "[G1] Invalid ROM file size: %ld bytes\n", sz);
        fclose(f);
        return false;
    }

    uint8_t *rom_buf = malloc((size_t)sz);
    if (!rom_buf) {
        fclose(f);
        return false;
    }

    if (fread(rom_buf, 1, (size_t)sz, f) != (size_t)sz) {
        fprintf(stderr, "[G1] Failed to read complete ROM file\n");
        free(rom_buf);
        fclose(f);
        return false;
    }
    fclose(f);

    /* Free previous ROM data */
    if (r->rom_data) free(r->rom_data);
    r->rom_data = rom_buf;
    r->rom_size = (size_t)sz;
    snprintf(r->rom_path, sizeof(r->rom_path), "%s", rom_path);

    /* Allocate cartridge RAM */
    if (!r->cart_ram) {
        r->cart_ram_size = CART_RAM_MAX_SIZE;
        r->cart_ram = calloc(1, r->cart_ram_size);
    }
    r->cart_ram_dirty = false;

    /* Build save file path under user/<tag>/<tag>.sav */
    const char *tag = (r->game_id == G1_GAME_RED) ? "red" :
                      (r->game_id == G1_GAME_BLUE) ? "blue" : "yellow";
    mkdir("user", 0755);
    char dir[128];
    snprintf(dir, sizeof(dir), "user/%s", tag);
    mkdir(dir, 0755);
    snprintf(r->save_path, sizeof(r->save_path), "%s/%s.sav", dir, tag);

    /* Restore previous save file if present */
    load_sram_if_exists(r);

    /* Initialize APU */
    minigb_apu_audio_init(&r->apu);

    /* Initialize Peanut-GB */
    s_active_runner = r;
    enum gb_init_error_e err = gb_init(&r->gb,
                                       gb_rom_read,
                                       gb_cart_ram_read,
                                       gb_cart_ram_write,
                                       gb_error,
                                       r);
    if (err != GB_INIT_NO_ERROR) {
        fprintf(stderr, "[G1] gb_init failed with error %d\n", (int)err);
        return false;
    }

    gb_init_lcd(&r->gb, lcd_draw_line);
    r->gb_initialized = true;

    pthread_mutex_lock(&r->audio_lock);
    r->audio_ring_read = 0;
    r->audio_ring_write = 1024;
    r->audio_ring_count = 1024;
    memset(r->audio_ring, 0, 1024 * 2 * sizeof(int16_t));
    pthread_mutex_unlock(&r->audio_lock);

    fprintf(stderr, "[G1] Peanut-GB loaded %s successfully (%zu bytes)\n", rom_path, r->rom_size);
    return true;
}

bool g1_runner_load_assets(struct g1_runner *r, const char *rom_path) {
    return g1_runner_load_rom(r, rom_path);
}

/* -------------------------------------------------------------------------
 * Simulation Tick & Joypad
 * ------------------------------------------------------------------------- */
void g1_runner_tick(struct g1_runner *r, const struct g1_input_state *in) {
    if (!r || !r->is_running || r->is_paused || !r->gb_initialized) return;

    s_active_runner = r;

    /* Game Boy Joypad register P1 is active-low: 0 = pressed, 1 = unpressed */
    uint8_t pad = 0xFF;
    if (in) {
        if (in->buttons & G1_BTN_A)      pad &= ~JOYPAD_A;
        if (in->buttons & G1_BTN_B)      pad &= ~JOYPAD_B;
        if (in->buttons & G1_BTN_SELECT) pad &= ~JOYPAD_SELECT;
        if (in->buttons & G1_BTN_START)  pad &= ~JOYPAD_START;
        if (in->buttons & G1_BTN_RIGHT)  pad &= ~JOYPAD_RIGHT;
        if (in->buttons & G1_BTN_LEFT)   pad &= ~JOYPAD_LEFT;
        if (in->buttons & G1_BTN_UP)     pad &= ~JOYPAD_UP;
        if (in->buttons & G1_BTN_DOWN)   pad &= ~JOYPAD_DOWN;
    }
    r->gb.direct.joypad = pad;

    /* Turbo support: run 2 frames per tick when turbo is active */
    int frames_to_run = (in && in->turbo_active) ? 2 : 1;
    for (int f = 0; f < frames_to_run; f++) {
        gb_run_frame(&r->gb);
        r->frame_count++;

        /* Synthesize audio samples for this frame */
        int16_t frame_audio[AUDIO_SAMPLES_TOTAL];
        minigb_apu_audio_callback(&r->apu, frame_audio);

        /* Push into ring buffer with adaptive drift compensation */
        pthread_mutex_lock(&r->audio_lock);

        int copy_frames = AUDIO_SAMPLES; // 738
        if (r->audio_ring_count < 768) {
            /* Gently nudge up buffer level */
            copy_frames = AUDIO_SAMPLES + 1; // 739
        } else if (r->audio_ring_count > 1536) {
            /* Gently nudge down buffer level */
            copy_frames = AUDIO_SAMPLES - 1; // 737
        }

        /* Protect against extreme hitch recovery */
        if (r->audio_ring_count > 3072) {
            int drop = r->audio_ring_count - 1024;
            r->audio_ring_read = (r->audio_ring_read + drop) % AUDIO_RING_FRAMES;
            r->audio_ring_count = 1024;
        }

        for (int i = 0; i < copy_frames; i++) {
            int src_i = (i < (int)AUDIO_SAMPLES) ? i : ((int)AUDIO_SAMPLES - 1);
            int idx = (r->audio_ring_write + i) % AUDIO_RING_FRAMES;
            r->audio_ring[idx * 2 + 0] = frame_audio[src_i * 2 + 0];
            r->audio_ring[idx * 2 + 1] = frame_audio[src_i * 2 + 1];
        }
        r->audio_ring_write = (r->audio_ring_write + copy_frames) % AUDIO_RING_FRAMES;
        r->audio_ring_count += copy_frames;

        pthread_mutex_unlock(&r->audio_lock);
    }

    /* Auto-save SRAM every ~5 seconds (300 frames) if dirty */
    if (r->cart_ram_dirty && (r->frame_count - r->last_save_frame > 300)) {
        g1_runner_save_sram(r);
        r->last_save_frame = r->frame_count;
    }
}

/* -------------------------------------------------------------------------
 * Audio Callback for ALSA Audio Engine
 * ------------------------------------------------------------------------- */
void g1_runner_fill_audio(struct g1_runner *r, int16_t *dst_stereo, int frames) {
    if (!dst_stereo || frames <= 0) return;

    if (!r || !r->is_running || r->is_paused || !r->gb_initialized) {
        memset(dst_stereo, 0, (size_t)frames * 2 * sizeof(int16_t));
        return;
    }

    pthread_mutex_lock(&r->audio_lock);
    int available = r->audio_ring_count;
    int to_copy = (available < frames) ? available : frames;

    for (int i = 0; i < to_copy; i++) {
        int idx = (r->audio_ring_read + i) % AUDIO_RING_FRAMES;
        dst_stereo[i * 2 + 0] = r->audio_ring[idx * 2 + 0];
        dst_stereo[i * 2 + 1] = r->audio_ring[idx * 2 + 1];
    }
    r->audio_ring_read = (r->audio_ring_read + to_copy) % AUDIO_RING_FRAMES;
    r->audio_ring_count -= to_copy;
    pthread_mutex_unlock(&r->audio_lock);

    /* If underrun, pad remainder with silence */
    if (to_copy < frames) {
        memset(dst_stereo + to_copy * 2, 0, (size_t)(frames - to_copy) * 2 * sizeof(int16_t));
    }
}

/* -------------------------------------------------------------------------
 * Video Rendering & Display Scaling
 * ------------------------------------------------------------------------- */
void g1_runner_render(struct g1_runner *r, uint32_t *dst_fb, int screen_mode, int palette_idx) {
    if (!dst_fb) return;

    const uint32_t *pal = get_palette(palette_idx, r ? r->game_id : G1_GAME_RED);

    /* Background border color */
    uint32_t bg_col = (!r) ? 0x10141E :
                      (r->game_id == G1_GAME_RED) ? 0x1A080C :
                      (r->game_id == G1_GAME_BLUE) ? 0x08121E : 0x1C1808;
    for (int i = 0; i < UI_W * UI_H; i++) dst_fb[i] = bg_col;

    if (!r || !r->gb_initialized) return;

    if (screen_mode == 0) {
        /* 1x Centered (160x144 placed at 80, 48 in 320x240) */
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
        /* Full Fill Stretch (320x240) */
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
