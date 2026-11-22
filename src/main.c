/*******************************************************************************
 * FILENAME :        main.c
 *
 * DESCRIPTION :
 *      Entry point and main logic for Named Timers, based on the official
 *      Pebble Timer app with support for naming saved timers via voice
 *      dictation, presets, phone settings sync, and PebbleOS touch navigation.
 *
 * ORIGINAL AUTHOR : Eric Phillips        START DATE :    07/10/15
 *
 */

#include <pebble.h>
#include "countdown_timer.h"
#include "menu_window.h"
#include "detail_window.h"
#include "name_window.h"
#include "setting_window.h"
#include "popup_window.h"
#include "phone.h"
#include "presets.h"

// constants
#define COUNTDOWN_TIMER_PERSIST_KEY 72445846
#define COUNTDOWN_TIMER_ID_PERSIST_KEY 3568356
#define PERSIST_VERSION 2
#define PERSIST_VERSION_KEY 46134672
#define SOUND_ENABLED_PERSIST_KEY 76863
#define COUNTDOWN_TIMERS_MAX 8
#define COUNTDOWN_TIMER_SNOOZE_DELAY 60000 // milliseconds
#define TIMER_MIN_LENGTH 5000 // milliseconds
#define INACTIVITY_THRESHOLD 900000 // length of time before refresh throttling in milliseconds
#define INACTIVE_REFRESH_DELAY 1000 // ms between frames after throttling
#define REFRESH_DELAY 1000 // ms between periodic redraws
#define POPUP_REFRESH_DELAY 35 // ms between popup animation frames
#define REFRESH_ALIGNMENT_DELAY 5 // ms after a second boundary to refresh
#define MIN_REFRESH_DELAY 25 // minimum delay when correcting near a boundary


typedef enum {
  PENDING_ACTION_NONE = 0,
  PENDING_ACTION_EDIT_TIME,
  PENDING_ACTION_EDIT_NAME,
} PendingEditAction;


/*******************************************************************************
 * MAIN LOCAL VARIABLES
 */

static MenuWindow *s_menu_window = NULL;
static DetailWindow *s_detail_window = NULL;
static NameWindow *s_name_window = NULL;
static SettingWindow *s_setting_window = NULL;
static PopupWindow *s_popup_window = NULL;
static uint8_t s_countdown_timers_count = 0;
static CountdownTimer *s_countdown_timers[COUNTDOWN_TIMERS_MAX] = {};
static int32_t s_countdown_timer_id_max = 0;
static AppTimer *s_app_timer = NULL;
static int64_t s_last_activity = 0;
static bool s_sound_enabled = false;

// Edit ActionMenu and Dictation state
static ActionMenu *s_action_menu = NULL;
static ActionMenuLevel *s_root_level = NULL;
static CountdownTimer *s_edit_target_timer = NULL;
static PendingEditAction s_pending_edit_action = PENDING_ACTION_NONE;
static DictationSession *s_dictation_session = NULL;
static CountdownTimer *s_dictation_target_timer = NULL;
static char s_popup_title_buff[COUNTDOWN_TIMER_NAME_MAX + 16];


static void prv_save_state(void) {
  persist_write_int(PERSIST_VERSION_KEY, PERSIST_VERSION);
  persist_write_int(COUNTDOWN_TIMER_ID_PERSIST_KEY, s_countdown_timer_id_max);
  countdown_timer_list_save(s_countdown_timers, s_countdown_timers_count,
    COUNTDOWN_TIMER_PERSIST_KEY);
}

static void prv_push_state_to_phone(void) {
  phone_send_state(s_countdown_timers, s_countdown_timers_count);
}

static void prv_save_and_sync(void) {
  prv_save_state();
  prv_push_state_to_phone();
}

static uint16_t prv_get_next_refresh_delay(void) {
  if (popup_window_get_topmost_window(s_popup_window)) {
    return POPUP_REFRESH_DELAY;
  }

  CountdownTimer *next_timer = countdown_timer_list_get_closest_timer(s_countdown_timers,
    s_countdown_timers_count);
  if (next_timer == NULL) {
    return REFRESH_DELAY;
  }

  int64_t remaining = countdown_timer_get_current_time(next_timer);
  if (remaining <= 0) {
    return MIN_REFRESH_DELAY;
  }

  int64_t delay = remaining % REFRESH_DELAY;
  if (delay == 0) {
    delay = REFRESH_DELAY;
  }
  delay += REFRESH_ALIGNMENT_DELAY;
  if (delay < MIN_REFRESH_DELAY) {
    delay = MIN_REFRESH_DELAY;
  }
  return (uint16_t)delay;
}

