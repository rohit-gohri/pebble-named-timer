/*******************************************************************************
 * FILENAME :        countdown_timer.c
 *
 * DESCRIPTION :
 *      Timer creation, destruction, modification, etc.
 *
 * PUBLIC FUNCTIONS :
 *      CountdownTimer  *countdown_timer_create(int64_t duration);
 *      void            countdown_timer_destroy(CountdownTimer
 *                          *countdown_timer);
 *      void            countdown_timer_start(CountdownTimer *countdown_timer);
 *      void            countdown_timer_stop(CountdownTimer *countdown_timer);
 *      void            countdown_timer_update(CountdownTimer *countdown_timer,
 *                          int64_t duration, bool update_duration);
 *      CountdownTimer  *countdown_timer_check_ended(CountdownTimer
 *                          **timer_array, uint8_t timer_array_count);
 *      void            countdown_timer_list_add(CountdownTimer **timer_array,
 *                          uint8_t timer_array_max, uint8_t *timer_array_count,
 *                          CountdownTimer *countdown_timer);
 *      void            countdown_timer_list_remove(CountdownTimer
 *                          **timer_array, uint8_t *timer_array_count,
 *                          uint8_t timer_index);
 *      int16_t         countdown_timer_list_get_timer_index(CountdownTimer
 *                          **timer_array, uint8_t timer_array_count,
 *                          CountdownTimer *countdown_timer);
 *      CountdownTimer  *countdown_timer_list_get_closest_timer(CountdownTimer
 *                          **timer_array, uint8_t timer_array_count);
 *      CountdownTimer  *countdown_timer_list_get_timer_by_id(
 *                          CountdownTimer **timer_array,
 *                          uint8_t timer_array_count, int32_t id);
 *      void            countdown_timer_list_destroy_all(CountdownTimer
 *                          **timer_array, uint8_t *timer_array_count);
 *      void            countdown_timer_list_save(CountdownTimer **timer_array,
 *                          uint8_t timer_array_count, uint32_t key);
 *      void            countdown_timer_list_load(CountdownTimer **timer_array,
 *                          uint8_t *timer_array_count, uint32_t key);
 *      bool            countdown_timer_get_paused(CountdownTimer
 *                          *countdown_timer);
 *      int64_t         countdown_timer_get_start(CountdownTimer
 *                          *countdown_timer);
 *      int64_t         countdown_timer_get_current_time(CountdownTimer
 *                          *countdown_timer);
 *      void            countdown_timer_rand_id(CountdownTimer
 *                          *countdown_timer);
 *      int32_t         countdown_timer_get_id(CountdownTimer *countdown_timer);
 *      int64_t         countdown_timer_get_duration(CountdownTimer
 *                          *countdown_timer);
 *      void            countdown_timer_format_text(int64_t value,
 *                          char *buff, uint8_t size);
 *      char            *countdown_timer_format_own_buff(CountdownTimer
 *                          *countdown_timer);
 *
 * NOTES :      The actual timer structure definition is not exposed to
 *              prevent direct modification of the structure.
 *
 * AUTHOR :     Eric Phillips        START DATE :    07/09/15
 *
 */


#include <pebble.h>
#include "countdown_timer.h"

#define COUNTDOWN_TIMER_EXPIRED ((int64_t)-9223372036854775807LL - 1)
#define MSEC_IN_SEC 1000
#define MSEC_IN_MIN 60000
#define MSEC_IN_HR 3600000



/*
 * v1 and v2 structure layouts for seamless persistence migration
 */

typedef struct {
  int64_t     start_ms;
  int64_t     duration_ms;
  int32_t     id;
  char        buff[16];
  char        name[COUNTDOWN_TIMER_NAME_MAX];
  bool        paused;
  time_t      last_update;
} __attribute__((__packed__)) CountdownTimerV1;

typedef struct {
  int64_t     start_ms;
  int64_t     duration_ms;
  int32_t     id;
  char        buff[16];
  char        name[COUNTDOWN_TIMER_NAME_MAX];
  bool        paused;
  time_t      last_update;
  int64_t     snooze_ms;
} __attribute__((__packed__)) CountdownTimerV2;

/*
 * the structure of a CountdownTimer
 */

