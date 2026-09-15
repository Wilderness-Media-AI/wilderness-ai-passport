#pragma once

#include <stddef.h>
#include <stdint.h>

#define BADGE_ADPCM_SAMPLES 1600
#define BADGE_ADPCM_BYTES   804

typedef struct {
    int16_t predictor;
    uint8_t index;
} badge_adpcm_state_t;

void badge_adpcm_reset(badge_adpcm_state_t *state);
size_t badge_adpcm_encode(badge_adpcm_state_t *state, const int16_t *pcm,
                          size_t samples, uint8_t *output, size_t capacity);
size_t badge_adpcm_decode(const uint8_t *input, size_t input_bytes,
                          size_t samples, int16_t *pcm, size_t pcm_capacity);
