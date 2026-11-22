/*******************************************************************************
 * FILENAME :        name_window.c
 *
 * DESCRIPTION :
 *      Picker window for editing a timer's name:
 *        - Optional remove/clear icon at the top when a name is already set
 *        - Microphone icon to start voice dictation
 *        - Customizable preset name list below
 *
 */

#include <pebble.h>
#include "name_window.h"
#include "presets.h"

struct NameWindow {
  Window *window;
  MenuLayer *menu;
  StatusBarLayer *status;
  GColor highlight_color;
  NameWindowCallbacks callbacks;
  CountdownTimer *timer;
};

static bool prv_has_clear_row(NameWindow *nw) {
  return nw != NULL && countdown_timer_has_name(nw->timer);
}

static uint16_t prv_get_num_sections(MenuLayer *menu_layer, void *context) {
  return 1;
}

static uint16_t prv_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  NameWindow *nw = (NameWindow*)context;
  uint16_t count = 1 + presets_get_active_count();
  if (prv_has_clear_row(nw)) {
    count += 1;
  }
  return count;
}

static int16_t prv_get_cell_height(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
#ifdef PBL_ROUND
  if (menu_layer_get_selected_index(menu_layer).row == cell_index->row) {
    return 56;
  }
  return 46;
#else
  return 46;
#endif
}

static void prv_draw_clear_icon(GContext *ctx, int16_t cx, int16_t cy, GColor fg) {
  graphics_context_set_antialiased(ctx, true);
  graphics_context_set_stroke_color(ctx, fg);
  graphics_context_set_stroke_width(ctx, 2);

  graphics_draw_circle(ctx, GPoint(cx, cy), 11);
  graphics_draw_line(ctx, GPoint(cx - 4, cy - 4), GPoint(cx + 4, cy + 4));
  graphics_draw_line(ctx, GPoint(cx + 4, cy - 4), GPoint(cx - 4, cy + 4));
}

static void prv_draw_mic_icon(GContext *ctx, int16_t cx, int16_t cy, GColor fg) {
  graphics_context_set_antialiased(ctx, true);
  graphics_context_set_fill_color(ctx, fg);
  graphics_context_set_stroke_color(ctx, fg);
  graphics_context_set_stroke_width(ctx, 2);

  // Capsule head (symmetric odd width around cx)
  graphics_fill_rect(ctx, GRect(cx - 4, cy - 12, 9, 14), 4, GCornersAll);

  // U-shaped cradle arc (90 deg = 3 o'clock through 180 deg = 6 o'clock to 270 deg = 9 o'clock)
  graphics_draw_arc(ctx, GRect(cx - 8, cy - 10, 17, 17), GOvalScaleModeFitCircle,
                    DEG_TO_TRIGANGLE(90), DEG_TO_TRIGANGLE(270));

  // Stem and base stand
  graphics_draw_line(ctx, GPoint(cx, cy + 7), GPoint(cx, cy + 12));
  graphics_draw_line(ctx, GPoint(cx - 5, cy + 12), GPoint(cx + 5, cy + 12));
}