static bool prv_timer_precedes(CountdownTimer *a, CountdownTimer *b) {
  bool a_running = !countdown_timer_get_paused(a);
  bool b_running = !countdown_timer_get_paused(b);
  if (a_running != b_running) {
    return a_running;
  }
  return countdown_timer_get_last_update(a) > countdown_timer_get_last_update(b);
}

static void prv_sort_timers_by_recency(void) {
  for (uint8_t i = 1; i < s_countdown_timers_count; i++) {
    CountdownTimer *key = s_countdown_timers[i];
    int16_t j = (int16_t)i - 1;
    while (j >= 0 && prv_timer_precedes(key, s_countdown_timers[j])) {
      s_countdown_timers[j + 1] = s_countdown_timers[j];
      j--;
    }
    s_countdown_timers[j + 1] = key;
  }
}

static void prv_promote_timer(CountdownTimer *countdown_timer) {
  int16_t index = countdown_timer_list_get_timer_index(s_countdown_timers,
    s_countdown_timers_count, countdown_timer);
  if (index > 0) {
    memmove(&s_countdown_timers[1], &s_countdown_timers[0],
      sizeof(CountdownTimer*) * index);
    s_countdown_timers[0] = countdown_timer;
  }
  prv_sort_timers_by_recency();
}


/*******************************************************************************
 * VOICE DICTATION & RENAME ACTION MENU
 */

static void prv_apply_timer_name(CountdownTimer *timer, const char *new_name) {
  if (timer == NULL) {
    return;
  }
  // Verify the timer still exists in our list before mutating it
  if (countdown_timer_list_get_timer_index(s_countdown_timers, s_countdown_timers_count, timer) < 0) {
    return;
  }
  countdown_timer_set_name(timer, new_name);
  prv_promote_timer(timer);
  prv_save_and_sync();
  menu_window_reload_data(s_menu_window);
  menu_window_refresh(s_menu_window);
  detail_window_deep_refresh(s_detail_window);
  s_last_activity = countdown_timer_get_epoch_ms();
}

static void prv_dictation_callback(DictationSession *session,
                                   DictationSessionStatus status,
                                   char *transcription,
                                   void *context) {
  CountdownTimer *target = s_dictation_target_timer;
  s_dictation_target_timer = NULL;

  if (status == DictationSessionStatusSuccess && transcription && transcription[0] != '\0') {
    prv_apply_timer_name(target, transcription);
  } else {
    APP_LOG(APP_LOG_LEVEL_INFO, "Dictation ended with status %d", (int)status);
  }
}

static void prv_start_dictation_for_timer(CountdownTimer *timer) {
  if (timer == NULL) {
    return;
  }
  if (s_dictation_session == NULL) {
    s_dictation_session = dictation_session_create(COUNTDOWN_TIMER_NAME_MAX,
                                                   prv_dictation_callback, NULL);
    if (s_dictation_session == NULL) {
      APP_LOG(APP_LOG_LEVEL_WARNING, "Dictation unavailable on this watch/connection");
      vibes_short_pulse();
      return;
    }
    dictation_session_enable_confirmation(s_dictation_session, true);
    dictation_session_enable_error_dialogs(s_dictation_session, true);
  }
  s_dictation_target_timer = timer;
  dictation_session_start(s_dictation_session);
}

static void prv_restart_timer(CountdownTimer *timer) {
  if (timer == NULL) {
    return;
  }
  if (countdown_timer_list_get_timer_index(s_countdown_timers, s_countdown_timers_count, timer) < 0) {
    return;
  }
  // Reset back to saved duration while preserving running vs paused state
  countdown_timer_update(timer, countdown_timer_get_duration(timer), false);
  prv_promote_timer(timer);
  prv_save_and_sync();
  menu_window_reload_data(s_menu_window);
  menu_window_refresh(s_menu_window);
  detail_window_deep_refresh(s_detail_window);
  if (s_app_timer != NULL) {
    app_timer_reschedule(s_app_timer, MIN_REFRESH_DELAY);
  }
  vibes_short_pulse();
  s_last_activity = countdown_timer_get_epoch_ms();
}

