#include "badge_audio_owner.h"

#include <stdatomic.h>

static atomic_int s_owner = ATOMIC_VAR_INIT(BADGE_AUDIO_OWNER_NONE);

bool badge_audio_owner_claim(badge_audio_owner_t owner)
{
    if (owner == BADGE_AUDIO_OWNER_NONE) return false;
    int expected = BADGE_AUDIO_OWNER_NONE;
    if (atomic_compare_exchange_strong(&s_owner, &expected, owner)) return true;
    return expected == (int)owner;
}

void badge_audio_owner_release(badge_audio_owner_t owner)
{
    int expected = owner;
    (void)atomic_compare_exchange_strong(&s_owner, &expected,
                                         BADGE_AUDIO_OWNER_NONE);
}

badge_audio_owner_t badge_audio_owner_current(void)
{
    return (badge_audio_owner_t)atomic_load(&s_owner);
}
