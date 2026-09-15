#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BADGE_WALKIE_KEY_UP = 0,
    BADGE_WALKIE_KEY_DOWN,
    BADGE_WALKIE_KEY_OK,
} badge_walkie_key_t;

typedef enum {
    BADGE_WALKIE_EVENT_PRESS = 0,
    BADGE_WALKIE_EVENT_RELEASE,
    BADGE_WALKIE_EVENT_CLICK,
    BADGE_WALKIE_EVENT_DOUBLE,
    BADGE_WALKIE_EVENT_LONG,
} badge_walkie_event_t;

typedef enum {
    BADGE_WALKIE_ACTION_NONE = 0,
    BADGE_WALKIE_ACTION_ENTER = 1U << 0,
    BADGE_WALKIE_ACTION_EXIT = 1U << 1,
    BADGE_WALKIE_ACTION_PTT_DOWN = 1U << 2,
    BADGE_WALKIE_ACTION_PTT_UP = 1U << 3,
    BADGE_WALKIE_ACTION_REDRAW = 1U << 4,
} badge_walkie_action_t;

typedef struct {
    bool active;
    bool talking;
} badge_walkie_logic_t;

void badge_walkie_logic_init(badge_walkie_logic_t *state);
bool badge_walkie_logic_is_active(const badge_walkie_logic_t *state);
bool badge_walkie_logic_remote_enter(badge_walkie_logic_t *state);
bool badge_walkie_logic_remote_exit(badge_walkie_logic_t *state);
badge_walkie_action_t badge_walkie_logic_handle(badge_walkie_logic_t *state,
                                                 badge_walkie_key_t key,
                                                 badge_walkie_event_t event);
