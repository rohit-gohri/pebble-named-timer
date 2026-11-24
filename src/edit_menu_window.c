/*******************************************************************************
 * FILENAME :        edit_menu_window.c
 *
 * DESCRIPTION :
 *      Wrap-around ActionMenu-styled picker for editing a timer's time,
 *      name, or snooze duration.
 *
 */

#include <pebble.h>
#include "edit_menu_window.h"
#include "wrap_menu.h"

#define EDIT_MENU_LEFT_BAR_WIDTH 16
#define EDIT_MENU_ROUND_RING_WIDTH 14

struct EditMenuWindow {
  Window *window;
  MenuLayer *menu;
  Layer *chrome_layer;
  WrapMenuConfig wrap_menu;
  GColor highlight_color;
  EditMenuWindowCallbacks callbacks;
  CountdownTimer *timer;
};

static const char * const s_labels[EDIT_MENU_ACTION_COUNT] = {
  "Edit Time",
  "Edit Name",
  "Edit Snooze",
};

static uint16_t prv_get_num_sections(MenuLayer *menu_layer, void *context) {
  return 1;
}

static uint16_t prv_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  return EDIT_MENU_ACTION_COUNT;
}

static uint16_t prv_edit_menu_get_row_count(void *context) {
  return EDIT_MENU_ACTION_COUNT;
}

static int16_t prv_get_cell_height(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  return PBL_IF_ROUND_ELSE(50, 48);
}

static void prv_chrome_update_proc(Layer *layer, GContext *ctx) {
  EditMenuWindow *emw = *(EditMenuWindow **)layer_get_data(layer);
  GRect bounds = layer_get_bounds(layer);

  graphics_context_set_fill_color(ctx, emw->highlight_color);
#ifdef PBL_ROUND
  graphics_fill_radial(ctx, bounds, GOvalScaleModeFitCircle, EDIT_MENU_ROUND_RING_WIDTH,
                       0, TRIG_MAX_ANGLE);
#else
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, GPoint(EDIT_MENU_LEFT_BAR_WIDTH / 2, 12), 2);
#endif
}

static void prv_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index,
                         void *context) {
  if (cell_index->row >= EDIT_MENU_ACTION_COUNT) {
    return;
  }

  GRect bounds = layer_get_bounds(cell_layer);
  bool highlighted = menu_cell_layer_is_highlighted(cell_layer);
  GColor fg = highlighted ? GColorWhite : PBL_IF_COLOR_ELSE(GColorDarkGray, GColorWhite);
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  const char *label = s_labels[cell_index->row];

#ifdef PBL_ROUND
  int16_t pad_x = 24;
  GTextAlignment align = GTextAlignmentCenter;
#else
  int16_t pad_x = 12;
  GTextAlignment align = GTextAlignmentLeft;
#endif
  int16_t text_y = (bounds.size.h - 32) / 2 - 2;
  GRect text_rect = GRect(pad_x, text_y, bounds.size.w - pad_x * 2, 34);

  graphics_context_set_text_color(ctx, fg);
  graphics_draw_text(ctx, label, font, text_rect, GTextOverflowModeTrailingEllipsis,
                     align, NULL);
}

static void prv_select_click(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  EditMenuWindow *emw = (EditMenuWindow *)context;
  if (emw == NULL || cell_index->row >= EDIT_MENU_ACTION_COUNT) {
    return;
  }

  CountdownTimer *timer = emw->timer;
  EditMenuAction action = (EditMenuAction)cell_index->row;
  edit_menu_window_pop(emw, false);

  if (emw->callbacks.on_select != NULL) {
    emw->callbacks.on_select(timer, action, emw->callbacks.context);
  }
}

