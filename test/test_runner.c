#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "input_map.h"
#include "config.h"
#include "rom_manager.h"
#include "g1_runner.h"
#include "usb_sd.h"
#include "ui.h"

static void test_presets(void) {
    printf("[TEST] Checking input map presets and names...\n");
    struct input_map map;
    input_map_init(&map);

    assert(map.current_preset == PRESET_ARCADE);
    assert(strcmp(input_map_preset_name(PRESET_ARCADE), "Arcade (Default)") == 0);

    /* Arcade: Pad 0 is Left, Pad 3 is A */
    assert(map.pads[0].code == G1_ACT_LEFT);
    assert(map.pads[3].code == G1_ACT_A);
    assert(map.pads[3].color == COLOR_GREEN);

    /* Apply Ergonomic */
    input_map_apply_preset(&map, PRESET_ERGONOMIC);
    assert(map.current_preset == PRESET_ERGONOMIC);
    assert(map.pads[0].code == G1_ACT_LEFT);
    assert(map.pads[2].code == G1_ACT_B);
    assert(map.pads[3].code == G1_ACT_A);

    /* Apply Classic GB */
    input_map_apply_preset(&map, PRESET_GB_CLASSIC);
    assert(map.current_preset == PRESET_GB_CLASSIC);
    assert(map.pads[0].code == G1_ACT_SELECT);
    assert(map.pads[1].code == G1_ACT_START);

    /* Apply Speedrunner */
    input_map_apply_preset(&map, PRESET_SPEEDRUNNER);
    assert(map.current_preset == PRESET_SPEEDRUNNER);
    assert(map.pads[8].code == G1_ACT_TURBO_HOLD);
    assert(map.pads[9].code == G1_ACT_TURBO);
}

static void test_input_translation_and_rebind(void) {
    printf("[TEST] Input translation, clearing, and re-binding...\n");
    struct input_map map;
    input_map_init(&map);

    struct input in;
    memset(&in, 0, sizeof(in));
    for (int i = 0; i < 16; i++) in.pad_hit[i] = -1;

    /* Press Pad 0 (Left) and Pad 3 (A) */
    in.pad_down[0] = 1;
    in.pad_down[3] = 1;

    struct g1_input_state out;
    input_map_translate(&map, &in, false, &out);
    assert((out.buttons & G1_BTN_LEFT) != 0);
    assert((out.buttons & G1_BTN_A) != 0);
    assert((out.buttons & G1_BTN_RIGHT) == 0);
    assert(!out.turbo_active);

    /* Clear pad 3 */
    input_map_clear_binding(&map, 3);
    assert(map.pads[3].type == BIND_TYPE_NONE);
    assert(map.pads[3].code == G1_ACT_NONE);
    assert(strcmp(map.pads[3].label, "-") == 0);

    input_map_translate(&map, &in, false, &out);
    assert((out.buttons & G1_BTN_LEFT) != 0);
    assert((out.buttons & G1_BTN_A) == 0);

    /* Rebind pad 3 to Turbo Hold */
    input_map_set_binding(&map, 3, BIND_TYPE_ACTION, G1_ACT_TURBO_HOLD, COLOR_PURPLE, "HOLD");
    assert(map.pads[3].code == G1_ACT_TURBO_HOLD);

    input_map_translate(&map, &in, false, &out);
    assert(out.turbo_active == true);

    /* Test bank toggling */
    input_map_toggle_bank(&map);
    assert(input_map_get_bank(&map) == 1);
    /* In Bank B, Pad 3 (Slot 19) is default A */
    assert(map.pads[19].code == G1_ACT_A);
}

static void test_config_roundtrip(void) {
    printf("[TEST] Config and bindings JSON roundtrip persistence...\n");
    const char *tmp_sett = "/tmp/mpc_g1_test_settings.json";
    const char *tmp_bind = "/tmp/mpc_g1_test_bindings.json";

    struct app_settings s_out, s_in;
    config_defaults(&s_out);
    s_out.volume = 90;
    s_out.speaker_on = false;
    s_out.preset = PRESET_SPEEDRUNNER;
    s_out.turbo_speed = 4;
    s_out.screen_mode = 2;
    s_out.palette = 2;
    snprintf(s_out.last_game, sizeof(s_out.last_game), "yellow");

    assert(config_save_settings(tmp_sett, &s_out));
    assert(config_load_settings(tmp_sett, &s_in));

    assert(s_in.volume == 90);
    assert(s_in.speaker_on == false);
    assert(s_in.preset == PRESET_SPEEDRUNNER);
    assert(s_in.turbo_speed == 4);
    assert(s_in.screen_mode == 2);
    assert(s_in.palette == 2);
    assert(strcmp(s_in.last_game, "yellow") == 0);

    /* Test bindings roundtrip */
    struct input_map m_out, m_in;
    input_map_init(&m_out);
    input_map_apply_preset(&m_out, PRESET_GB_CLASSIC);
    input_map_set_binding(&m_out, 0, BIND_TYPE_ACTION, G1_ACT_TURBO, COLOR_PURPLE, "TURBO");

    assert(config_save_bindings(tmp_bind, &m_out));
    input_map_init(&m_in);
    assert(config_load_bindings(tmp_bind, &m_in));

    assert(m_in.current_preset == PRESET_GB_CLASSIC);
    assert(m_in.pads[0].code == G1_ACT_TURBO);
    assert(strcmp(m_in.pads[0].label, "TURBO") == 0);

    unlink(tmp_sett);
    unlink(tmp_bind);
}