struct CountdownTimer {
  int64_t     start_ms;     //< epoch of start time in milliseconds
  int64_t     duration_ms;  //< total duration in milliseconds
  int32_t     id;           //< random unique integer for timeline pins
  char        buff[16];     //< buffer for printing time string into
  char        name[COUNTDOWN_TIMER_NAME_MAX]; //< optional user-defined label
  bool        paused;       //< current state
  time_t      last_update;
  int64_t     snooze_ms;    //< per-timer snooze duration in milliseconds
  int64_t     original_end_ms; //< epoch ms when original timer ended across snoozes (0 if none)
} __attribute__((__packed__));



/*
 * gets the current epoch time in ms
 */

int64_t countdown_timer_get_epoch_ms(void) {
  time_t sec;
  uint16_t msec;
  time_ms(&sec, &msec);
  return (int64_t)sec * 1000 + (int64_t)msec;
}



/*
 * creates a CountdownTimer and sets its values to defaults
 */

CountdownTimer *countdown_timer_create(int64_t duration, int32_t *current_id_max) {
  CountdownTimer *countdown_timer = (CountdownTimer*)malloc(sizeof(CountdownTimer));
  if (countdown_timer != NULL) {
    (*countdown_timer) = (CountdownTimer) {
      .duration_ms = duration,
      .paused = true,
      .snooze_ms = countdown_timer_default_snooze_for_duration(duration),
      .original_end_ms = 0,
    };
    countdown_timer_rand_id(countdown_timer, current_id_max);
    return countdown_timer;
  }
  // error handling
  APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to create CountdownTimer");
  return NULL;
}



/*
 * destroys a CountdownTimer freeing its memory
 */

void countdown_timer_destroy(CountdownTimer *countdown_timer) {
  free(countdown_timer);
  countdown_timer = NULL;
}



/*
 * starts a CountdownTimer
 *
 * the "start" time is kept up to date by adding the current
 * epoch when starting, and subtracting it when stopping
 */

void countdown_timer_start(CountdownTimer *countdown_timer) {
  if (countdown_timer->paused) {
    if (countdown_timer->start_ms == COUNTDOWN_TIMER_EXPIRED) {
      return;
    }
    int64_t now = countdown_timer_get_epoch_ms();
    countdown_timer->start_ms += now;
    countdown_timer->paused = false;
    countdown_timer->last_update = time(NULL);
  }
}



/*
 * stops a CountdownTimer
 *
 * the "start" time is kept up to date by adding the current
 * epoch when starting, and subtracting it when stopping
 */

void countdown_timer_stop(CountdownTimer *countdown_timer, int32_t *current_id_max) {
  if (!countdown_timer->paused) {
    countdown_timer->start_ms -= countdown_timer_get_epoch_ms();
    countdown_timer->paused = true;
    countdown_timer->last_update = time(NULL);
  }
}



/*
 * update the current time of a CountdownTimer
 *
 * also modifies the start time as necessary to reflect the change
 * and if the duration is less than the update, changes the duration
 */

void countdown_timer_update(CountdownTimer *countdown_timer, int64_t duration,
    bool update_duration) {
  if (countdown_timer->duration_ms < duration || update_duration) {
    countdown_timer->duration_ms = duration;
  }
  int64_t now = countdown_timer_get_epoch_ms();
  countdown_timer->start_ms = ((countdown_timer->paused) ? 0 : now)
    + duration - countdown_timer->duration_ms;
  countdown_timer->original_end_ms = 0;
  countdown_timer->last_update = time(NULL);
}



/*
 * finds the first expired CountdownTimer
 *
 * checks through all timers and returns a pointer to the first CountdownTimer
 * ended also resets those timers that have ended
 */

CountdownTimer *countdown_timer_check_ended(CountdownTimer **timer_array,
                                            uint8_t timer_array_count) {
  // get current time
  CountdownTimer *return_timer = NULL;
  int64_t now = countdown_timer_get_epoch_ms();
  // loop over timers
  for (uint8_t ii = 0; ii < timer_array_count; ii++) {
    // check if expired and not paused
    if (!timer_array[ii]->paused &&
        timer_array[ii]->start_ms + timer_array[ii]->duration_ms <= now) {
      int64_t ended_at = timer_array[ii]->start_ms + timer_array[ii]->duration_ms;
      if (timer_array[ii]->original_end_ms <= 0 || timer_array[ii]->original_end_ms > now) {
        timer_array[ii]->original_end_ms = (ended_at <= now) ? ended_at : now;
      }
      timer_array[ii]->start_ms = COUNTDOWN_TIMER_EXPIRED;
      timer_array[ii]->paused = true;
      if (return_timer == NULL) {
        return_timer = timer_array[ii];
      }
    }
  }
  return return_timer;
}



