#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "app.h"
#include "input.h"
#include "ui.h"

#include "audio_engine.h"
#include "config.h"
#include "input_map.h"
#include "rom_manager.h"
#include "g1_runner.h"
#include "ui_overlay.h"

struct app_state {
    struct g1_runner *runner;
    struct input_map map;
    struct app_settings settings;
    struct rom_manager rm;
    struct ui_overlay overlay;
    struct audio_engine *audio;
    bool audio_active;
    int64_t start_mono_ns;

    /* Fixed 60Hz / Game Boy VSYNC decoupled timing */
    int64_t next_tick_ns;
    int encoder_accum;
    int pad_latched[16];
    int pad_latch_vel[16];
};

#define TICK_NS 16742706LL // Game Boy VSYNC: 70224 * 10^9 / 4194304 = 16.742706 ms (~59.7275 Hz)

static int64_t mono_ns(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (int64_t)t.tv_sec * 1000000000LL + (int64_t)t.tv_nsec;
}

static int ticks_due(int64_t now, int64_t *next) {
    if (*next <= 0) {
        *next = now + TICK_NS;
        return 1;
    }
    int n = 0;
    while (now >= *next && n < 2) {
        *next += TICK_NS;
        n++;
    }
    if (now >= *next)
        *next = now + TICK_NS;
    return n;
}

static int wake_for(int64_t now_ns, int64_t next_ns, bool playing) {
    if (!playing) return 16;
    if (next_ns <= now_ns) return 1;
    int64_t ms = (next_ns - now_ns + 999999LL) / 1000000LL;
    if (ms < 1) return 1;
    if (ms > 16) return 16;
    return (int)ms;
}

static void latch_game_input(struct app_state *app, const struct input *in) {
    app->encoder_accum += in->encoder;
    for (int p = 0; p < 16; p++) {
        if (in->pad_hit[p] >= 0) {
            app->pad_latched[p] = 1;
            app->pad_latch_vel[p] = in->pad_hit[p];
        }
    }
}

static void app_audio_fill(void *ctx, int16_t *dst, int frames) {
    struct app_state *app = (struct app_state *)ctx;
    if (app && app->runner) {
        g1_runner_fill_audio(app->runner, dst, frames);
    } else if (dst && frames > 0) {
        memset(dst, 0, (size_t)frames * 2 * sizeof(int16_t));
    }
}

static void audio_set_active(struct app_state *app, bool active) {
    if (!app->audio || app->audio_active == active) return;
    audio_engine_set_active(app->audio, active);
    app->audio_active = active;
}