static void test_rom_manager_and_extraction(void) {
    printf("[TEST] ROM manager scanning and on-device extraction...\n");
    struct rom_manager rm;
    rom_manager_init(&rm);
    rom_manager_scan(&rm, ".");

    /* Verify all 3 roms are detected */
    assert(rm.games[G1_GAME_RED].rom_found == true);
    assert(rm.games[G1_GAME_BLUE].rom_found == true);
    assert(rm.games[G1_GAME_YELLOW].rom_found == true);
    assert(rom_manager_count_found(&rm) == 3);

    /* Test Extraction flow on Red */
    rom_manager_start_extraction(&rm, G1_GAME_RED);
    assert(rm.extract.active == true);
    assert(rm.extract.percent < 1.0f);

    while (rm.extract.active) {
        rom_manager_step_extraction(&rm, 100);
    }
    assert(rm.extract.finished == true);
    assert(rm.games[G1_GAME_RED].cache_ready == true);
    assert(rom_manager_is_ready(&rm, G1_GAME_RED));
}

static void test_usb_sd_mock(void) {
    printf("[TEST] USB mass storage (SD card) mock...\n");
    ui_usb_sd_set_mock(1, 0);

    struct rom_manager rm;
    rom_manager_init(&rm);

    rom_manager_start_usb(&rm);
    assert(rm.usb_active == true);

    uint32_t buf[UI_W * UI_H];
    memset(buf, 0, sizeof(buf));
    struct ui u = { .px = buf };
    struct input in;
    memset(&in, 0, sizeof(in));

    /* Frame 1: Preparing */
    int rc = rom_manager_draw_usb(&rm, &u, &in);
    assert(rc == UI_USB_SD_ACTIVE);

    /* Frame 2: Active */
    rc = rom_manager_draw_usb(&rm, &u, &in);
    assert(rc == UI_USB_SD_ACTIVE);
    assert(ui_usb_sd_is_active(&rm.usb));

    /* User clicks Exit */
    in.down = (1u << IN_B1);
    rc = rom_manager_draw_usb(&rm, &u, &in);
    assert(rc == UI_USB_SD_ACTIVE);

    in.down = 0;
    rc = rom_manager_draw_usb(&rm, &u, &in);
    assert(rc == UI_USB_SD_DONE);
    assert(rm.usb_active == false);
}

static void test_runner_rendering(void) {
    printf("[TEST] Gen1 Runner tick & multi-mode framebuffer scaling...\n");
    struct g1_runner *r = g1_runner_create(G1_GAME_RED, 44100);
    assert(r != NULL);

    /* Test loading real ROM assets if ROM exists */
    bool loaded = g1_runner_load_assets(r, "roms/pokemon_red.gb");
    if (!loaded) g1_runner_load_assets(r, "user/roms/pokemon_red.gb");
    if (!loaded) g1_runner_load_assets(r, "/media/MPC/roms/pokemon_red.gb");

    struct g1_input_state in;
    memset(&in, 0, sizeof(in));
    in.buttons = G1_BTN_RIGHT;

    g1_runner_tick(r, &in);
    assert(g1_runner_get_frame_count(r) == 1);

    uint32_t fb[UI_W * UI_H];
    memset(fb, 0, sizeof(fb));

    /* Render 1x */
    g1_runner_render(r, fb, 0, 0);
    assert(fb[(48 + 72) * UI_W + (80 + 81)] != 0);

    /* Render 1.5x Aspect-Preserved */
    g1_runner_render(r, fb, 1, 0);
    assert(fb[12 * UI_W + 40] != 0);

    /* Render Full Stretched */
    g1_runner_render(r, fb, 2, 0);
    assert(fb[0] != 0);

    /* Audio buffer generation: default must be silence (no harsh square waves) */
    int16_t audio_buf[256 * 2];
    g1_runner_fill_audio(r, audio_buf, 256);
    int non_zero = 0;
    for (int i = 0; i < 512; i++) {
        if (audio_buf[i] != 0) non_zero++;
    }
    assert(non_zero == 0);

    g1_runner_destroy(r);
}

int main(void) {
    printf("===========================================\n");
    printf("Starting mpc-gen1recomp Host Test Suite\n");
    printf("===========================================\n");

    test_presets();
    test_input_translation_and_rebind();
    test_config_roundtrip();
    test_rom_manager_and_extraction();
    test_usb_sd_mock();
    test_runner_rendering();

    printf("===========================================\n");
    printf("All mpc-gen1recomp unit tests PASSED!\n");
    printf("===========================================\n");
    return 0;
}