static void prv_name_window_clear_cb(CountdownTimer *timer, void *context) {
  prv_apply_timer_name(timer, "");
  name_window_pop(s_name_window, true);
}

static void prv_name_window_dictate_cb(CountdownTimer *timer, void *context) {
  name_window_pop(s_name_window, false);
  prv_start_dictation_for_timer(timer);
}

static void prv_name_window_preset_cb(CountdownTimer *timer, const char *preset_name,
                                      void *context) {
  prv_apply_timer_name(timer, preset_name);
  name_window_pop(s_name_window, true);
}

static void prv_action_edit_time_cb(ActionMenu *action_menu, const ActionMenuItem *action,
                                    void *context) {
  s_pending_edit_action = PENDING_ACTION_EDIT_TIME;
}

static void prv_action_edit_name_cb(ActionMenu *action_menu, const ActionMenuItem *action,
                                    void *context) {
  s_pending_edit_action = PENDING_ACTION_EDIT_NAME;
}

static void prv_action_menu_did_close(ActionMenu *menu, const ActionMenuItem *performed_action,
                                      void *context) {
  if (s_root_level != NULL) {
    action_menu_hierarchy_destroy(s_root_level, NULL, NULL);
    s_root_level = NULL;
  }
  s_action_menu = NULL;

  PendingEditAction pending = s_pending_edit_action;
  CountdownTimer *target = s_edit_target_timer;
  s_pending_edit_action = PENDING_ACTION_NONE;
  s_edit_target_timer = NULL;

  if (target == NULL) {
    return;
  }
  if (pending == PENDING_ACTION_EDIT_TIME) {
    setting_window_set_timer(s_setting_window, target);
    setting_window_push(s_setting_window, true);
  } else if (pending == PENDING_ACTION_EDIT_NAME) {
    name_window_set_timer(s_name_window, target);
    name_window_push(s_name_window, true);
  }
}

static void prv_open_edit_action_menu(CountdownTimer *timer) {
  if (timer == NULL || s_action_menu != NULL) {
    return;
  }

  s_edit_target_timer = timer;
  s_pending_edit_action = PENDING_ACTION_NONE;

  s_root_level = action_menu_level_create(2);
  action_menu_level_add_action(s_root_level, "Edit Time", prv_action_edit_time_cb, NULL);
  action_menu_level_add_action(s_root_level, "Edit Name", prv_action_edit_name_cb, NULL);

  ActionMenuConfig config = (ActionMenuConfig) {
    .root_level = s_root_level,
    .colors = {
      .background = PBL_IF_COLOR_ELSE(GColorPictonBlue, GColorWhite),
      .foreground = GColorBlack,
    },
    .align = ActionMenuAlignCenter,
    .did_close = prv_action_menu_did_close,
  };
  s_action_menu = action_menu_open(&config);
}


/*******************************************************************************
 * PHONE SETTINGS & SYNC CALLBACKS
 */

static void prv_phone_settings_updated_callback(void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Updated preset names from phone (%u active)",
          (unsigned)presets_get_active_count());
  name_window_reload_data(s_name_window);
  s_last_activity = countdown_timer_get_epoch_ms();
}

static void prv_phone_sync_requested_callback(void *context) {
  prv_push_state_to_phone();
}

