#include "badge_adpcm.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    int16_t input[16] = {
        0, 1000, 2000, 4000, 8000, 4000, 0, -4000,
        -8000, -4000, 0, 100, 50, 0, -50, -100,
    };
    uint8_t encoded[12] = { 0 };
    static const uint8_t expected[12] = {
        0x00, 0x00, 0x00, 0x00, 0x77, 0x77,
        0xe7, 0xff, 0x68, 0x08, 0x08, 0x08,
    };
    badge_adpcm_state_t state;
    badge_adpcm_reset(&state);
    assert(badge_adpcm_encode(&state, input, 16, encoded, sizeof(encoded)) == 12);
    assert(memcmp(encoded, expected, sizeof(expected)) == 0);
    assert(badge_adpcm_encode(NULL, input, 16, encoded, sizeof(encoded)) == 0);
    assert(badge_adpcm_encode(&state, input, 16, encoded, sizeof(encoded) - 1) == 0);
    puts("test_badge_adpcm: OK");
    return 0;
}