/*
 * adds a new CountdownTimer to an array of them
 *
 * indexes CountdownTimer count as well, and pops oldest CountdownTimer if full
 */

void countdown_timer_list_add(CountdownTimer **timer_array, uint8_t timer_array_max,
                              uint8_t *timer_array_count, CountdownTimer *countdown_timer) {
  if ((*timer_array_count) == timer_array_max) {
    countdown_timer_destroy(timer_array[timer_array_max - 1]);
  } else {
    (*timer_array_count)++;
  }
  memmove(&timer_array[1], &timer_array[0], sizeof(CountdownTimer*) * (timer_array_max - 1));
  timer_array[0] = countdown_timer;
}



/*
 * removes a CountdownTimer
 *
 * removes a CountdownTimer pointer from an array of pointers
 * also decreases array size by one and moves memory to fill empty place
 */

void countdown_timer_list_remove(CountdownTimer **timer_array, uint8_t *timer_array_count,
                                 uint8_t timer_index) {
  // check inputs
  if (timer_array == NULL || (*timer_array_count) == 0) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Attempted to remove CountdownTimer from NULL or empty array");
    return;
  }
  // remove the timer
  if (timer_index < (*timer_array_count)) {
    memmove(&timer_array[timer_index], &timer_array[timer_index + 1],
      sizeof(CountdownTimer*) * ((*timer_array_count) - timer_index - 1));
    (*timer_array_count)--;
  }
}



/*
 * gets CountdownTimer index by pointer
 *
 * loops through an array of CountdownTimer pointers looking for a specific one
 * and returns the index of that pointer
 */

int16_t countdown_timer_list_get_timer_index(CountdownTimer **timer_array,
    uint8_t timer_array_count, CountdownTimer *countdown_timer) {
  // check inputs
  if (timer_array == NULL || timer_array_count == 0) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Attempted to find CountdownTimer in NULL or empty array");
    return -1;
  }
  // find timer
  for (uint8_t ii = 0; ii < timer_array_count; ii++) {
    if (timer_array[ii] == countdown_timer) {
      return ii;
    }
  }
  // if it failed to find the pointer
  return -1;
}



/*
 * gets the nearest to expired CountdownTimer
 *
 * finds the CountdownTimer with the least time left and returns a pointer to it
 */

CountdownTimer *countdown_timer_list_get_closest_timer(CountdownTimer **timer_array,
                                                       uint8_t timer_array_count) {
  CountdownTimer *countdown_timer = NULL;
  for (uint8_t ii = 0; ii < timer_array_count; ii++) {
    if (countdown_timer_get_paused(timer_array[ii])) {
      continue;
    }
    if (countdown_timer ==  NULL || countdown_timer_get_current_time(timer_array[ii]) <
          countdown_timer_get_current_time(countdown_timer)) {
      countdown_timer = timer_array[ii];
    }
  }
  return countdown_timer;
}

CountdownTimer *countdown_timer_list_get_closest_timer_after(CountdownTimer **timer_array,
                                                       uint8_t timer_array_count,
                                                       CountdownTimer *after_countdown_timer) {
  CountdownTimer *countdown_timer = NULL;
  for (uint8_t ii = 0; ii < timer_array_count; ii++) {
    if (countdown_timer_get_paused(timer_array[ii]) || timer_array[ii] == after_countdown_timer) {
      continue;
    }
    if (countdown_timer ==  NULL || countdown_timer_get_current_time(timer_array[ii]) <
          countdown_timer_get_current_time(countdown_timer)) {
      countdown_timer = timer_array[ii];
    }
  }
  return countdown_timer;
}

/*
 * gets the last updated CountdownTimer
 *
 * finds the CountdownTimer which was updated the most recently
 */

CountdownTimer *countdown_timer_list_get_last_updated_timer(CountdownTimer **timer_array,
                                                       uint8_t timer_array_count) {
  CountdownTimer *countdown_timer = NULL;
  for (uint8_t ii = 0; ii < timer_array_count; ii++) {
    if (countdown_timer ==  NULL || timer_array[ii]->last_update > countdown_timer->last_update) {
      countdown_timer = timer_array[ii];
    }
  }
  return countdown_timer;
}



/*
 * gets a CountdownTimer by its ID or returns NULL if none were found
 */

CountdownTimer *countdown_timer_list_get_timer_by_id(CountdownTimer **timer_array,
                                                     uint8_t timer_array_count, int32_t id) {
  for (uint8_t ii = 0; ii < timer_array_count; ii++) {
    if (countdown_timer_get_id(timer_array[ii]) == id) {
      return timer_array[ii];
    }
  }
  // no match was found
  return NULL;
}



