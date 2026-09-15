#include <assert.h>

#include "badge_audio_owner.h"

int main(void)
{
    assert(badge_audio_owner_current() == BADGE_AUDIO_OWNER_NONE);
    assert(badge_audio_owner_claim(BADGE_AUDIO_OWNER_BLE_MIC));
    assert(badge_audio_owner_claim(BADGE_AUDIO_OWNER_BLE_MIC));
    assert(!badge_audio_owner_claim(BADGE_AUDIO_OWNER_WALKIE));
    badge_audio_owner_release(BADGE_AUDIO_OWNER_WALKIE);
    assert(badge_audio_owner_current() == BADGE_AUDIO_OWNER_BLE_MIC);
    badge_audio_owner_release(BADGE_AUDIO_OWNER_BLE_MIC);
    assert(badge_audio_owner_claim(BADGE_AUDIO_OWNER_WALKIE));
    badge_audio_owner_release(BADGE_AUDIO_OWNER_WALKIE);
    assert(badge_audio_owner_current() == BADGE_AUDIO_OWNER_NONE);
    assert(!badge_audio_owner_claim(BADGE_AUDIO_OWNER_NONE));
    return 0;
}