static void prv_phone_timers_updated_callback(const PhoneTimerSpec *specs, uint8_t count,
                                              void *context) {
  CountdownTimer *new_list[COUNTDOWN_TIMERS_MAX] = {0};
  bool kept[COUNTDOWN_TIMERS_MAX] = {0};
  uint8_t new_count = 0;

  if (count > COUNTDOWN_TIMERS_MAX) {
    count = COUNTDOWN_TIMERS_MAX;
  }

  for (uint8_t i = 0; i < count; i++) {
    const PhoneTimerSpec *spec = &specs[i];
    if (spec->duration_ms < TIMER_MIN_LENGTH) {
      continue;
    }

    CountdownTimer *matched = NULL;
    if (spec->id != 0) {
      for (uint8_t j = 0; j < s_countdown_timers_count; j++) {
        if (!kept[j] && countdown_timer_get_id(s_countdown_timers[j]) == spec->id) {
          kept[j] = true;
          matched = s_countdown_timers[j];
          break;
        }
      }
    }

    if (matched != NULL) {
      if (countdown_timer_get_duration(matched) != spec->duration_ms) {
        countdown_timer_update(matched, spec->duration_ms, true);
      }
      if (strcmp(countdown_timer_get_name(matched), spec->name) != 0) {
        countdown_timer_set_name(matched, spec->name);
      }
      new_list[new_count++] = matched;
    } else {
      CountdownTimer *created = countdown_timer_create(spec->duration_ms, &s_countdown_timer_id_max);
      if (created == NULL) {
        continue;
      }
      if (spec->name[0] != '\0') {
        countdown_timer_set_name(created, spec->name);
      }
      if (spec->start_immediately) {
        countdown_timer_start(created);
      }
      new_list[new_count++] = created;
    }
  }

  // Clean up any windows/pointers referencing timers that were removed on the phone
  CountdownTimer *detail_timer = detail_window_get_countdown_timer(s_detail_window);
  CountdownTimer *setting_timer = setting_window_get_timer(s_setting_window);
  bool detail_deleted = false;
  bool setting_deleted = false;

  for (uint8_t j = 0; j < s_countdown_timers_count; j++) {
    if (!kept[j]) {
      CountdownTimer *removed = s_countdown_timers[j];
      if (removed == detail_timer) {
        detail_deleted = true;
      }
      if (removed == setting_timer) {
        setting_deleted = true;
      }
      if (removed == s_edit_target_timer) {
        s_edit_target_timer = NULL;
      }
      if (removed == s_dictation_target_timer) {
        s_dictation_target_timer = NULL;
      }
      countdown_timer_destroy(removed);
    }
  }

  if (detail_deleted) {
    name_window_pop(s_name_window, false);
    if (setting_deleted) {
      setting_window_pop(s_setting_window, false);
      setting_window_set_timer(s_setting_window, NULL);
    }
    detail_window_pop(s_detail_window, false);
    detail_window_set_countdown_timer(s_detail_window, NULL);
    name_window_set_timer(s_name_window, NULL);
  } else if (setting_deleted) {
    setting_window_pop(s_setting_window, false);
    setting_window_set_timer(s_setting_window, NULL);
  }

  memcpy(s_countdown_timers, new_list, sizeof(CountdownTimer*) * COUNTDOWN_TIMERS_MAX);
  s_countdown_timers_count = new_count;

  prv_sort_timers_by_recency();
  prv_save_state();

  // If on the initial empty SettingWindow and timers were added, reveal the list
  if (s_countdown_timers_count > 0 &&
      setting_window_get_topmost_window(s_setting_window) &&
      setting_window_get_timer(s_setting_window) == NULL) {
    setting_window_pop(s_setting_window, false);
  }

  menu_window_reload_data(s_menu_window);
  menu_window_refresh(s_menu_window);
  detail_window_deep_refresh(s_detail_window);
  name_window_reload_data(s_name_window);

  if (s_app_timer != NULL) {
    app_timer_reschedule(s_app_timer, MIN_REFRESH_DELAY);
  }
  vibes_short_pulse();
  s_last_activity = countdown_timer_get_epoch_ms();

  // Send back canonical timer IDs (e.g. for newly created timers)
  prv_push_state_to_phone();
}


/*******************************************************************************
 * APP CALLBACKS
 */

