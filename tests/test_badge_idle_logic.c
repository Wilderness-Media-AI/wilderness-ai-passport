#include <assert.h>

#include "badge_idle_logic.h"

int main(void)
{
    badge_idle_logic_t state;
    badge_idle_logic_init(&state);
    assert(!badge_idle_logic_is_screen_off(&state));

    badge_idle_logic_timeout(&state);
    assert(badge_idle_logic_is_screen_off(&state));

    /* The first physical gesture wakes the screen but never changes the page. */
    assert(!badge_idle_logic_on_event(&state, BADGE_IDLE_EVENT_PRESS));
    assert(!badge_idle_logic_is_screen_off(&state));
    assert(!badge_idle_logic_on_event(&state, BADGE_IDLE_EVENT_RELEASE));
    assert(!badge_idle_logic_on_event(&state, BADGE_IDLE_EVENT_CLICK));

    /* The next gesture is forwarded normally. */
    assert(badge_idle_logic_on_event(&state, BADGE_IDLE_EVENT_PRESS));
    assert(badge_idle_logic_on_event(&state, BADGE_IDLE_EVENT_RELEASE));
    assert(badge_idle_logic_on_event(&state, BADGE_IDLE_EVENT_CLICK));

    /* A terminal event can also arrive without a preceding PRESS notification. */
    badge_idle_logic_timeout(&state);
    assert(!badge_idle_logic_on_event(&state, BADGE_IDLE_EVENT_LONG));
    assert(badge_idle_logic_on_event(&state, BADGE_IDLE_EVENT_LONG));
    return 0;
}
