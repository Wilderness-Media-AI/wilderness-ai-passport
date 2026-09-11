#pragma once

#include <stdbool.h>
#include <stdatomic.h>

typedef enum {
    BADGE_IDLE_EVENT_PRESS = 0,
    BADGE_IDLE_EVENT_CLICK,
    BADGE_IDLE_EVENT_DOUBLE,
    BADGE_IDLE_EVENT_LONG,
    BADGE_IDLE_EVENT_RELEASE,
} badge_idle_event_t;

typedef struct {
    atomic_bool screen_off;
    atomic_bool suppress_current_gesture;
} badge_idle_logic_t;

void badge_idle_logic_init(badge_idle_logic_t *state);
void badge_idle_logic_timeout(badge_idle_logic_t *state);
bool badge_idle_logic_is_screen_off(const badge_idle_logic_t *state);

/* Returns true only when the event should reach the badge UI. */
bool badge_idle_logic_on_event(badge_idle_logic_t *state, badge_idle_event_t event);