static void app_timer_callback(void *data) {
  s_app_timer = NULL;

  // check for expired timers
  CountdownTimer *countdown_timer = countdown_timer_check_ended(s_countdown_timers,
    s_countdown_timers_count);

  if (countdown_timer != NULL) {
    prv_sort_timers_by_recency();
    prv_save_and_sync();
    detail_window_deep_refresh(s_detail_window);

    if (countdown_timer_has_name(countdown_timer)) {
      snprintf(s_popup_title_buff, sizeof(s_popup_title_buff), "%s\nTime's Up!",
               countdown_timer_get_name(countdown_timer));
    } else {
      snprintf(s_popup_title_buff, sizeof(s_popup_title_buff), "Time's Up!");
    }

    popup_window_set_countdown_timer(s_popup_window, countdown_timer);
    popup_window_set_title(s_popup_window, s_popup_title_buff);
    popup_window_set_highlight_color(s_popup_window, PBL_IF_COLOR_ELSE(GColorPictonBlue, GColorWhite));
#ifdef PBL_PLATFORM_APLITE
    popup_window_set_image(s_popup_window, RESOURCE_ID_IMAGE_ALARM);
#else
    popup_window_set_pdc(s_popup_window, RESOURCE_ID_ICON_ALARM_CLOCK, true);
#endif
    popup_window_set_auto_close_duration(s_popup_window,
      s_sound_enabled ? POPUP_CHIME_DURATION_MS : 15000);
    popup_window_add_action_bar(s_popup_window);
    popup_window_push(s_popup_window, true);
    popup_window_set_vibes(s_sound_enabled);

    s_last_activity = countdown_timer_get_epoch_ms();
  }

  // refresh
  bool menu_top = menu_window_get_topmost_window(s_menu_window);
  bool detail_top = detail_window_get_topmost_window(s_detail_window);
  bool popup_top = popup_window_get_topmost_window(s_popup_window);
  if (menu_top) menu_window_refresh(s_menu_window);
  if (detail_top) detail_window_refresh(s_detail_window);
  if (popup_top) popup_window_refresh(s_popup_window);

  // check activity
  int64_t inactivity_duration = countdown_timer_get_epoch_ms() - s_last_activity;

  // schedule next refresh
  uint16_t refresh_rate = prv_get_next_refresh_delay();
  if (popup_top) {
    inactivity_duration = 0;
  }
  if (refresh_rate == 0) {
    return;
  }
  if (inactivity_duration > INACTIVITY_THRESHOLD) {
    refresh_rate = (refresh_rate > INACTIVE_REFRESH_DELAY) ? refresh_rate : INACTIVE_REFRESH_DELAY;
  }
  s_app_timer = app_timer_register(refresh_rate, app_timer_callback, NULL);
}

static void popup_window_snooze_timer_callback(CountdownTimer *countdown_timer, void *context) {
  if (countdown_timer_list_get_timer_index(s_countdown_timers, s_countdown_timers_count,
                                           countdown_timer) < 0) {
    popup_window_pop(s_popup_window, true);
    return;
  }
  countdown_timer_update(countdown_timer, COUNTDOWN_TIMER_SNOOZE_DELAY, false);
  countdown_timer_start(countdown_timer);
  prv_promote_timer(countdown_timer);
  prv_save_and_sync();
  popup_window_pop(s_popup_window, true);
  if (!detail_window_get_topmost_window(s_detail_window)) {
    detail_window_set_countdown_timer(s_detail_window, countdown_timer);
    detail_window_push(s_detail_window, false);
  }
  detail_window_deep_refresh(s_detail_window);
  s_last_activity = countdown_timer_get_epoch_ms();
}

static void popup_window_stop_timer_callback(void *context) {
  popup_window_pop(s_popup_window, true);
  s_last_activity = countdown_timer_get_epoch_ms();
}

static void setting_window_complete_callback(int64_t duration, void *context) {
  SettingWindow *setting_window = (SettingWindow*)context;
  CountdownTimer *countdown_timer = setting_window_get_timer(setting_window);
  if (duration < TIMER_MIN_LENGTH) {
    setting_window_pop(setting_window, true);
    if (s_app_timer != NULL) {
      app_timer_reschedule(s_app_timer, MIN_REFRESH_DELAY);
    }
    return;
  }

  if (countdown_timer == NULL) {
    countdown_timer = countdown_timer_create(duration, &s_countdown_timer_id_max);
    countdown_timer_list_add(s_countdown_timers, COUNTDOWN_TIMERS_MAX,
      &s_countdown_timers_count, countdown_timer);
    countdown_timer_start(countdown_timer);
    menu_window_reload_data(s_menu_window);
    menu_window_refresh(s_menu_window);
    detail_window_set_countdown_timer(s_detail_window, countdown_timer);
    setting_window_pop(setting_window, false);
    detail_window_push(s_detail_window, true);
    detail_window_deep_refresh(s_detail_window);
  } else {
    countdown_timer_update(countdown_timer, duration, true);
    countdown_timer_start(countdown_timer);
    detail_window_deep_refresh(s_detail_window);
    setting_window_pop(setting_window, true);
  }

  prv_promote_timer(countdown_timer);
  prv_save_and_sync();

  if (s_app_timer != NULL) {
    app_timer_reschedule(s_app_timer, MIN_REFRESH_DELAY);
  }
  s_last_activity = countdown_timer_get_epoch_ms();
}

