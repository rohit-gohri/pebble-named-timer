#pragma once

#include <pebble.h>

typedef void (*PhoneSettingsUpdatedCallback)(void *context);
typedef void (*PhoneQuickAddTimerCallback)(const char *name, int64_t duration_ms,
                                           bool start_immediately, void *context);

typedef struct {
  PhoneSettingsUpdatedCallback settings_updated;
  PhoneQuickAddTimerCallback quick_add_timer;
  void *context;
} PhoneCallbacks;

void phone_connect(PhoneCallbacks callbacks);
void phone_disconnect(void);
