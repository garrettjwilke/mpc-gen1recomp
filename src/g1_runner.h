#ifndef MPC_G1_RUNNER_H
#define MPC_G1_RUNNER_H

#include <stdbool.h>
#include <stdint.h>
#include "rom_manager.h"
#include "input_map.h"

#define GB_SCREEN_W 160
#define GB_SCREEN_H 144

struct g1_runner;

struct g1_runner *g1_runner_create(enum g1_game_id game, int samplerate);
void g1_runner_destroy(struct g1_runner *r);

void g1_runner_reset(struct g1_runner *r);
void g1_runner_set_paused(struct g1_runner *r, bool paused);
bool g1_runner_is_paused(const struct g1_runner *r);

void g1_runner_set_game(struct g1_runner *r, enum g1_game_id game);
bool g1_runner_load_rom(struct g1_runner *r, const char *rom_path);

/* Backwards-compatible asset loading alias */
bool g1_runner_load_assets(struct g1_runner *r, const char *rom_path);

void g1_runner_tick(struct g1_runner *r, const struct g1_input_state *in);
void g1_runner_render(struct g1_runner *r, uint32_t *dst_fb, int screen_mode, int palette);
void g1_runner_fill_audio(struct g1_runner *r, int16_t *dst_stereo, int frames);

bool g1_runner_save_sram(struct g1_runner *r);
int64_t g1_runner_get_frame_count(const struct g1_runner *r);
bool g1_runner_is_ready(const struct g1_runner *r);

#endif /* MPC_G1_RUNNER_H */