static void detail_window_edit_timer_callback(CountdownTimer *countdown_timer, void *context) {
  prv_open_edit_action_menu(countdown_timer);
  s_last_activity = countdown_timer_get_epoch_ms();
}

static void detail_window_quick_dictate_callback(CountdownTimer *countdown_timer, void *context) {
  prv_start_dictation_for_timer(countdown_timer);
  s_last_activity = countdown_timer_get_epoch_ms();
}

static void detail_window_playpause_timer_callback(CountdownTimer *countdown_timer, void *context) {
  if (countdown_timer_get_paused(countdown_timer)) {
    if (countdown_timer_get_current_time(countdown_timer) <= 0) {
      countdown_timer_update(countdown_timer, countdown_timer_get_duration(countdown_timer), false);
    }
    countdown_timer_start(countdown_timer);
  } else {
    countdown_timer_stop(countdown_timer, &s_countdown_timer_id_max);
  }
  prv_promote_timer(countdown_timer);
  prv_save_and_sync();
  detail_window_deep_refresh(s_detail_window);
  s_last_activity = countdown_timer_get_epoch_ms();
}

static void detail_window_restart_timer_callback(CountdownTimer *countdown_timer, void *context) {
  prv_restart_timer(countdown_timer);
}

static void detail_window_delete_timer_callback(CountdownTimer *countdown_timer, void *context) {
  int16_t timer_index = countdown_timer_list_get_timer_index(s_countdown_timers,
    s_countdown_timers_count, countdown_timer);
  countdown_timer_destroy(countdown_timer);
  countdown_timer_list_remove(s_countdown_timers, &s_countdown_timers_count, timer_index);
  prv_save_and_sync();

  menu_window_reload_data(s_menu_window);
  menu_window_reload_data(s_menu_window);
  detail_window_pop(s_detail_window, true);

  popup_window_set_title(s_popup_window, "Timer Deleted");
  popup_window_set_highlight_color(s_popup_window, PBL_IF_COLOR_ELSE(GColorPictonBlue, GColorWhite));
#ifdef PBL_PLATFORM_APLITE
  popup_window_set_image(s_popup_window, RESOURCE_ID_IMAGE_SHREADER);
  popup_window_set_auto_close_duration(s_popup_window, 1000);
#else
  popup_window_set_pdc(s_popup_window, RESOURCE_ID_ICON_DELETED, false);
  int64_t pdc_duration = popup_window_get_pdc_duration(s_popup_window);
  popup_window_set_auto_close_duration(s_popup_window, pdc_duration);
#endif
  popup_window_remove_action_bar(s_popup_window);
  popup_window_push(s_popup_window, true);
  popup_window_refresh(s_popup_window);

  if (s_app_timer) {
    app_timer_reschedule(s_app_timer, POPUP_REFRESH_DELAY);
  } else {
    s_app_timer = app_timer_register(POPUP_REFRESH_DELAY, app_timer_callback, NULL);
  }

  s_last_activity = countdown_timer_get_epoch_ms();
}

static CountdownTimer *menu_window_get_timer_callback(uint8_t index, void *context) {
  if (index < s_countdown_timers_count) {
    return s_countdown_timers[index];
  }
  APP_LOG(APP_LOG_LEVEL_ERROR, "Attempted to access timer outside array bounds");
  return NULL;
}

static uint8_t menu_window_get_timer_count_callback(void *context) {
  return s_countdown_timers_count;
}

#ifdef TIMER_HAS_SPEAKER
static bool menu_window_get_sound_enabled_callback(void *context) {
  return s_sound_enabled;
}
#endif

static void menu_window_click_callback(uint8_t index, void *context) {
  if (index == 0) {
    setting_window_set_timer(s_setting_window, NULL);
    setting_window_push(s_setting_window, true);
  } else if (index > s_countdown_timers_count) {
    s_sound_enabled = !s_sound_enabled;
    persist_write_bool(SOUND_ENABLED_PERSIST_KEY, s_sound_enabled);
    menu_window_refresh(s_menu_window);
  } else {
    detail_window_set_countdown_timer(s_detail_window, s_countdown_timers[index - 1]);
    detail_window_push(s_detail_window, true);
    detail_window_deep_refresh(s_detail_window);
    if (s_app_timer != NULL) {
      app_timer_reschedule(s_app_timer, MIN_REFRESH_DELAY);
    }
  }
  s_last_activity = countdown_timer_get_epoch_ms();
}


