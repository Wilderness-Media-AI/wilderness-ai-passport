#pragma once

#include <stdbool.h>

#include "bsp_button.h"

void wilderness_badge_start(bool battery_ready);
void wilderness_badge_key(bsp_btn_t btn, bsp_btn_ev_t ev);

/* Restores the display and resets the idle timer. Returns true only when this
 * event should continue to the badge UI; the first gesture after screen-off
 * is consumed entirely as a wake gesture. */
bool wilderness_badge_activity(bsp_btn_ev_t ev);
