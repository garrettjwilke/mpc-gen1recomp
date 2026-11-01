#ifndef MPC_G1_CONFIG_H
#define MPC_G1_CONFIG_H

#include "input_map.h"
#include <stdbool.h>

struct app_settings {
    int volume;             /* 0..100 */
    bool speaker_on;        /* true = internal speaker on */
    int preset;             /* enum input_preset_id */
    int turbo_speed;        /* 2, 3, 4 */
    int screen_mode;        /* 0 = 1x, 1 = 1.5x, 2 = stretched */
    int palette;            /* 0 = GBC, 1 = SGB, 2 = DMG, 3 = Pocket */
    char last_game[32];     /* "red", "blue", "yellow" */
};

void config_defaults(struct app_settings *s);

bool config_load_settings(const char *path, struct app_settings *s);
bool config_save_settings(const char *path, const struct app_settings *s);

bool config_load_bindings(const char *path, struct input_map *map);
bool config_save_bindings(const char *path, const struct input_map *map);

#endif /* MPC_G1_CONFIG_H */