/*******************************************************************************
 * INITIALIZE AND DEINITIALIZE
 */

static void initialize(void) {
  // Enable PebbleOS system touch navigation on touchscreen watches
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
  app_touch_navigation_enable(true);
#endif

  // Load customizable preset names
  presets_load();

  // Load saved timers
  if (persist_exists(COUNTDOWN_TIMER_PERSIST_KEY)) {
    countdown_timer_list_load(s_countdown_timers, COUNTDOWN_TIMERS_MAX,
      &s_countdown_timers_count, COUNTDOWN_TIMER_PERSIST_KEY);
  }
  if (persist_exists(COUNTDOWN_TIMER_ID_PERSIST_KEY)) {
    s_countdown_timer_id_max = persist_read_int(COUNTDOWN_TIMER_ID_PERSIST_KEY);
  }
  // Ensure every loaded timer has a unique positive ID for two-way phone sync
  for (uint8_t i = 0; i < s_countdown_timers_count; i++) {
    int32_t id = countdown_timer_get_id(s_countdown_timers[i]);
    bool dup = (id <= 0);
    for (uint8_t j = 0; !dup && j < i; j++) {
      if (countdown_timer_get_id(s_countdown_timers[j]) == id) {
        dup = true;
      }
    }
    if (dup) {
      countdown_timer_rand_id(s_countdown_timers[i], &s_countdown_timer_id_max);
    } else if (id > s_countdown_timer_id_max) {
      s_countdown_timer_id_max = id;
    }
  }
#ifdef TIMER_HAS_SPEAKER
  s_sound_enabled = persist_read_bool(SOUND_ENABLED_PERSIST_KEY);
#endif
  prv_sort_timers_by_recency();
  wakeup_cancel_all();

  // create menu window
  MenuWindowCallbacks menu_callbacks = {
    .get_timer = menu_window_get_timer_callback,
    .get_timer_count = menu_window_get_timer_count_callback,
    .clicked = menu_window_click_callback,
#ifdef TIMER_HAS_SPEAKER
    .get_sound_enabled = menu_window_get_sound_enabled_callback,
#endif
  };
  s_menu_window = menu_window_create(menu_callbacks, true);
  menu_window_set_highlight_color(s_menu_window, PBL_IF_COLOR_ELSE(GColorPictonBlue, GColorBlack));
  menu_window_refresh(s_menu_window);

  // create detail window
  DetailWindowCallbacks detail_callbacks = {
    .edit_timer = detail_window_edit_timer_callback,
    .quick_dictate = detail_window_quick_dictate_callback,
    .playpause_timer = detail_window_playpause_timer_callback,
    .restart_timer = detail_window_restart_timer_callback,
    .delete_timer = detail_window_delete_timer_callback,
  };
  s_detail_window = detail_window_create(detail_callbacks);
  detail_window_set_highlight_color(s_detail_window, PBL_IF_COLOR_ELSE(GColorPictonBlue, GColorWhite));

  // create name picker window
  NameWindowCallbacks name_callbacks = {
    .on_clear = prv_name_window_clear_cb,
    .on_dictate = prv_name_window_dictate_cb,
    .on_preset = prv_name_window_preset_cb,
    .context = NULL,
  };
  s_name_window = name_window_create(name_callbacks);
  name_window_set_highlight_color(s_name_window, PBL_IF_COLOR_ELSE(GColorPictonBlue, GColorBlack));

  // create setting window
  SettingWindowCallbacks setting_callbacks = {
    .setting_complete = setting_window_complete_callback,
  };
  s_setting_window = setting_window_create(setting_callbacks);
  setting_window_set_highlight_color(s_setting_window, PBL_IF_COLOR_ELSE(GColorPictonBlue, GColorBlack));

  // create pop-up window
  PopupWindowCallbacks popup_callbacks = {
    .up_click = popup_window_snooze_timer_callback,
    .down_click = popup_window_stop_timer_callback,
  };
  s_popup_window = popup_window_create();
  popup_window_set_action_bar_callbacks(s_popup_window, popup_callbacks);

  // open the setting screen if no timers
  if (s_countdown_timers_count == 0) {
    setting_window_set_timer(s_setting_window, NULL);
    setting_window_push(s_setting_window, true);
  }

  // Connect to phone for two-way timer & preset sync (after windows & timers are ready)
  phone_connect((PhoneCallbacks) {
    .settings_updated = prv_phone_settings_updated_callback,
    .timers_updated = prv_phone_timers_updated_callback,
    .sync_requested = prv_phone_sync_requested_callback,
    .context = NULL,
  });
  prv_push_state_to_phone();

  // start the main update timer
  s_app_timer = app_timer_register(prv_get_next_refresh_delay(), app_timer_callback, NULL);
  s_last_activity = countdown_timer_get_epoch_ms();
}