static int frame_cb(struct ui *u, const struct input *in, void *state) {
    struct app_state *app = (struct app_state *)state;
    if (!app) return 1;

    int64_t now_ns = mono_ns();
    u->now_ms = (now_ns - app->start_mono_ns) / 1000000LL;

    /* Handle volume knob */
    if (in->main_moved) {
        int vol = (in->main * 100) / 127;
        if (vol < 0) vol = 0;
        if (vol > 100) vol = 100;
        app->settings.volume = vol;
        if (app->audio) {
            audio_engine_set_volume(app->audio, in->main);
        }
    }

    /* Shift + Stop immediate clean exit to boot menu / system */
    if ((in->held & (1u << IN_SHIFT)) && (in->down & (1u << IN_STOP))) {
        return 1;
    }

    /* Stop button toggles Game Menu */
    if (in->down & (1u << IN_STOP)) {
        if (app->overlay.mode == UI_MODE_GAMEPLAY) {
            ui_overlay_set_mode(&app->overlay, UI_MODE_GAME_MENU);
        }
    }

    /* Play button resumes gameplay */
    if (in->down & (1u << IN_PLAY)) {
        if (app->overlay.mode != UI_MODE_GAMEPLAY && rom_manager_is_ready(&app->rm, (enum g1_game_id)app->rm.selected_game)) {
            ui_overlay_set_mode(&app->overlay, UI_MODE_GAMEPLAY);
            app->next_tick_ns = 0;
        }
    }

    /* Update Pad RGB LEDs (0..15) using ui_led */
    for (int p = 0; p < 16; p++) {
        uint8_t r, g, b;
        input_map_get_pad_led(&app->map, in, p, &r, &g, &b);
        ui_led(u, p, r, g, b);
    }

    /* Surface meters & button LEDs */
    if (app->audio) {
        int l_mask = 0, r_mask = 0;
        audio_engine_get_meter_masks(app->audio, &l_mask, &r_mask);
        u->meter_left = (unsigned char)l_mask;
        u->meter_right = (unsigned char)r_mask;
    }

    bool is_gameplay = (app->overlay.mode == UI_MODE_GAMEPLAY);
    ui_button_led(u, IN_PLAY, is_gameplay ? 2 : 0);
    ui_button_led(u, IN_STOP, is_gameplay ? 0 : 2);
    ui_button_led(u, IN_PAD_BANK, (input_map_get_bank(&app->map) == 0) ? 1 : 2);
    ui_button_led(u, IN_ERASE, (app->overlay.mode == UI_MODE_REBIND) ? 2 : 0);

    /* Mute/unmute audio based on speaker setting and gameplay state */
    bool should_play_audio = is_gameplay && app->settings.speaker_on;
    audio_set_active(app, should_play_audio);

    /* Fixed-rate simulation timing decoupled from UI events */
    int ticks = 0;
    struct input tick_in = *in;

    if (is_gameplay && app->runner && g1_runner_is_ready(app->runner) && !g1_runner_is_paused(app->runner)) {
        latch_game_input(app, in);
        ticks = ticks_due(now_ns, &app->next_tick_ns);
        if (ticks > 0) {
            tick_in.encoder = app->encoder_accum;
            for (int p = 0; p < 16; p++) {
                if (app->pad_latched[p] && !tick_in.pad_down[p]) {
                    tick_in.pad_down[p] = 1;
                    if (tick_in.pad_vel[p] <= 0)
                        tick_in.pad_vel[p] = app->pad_latch_vel[p];
                }
            }
            app->encoder_accum = 0;
            memset(app->pad_latched, 0, sizeof(app->pad_latched));
        }
    } else {
        app->next_tick_ns = 0;
        app->encoder_accum = 0;
        memset(app->pad_latched, 0, sizeof(app->pad_latched));
    }

    /* Overlay and UI update */
    int should_quit = ui_overlay_draw(&app->overlay, u, &tick_in, app->runner,
                                      &app->map, &app->settings, &app->rm, ticks);

    /* Wake exactly when next tick is due, or 16ms for menus */
    ui_wake(u, wake_for(now_ns, app->next_tick_ns, is_gameplay));

    return should_quit;
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    const char *app_dir = getenv("MPC_APP_DIR");
    if (app_dir && app_dir[0]) {
        chdir(app_dir);
    }
    fprintf(stderr, "[G1] Starting Gen 1 Recomp (app_dir=%s)\n", app_dir ? app_dir : ".");

    mkdir("user", 0755);
    const char *bind_path = "user/bindings.json";
    const char *sett_path = "user/settings.json";

    struct app_state app;
    memset(&app, 0, sizeof(app));
    app.start_mono_ns = mono_ns();

    config_defaults(&app.settings);
    config_load_settings(sett_path, &app.settings);

    input_map_init(&app.map);
    config_load_bindings(bind_path, &app.map);

    rom_manager_init(&app.rm);
    rom_manager_scan(&app.rm, NULL);

    fprintf(stderr, "[G1] Scan found %d ROMs, %d cache ready\n",
            rom_manager_count_found(&app.rm), rom_manager_count_ready(&app.rm));
    for (int g = 0; g < G1_GAME_COUNT; g++) {
        if (app.rm.games[g].rom_found) {
            fprintf(stderr, "     - %s at %s\n", app.rm.games[g].display_name, app.rm.games[g].rom_path);
        }
    }

    /* Determine initial selected game */
    for (int g = 0; g < G1_GAME_COUNT; g++) {
        if (strcmp(app.settings.last_game, app.rm.games[g].tag) == 0) {
            app.rm.selected_game = g;
            break;
        }
    }

    /* Audio setup */
    app.audio = audio_engine_create(44100, 512);
    if (app.audio) {
        audio_engine_set_source(app.audio, app_audio_fill, &app);
        audio_engine_start(app.audio);
    }

    /* Runner setup */
    app.runner = g1_runner_create((enum g1_game_id)app.rm.selected_game, 44100);
    if (app.runner) {
        g1_runner_load_assets(app.runner, app.rm.games[app.rm.selected_game].rom_path);
    }

    /* Overlay setup */
    ui_overlay_init(&app.overlay, bind_path, sett_path);

    /* Initial screen state determination */
    if (rom_manager_count_ready(&app.rm) > 0 && app.rm.games[app.rm.selected_game].cache_ready) {
        ui_overlay_set_mode(&app.overlay, UI_MODE_GAMEPLAY);
    } else if (app.rm.games[app.rm.selected_game].rom_found) {
        rom_manager_start_extraction(&app.rm, (enum g1_game_id)app.rm.selected_game);
        ui_overlay_set_mode(&app.overlay, UI_MODE_EXTRACTION);
    } else if (rom_manager_count_found(&app.rm) > 0) {
        for (int g = 0; g < G1_GAME_COUNT; g++) {
            if (app.rm.games[g].rom_found) {
                app.rm.selected_game = g;
                rom_manager_start_extraction(&app.rm, (enum g1_game_id)g);
                ui_overlay_set_mode(&app.overlay, UI_MODE_EXTRACTION);
                break;
            }
        }
    } else {
        /* No ROMs at all: launch directly into USB SD card transfer screen */
        rom_manager_start_usb(&app.rm);
        ui_overlay_set_mode(&app.overlay, UI_MODE_USB_STORAGE);
    }
    fprintf(stderr, "[G1] Initial mode=%d, selected=%s\n",
            app.overlay.mode, app.rm.games[app.rm.selected_game].display_name);

    /* Run the application through mpc-ui-toolkit */
    app_run(frame_cb, &app);

    /* Cleanup & auto-save */
    config_save_settings(sett_path, &app.settings);
    config_save_bindings(bind_path, &app.map);

    if (app.audio) {
        audio_engine_stop(app.audio);
        audio_engine_destroy(app.audio);
    }
    if (app.runner) {
        g1_runner_destroy(app.runner);
    }

    return 0;
}

