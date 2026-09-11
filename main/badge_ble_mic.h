#pragma once

#include "esp_err.h"

#include <stdbool.h>

typedef enum {
    BADGE_BLE_INPUT_PTT_DOWN = 1,
    BADGE_BLE_INPUT_PTT_UP = 2,
    BADGE_BLE_INPUT_SEND = 3,
} badge_ble_input_event_t;

/* Starts a reconnecting BLE microphone peripheral. Audio capture stays off
 * until the Mac writes 0x01 to CTRL, and stops when it writes 0x00. */
esp_err_t badge_ble_mic_init(void);

/* Non-blocking UI-facing controls. Audio I/O remains in the worker task. */
bool badge_ble_mic_set_streaming(bool start);
bool badge_ble_mic_send_input_event(badge_ble_input_event_t event);
bool badge_ble_mic_is_ready(void);
