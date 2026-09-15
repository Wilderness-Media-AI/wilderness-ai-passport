#include <assert.h>

#include "badge_walkie_logic.h"

int main(void)
{
    badge_walkie_logic_t state;
    badge_walkie_logic_init(&state);
    assert(!badge_walkie_logic_is_active(&state));

    assert(badge_walkie_logic_remote_enter(&state));
    assert(badge_walkie_logic_is_active(&state));
    assert(!badge_walkie_logic_remote_enter(&state));
    assert(badge_walkie_logic_remote_exit(&state));
    assert(!badge_walkie_logic_is_active(&state));
    assert(!badge_walkie_logic_remote_exit(&state));

    assert(badge_walkie_logic_handle(&state, BADGE_WALKIE_KEY_DOWN,
                                      BADGE_WALKIE_EVENT_CLICK) ==
           BADGE_WALKIE_ACTION_NONE);
    assert(badge_walkie_logic_handle(&state, BADGE_WALKIE_KEY_DOWN,
                                      BADGE_WALKIE_EVENT_LONG) ==
           (BADGE_WALKIE_ACTION_ENTER | BADGE_WALKIE_ACTION_REDRAW));
    assert(badge_walkie_logic_is_active(&state));

    assert(badge_walkie_logic_handle(&state, BADGE_WALKIE_KEY_UP,
                                      BADGE_WALKIE_EVENT_PRESS) ==
           (BADGE_WALKIE_ACTION_PTT_DOWN | BADGE_WALKIE_ACTION_REDRAW));
    assert(badge_walkie_logic_handle(&state, BADGE_WALKIE_KEY_UP,
                                      BADGE_WALKIE_EVENT_LONG) ==
           BADGE_WALKIE_ACTION_NONE);
    assert(badge_walkie_logic_handle(&state, BADGE_WALKIE_KEY_UP,
                                      BADGE_WALKIE_EVENT_RELEASE) ==
           (BADGE_WALKIE_ACTION_PTT_UP | BADGE_WALKIE_ACTION_REDRAW));

    assert(badge_walkie_logic_handle(&state, BADGE_WALKIE_KEY_OK,
                                      BADGE_WALKIE_EVENT_CLICK) ==
           (BADGE_WALKIE_ACTION_EXIT | BADGE_WALKIE_ACTION_REDRAW));
    assert(!badge_walkie_logic_is_active(&state));

    assert(badge_walkie_logic_handle(&state, BADGE_WALKIE_KEY_DOWN,
                                      BADGE_WALKIE_EVENT_LONG) ==
           (BADGE_WALKIE_ACTION_ENTER | BADGE_WALKIE_ACTION_REDRAW));
    assert(badge_walkie_logic_handle(&state, BADGE_WALKIE_KEY_UP,
                                      BADGE_WALKIE_EVENT_PRESS) ==
           (BADGE_WALKIE_ACTION_PTT_DOWN | BADGE_WALKIE_ACTION_REDRAW));
    assert(badge_walkie_logic_handle(&state, BADGE_WALKIE_KEY_OK,
                                      BADGE_WALKIE_EVENT_CLICK) ==
           (BADGE_WALKIE_ACTION_PTT_UP | BADGE_WALKIE_ACTION_EXIT |
            BADGE_WALKIE_ACTION_REDRAW));
    return 0;
}
