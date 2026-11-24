#include "wrap_menu.h"

#define WRAP_MENU_HOLD_REPEAT_MS 100

static void prv_move_selection(WrapMenuConfig *cfg, bool up) {
  if (cfg == NULL || cfg->menu_layer == NULL || cfg->get_row_count == NULL) {
    return;
  }
  uint16_t count = cfg->get_row_count(cfg->row_count_context);
  if (count <= 1) {
    return;
  }

  MenuIndex sel = menu_layer_get_selected_index(cfg->menu_layer);
  if (up) {
    if (sel.row == 0) {
      menu_layer_set_selected_index(cfg->menu_layer, MenuIndex(0, count - 1),
                                    MenuRowAlignCenter, true);
    } else {
      menu_layer_set_selected_next(cfg->menu_layer, true, MenuRowAlignCenter, true);
    }
  } else {
    if (sel.row >= count - 1) {
      menu_layer_set_selected_index(cfg->menu_layer, MenuIndex(0, 0),
                                    MenuRowAlignCenter, true);
    } else {
      menu_layer_set_selected_next(cfg->menu_layer, false, MenuRowAlignCenter, true);
    }
  }
}

static void prv_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  prv_move_selection((WrapMenuConfig *)context, true);
}

static void prv_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  prv_move_selection((WrapMenuConfig *)context, false);
}

static void prv_wrap_ccp(void *context) {
  WrapMenuConfig *cfg = (WrapMenuConfig *)context;
  if (cfg == NULL) {
    return;
  }

  // Restore the MenuLayer/ScrollLayer click context before invoking its original CCP
  // so SELECT handlers receive the MenuLayer pointer they expect.
  window_set_click_context(BUTTON_ID_UP, cfg->orig_ctx);
  window_set_click_context(BUTTON_ID_DOWN, cfg->orig_ctx);
  window_set_click_context(BUTTON_ID_SELECT, cfg->orig_ctx);
  if (cfg->orig_ccp != NULL) {
    cfg->orig_ccp(cfg->orig_ctx);
  }

  window_set_click_context(BUTTON_ID_UP, cfg);
  window_set_click_context(BUTTON_ID_DOWN, cfg);
  window_single_repeating_click_subscribe(BUTTON_ID_UP, WRAP_MENU_HOLD_REPEAT_MS,
                                          prv_up_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, WRAP_MENU_HOLD_REPEAT_MS,
                                          prv_down_click_handler);
}

void wrap_menu_attach(WrapMenuConfig *cfg, MenuLayer *menu_layer, Window *window,
                      WrapMenuGetRowCountCb get_row_count, void *row_count_context) {
  if (cfg == NULL || menu_layer == NULL || window == NULL) {
    return;
  }

  menu_layer_set_click_config_onto_window(menu_layer, window);
  *cfg = (WrapMenuConfig) {
    .menu_layer = menu_layer,
    .orig_ccp = window_get_click_config_provider(window),
    .orig_ctx = window_get_click_config_context(window),
    .get_row_count = get_row_count,
    .row_count_context = row_count_context,
  };
  window_set_click_config_provider_with_context(window, prv_wrap_ccp, cfg);
}