/*
 * destroys all CountdownTimers in an array
 *
 * iterates over all timers and calls countdown_timer_destroy,
 * which checks for NULL pointers automatically
 */

void countdown_timer_list_destroy_all(CountdownTimer **timer_array, uint8_t *timer_array_count) {
  for (uint8_t ii = 0; ii < (*timer_array_count); ii++) {
    countdown_timer_destroy(timer_array[ii]);
  }
  (*timer_array_count) = 0;
}



/*
 * saves all the timers to persistent storage
 */

void countdown_timer_list_save(CountdownTimer **timer_array, uint8_t timer_array_count,
                               uint32_t key) {
  persist_write_int(key++, timer_array_count);
  for (uint8_t ii = 0; ii < timer_array_count; ii++) {
    persist_write_data(key++, timer_array[ii], sizeof(CountdownTimer));
  }
}



/*
 * loads all timers from persistent storage
 */

void countdown_timer_list_load(CountdownTimer **timer_array, uint8_t timer_array_max,
                               uint8_t *timer_array_count, uint32_t key) {
  (*timer_array_count) = 0;
  int32_t stored_count = persist_read_int(key++);
  // Reject a corrupt or foreign-format count (e.g. data left behind by a
  // different/older build that reuses this UUID). Loading more than the array
  // can hold would overflow timer_array.
  if (stored_count < 0 || stored_count > timer_array_max) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Ignoring persisted timers: bad count %d", (int)stored_count);
    return;
  }
  for (int32_t ii = 0; ii < stored_count; ii++, key++) {
    if (!persist_exists(key)) {
      APP_LOG(APP_LOG_LEVEL_WARNING, "Ignoring persisted timers: missing blob");
      countdown_timer_list_destroy_all(timer_array, timer_array_count);
      return;
    }
    int blob_size = persist_get_size(key);
    bool is_v1 = (blob_size == (int)sizeof(CountdownTimerV1));
    bool is_v2 = (blob_size == (int)sizeof(CountdownTimerV2));
    if (blob_size != (int)sizeof(CountdownTimer) && !is_v1 && !is_v2) {
      APP_LOG(APP_LOG_LEVEL_WARNING, "Ignoring persisted timers: incompatible blob size");
      countdown_timer_list_destroy_all(timer_array, timer_array_count);
      return;
    }
    CountdownTimer *timer = (CountdownTimer*)malloc(sizeof(CountdownTimer));
    if (!timer) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to allocate memory while loading timers!");
      return;
    }
    if (is_v1) {
      CountdownTimerV1 v1 = {0};
      persist_read_data(key, &v1, sizeof(CountdownTimerV1));
      memset(timer, 0, sizeof(CountdownTimer));
      timer->start_ms = v1.start_ms;
      timer->duration_ms = v1.duration_ms;
      timer->id = v1.id;
      memcpy(timer->buff, v1.buff, sizeof(timer->buff));
      memcpy(timer->name, v1.name, sizeof(timer->name));
      timer->paused = v1.paused;
      timer->last_update = v1.last_update;
      timer->snooze_ms = countdown_timer_default_snooze_for_duration(v1.duration_ms);
      timer->original_end_ms = 0;
    } else if (is_v2) {
      CountdownTimerV2 v2 = {0};
      persist_read_data(key, &v2, sizeof(CountdownTimerV2));
      memset(timer, 0, sizeof(CountdownTimer));
      timer->start_ms = v2.start_ms;
      timer->duration_ms = v2.duration_ms;
      timer->id = v2.id;
      memcpy(timer->buff, v2.buff, sizeof(timer->buff));
      memcpy(timer->name, v2.name, sizeof(timer->name));
      timer->paused = v2.paused;
      timer->last_update = v2.last_update;
      timer->snooze_ms = (v2.snooze_ms < COUNTDOWN_TIMER_MIN_SNOOZE_MS)
        ? countdown_timer_default_snooze_for_duration(v2.duration_ms)
        : v2.snooze_ms;
      timer->original_end_ms = 0;
    } else {
      persist_read_data(key, timer, sizeof(CountdownTimer));
      if (timer->snooze_ms < COUNTDOWN_TIMER_MIN_SNOOZE_MS) {
        timer->snooze_ms = countdown_timer_default_snooze_for_duration(timer->duration_ms);
      }
      if (timer->original_end_ms < 0) {
        timer->original_end_ms = 0;
      }
    }
    timer->name[COUNTDOWN_TIMER_NAME_MAX - 1] = '\0';
    // Reject blobs whose contents are nonsensical even though the size matched
    // (e.g. same-sized foreign data). A zero/negative duration would later
    // divide by zero when drawing the menu progress bar, and the expired
    // sentinel must only ever appear on a paused timer.
    bool valid_duration = timer->duration_ms > 0;
    bool valid_expired = timer->start_ms != COUNTDOWN_TIMER_EXPIRED || timer->paused;
    if (!valid_duration || !valid_expired) {
      APP_LOG(APP_LOG_LEVEL_WARNING, "Ignoring persisted timers: invalid timer contents");
      free(timer);
      countdown_timer_list_destroy_all(timer_array, timer_array_count);
      return;
    }
    timer_array[(*timer_array_count)++] = timer;
  }
}



