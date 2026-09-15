#pragma once

#include "esp_err.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BADGE_WALKIE_RADIO_OFF = 0,
    BADGE_WALKIE_RADIO_STARTING,
    BADGE_WALKIE_RADIO_STANDBY,
    BADGE_WALKIE_RADIO_CALLING,
    BADGE_WALKIE_RADIO_SEARCHING,
    BADGE_WALKIE_RADIO_READY,
    BADGE_WALKIE_RADIO_ERROR,
} badge_walkie_radio_state_t;

typedef struct {
    badge_walkie_radio_state_t state;
    bool local_talking;
    bool remote_talking;
    bool session_active;
    bool incoming_call;
    char peer_name[16];
    uint32_t received_frames;
    uint32_t dropped_frames;
} badge_walkie_radio_status_t;

typedef void (*badge_walkie_status_callback_t)(void *context);

esp_err_t badge_walkie_radio_init(const char *device_name,
                                  badge_walkie_status_callback_t callback,
                                  void *callback_context);
void badge_walkie_radio_enter(void);
void badge_walkie_radio_exit(void);
void badge_walkie_radio_resume(void);
void badge_walkie_radio_suspend(void);
bool badge_walkie_radio_set_talking(bool talking);
void badge_walkie_radio_get_status(badge_walkie_radio_status_t *status);