static void prv_window_load(Window *window) {
  EditMenuWindow *emw = (EditMenuWindow *)window_get_user_data(window);
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_frame(root);

  window_set_background_color(window, GColorBlack);

#ifdef PBL_ROUND
  emw->menu = menu_layer_create(bounds);
  emw->chrome_layer = layer_create_with_data(bounds, sizeof(EditMenuWindow *));
#else
  emw->menu = menu_layer_create(GRect(EDIT_MENU_LEFT_BAR_WIDTH, 0,
                                      bounds.size.w - EDIT_MENU_LEFT_BAR_WIDTH,
                                      bounds.size.h));
  emw->chrome_layer = layer_create_with_data(GRect(0, 0, EDIT_MENU_LEFT_BAR_WIDTH, bounds.size.h),
                                             sizeof(EditMenuWindow *));
#endif
  *(EditMenuWindow **)layer_get_data(emw->chrome_layer) = emw;
  layer_set_update_proc(emw->chrome_layer, prv_chrome_update_proc);

  menu_layer_set_center_focused(emw->menu, true);
  menu_layer_pad_bottom_enable(emw->menu, false);
  menu_layer_set_normal_colors(emw->menu, GColorBlack,
                               PBL_IF_COLOR_ELSE(GColorDarkGray, GColorWhite));
  menu_layer_set_highlight_colors(emw->menu, GColorBlack, GColorWhite);
  menu_layer_set_callbacks(emw->menu, emw, (MenuLayerCallbacks) {
    .get_num_sections = prv_get_num_sections,
    .get_num_rows = prv_get_num_rows,
    .get_cell_height = prv_get_cell_height,
    .draw_row = prv_draw_row,
    .select_click = prv_select_click,
  });

  wrap_menu_attach(&emw->wrap_menu, emw->menu, window, prv_edit_menu_get_row_count, emw);

  layer_add_child(root, menu_layer_get_layer(emw->menu));
  layer_add_child(root, emw->chrome_layer);
}

static void prv_window_unload(Window *window) {
  EditMenuWindow *emw = (EditMenuWindow *)window_get_user_data(window);
  layer_destroy(emw->chrome_layer);
  menu_layer_destroy(emw->menu);
  window_destroy(emw->window);
  emw->chrome_layer = NULL;
  emw->menu = NULL;
  emw->window = NULL;
}

EditMenuWindow *edit_menu_window_create(EditMenuWindowCallbacks callbacks) {
  EditMenuWindow *emw = (EditMenuWindow *)malloc(sizeof(EditMenuWindow));
  if (emw == NULL) {
    return NULL;
  }
  *emw = (EditMenuWindow) {
    .highlight_color = PBL_IF_COLOR_ELSE(GColorPictonBlue, GColorWhite),
    .callbacks = callbacks,
  };
  return emw;
}

void edit_menu_window_destroy(EditMenuWindow *edit_menu_window) {
  if (edit_menu_window == NULL) {
    return;
  }
  if (edit_menu_window->window != NULL) {
    window_stack_remove(edit_menu_window->window, false);
  }
  free(edit_menu_window);
}

void edit_menu_window_push(EditMenuWindow *edit_menu_window, bool animated) {
  if (edit_menu_window == NULL) {
    return;
  }
  if (edit_menu_window->window == NULL) {
    edit_menu_window->window = window_create();
    window_set_user_data(edit_menu_window->window, edit_menu_window);
    window_set_window_handlers(edit_menu_window->window, (WindowHandlers) {
      .load = prv_window_load,
      .unload = prv_window_unload,
    });
  }
  if (edit_menu_window->window != NULL) {
    window_stack_push(edit_menu_window->window, animated);
  }
}

void edit_menu_window_pop(EditMenuWindow *edit_menu_window, bool animated) {
  if (edit_menu_window != NULL && edit_menu_window->window != NULL) {
    window_stack_remove(edit_menu_window->window, animated);
  }
}

void edit_menu_window_set_timer(EditMenuWindow *edit_menu_window, CountdownTimer *timer) {
  if (edit_menu_window != NULL) {
    edit_menu_window->timer = timer;
  }
}

CountdownTimer *edit_menu_window_get_timer(EditMenuWindow *edit_menu_window) {
  if (edit_menu_window == NULL) {
    return NULL;
  }
  return edit_menu_window->timer;
}

void edit_menu_window_set_highlight_color(EditMenuWindow *edit_menu_window, GColor color) {
  if (edit_menu_window != NULL) {
    edit_menu_window->highlight_color = color;
    if (edit_menu_window->chrome_layer != NULL) {
      layer_mark_dirty(edit_menu_window->chrome_layer);
    }
  }
}