/*
 * gets whether a CountdownTimer is paused or not
 */

bool countdown_timer_get_paused(CountdownTimer *countdown_timer) {
  return countdown_timer->paused;
}



/*
 * gets the start time of a CountdownTimer
 */

int64_t countdown_timer_get_start(CountdownTimer *countdown_timer) {
  return countdown_timer->start_ms;
}



/*
 * gets the current remaining time of the CountdownTimer in milliseconds
 */

int64_t countdown_timer_get_current_time(CountdownTimer *countdown_timer) {
  if (countdown_timer->start_ms == COUNTDOWN_TIMER_EXPIRED) {
    return 0;
  }

  int64_t current_time = countdown_timer->duration_ms;
  if (countdown_timer->paused) {
    current_time += countdown_timer->start_ms;
  } else {
    current_time -= countdown_timer_get_epoch_ms() - countdown_timer->start_ms;
  }

  if (current_time < 0) {
    current_time = 0;
  } else if (current_time > countdown_timer->duration_ms) {
    current_time = countdown_timer->duration_ms;
  }
  return current_time;
}



/*
 * gets the time to display for the CountdownTimer in milliseconds
 *
 * an expired timer displays its total duration rather than zero,
 * matching how it looked before it was started
 */

int64_t countdown_timer_get_display_time(CountdownTimer *countdown_timer) {
  if (countdown_timer->start_ms == COUNTDOWN_TIMER_EXPIRED) {
    return countdown_timer->duration_ms;
  }
  return countdown_timer_get_current_time(countdown_timer);
}



/*
 * gives the CountdownTimer a random new id
 */

void countdown_timer_rand_id(CountdownTimer *countdown_timer, int32_t *current_id_max) {
  int32_t candidate = (int32_t)time(NULL);
  if (candidate <= 0) {
    candidate = 1;
  }
  if (current_id_max != NULL) {
    if (candidate <= *current_id_max) {
      candidate = *current_id_max + 1;
    }
    *current_id_max = candidate;
  }
  countdown_timer->id = candidate;
}



/*
 * gets the id of the CountdownTimer
 */

int32_t countdown_timer_get_id(CountdownTimer *countdown_timer) {
  return countdown_timer->id;
}



/*
 * gets the total duration of the CountdownTimer
 */

int64_t countdown_timer_get_duration(CountdownTimer *countdown_timer) {
  return countdown_timer->duration_ms;
}



/*
 * gets the buffer of the provided CountdownTimer
 */

char *countdown_timer_get_buffer(CountdownTimer *countdown_timer) {
  return countdown_timer->buff;
}



/*
 * formats a value as text and prints it onto the provided memory
 */

void countdown_timer_format_text(int64_t value, char *buff, uint8_t size) {
  uint8_t hr = value / MSEC_IN_HR;
  uint8_t min = value % MSEC_IN_HR / MSEC_IN_MIN;
  uint8_t sec = value % MSEC_IN_MIN / MSEC_IN_SEC;
  if (hr > 0) {
    snprintf(buff, size, "%d:%02d:%02d", hr, min, sec);
  } else {
    snprintf(buff, size, "%d:%02d", min, sec);
  }
}



/*
 * formats the timers value and prints it into it's own buffer
 * also returns a pointer to that buffer for convenience
 */

char *countdown_timer_format_own_buff(CountdownTimer *countdown_timer) {
  countdown_timer_format_text(countdown_timer_get_display_time(countdown_timer),
    countdown_timer->buff, sizeof(countdown_timer->buff));
  return countdown_timer->buff;
}


time_t countdown_timer_get_last_update(CountdownTimer *countdown_timer) {
  return countdown_timer->last_update;
}

