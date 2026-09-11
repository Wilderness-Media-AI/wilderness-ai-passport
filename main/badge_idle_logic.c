#include "badge_idle_logic.h"

void badge_idle_logic_init(badge_idle_logic_t *state)
{
    atomic_init(&state->screen_off, false);
    atomic_init(&state->suppress_current_gesture, false);
}

void badge_idle_logic_timeout(badge_idle_logic_t *state)
{
    atomic_store(&state->screen_off, true);
}

bool badge_idle_logic_is_screen_off(const badge_idle_logic_t *state)
{
    return atomic_load(&state->screen_off);
}

bool badge_idle_logic_on_event(badge_idle_logic_t *state, badge_idle_event_t event)
{
    if (atomic_exchange(&state->screen_off, false)) {
        /* A PRESS normally has a CLICK/LONG terminal event. Suppress that
         * complete gesture so waking the display cannot change the UI. */
        atomic_store(&state->suppress_current_gesture,
                     event == BADGE_IDLE_EVENT_PRESS);
        return false;
    }

    if (atomic_load(&state->suppress_current_gesture)) {
        /* A short gesture reports RELEASE before CLICK. Keep suppressing until
         * CLICK/DOUBLE/LONG so the wake gesture cannot leak into the UI. */
        if (event != BADGE_IDLE_EVENT_PRESS &&
            event != BADGE_IDLE_EVENT_RELEASE) {
            atomic_store(&state->suppress_current_gesture, false);
        }
        return false;
    }

    /* Awake PRESS/RELEASE must reach push-to-talk mode. Ordinary badge pages
     * simply ignore them. */
    return true;
}
