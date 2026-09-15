#pragma once

#include "esp_err.h"

#include <stdbool.h>

typedef enum {
    BADGE_BLE_INPUT_PTT_DOWN = 1,
    BADGE_BLE_INPUT_PTT_UP = 2,
    BADGE_BLE_INPUT_SEND = 3,
} badge_ble_input_event_t;

/* Initializes the BLE microphone service in a disabled state. */
esp_err_t badge_ble_mic_init(void);

/* Enables advertising only while the voice page is active. Disabling stops
 * capture, disconnects the Mac, stops advertising, and drops queued inputs. */
bool badge_ble_mic_set_enabled(bool enabled);

/* Non-blocking UI-facing controls. Audio I/O remains in the worker task. */
bool badge_ble_mic_set_streaming(bool start);
bool badge_ble_mic_send_input_event(badge_ble_input_event_t event);
bool badge_ble_mic_is_ready(void);
