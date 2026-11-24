#include <pebble.h>
#include <string.h>
#include "phone.h"
#include "presets.h"

#define PHONE_TIMERS_BUF_SIZE 512
#define PHONE_TIMER_MIN_SEC 5
#define PHONE_TIMER_MAX_SEC 359999
#define PHONE_SNOOZE_DEFAULT_SEC (COUNTDOWN_TIMER_DEFAULT_SNOOZE_MS / 1000)
#define PHONE_SNOOZE_MIN_SEC (COUNTDOWN_TIMER_MIN_SNOOZE_MS / 1000)

static PhoneCallbacks s_callbacks = {0};
static bool s_resend_needed = false;

static uint32_t prv_get_preset_key(uint8_t idx) {
  switch (idx) {
    case 0: return MESSAGE_KEY_PRESET_0;
    case 1: return MESSAGE_KEY_PRESET_1;
    case 2: return MESSAGE_KEY_PRESET_2;
    case 3: return MESSAGE_KEY_PRESET_3;
    case 4: return MESSAGE_KEY_PRESET_4;
    case 5: return MESSAGE_KEY_PRESET_5;
    case 6: return MESSAGE_KEY_PRESET_6;
    default: return MESSAGE_KEY_PRESET_7;
  }
}

static bool prv_parse_int64(const char **cursor, int64_t *out) {
  const char *p = *cursor;
  bool neg = false;
  if (*p == '-') {
    neg = true;
    p++;
  }
  if (*p < '0' || *p > '9') {
    return false;
  }
  int64_t val = 0;
  while (*p >= '0' && *p <= '9') {
    val = val * 10 + (*p - '0');
    p++;
  }
  *out = neg ? -val : val;
  *cursor = p;
  return true;
}

static uint8_t prv_parse_timers_data(const char *data, PhoneTimerSpec *specs, uint8_t max_count) {
  uint8_t count = 0;
  if (data == NULL || specs == NULL || max_count == 0) {
    return 0;
  }

  const char *p = data;
  while (*p != '\0' && count < max_count) {
    while (*p == '\n' || *p == '\r') {
      p++;
    }
    if (*p == '\0') {
      break;
    }

    const char *line = p;
    int64_t id_val = 0;
    int64_t dur_sec = 0;
    int64_t flags = 0;
    int64_t snooze_sec = 0;

    bool ok = prv_parse_int64(&line, &id_val) && (*line == '|');
    if (ok) {
      line++;
      ok = prv_parse_int64(&line, &dur_sec) && (*line == '|');
    }
    if (ok) {
      line++;
      ok = prv_parse_int64(&line, &flags) && (*line == '|');
    }

    if (!ok) {
      while (*p != '\0' && *p != '\n') {
        p++;
      }
      continue;
    }

    line++; // skip '|' after flags

    // Support both 5-field (id|dur|flags|snooze|name) and legacy 4-field (id|dur|flags|name)
    const char *probe = line;
    int64_t parsed_snooze = 0;
    if (prv_parse_int64(&probe, &parsed_snooze) && (*probe == '|')) {
      snooze_sec = parsed_snooze;
      line = probe + 1;
    }

    const char *name_start = line;
    while (*line != '\0' && *line != '\n' && *line != '\r') {
      line++;
    }
    size_t name_len = (size_t)(line - name_start);
    p = line;

    if (dur_sec < PHONE_TIMER_MIN_SEC) {
      continue;
    }
    if (dur_sec > PHONE_TIMER_MAX_SEC) {
      dur_sec = PHONE_TIMER_MAX_SEC;
    }

    int64_t snooze_ms;
    if (snooze_sec < PHONE_SNOOZE_MIN_SEC) {
      snooze_ms = countdown_timer_default_snooze_for_duration(dur_sec * 1000);
    } else {
      if (snooze_sec > PHONE_TIMER_MAX_SEC) {
        snooze_sec = PHONE_TIMER_MAX_SEC;
      }
      snooze_ms = snooze_sec * 1000;
    }

    PhoneTimerSpec *spec = &specs[count];
    memset(spec, 0, sizeof(*spec));
    spec->id = (int32_t)id_val;
    spec->duration_ms = dur_sec * 1000;
    spec->snooze_ms = snooze_ms;
    spec->start_immediately = (flags & 1) != 0;
    if (name_len >= COUNTDOWN_TIMER_NAME_MAX) {
      name_len = COUNTDOWN_TIMER_NAME_MAX - 1;
    }
    if (name_len > 0) {
      memcpy(spec->name, name_start, name_len);
    }
    spec->name[name_len] = '\0';
    count++;
  }

  return count;
}

