#pragma once

#include <stdbool.h>

typedef enum {
    BADGE_AUDIO_OWNER_NONE = 0,
    BADGE_AUDIO_OWNER_BLE_MIC,
    BADGE_AUDIO_OWNER_WALKIE,
} badge_audio_owner_t;

bool badge_audio_owner_claim(badge_audio_owner_t owner);
void badge_audio_owner_release(badge_audio_owner_t owner);
badge_audio_owner_t badge_audio_owner_current(void);
