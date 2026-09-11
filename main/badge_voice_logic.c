#include "badge_voice_logic.h"

void badge_voice_logic_init(badge_voice_logic_t *state)
{
    state->active = false;
    state->talking = false;
}

bool badge_voice_logic_is_active(const badge_voice_logic_t *state)
{
    return state->active;
}

bool badge_voice_logic_is_talking(const badge_voice_logic_t *state)
{
    return state->talking;
}

badge_voice_action_t badge_voice_logic_handle(badge_voice_logic_t *state,
                                               badge_voice_key_t key,
                                               badge_voice_event_t event)
{
    if (!state->active) {
        if (key == BADGE_VOICE_KEY_UP && event == BADGE_VOICE_EVENT_LONG) {
            state->active = true;
            return BADGE_VOICE_ACTION_ENTER | BADGE_VOICE_ACTION_REDRAW;
        }
        return BADGE_VOICE_ACTION_NONE;
    }

    if (key == BADGE_VOICE_KEY_OK && event == BADGE_VOICE_EVENT_CLICK) {
        badge_voice_action_t actions = BADGE_VOICE_ACTION_EXIT |
                                       BADGE_VOICE_ACTION_REDRAW;
        if (state->talking) actions |= BADGE_VOICE_ACTION_PTT_UP;
        state->active = false;
        state->talking = false;
        return actions;
    }

    if (key == BADGE_VOICE_KEY_UP && event == BADGE_VOICE_EVENT_PRESS &&
        !state->talking) {
        state->talking = true;
        return BADGE_VOICE_ACTION_PTT_DOWN | BADGE_VOICE_ACTION_REDRAW;
    }
    if (key == BADGE_VOICE_KEY_UP && event == BADGE_VOICE_EVENT_RELEASE &&
        state->talking) {
        state->talking = false;
        return BADGE_VOICE_ACTION_PTT_UP | BADGE_VOICE_ACTION_REDRAW;
    }
    if (key == BADGE_VOICE_KEY_DOWN && event == BADGE_VOICE_EVENT_CLICK) {
        return BADGE_VOICE_ACTION_SEND | BADGE_VOICE_ACTION_REDRAW;
    }
    return BADGE_VOICE_ACTION_NONE;
}
