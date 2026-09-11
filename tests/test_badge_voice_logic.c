#include <assert.h>

#include "badge_voice_logic.h"

int main(void)
{
    badge_voice_logic_t state;
    badge_voice_logic_init(&state);
    assert(!badge_voice_logic_is_active(&state));

    /* The badge's existing short-UP page navigation remains untouched. */
    assert(badge_voice_logic_handle(&state, BADGE_VOICE_KEY_UP,
                                    BADGE_VOICE_EVENT_CLICK) == BADGE_VOICE_ACTION_NONE);

    /* From badge mode, long-UP enters the dedicated voice-input screen. */
    assert(badge_voice_logic_handle(&state, BADGE_VOICE_KEY_UP,
                                    BADGE_VOICE_EVENT_LONG) ==
           (BADGE_VOICE_ACTION_ENTER | BADGE_VOICE_ACTION_REDRAW));
    assert(badge_voice_logic_is_active(&state));

    /* In voice mode, press/hold UP starts PTT immediately; only release stops. */
    assert(badge_voice_logic_handle(&state, BADGE_VOICE_KEY_UP,
                                    BADGE_VOICE_EVENT_PRESS) ==
           (BADGE_VOICE_ACTION_PTT_DOWN | BADGE_VOICE_ACTION_REDRAW));
    assert(badge_voice_logic_is_talking(&state));
    assert(badge_voice_logic_handle(&state, BADGE_VOICE_KEY_UP,
                                    BADGE_VOICE_EVENT_LONG) == BADGE_VOICE_ACTION_NONE);
    assert(badge_voice_logic_is_talking(&state));
    assert(badge_voice_logic_handle(&state, BADGE_VOICE_KEY_UP,
                                    BADGE_VOICE_EVENT_RELEASE) ==
           (BADGE_VOICE_ACTION_PTT_UP | BADGE_VOICE_ACTION_REDRAW));
    assert(!badge_voice_logic_is_talking(&state));

    /* DOWN sends the transcribed text; short-OK returns to badge mode. */
    assert(badge_voice_logic_handle(&state, BADGE_VOICE_KEY_DOWN,
                                    BADGE_VOICE_EVENT_CLICK) ==
           (BADGE_VOICE_ACTION_SEND | BADGE_VOICE_ACTION_REDRAW));
    assert(badge_voice_logic_handle(&state, BADGE_VOICE_KEY_OK,
                                    BADGE_VOICE_EVENT_CLICK) ==
           (BADGE_VOICE_ACTION_EXIT | BADGE_VOICE_ACTION_REDRAW));
    assert(!badge_voice_logic_is_active(&state));

    /* Exiting while held must release Control and stop the microphone first. */
    assert(badge_voice_logic_handle(&state, BADGE_VOICE_KEY_UP,
                                    BADGE_VOICE_EVENT_LONG) ==
           (BADGE_VOICE_ACTION_ENTER | BADGE_VOICE_ACTION_REDRAW));
    assert(badge_voice_logic_handle(&state, BADGE_VOICE_KEY_UP,
                                    BADGE_VOICE_EVENT_PRESS) ==
           (BADGE_VOICE_ACTION_PTT_DOWN | BADGE_VOICE_ACTION_REDRAW));
    assert(badge_voice_logic_handle(&state, BADGE_VOICE_KEY_OK,
                                    BADGE_VOICE_EVENT_CLICK) ==
           (BADGE_VOICE_ACTION_PTT_UP | BADGE_VOICE_ACTION_EXIT |
            BADGE_VOICE_ACTION_REDRAW));
    assert(!badge_voice_logic_is_active(&state));
    assert(!badge_voice_logic_is_talking(&state));
    return 0;
}
