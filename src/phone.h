#pragma once

#include <pebble.h>
#include "countdown_timer.h"

#define PHONE_TIMERS_MAX 8

typedef struct {
  int32_t id;
  int64_t duration_ms;
  bool start_immediately;
  char name[COUNTDOWN_TIMER_NAME_MAX];
} PhoneTimerSpec;

typedef void (*PhoneSettingsUpdatedCallback)(void *context);
typedef void (*PhoneTimersUpdatedCallback)(const PhoneTimerSpec *specs, uint8_t count,
                                           void *context);
typedef void (*PhoneSyncRequestedCallback)(void *context);

typedef struct {
  PhoneSettingsUpdatedCallback settings_updated;
  PhoneTimersUpdatedCallback timers_updated;
  PhoneSyncRequestedCallback sync_requested;
  void *context;
} PhoneCallbacks;

void phone_connect(PhoneCallbacks callbacks);
void phone_disconnect(void);
void phone_send_state(CountdownTimer **timers, uint8_t count);
