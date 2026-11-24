/*******************************************************************************
 * FILENAME :        edit_menu_window.h
 *
 * DESCRIPTION :
 *      Wrap-around ActionMenu-styled picker for editing a timer's time,
 *      name, or snooze duration.
 *
 */

#pragma once

#include <pebble.h>
#include "countdown_timer.h"

typedef enum {
  EDIT_MENU_ACTION_TIME = 0,
  EDIT_MENU_ACTION_NAME,
  EDIT_MENU_ACTION_SNOOZE,
  EDIT_MENU_ACTION_COUNT
} EditMenuAction;

typedef void (*EditMenuSelectCallback)(CountdownTimer *timer, EditMenuAction action,
                                       void *context);

typedef struct EditMenuWindowCallbacks {
  EditMenuSelectCallback on_select;
  void *context;
} EditMenuWindowCallbacks;

typedef struct EditMenuWindow EditMenuWindow;

EditMenuWindow *edit_menu_window_create(EditMenuWindowCallbacks callbacks);
void edit_menu_window_destroy(EditMenuWindow *edit_menu_window);
void edit_menu_window_push(EditMenuWindow *edit_menu_window, bool animated);
void edit_menu_window_pop(EditMenuWindow *edit_menu_window, bool animated);
void edit_menu_window_set_timer(EditMenuWindow *edit_menu_window, CountdownTimer *timer);
CountdownTimer *edit_menu_window_get_timer(EditMenuWindow *edit_menu_window);
void edit_menu_window_set_highlight_color(EditMenuWindow *edit_menu_window, GColor color);