static void prv_sanitize_name(char *dst, size_t dst_size, const char *src) {
  if (dst == NULL || dst_size == 0) {
    return;
  }
  size_t out = 0;
  if (src != NULL) {
    for (size_t i = 0; src[i] != '\0' && out + 1 < dst_size; i++) {
      char c = src[i];
      if (c == '|' || c == '\n' || c == '\r') {
        c = ' ';
      }
      dst[out++] = c;
    }
  }
  dst[out] = '\0';
}

static void prv_inbox_received_callback(DictionaryIterator *iterator, void *context) {
  if (dict_find(iterator, MESSAGE_KEY_SYNC_REQUEST)) {
    if (s_callbacks.sync_requested) {
      s_callbacks.sync_requested(s_callbacks.context);
    }
    return;
  }

  bool presets_changed = false;
  for (uint8_t i = 0; i < PRESETS_MAX_COUNT; i++) {
    Tuple *t = dict_find(iterator, prv_get_preset_key(i));
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

  Tuple *timers_tuple = dict_find(iterator, MESSAGE_KEY_TIMERS_DATA);
  if (timers_tuple && timers_tuple->type == TUPLE_CSTRING && s_callbacks.timers_updated) {
    PhoneTimerSpec specs[PHONE_TIMERS_MAX];
    uint8_t count = prv_parse_timers_data(timers_tuple->value->cstring, specs, PHONE_TIMERS_MAX);
    s_callbacks.timers_updated(specs, count, s_callbacks.context);
  }
}

static void prv_inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "AppMessage inbox dropped: %d", (int)reason);
}

static void prv_outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  if (s_resend_needed) {
    s_resend_needed = false;
    if (s_callbacks.sync_requested) {
      s_callbacks.sync_requested(s_callbacks.context);
    }
  }
}

static void prv_outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason,
                                       void *context) {
  s_resend_needed = false;
  APP_LOG(APP_LOG_LEVEL_INFO, "AppMessage outbox failed: %d", (int)reason);
}

void phone_connect(PhoneCallbacks callbacks) {
  s_callbacks = callbacks;
  s_resend_needed = false;
  app_message_register_inbox_received(prv_inbox_received_callback);
  app_message_register_inbox_dropped(prv_inbox_dropped_callback);
  app_message_register_outbox_sent(prv_outbox_sent_callback);
  app_message_register_outbox_failed(prv_outbox_failed_callback);
  app_message_open(1024, 1024);
}

void phone_disconnect(void) {
  app_message_deregister_callbacks();
  s_callbacks = (PhoneCallbacks){0};
  s_resend_needed = false;
}

void phone_send_state(CountdownTimer **timers, uint8_t count) {
  DictionaryIterator *iter = NULL;
  AppMessageResult res = app_message_outbox_begin(&iter);
  if (res == APP_MSG_BUSY) {
    s_resend_needed = true;
    return;
  }
  if (res != APP_MSG_OK || iter == NULL) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Cannot open AppMessage outbox: %d", (int)res);
    return;
  }

  char buf[PHONE_TIMERS_BUF_SIZE];
  size_t pos = 0;
  buf[0] = '\0';

  if (count > PHONE_TIMERS_MAX) {
    count = PHONE_TIMERS_MAX;
  }

  for (uint8_t i = 0; i < count; i++) {
    CountdownTimer *t = timers[i];
    if (t == NULL) {
      continue;
    }
    int32_t id = countdown_timer_get_id(t);
    int32_t dur_sec = (int32_t)((countdown_timer_get_duration(t) + 500) / 1000);
    int32_t snooze_sec = (int32_t)((countdown_timer_get_snooze(t) + 500) / 1000);
    int running = countdown_timer_get_paused(t) ? 0 : 1;
    char safe_name[COUNTDOWN_TIMER_NAME_MAX];
    prv_sanitize_name(safe_name, sizeof(safe_name), countdown_timer_get_name(t));

    int written = snprintf(buf + pos, sizeof(buf) - pos, "%s%ld|%ld|%d|%ld|%s",
                           (pos > 0) ? "\n" : "", (long)id, (long)dur_sec, running,
                           (long)snooze_sec, safe_name);
    if (written < 0 || (size_t)written >= sizeof(buf) - pos) {
      break;
    }
    pos += (size_t)written;
  }

  dict_write_int8(iter, MESSAGE_KEY_STATE_SYNC, 1);
  dict_write_cstring(iter, MESSAGE_KEY_TIMERS_DATA, buf);
  for (uint8_t i = 0; i < PRESETS_MAX_COUNT; i++) {
    dict_write_cstring(iter, prv_get_preset_key(i), presets_get_slot(i));
  }

  res = app_message_outbox_send();
  if (res != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_INFO, "AppMessage outbox send failed: %d", (int)res);
  }
}
