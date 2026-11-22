/*******************************************************************************
 * FILENAME :        name_window.h
 *
 * DESCRIPTION :
 *      Picker window for editing a timer's name (optional clear icon,
 *      voice dictation mic icon, and customizable preset list).
 *
 */

#pragma once

#include <pebble.h>
#include "countdown_timer.h"

typedef void (*NameWindowClearCallback)(CountdownTimer *timer, void *context);
typedef void (*NameWindowDictateCallback)(CountdownTimer *timer, void *context);
typedef void (*NameWindowPresetCallback)(CountdownTimer *timer, const char *preset_name,
                                         void *context);

typedef struct NameWindowCallbacks {
  NameWindowClearCallback on_clear;
  NameWindowDictateCallback on_dictate;
  NameWindowPresetCallback on_preset;
  void *context;
} NameWindowCallbacks;

typedef struct NameWindow NameWindow;

NameWindow *name_window_create(NameWindowCallbacks callbacks);
void name_window_destroy(NameWindow *name_window);
void name_window_push(NameWindow *name_window, bool animated);
void name_window_pop(NameWindow *name_window, bool animated);
void name_window_set_timer(NameWindow *name_window, CountdownTimer *timer);
void name_window_set_highlight_color(NameWindow *name_window, GColor color);
void name_window_reload_data(NameWindow *name_window);