const char *countdown_timer_get_name(CountdownTimer *countdown_timer) {
  if (countdown_timer == NULL) {
    return "";
  }
  return countdown_timer->name;
}

bool countdown_timer_has_name(CountdownTimer *countdown_timer) {
  return countdown_timer != NULL && countdown_timer->name[0] != '\0';
}

void countdown_timer_set_name(CountdownTimer *countdown_timer, const char *name) {
  if (countdown_timer == NULL) {
    return;
  }
  countdown_timer->name[0] = '\0';
  if (name != NULL) {
    while (*name == ' ' || *name == '\t' || *name == '\r' || *name == '\n') {
      name++;
    }
    size_t len = strlen(name);
    while (len > 0 && (name[len - 1] == ' ' || name[len - 1] == '\t' ||
                       name[len - 1] == '\r' || name[len - 1] == '\n')) {
      len--;
    }
    if (len >= COUNTDOWN_TIMER_NAME_MAX) {
      len = COUNTDOWN_TIMER_NAME_MAX - 1;
    }
    if (len > 0) {
      memcpy(countdown_timer->name, name, len);
    }
    countdown_timer->name[len] = '\0';
  }
  countdown_timer->last_update = time(NULL);
}

int64_t countdown_timer_default_snooze_for_duration(int64_t duration_ms) {
  int64_t half_sec = (duration_ms / MSEC_IN_SEC) / 2;
  int64_t snooze_ms = half_sec * MSEC_IN_SEC;
  if (snooze_ms > COUNTDOWN_TIMER_DEFAULT_SNOOZE_MS) {
    snooze_ms = COUNTDOWN_TIMER_DEFAULT_SNOOZE_MS;
  }
  if (snooze_ms < COUNTDOWN_TIMER_MIN_SNOOZE_MS) {
    snooze_ms = COUNTDOWN_TIMER_MIN_SNOOZE_MS;
  }
  return snooze_ms;
}

int64_t countdown_timer_get_snooze(CountdownTimer *countdown_timer) {
  if (countdown_timer == NULL) {
    return COUNTDOWN_TIMER_DEFAULT_SNOOZE_MS;
  }
  if (countdown_timer->snooze_ms < COUNTDOWN_TIMER_MIN_SNOOZE_MS) {
    return countdown_timer_default_snooze_for_duration(countdown_timer->duration_ms);
  }
  return countdown_timer->snooze_ms;
}

void countdown_timer_set_snooze(CountdownTimer *countdown_timer, int64_t snooze_ms) {
  if (countdown_timer == NULL) {
    return;
  }
  if (snooze_ms < COUNTDOWN_TIMER_MIN_SNOOZE_MS) {
    snooze_ms = COUNTDOWN_TIMER_MIN_SNOOZE_MS;
  }
  countdown_timer->snooze_ms = snooze_ms;
  countdown_timer->last_update = time(NULL);
}

void countdown_timer_snooze(CountdownTimer *countdown_timer) {
  if (countdown_timer == NULL) {
    return;
  }
  int64_t now = countdown_timer_get_epoch_ms();
  int64_t saved_origin = countdown_timer->original_end_ms;
  if (saved_origin <= 0 || saved_origin > now) {
    saved_origin = now;
  }
  countdown_timer_update(countdown_timer, countdown_timer_get_snooze(countdown_timer), false);
  countdown_timer->original_end_ms = saved_origin;
  countdown_timer_start(countdown_timer);
}

int64_t countdown_timer_get_original_end(CountdownTimer *countdown_timer) {
  if (countdown_timer == NULL) {
    return 0;
  }
  return countdown_timer->original_end_ms;
}

void countdown_timer_clear_original_end(CountdownTimer *countdown_timer) {
  if (countdown_timer != NULL) {
    countdown_timer->original_end_ms = 0;
  }
}

void countdown_timer_format_overtime(int64_t elapsed_ms, char *buff, size_t size) {
  if (buff == NULL || size == 0) {
    return;
  }
  if (elapsed_ms < 0) {
    elapsed_ms = 0;
  }
  int64_t total_sec = elapsed_ms / MSEC_IN_SEC;
  int64_t hr = total_sec / 3600;
  int64_t min = (total_sec % 3600) / 60;
  int64_t sec = total_sec % 60;
  if (hr > 0) {
    snprintf(buff, size, "+%ld:%02ld:%02ld", (long)hr, (long)min, (long)sec);
  } else {
    snprintf(buff, size, "+%ld:%02ld", (long)min, (long)sec);
  }
}

