#pragma once

#include <pebble.h>

#define PRESETS_MAX_COUNT 8
#define PRESET_NAME_MAX_LEN 24

void presets_load(void);
void presets_save(void);
void presets_set_slot(uint8_t slot, const char *name);
const char *presets_get_slot(uint8_t slot);
uint8_t presets_get_active_count(void);
const char *presets_get_active_name(uint8_t active_index);
