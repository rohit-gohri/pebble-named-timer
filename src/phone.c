#include <pebble.h>
#include "phone.h"
#include "presets.h"

static PhoneCallbacks s_callbacks = {0};

static void prv_inbox_received_callback(DictionaryIterator *iterator, void *context) {
  const uint32_t preset_keys[PRESETS_MAX_COUNT] = {
    MESSAGE_KEY_PRESET_0,
    MESSAGE_KEY_PRESET_1,
    MESSAGE_KEY_PRESET_2,
    MESSAGE_KEY_PRESET_3,
    MESSAGE_KEY_PRESET_4,
    MESSAGE_KEY_PRESET_5,
    MESSAGE_KEY_PRESET_6,
    MESSAGE_KEY_PRESET_7,
  };

  bool presets_changed = false;
  for (uint8_t i = 0; i < PRESETS_MAX_COUNT; i++) {
    Tuple *t = dict_find(iterator, preset_keys[i]);
    if (t && t->type == TUPLE_CSTRING) {
      presets_set_slot(i, t->value->cstring);
      presets_changed = true;
    }
  }

  if (presets_changed) {
    presets_save();
    if (s_callbacks.settings_updated) {
      s_callbacks.settings_updated(s_callbacks.context);
    }
  }

  // Optional one-shot Quick-Add Named Timer from Clay settings
  Tuple *min_tuple = dict_find(iterator, MESSAGE_KEY_QUICK_ADD_MINUTES);
  Tuple *sec_tuple = dict_find(iterator, MESSAGE_KEY_QUICK_ADD_SECONDS);
  if (min_tuple || sec_tuple) {
    int32_t mins = min_tuple ? min_tuple->value->int32 : 0;
    int32_t secs = sec_tuple ? sec_tuple->value->int32 : 0;
    if (mins < 0) mins = 0;
    if (secs < 0) secs = 0;
    int64_t total_ms = ((int64_t)mins * 60 + (int64_t)secs) * 1000;
    if (total_ms >= 5000 && s_callbacks.quick_add_timer) {
      Tuple *name_tuple = dict_find(iterator, MESSAGE_KEY_QUICK_ADD_NAME);
      Tuple *start_tuple = dict_find(iterator, MESSAGE_KEY_QUICK_ADD_START);
      const char *name = (name_tuple && name_tuple->type == TUPLE_CSTRING)
                           ? name_tuple->value->cstring
                           : "";
      bool start_now = start_tuple ? (start_tuple->value->int32 != 0) : false;
      s_callbacks.quick_add_timer(name, total_ms, start_now, s_callbacks.context);
    }
  }
}

static void prv_inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "AppMessage inbox dropped: %d", (int)reason);
}

static void prv_outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason,
                                       void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "AppMessage outbox failed: %d", (int)reason);
}

void phone_connect(PhoneCallbacks callbacks) {
  s_callbacks = callbacks;
  app_message_register_inbox_received(prv_inbox_received_callback);
  app_message_register_inbox_dropped(prv_inbox_dropped_callback);
  app_message_register_outbox_failed(prv_outbox_failed_callback);
  app_message_open(512, 128);
}

void phone_disconnect(void) {
  app_message_deregister_callbacks();
  s_callbacks = (PhoneCallbacks){0};
}
