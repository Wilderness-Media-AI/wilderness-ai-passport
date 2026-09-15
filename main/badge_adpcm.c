/*
 * IMA ADPCM encoder adapted from folo-ai-passport-voice (MIT License).
 * Each 100 ms block is independently decodable:
 * [int16 predictor LE][uint8 step index][0][800 bytes nibbles].
 */
#include "badge_adpcm.h"

static const int16_t s_steps[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130,
    143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449,
    494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411,
    1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660,
    4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493,
    10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385,
    24623, 27086, 29794, 32767,
};

static const int8_t s_index_delta[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8,
};

static int clamp_index(int index)
{
    if (index < 0) return 0;
    if (index > 88) return 88;
    return index;
}

static int16_t clamp_sample(int32_t sample)
{
    if (sample > 32767) return 32767;
    if (sample < -32768) return -32768;
    return (int16_t)sample;
}

void badge_adpcm_reset(badge_adpcm_state_t *state)
{
    if (!state) return;
    state->predictor = 0;
    state->index = 0;
}

static uint8_t encode_sample(badge_adpcm_state_t *state, int16_t sample)
{
    int step = s_steps[state->index];
    int32_t difference = (int32_t)sample - state->predictor;
    uint8_t code = 0;
    if (difference < 0) {
        code = 8;
        difference = -difference;
    }
    int32_t threshold = step;
    if (difference >= threshold) {
        code |= 4;
        difference -= threshold;
    }
    threshold >>= 1;
    if (difference >= threshold) {
        code |= 2;
        difference -= threshold;
    }
    threshold >>= 1;
    if (difference >= threshold) code |= 1;

    int32_t delta = step >> 3;
    if (code & 4) delta += step;
    if (code & 2) delta += step >> 1;
    if (code & 1) delta += step >> 2;
    state->predictor = clamp_sample((int32_t)state->predictor +
                                    ((code & 8) ? -delta : delta));
    state->index = (uint8_t)clamp_index((int)state->index + s_index_delta[code]);
    return code;
}

size_t badge_adpcm_encode(badge_adpcm_state_t *state, const int16_t *pcm,
                          size_t samples, uint8_t *output, size_t capacity)
{
    if (!state || !pcm || !output || samples == 0) return 0;
    size_t required = 4 + samples / 2;
    if (capacity < required) return 0;

    state->predictor = pcm[0];
    output[0] = (uint8_t)((uint16_t)pcm[0] & 0xff);
    output[1] = (uint8_t)(((uint16_t)pcm[0] >> 8) & 0xff);
    output[2] = state->index;
    output[3] = 0;

    size_t write_index = 4;
    for (size_t i = 1; i < samples; ++i) {
        uint8_t code = encode_sample(state, pcm[i]);
        if (i & 1) {
            output[write_index] = code & 0x0f;
            if (i + 1 == samples) ++write_index;
        } else {
            output[write_index] |= (uint8_t)(code << 4);
            ++write_index;
        }
    }
    return required;
}

size_t badge_adpcm_decode(const uint8_t *input, size_t input_bytes,
                          size_t samples, int16_t *pcm, size_t pcm_capacity)
{
    if (!input || !pcm || samples == 0 || pcm_capacity < samples ||
        input_bytes < 4 + samples / 2 || input[2] > 88) {
        return 0;
    }

    badge_adpcm_state_t state = {
        .predictor = (int16_t)((uint16_t)input[0] | ((uint16_t)input[1] << 8)),
        .index = input[2],
    };
    pcm[0] = state.predictor;
    for (size_t i = 1; i < samples; ++i) {
        uint8_t packed = input[4 + (i - 1) / 2];
        uint8_t code = (i & 1) ? (packed & 0x0f) : (packed >> 4);
        int step = s_steps[state.index];
        int32_t delta = step >> 3;
        if (code & 4) delta += step;
        if (code & 2) delta += step >> 1;
        if (code & 1) delta += step >> 2;
        state.predictor = clamp_sample((int32_t)state.predictor +
                                       ((code & 8) ? -delta : delta));
        state.index = (uint8_t)clamp_index((int)state.index +
                                           s_index_delta[code]);
        pcm[i] = state.predictor;
    }
    return samples;
}
