#include <pebble.h>
#include <string.h>
#include "presets.h"

#define PRESETS_PERSIST_KEY 88210401

typedef struct {
  char names[PRESETS_MAX_COUNT][PRESET_NAME_MAX_LEN];
} PresetStore;

static const char *s_default_presets[PRESETS_MAX_COUNT] = {
  "Coffee",
  "Laundry",
  "Tea",
  "Cooking",
  "Workout",
  "Focus",
  "Eggs",
  "Nap",
};

static PresetStore s_store;

static void prv_copy_trimmed(char *dst, size_t dst_size, const char *src) {
  if (!dst || dst_size == 0) {
    return;
  }
  dst[0] = '\0';
  if (!src) {
    return;
  }
  while (*src == ' ' || *src == '\t' || *src == '\r' || *src == '\n') {
    src++;
  }
  size_t len = strlen(src);
  while (len > 0 && (src[len - 1] == ' ' || src[len - 1] == '\t' ||
                     src[len - 1] == '\r' || src[len - 1] == '\n')) {
    len--;
  }
  if (len >= dst_size) {
    len = dst_size - 1;
  }
  if (len > 0) {
    memcpy(dst, src, len);
  }
  dst[len] = '\0';
}

static void prv_load_defaults(void) {
  memset(&s_store, 0, sizeof(s_store));
  for (uint8_t i = 0; i < PRESETS_MAX_COUNT; i++) {
    prv_copy_trimmed(s_store.names[i], PRESET_NAME_MAX_LEN, s_default_presets[i]);
  }
}

void presets_load(void) {
  if (persist_exists(PRESETS_PERSIST_KEY) &&
      persist_get_size(PRESETS_PERSIST_KEY) == (int)sizeof(PresetStore)) {
    persist_read_data(PRESETS_PERSIST_KEY, &s_store, sizeof(PresetStore));
    for (uint8_t i = 0; i < PRESETS_MAX_COUNT; i++) {
      s_store.names[i][PRESET_NAME_MAX_LEN - 1] = '\0';
    }
  } else {
    prv_load_defaults();
  }
}

void presets_save(void) {
  persist_write_data(PRESETS_PERSIST_KEY, &s_store, sizeof(PresetStore));
}

void presets_set_slot(uint8_t slot, const char *name) {
  if (slot >= PRESETS_MAX_COUNT) {
    return;
  }
  prv_copy_trimmed(s_store.names[slot], PRESET_NAME_MAX_LEN, name);
}

const char *presets_get_slot(uint8_t slot) {
  if (slot >= PRESETS_MAX_COUNT) {
    return "";
  }
  return s_store.names[slot];
}

uint8_t presets_get_active_count(void) {
  uint8_t count = 0;
  for (uint8_t i = 0; i < PRESETS_MAX_COUNT; i++) {
    if (s_store.names[i][0] != '\0') {
      count++;
    }
  }
  return count;
}

const char *presets_get_active_name(uint8_t active_index) {
  uint8_t seen = 0;
  for (uint8_t i = 0; i < PRESETS_MAX_COUNT; i++) {
    if (s_store.names[i][0] != '\0') {
      if (seen == active_index) {
        return s_store.names[i];
      }
      seen++;
    }
  }
  return NULL;
}
