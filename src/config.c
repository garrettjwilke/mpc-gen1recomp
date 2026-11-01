#include "config.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void config_defaults(struct app_settings *s) {
    if (!s) return;
    memset(s, 0, sizeof(*s));
    s->volume = 80;
    s->speaker_on = true;
    s->preset = PRESET_ARCADE;
    s->turbo_speed = 3;
    s->screen_mode = 1; /* 1.5x letterbox default */
    s->palette = 0;     /* GBC Color */
    snprintf(s->last_game, sizeof(s->last_game), "red");
}

static char *read_file_text(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (sz <= 0 || sz > 256 * 1024) {
        fclose(fp);
        return NULL;
    }
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) {
        fclose(fp);
        return NULL;
    }
    size_t n = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);
    buf[n] = '\0';
    return buf;
}

static bool write_file_text(const char *path, const char *text) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return false;
    size_t len = strlen(text);
    size_t n = fwrite(text, 1, len, fp);
    fclose(fp);
    return n == len;
}

bool config_load_settings(const char *path, struct app_settings *s) {
    if (!path || !s) return false;
    char *txt = read_file_text(path);
    if (!txt) return false;

    cJSON *root = cJSON_Parse(txt);
    free(txt);
    if (!root) return false;

    cJSON *vol = cJSON_GetObjectItemCaseSensitive(root, "volume");
    if (cJSON_IsNumber(vol)) {
        int v = vol->valueint;
        if (v < 0) v = 0;
        if (v > 100) v = 100;
        s->volume = v;
    }

    cJSON *spk = cJSON_GetObjectItemCaseSensitive(root, "speaker_on");
    if (cJSON_IsBool(spk)) {
        s->speaker_on = cJSON_IsTrue(spk);
    }

    cJSON *pre = cJSON_GetObjectItemCaseSensitive(root, "preset");
    if (cJSON_IsNumber(pre)) {
        int p = pre->valueint;
        if (p >= 0 && p < PRESET_COUNT) s->preset = p;
    }

    cJSON *spd = cJSON_GetObjectItemCaseSensitive(root, "turbo_speed");
    if (cJSON_IsNumber(spd)) {
        int sp = spd->valueint;
        if (sp >= 2 && sp <= 8) s->turbo_speed = sp;
    }

    cJSON *scr = cJSON_GetObjectItemCaseSensitive(root, "screen_mode");
    if (cJSON_IsNumber(scr)) {
        int sm = scr->valueint;
        if (sm >= 0 && sm <= 2) s->screen_mode = sm;
    }

    cJSON *pal = cJSON_GetObjectItemCaseSensitive(root, "palette");
    if (cJSON_IsNumber(pal)) {
        int pl = pal->valueint;
        if (pl >= 0 && pl <= 3) s->palette = pl;
    }

    cJSON *gm = cJSON_GetObjectItemCaseSensitive(root, "last_game");
    if (cJSON_IsString(gm) && gm->valuestring) {
        snprintf(s->last_game, sizeof(s->last_game), "%s", gm->valuestring);
    }

    cJSON_Delete(root);
    return true;
}

bool config_save_settings(const char *path, const struct app_settings *s) {
    if (!path || !s) return false;
    cJSON *root = cJSON_CreateObject();
    if (!root) return false;

    cJSON_AddNumberToObject(root, "volume", s->volume);
    cJSON_AddBoolToObject(root, "speaker_on", s->speaker_on);
    cJSON_AddNumberToObject(root, "preset", s->preset);
    cJSON_AddNumberToObject(root, "turbo_speed", s->turbo_speed);
    cJSON_AddNumberToObject(root, "screen_mode", s->screen_mode);
    cJSON_AddNumberToObject(root, "palette", s->palette);
    cJSON_AddStringToObject(root, "last_game", s->last_game);

    char *out = cJSON_Print(root);
    cJSON_Delete(root);
    if (!out) return false;

    bool ok = write_file_text(path, out);
    free(out);
    return ok;
}

bool config_load_bindings(const char *path, struct input_map *map) {
    if (!path || !map) return false;
    char *txt = read_file_text(path);
    if (!txt) return false;

    cJSON *root = cJSON_Parse(txt);
    free(txt);
    if (!root) return false;

    cJSON *preset = cJSON_GetObjectItemCaseSensitive(root, "preset");
    if (cJSON_IsNumber(preset)) {
        int p = preset->valueint;
        if (p >= 0 && p < PRESET_COUNT) {
            map->current_preset = (enum input_preset_id)p;
        }
    }

    cJSON *pads = cJSON_GetObjectItemCaseSensitive(root, "pads");
    if (cJSON_IsArray(pads)) {
        int count = cJSON_GetArraySize(pads);
        if (count > PAD_SLOT_COUNT) count = PAD_SLOT_COUNT;
        for (int i = 0; i < count; i++) {
            cJSON *item = cJSON_GetArrayItem(pads, i);
            if (!cJSON_IsObject(item)) continue;

            cJSON *t = cJSON_GetObjectItemCaseSensitive(item, "type");
            cJSON *c = cJSON_GetObjectItemCaseSensitive(item, "code");
            cJSON *col = cJSON_GetObjectItemCaseSensitive(item, "color");
            cJSON *lbl = cJSON_GetObjectItemCaseSensitive(item, "label");

            enum bind_type btype = cJSON_IsNumber(t) ? (enum bind_type)t->valueint : BIND_TYPE_NONE;
            uint16_t bcode = cJSON_IsNumber(c) ? (uint16_t)c->valueint : G1_ACT_NONE;
            uint32_t bcol = cJSON_IsNumber(col) ? (uint32_t)col->valuedouble : COLOR_DIM;
            const char *blbl = cJSON_IsString(lbl) ? lbl->valuestring : "-";

            input_map_set_binding(map, i, btype, bcode, bcol, blbl);
        }
    }

    cJSON_Delete(root);
    return true;
}

bool config_save_bindings(const char *path, const struct input_map *map) {
    if (!path || !map) return false;
    cJSON *root = cJSON_CreateObject();
    if (!root) return false;

    cJSON_AddNumberToObject(root, "preset", (int)map->current_preset);
    cJSON *pads = cJSON_AddArrayToObject(root, "pads");
    if (!pads) {
        cJSON_Delete(root);
        return false;
    }

    for (int i = 0; i < PAD_SLOT_COUNT; i++) {
        const struct pad_binding *b = &map->pads[i];
        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "slot", i);
        cJSON_AddNumberToObject(item, "type", (int)b->type);
        cJSON_AddNumberToObject(item, "code", (int)b->code);
        cJSON_AddNumberToObject(item, "color", (double)b->color);
        cJSON_AddStringToObject(item, "label", b->label);
        cJSON_AddItemToArray(pads, item);
    }

    char *out = cJSON_Print(root);
    cJSON_Delete(root);
    if (!out) return false;

    bool ok = write_file_text(path, out);
    free(out);
    return ok;
}