static void prv_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index,
                         void *context) {
  NameWindow *nw = (NameWindow*)context;
  GRect bounds = layer_get_bounds(cell_layer);
  GColor fg = menu_cell_layer_is_highlighted(cell_layer)
                ? gcolor_legible_over(nw->highlight_color)
                : GColorBlack;

  bool has_clear = prv_has_clear_row(nw);
  uint16_t row = cell_index->row;

  if (has_clear && row == 0) {
    prv_draw_clear_icon(ctx, bounds.size.w / 2, bounds.size.h / 2, fg);
    return;
  }

  uint16_t mic_row = has_clear ? 1 : 0;
  if (row == mic_row) {
    prv_draw_mic_icon(ctx, bounds.size.w / 2, bounds.size.h / 2, fg);
    return;
  }

  uint8_t preset_index = (uint8_t)(row - mic_row - 1);
  const char *preset_name = presets_get_active_name(preset_index);
  if (preset_name == NULL) {
    return;
  }

#ifdef PBL_ROUND
  int16_t pad_x = 28;
#else
  int16_t pad_x = 12;
#endif
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  GRect text_rect = GRect(pad_x, (bounds.size.h - 28) / 2 - 2, bounds.size.w - pad_x * 2, 30);
  graphics_context_set_text_color(ctx, fg);
  graphics_draw_text(ctx, preset_name, font, text_rect, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
}

static void prv_select_click(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  NameWindow *nw = (NameWindow*)context;
  if (nw == NULL || nw->timer == NULL) {
    return;
  }

  bool has_clear = prv_has_clear_row(nw);
  uint16_t row = cell_index->row;

  if (has_clear && row == 0) {
    if (nw->callbacks.on_clear) {
      nw->callbacks.on_clear(nw->timer, nw->callbacks.context);
    }
    return;
  }

  uint16_t mic_row = has_clear ? 1 : 0;
  if (row == mic_row) {
    if (nw->callbacks.on_dictate) {
      nw->callbacks.on_dictate(nw->timer, nw->callbacks.context);
    }
    return;
  }

  uint8_t preset_index = (uint8_t)(row - mic_row - 1);
  const char *preset_name = presets_get_active_name(preset_index);
  if (preset_name != NULL && nw->callbacks.on_preset) {
    nw->callbacks.on_preset(nw->timer, preset_name, nw->callbacks.context);
  }
}

static void prv_window_load(Window *window) {
  NameWindow *nw = (NameWindow*)window_get_user_data(window);
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_frame(root);

#ifdef PBL_ROUND
  nw->menu = menu_layer_create(bounds);
  menu_layer_set_center_focused(nw->menu, true);
#else
  nw->menu = menu_layer_create(GRect(0, STATUS_BAR_LAYER_HEIGHT, bounds.size.w,
                                     bounds.size.h - STATUS_BAR_LAYER_HEIGHT));
#endif
  menu_layer_set_callbacks(nw->menu, nw, (MenuLayerCallbacks) {
    .get_num_sections = prv_get_num_sections,
    .get_num_rows = prv_get_num_rows,
    .get_cell_height = prv_get_cell_height,
    .draw_row = prv_draw_row,
    .select_click = prv_select_click,
  });
  menu_layer_set_highlight_colors(nw->menu, nw->highlight_color,
                                  gcolor_legible_over(nw->highlight_color));
  menu_layer_set_click_config_onto_window(nw->menu, window);
  layer_add_child(root, menu_layer_get_layer(nw->menu));

  // Default highlight on the mic row when a clear row sits above it so a quick tap
  // won't accidentally clear an existing name, while keeping clear visible right above.
  if (prv_has_clear_row(nw)) {
#ifdef PBL_ROUND
    menu_layer_set_selected_index(nw->menu, MenuIndex(0, 1), MenuRowAlignCenter, false);
#else
    menu_layer_set_selected_index(nw->menu, MenuIndex(0, 1), MenuRowAlignNone, false);
#endif
  }

  nw->status = status_bar_layer_create();
  status_bar_layer_set_colors(nw->status, GColorClear, GColorBlack);
  layer_add_child(root, status_bar_layer_get_layer(nw->status));
}

static void prv_window_unload(Window *window) {
  NameWindow *nw = (NameWindow*)window_get_user_data(window);
  status_bar_layer_destroy(nw->status);
  menu_layer_destroy(nw->menu);
  window_destroy(nw->window);
  nw->status = NULL;
  nw->menu = NULL;
  nw->window = NULL;
}

NameWindow *name_window_create(NameWindowCallbacks callbacks) {
  NameWindow *nw = (NameWindow*)malloc(sizeof(NameWindow));
  if (nw == NULL) {
    return NULL;
  }
  *nw = (NameWindow) {
    .highlight_color = PBL_IF_COLOR_ELSE(GColorPictonBlue, GColorBlack),
    .callbacks = callbacks,
  };
  return nw;
}

void name_window_destroy(NameWindow *name_window) {
  if (name_window == NULL) {
    return;
  }
  if (name_window->window != NULL) {
    window_stack_remove(name_window->window, false);
  }
  free(name_window);
}

void name_window_push(NameWindow *name_window, bool animated) {
  if (name_window == NULL) {
    return;
  }
  if (name_window->window == NULL) {
    name_window->window = window_create();
    window_set_user_data(name_window->window, name_window);
    window_set_window_handlers(name_window->window, (WindowHandlers) {
      .load = prv_window_load,
      .unload = prv_window_unload,
    });
  }
  if (name_window->window != NULL) {
    window_stack_push(name_window->window, animated);
  }
}

void name_window_pop(NameWindow *name_window, bool animated) {
  if (name_window != NULL && name_window->window != NULL) {
    window_stack_remove(name_window->window, animated);
  }
}

void name_window_set_timer(NameWindow *name_window, CountdownTimer *timer) {
  if (name_window != NULL) {
    name_window->timer = timer;
  }
}

void name_window_set_highlight_color(NameWindow *name_window, GColor color) {
  if (name_window != NULL) {
    name_window->highlight_color = color;
    if (name_window->menu != NULL) {
      menu_layer_set_highlight_colors(name_window->menu, color, gcolor_legible_over(color));
    }
  }
}

void name_window_reload_data(NameWindow *name_window) {
  if (name_window != NULL && name_window->menu != NULL) {
    menu_layer_reload_data(name_window->menu);
  }
}