static void prv_add_slice(AppGlanceReloadSession *session, const char *str, time_t expiration_time) {
  const AppGlanceSlice slice = {
    .layout.subtitle_template_string = str,
    .expiration_time = expiration_time
  };
  AppGlanceResult result = app_glance_add_slice(session, slice);
  if (result != APP_GLANCE_RESULT_SUCCESS) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "AppGlance Error: %d", result);
  }
}

static void prv_update_app_glance(AppGlanceReloadSession *session, size_t limit, void *context) {
  if (limit < 1) {
    return;
  }

  time_t expiration_time = APP_GLANCE_SLICE_NO_EXPIRATION;
  CountdownTimer *countdown_timer = countdown_timer_list_get_closest_timer(s_countdown_timers,
    s_countdown_timers_count);

  if (countdown_timer != NULL) {
    expiration_time = time(NULL) + countdown_timer_get_current_time(countdown_timer) / 1000;
    char buff_glance[64] = {0};
    if (countdown_timer_has_name(countdown_timer)) {
      snprintf(buff_glance, sizeof(buff_glance), "%s: {time_until(%ld)|format('%%fT')}",
               countdown_timer_get_name(countdown_timer), (long)expiration_time);
    } else {
      snprintf(buff_glance, sizeof(buff_glance), "{time_until(%ld)|format('%%fT')}",
               (long)expiration_time);
    }
    prv_add_slice(session, buff_glance, expiration_time);
  } else {
    countdown_timer = countdown_timer_list_get_last_updated_timer(s_countdown_timers,
      s_countdown_timers_count);
    if (countdown_timer != NULL) {
      expiration_time = countdown_timer_get_last_update(countdown_timer) + SECONDS_PER_HOUR;
      if (expiration_time > time(NULL)) {
        char buff_glance[48] = {0};
        if (countdown_timer_has_name(countdown_timer)) {
          snprintf(buff_glance, sizeof(buff_glance), "%s: %s",
                   countdown_timer_get_name(countdown_timer),
                   countdown_timer_format_own_buff(countdown_timer));
          prv_add_slice(session, buff_glance, expiration_time);
        } else {
          prv_add_slice(session, countdown_timer_format_own_buff(countdown_timer), expiration_time);
        }
      }
    }
  }
}

static void deinitialize(void) {
  if (s_app_timer != NULL) {
    app_timer_cancel(s_app_timer);
    s_app_timer = NULL;
  }
  if (s_dictation_session != NULL) {
    dictation_session_destroy(s_dictation_session);
    s_dictation_session = NULL;
  }
  if (s_root_level != NULL) {
    action_menu_hierarchy_destroy(s_root_level, NULL, NULL);
    s_root_level = NULL;
  }
  phone_disconnect();

  prv_save_state();

  // schedule wakeup for soonest running timer
  CountdownTimer *countdown_timer = countdown_timer_list_get_closest_timer(s_countdown_timers,
    s_countdown_timers_count);
  if (countdown_timer != NULL) {
    time_t timestamp = time(NULL) + countdown_timer_get_current_time(countdown_timer) / 1000;
    wakeup_schedule(timestamp + 1, 0, true);
  }

  app_glance_reload(prv_update_app_glance, NULL);

  popup_window_destroy(s_popup_window);
  setting_window_destroy(s_setting_window);
  name_window_destroy(s_name_window);
  detail_window_destroy(s_detail_window);
  menu_window_destroy(s_menu_window);
  countdown_timer_list_destroy_all(s_countdown_timers, &s_countdown_timers_count);
}

int main(void) {
  initialize();
  app_event_loop();
  deinitialize();
}
