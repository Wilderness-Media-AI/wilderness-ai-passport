#include "badge_walkie_logic.h"

void badge_walkie_logic_init(badge_walkie_logic_t *state)
{
    state->active = false;
    state->talking = false;
}

bool badge_walkie_logic_is_active(const badge_walkie_logic_t *state)
{
    return state->active;
}

bool badge_walkie_logic_remote_enter(badge_walkie_logic_t *state)
{
    if (state->active) return false;
    state->active = true;
    state->talking = false;
    return true;
}

bool badge_walkie_logic_remote_exit(badge_walkie_logic_t *state)
{
    if (!state->active) return false;
    state->active = false;
    state->talking = false;
    return true;
}

badge_walkie_action_t badge_walkie_logic_handle(badge_walkie_logic_t *state,
                                                 badge_walkie_key_t key,
                                                 badge_walkie_event_t event)
{
    if (!state->active) {
        if (key == BADGE_WALKIE_KEY_DOWN && event == BADGE_WALKIE_EVENT_LONG) {
            state->active = true;
            return BADGE_WALKIE_ACTION_ENTER | BADGE_WALKIE_ACTION_REDRAW;
        }
        return BADGE_WALKIE_ACTION_NONE;
    }

    if (key == BADGE_WALKIE_KEY_OK && event == BADGE_WALKIE_EVENT_CLICK) {
        badge_walkie_action_t actions = BADGE_WALKIE_ACTION_EXIT |
                                        BADGE_WALKIE_ACTION_REDRAW;
        if (state->talking) actions |= BADGE_WALKIE_ACTION_PTT_UP;
        state->active = false;
        state->talking = false;
        return actions;
    }

    if (key == BADGE_WALKIE_KEY_UP && event == BADGE_WALKIE_EVENT_PRESS &&
        !state->talking) {
        state->talking = true;
        return BADGE_WALKIE_ACTION_PTT_DOWN | BADGE_WALKIE_ACTION_REDRAW;
    }
    if (key == BADGE_WALKIE_KEY_UP && event == BADGE_WALKIE_EVENT_RELEASE &&
        state->talking) {
        state->talking = false;
        return BADGE_WALKIE_ACTION_PTT_UP | BADGE_WALKIE_ACTION_REDRAW;
    }
    return BADGE_WALKIE_ACTION_NONE;
}
