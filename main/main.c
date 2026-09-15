#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "bsp_pins.h"
#include "badge_ble_mic.h"
#include "wilderness_badge.h"

#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "wilderness_badge";

static void on_key(bsp_btn_t btn, bsp_btn_ev_t ev, void *user)
{
    (void)user;
    if (!wilderness_badge_activity(ev)) {
        return;
    }
    if (!bsp_lvgl_lock(500)) {
        ESP_LOGW(TAG, "LVGL lock timeout while handling key");
        return;
    }
    wilderness_badge_key(btn, ev);
    bsp_lvgl_unlock();
}

void app_main(void)
{
    ESP_LOGI(TAG, "WILDERNESS digital badge starting");

    esp_err_t nvs_status = nvs_flash_init();
    if (nvs_status != ESP_OK) {
        ESP_LOGW(TAG, "NVS unavailable: %s", esp_err_to_name(nvs_status));
    }

    esp_err_t i2c_status = bsp_i2c_init();
    if (i2c_status != ESP_OK) {
        ESP_LOGW(TAG, "I2C unavailable: %s", esp_err_to_name(i2c_status));
    }

    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG,
                 "Display init failed (MOSI=%d SCLK=%d CS=%d DC=%d BL=%d)",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS, BSP_LCD_DC, BSP_LCD_BL);
        return;
    }
    bsp_display_backlight(100);

    bool battery_ready = (i2c_status == ESP_OK && bsp_battery_init() == ESP_OK);
    esp_err_t button_status = bsp_button_init(on_key, NULL);
    if (button_status != ESP_OK) {
        ESP_LOGW(TAG, "Buttons unavailable: %s", esp_err_to_name(button_status));
    }

    if (bsp_lvgl_lock(1000)) {
        wilderness_badge_start(battery_ready);
        bsp_lvgl_unlock();
    } else {
        ESP_LOGE(TAG, "Unable to acquire LVGL lock for initial screen");
        return;
    }

    ESP_LOGI(TAG, "Ready: display=1 buttons=%d battery=%d cardid untouched",
             button_status == ESP_OK, battery_ready);

    esp_err_t mic_status = badge_ble_mic_init();
    if (mic_status != ESP_OK) {
        ESP_LOGW(TAG, "BLE microphone unavailable: %s", esp_err_to_name(mic_status));
    }
}
