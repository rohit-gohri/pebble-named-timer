#pragma once

#include <pebble.h>

typedef uint16_t (*WrapMenuGetRowCountCb)(void *context);

typedef struct WrapMenuConfig {
  MenuLayer *menu_layer;
  ClickConfigProvider orig_ccp;
  void *orig_ctx;
  WrapMenuGetRowCountCb get_row_count;
  void *row_count_context;
} WrapMenuConfig;

void wrap_menu_attach(WrapMenuConfig *cfg, MenuLayer *menu_layer, Window *window,
                      WrapMenuGetRowCountCb get_row_count, void *row_count_context);
