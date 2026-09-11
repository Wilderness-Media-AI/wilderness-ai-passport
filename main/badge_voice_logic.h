#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BADGE_VOICE_KEY_UP = 0,
    BADGE_VOICE_KEY_DOWN,
    BADGE_VOICE_KEY_OK,
} badge_voice_key_t;

typedef enum {
    BADGE_VOICE_EVENT_PRESS = 0,
    BADGE_VOICE_EVENT_RELEASE,
    BADGE_VOICE_EVENT_CLICK,
    BADGE_VOICE_EVENT_DOUBLE,
    BADGE_VOICE_EVENT_LONG,
} badge_voice_event_t;

typedef enum {
    BADGE_VOICE_ACTION_NONE = 0,
    BADGE_VOICE_ACTION_ENTER = 1U << 0,
    BADGE_VOICE_ACTION_EXIT = 1U << 1,
    BADGE_VOICE_ACTION_PTT_DOWN = 1U << 2,
    BADGE_VOICE_ACTION_PTT_UP = 1U << 3,
    BADGE_VOICE_ACTION_SEND = 1U << 4,
    BADGE_VOICE_ACTION_REDRAW = 1U << 5,
} badge_voice_action_t;

typedef struct {
    bool active;
    bool talking;
} badge_voice_logic_t;

void badge_voice_logic_init(badge_voice_logic_t *state);
bool badge_voice_logic_is_active(const badge_voice_logic_t *state);
bool badge_voice_logic_is_talking(const badge_voice_logic_t *state);
badge_voice_action_t badge_voice_logic_handle(badge_voice_logic_t *state,
                                               badge_voice_key_t key,
                                               badge_voice_event_t event);
