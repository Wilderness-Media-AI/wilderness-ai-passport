#include "wilderness_badge.h"

#include "badge_idle_logic.h"
#include "badge_ble_mic.h"
#include "badge_voice_logic.h"
#include "badge_walkie_logic.h"
#include "badge_walkie_radio.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "wilderness_logo.h"
#include "wilderness_services_cn.h"

#include "esp_log.h"
#include "esp_timer.h"

#if defined(BADGE_PROFILE_ACTIVE)
#include <badge_profile_private.h>
#else
#define BADGE_PERSON_NAME "EMPLOYEE NAME"
#define BADGE_PERSON_ROLE "EMPLOYEE ROLE"
#define BADGE_WECHAT_LABEL "EMPLOYEE / WECHAT"
#endif

#if defined(BADGE_PROFILE_ACTIVE)
#include <badge_avatar_private.h>
#define BADGE_AVATAR_PROVISIONED 1
#define BADGE_AVATAR_SOURCE badge_avatar_private
#else
#define BADGE_AVATAR_PROVISIONED 0
#endif

#include "lvgl.h"

#include <stdio.h>

#if defined(BADGE_PROFILE_ACTIVE)
#include <wechat_qr_private.h>
#define WECHAT_QR_PROVISIONED 1
#else
#define WECHAT_QR_PROVISIONED 0
#endif

#define COLOR_BLACK  0x090909
#define COLOR_PAPER  0xE8E8E3
#define COLOR_GRAY   0x7A7A78
#define COLOR_GREEN  0xA8FF60

#define PAGE_COUNT 3

#define BADGE_BL_ACTIVE_PERCENT 100
#define BADGE_BL_IDLE_PERCENT   10
#define BADGE_IDLE_DIM_SECONDS        3
#define BADGE_VOICE_IDLE_DIM_SECONDS 60

static const char *TAG = "wilderness_badge";

static lv_obj_t *s_screen;
static int s_page;
static bool s_battery_ready;
static bool s_show_wechat_qr;
static bool s_voice_sent;
static esp_timer_handle_t s_idle_off_timer;
static badge_idle_logic_t s_idle_logic;
static badge_voice_logic_t s_voice_logic;
static badge_walkie_logic_t s_walkie_logic;

static void render(void);
static void restart_idle_timer(int timeout_seconds);

static lv_obj_t *label(lv_obj_t *parent, const char *text, const lv_font_t *font,
                       uint32_t color, int x, int y)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_label_set_text(obj, text);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    lv_obj_set_pos(obj, x, y);
    return obj;
}

static lv_obj_t *centered_label(lv_obj_t *parent, const char *text,
                                const lv_font_t *font, uint32_t color, int y)
{
    lv_obj_t *obj = label(parent, text, font, color, 0, y);
    lv_obj_set_width(obj, 240);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, 0);
    return obj;
}

static lv_obj_t *block(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    return obj;
}

static void add_avatar(lv_obj_t *parent)
{
#if BADGE_AVATAR_PROVISIONED
    lv_obj_t *avatar = lv_image_create(parent);
    lv_image_set_src(avatar, &BADGE_AVATAR_SOURCE);
    lv_obj_set_pos(avatar, 8, -8);
#else
    lv_obj_t *avatar = block(parent, 60, 16, 120, 120, COLOR_PAPER);
    lv_obj_set_style_radius(avatar, LV_RADIUS_CIRCLE, 0);
    lv_obj_t *inner = block(avatar, 4, 4, 104, 104, COLOR_BLACK);
    lv_obj_set_style_radius(inner, LV_RADIUS_CIRCLE, 0);
    lv_obj_t *monogram = label(inner, "WM", &lv_font_montserrat_28,
                              COLOR_PAPER, 0, 0);
    lv_obj_center(monogram);
#endif
}

static void add_battery(lv_obj_t *parent)
{
    if (!s_battery_ready) {
        return;
    }
    int soc = bsp_battery_soc();
    if (soc < 0 || soc > 100) {
        return;
    }
    char text[8];
    snprintf(text, sizeof(text), "%d%%", soc);
    lv_obj_t *obj = label(parent, text, &lv_font_montserrat_14, COLOR_PAPER, 0, 0);
    lv_obj_align(obj, LV_ALIGN_TOP_RIGHT, -16, 14);
}

static void page_identity(lv_obj_t *parent)
{
    add_avatar(parent);
    centered_label(parent, BADGE_PERSON_NAME, &lv_font_montserrat_32, COLOR_PAPER, 208);
    centered_label(parent, "WILDERNESS MEDIA", &lv_font_montserrat_18, COLOR_GREEN, 244);
    centered_label(parent, BADGE_PERSON_ROLE, &lv_font_montserrat_14, COLOR_PAPER, 271);
}

static void page_work(lv_obj_t *parent)
{
    lv_obj_t *services = lv_image_create(parent);
    lv_image_set_src(services, &wilderness_services_cn);
    lv_obj_set_pos(services, 0, 0);
}

static void page_brand(lv_obj_t *parent)
{
    lv_obj_t *logo = lv_image_create(parent);
    lv_image_set_src(logo, &wilderness_logo);
    lv_obj_set_pos(logo, 24, 10);
    centered_label(parent, "WILDERNESS MEDIA", &lv_font_montserrat_20, COLOR_PAPER, 224);
}

static void page_wechat(lv_obj_t *parent)
{
#if WECHAT_QR_PROVISIONED
    lv_obj_t *qr = lv_qrcode_create(parent);
    lv_qrcode_set_size(qr, 232);
    lv_qrcode_set_dark_color(qr, lv_color_hex(0x000000));
    lv_qrcode_set_light_color(qr, lv_color_hex(0xFFFFFF));
    lv_qrcode_set_quiet_zone(qr, true);
    if (lv_qrcode_update(qr, WECHAT_QR_DATA, WECHAT_QR_DATA_LEN) == LV_RESULT_OK) {
        lv_obj_set_pos(qr, 4, 12);
    } else {
        lv_obj_delete(qr);
        label(parent, "QR RENDER ERROR", &lv_font_montserrat_14, COLOR_PAPER, 16, 132);
    }
#else
    label(parent, "QR NOT PROVISIONED", &lv_font_montserrat_14, COLOR_PAPER, 16, 132);
#endif

    centered_label(parent, BADGE_WECHAT_LABEL, &lv_font_montserrat_14, COLOR_PAPER, 258);
    centered_label(parent, "ANY KEY TO RETURN", &lv_font_montserrat_14, COLOR_GRAY, 291);
}

static void page_voice(lv_obj_t *parent)
{
    bool ready = badge_ble_mic_is_ready();
    bool talking = badge_voice_logic_is_talking(&s_voice_logic);
    uint32_t accent = talking ? COLOR_GREEN : COLOR_PAPER;

    label(parent, "WILDERNESS VOICE", &lv_font_montserrat_14,
          COLOR_GREEN, 14, 15);
    lv_obj_t *frame = block(parent, 32, 54, 176, 156, accent);
    lv_obj_set_style_radius(frame, 24, 0);
    lv_obj_t *inside = block(frame, 4, 4, 168, 148, COLOR_BLACK);
    lv_obj_set_style_radius(inside, 21, 0);

    centered_label(parent, talking ? "LISTENING" : "MIC",
                   &lv_font_montserrat_28, accent, 91);
    centered_label(parent,
                   !ready ? "MAC OFFLINE" : (talking ? "RECORDING" :
                                               (s_voice_sent ? "SENT" : "READY")),
                   &lv_font_montserrat_14,
                   ready ? COLOR_GREEN : COLOR_PAPER, 151);
    centered_label(parent, "HOLD UP TO TALK", &lv_font_montserrat_14,
                   COLOR_PAPER, 229);
    centered_label(parent, "DOWN = SEND", &lv_font_montserrat_14,
                   COLOR_GREEN, 257);
    centered_label(parent, "OK = BADGE", &lv_font_montserrat_14,
                   COLOR_PAPER, 285);
}

static void page_walkie(lv_obj_t *parent)
{
    badge_walkie_radio_status_t status;
    badge_walkie_radio_get_status(&status);
    bool talking = status.local_talking || status.remote_talking;
    uint32_t accent = talking ? COLOR_GREEN : COLOR_PAPER;
    const char *main_text = "RADIO";
    const char *state_text = "STARTING";

    if (status.state == BADGE_WALKIE_RADIO_ERROR) {
        state_text = "RADIO ERROR";
    } else if (status.state == BADGE_WALKIE_RADIO_CALLING) {
        state_text = "CALLING";
    } else if (status.state == BADGE_WALKIE_RADIO_SEARCHING) {
        state_text = "SEARCHING";
    } else if (status.state == BADGE_WALKIE_RADIO_READY) {
        state_text = status.peer_name[0] ? status.peer_name : "CONNECTED";
    }
    if (status.local_talking) {
        main_text = "TALKING";
        state_text = "YOU ARE LIVE";
    } else if (status.remote_talking) {
        main_text = "LISTEN";
    }

    label(parent, "WILDERNESS RADIO", &lv_font_montserrat_14,
          COLOR_GREEN, 14, 15);
    lv_obj_t *frame = block(parent, 32, 54, 176, 156, accent);
    lv_obj_set_style_radius(frame, 24, 0);
    lv_obj_t *inside = block(frame, 4, 4, 168, 148, COLOR_BLACK);
    lv_obj_set_style_radius(inside, 21, 0);
    centered_label(parent, main_text, &lv_font_montserrat_28, accent, 91);
    centered_label(parent, state_text, &lv_font_montserrat_14,
                   status.state == BADGE_WALKIE_RADIO_ERROR ? COLOR_PAPER :
                                                              COLOR_GREEN,
                   151);
    centered_label(parent, "HOLD UP TO TALK", &lv_font_montserrat_14,
                   COLOR_PAPER, 237);
    centered_label(parent, "OK = BADGE", &lv_font_montserrat_14,
                   COLOR_PAPER, 276);
}

static void render(void)
{
    lv_obj_t *old = s_screen;
    s_screen = lv_obj_create(NULL);
    lv_obj_remove_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(COLOR_BLACK), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_screen, 0, 0);
    lv_obj_set_style_pad_all(s_screen, 0, 0);

    if (badge_walkie_logic_is_active(&s_walkie_logic)) {
        page_walkie(s_screen);
        add_battery(s_screen);
    } else if (badge_voice_logic_is_active(&s_voice_logic)) {
        page_voice(s_screen);
        add_battery(s_screen);
    } else if (s_show_wechat_qr) {
        page_wechat(s_screen);
    } else {
        if (s_page == 0) {
            page_identity(s_screen);
        } else if (s_page == 1) {
            page_work(s_screen);
        } else {
            page_brand(s_screen);
        }
        /* Keep status text above opaque RGB565 image rectangles. */
        add_battery(s_screen);
    }
    lv_screen_load(s_screen);
    if (old) {
        lv_obj_delete(old);
    }
}

static void walkie_status_changed(void *context)
{
    (void)context;
    badge_walkie_radio_status_t status;
    badge_walkie_radio_get_status(&status);
    bool active = badge_walkie_logic_is_active(&s_walkie_logic);
    bool incoming = status.session_active && status.incoming_call && !active;
    bool ended = active && !status.session_active &&
                 (status.state == BADGE_WALKIE_RADIO_STANDBY ||
                  status.state == BADGE_WALKIE_RADIO_OFF);
    if (!active && !incoming) return;
    if (!bsp_lvgl_lock(100)) return;
    if (incoming && badge_walkie_logic_remote_enter(&s_walkie_logic)) {
        s_show_wechat_qr = false;
        s_voice_sent = false;
        (void)badge_idle_logic_on_event(&s_idle_logic, BADGE_IDLE_EVENT_CLICK);
        bsp_display_backlight(BADGE_BL_ACTIVE_PERCENT);
        restart_idle_timer(BADGE_VOICE_IDLE_DIM_SECONDS);
        ESP_LOGI(TAG, "Incoming radio call from %s; page opened",
                 status.peer_name[0] ? status.peer_name : "peer");
        render();
    } else if (ended && badge_walkie_logic_remote_exit(&s_walkie_logic)) {
        s_page = 0;
        restart_idle_timer(BADGE_IDLE_DIM_SECONDS);
        ESP_LOGI(TAG, "Radio session ended remotely; badge page restored");
        render();
    } else if (badge_walkie_logic_is_active(&s_walkie_logic)) {
        render();
    }
    bsp_lvgl_unlock();
}

/* This callback only touches the atomic idle state and backlight PWM. */
static void idle_off_callback(void *arg)
{
    (void)arg;
    badge_idle_logic_timeout(&s_idle_logic);
    if (badge_idle_logic_is_screen_off(&s_idle_logic)) {
        bsp_display_backlight(BADGE_BL_IDLE_PERCENT);
        ESP_LOGI(TAG, "Backlight dimmed to %d%% after idle timeout",
                 BADGE_BL_IDLE_PERCENT);
    } else {
        /* A button raced with the timeout and already woke the badge. */
        bsp_display_backlight(BADGE_BL_ACTIVE_PERCENT);
    }
}

static void restart_idle_timer(int timeout_seconds)
{
    if (s_idle_off_timer) {
        (void)esp_timer_stop(s_idle_off_timer);
        esp_err_t err = esp_timer_start_once(
            s_idle_off_timer, (uint64_t)timeout_seconds * 1000000ULL);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Unable to start screen-off timer: %s", esp_err_to_name(err));
        }
    }
}

static badge_idle_event_t idle_event_from_button(bsp_btn_ev_t ev)
{
    switch (ev) {
    case BSP_BTN_PRESS:
        return BADGE_IDLE_EVENT_PRESS;
    case BSP_BTN_DOUBLE:
        return BADGE_IDLE_EVENT_DOUBLE;
    case BSP_BTN_LONG:
        return BADGE_IDLE_EVENT_LONG;
    case BSP_BTN_RELEASE:
        return BADGE_IDLE_EVENT_RELEASE;
    case BSP_BTN_CLICK:
    default:
        return BADGE_IDLE_EVENT_CLICK;
    }
}

bool wilderness_badge_activity(bsp_btn_ev_t ev)
{
    bool was_off = badge_idle_logic_is_screen_off(&s_idle_logic);
    bool forward = badge_idle_logic_on_event(&s_idle_logic,
                                              idle_event_from_button(ev));
    bsp_display_backlight(BADGE_BL_ACTIVE_PERCENT);
    restart_idle_timer((badge_voice_logic_is_active(&s_voice_logic) ||
                        badge_walkie_logic_is_active(&s_walkie_logic))
                           ? BADGE_VOICE_IDLE_DIM_SECONDS
                           : BADGE_IDLE_DIM_SECONDS);
    if (was_off) {
        ESP_LOGI(TAG, "Screen wake by button");
    }
    return forward;
}

void wilderness_badge_start(bool battery_ready)
{
    s_battery_ready = battery_ready;
    s_page = 0;
    s_show_wechat_qr = false;
    s_voice_sent = false;
    badge_idle_logic_init(&s_idle_logic);
    badge_voice_logic_init(&s_voice_logic);
    badge_walkie_logic_init(&s_walkie_logic);
    esp_err_t walkie_status = badge_walkie_radio_init(BADGE_PERSON_NAME,
                                                       walkie_status_changed,
                                                       NULL);
    if (walkie_status != ESP_OK) {
        ESP_LOGW(TAG, "Walkie-talkie unavailable: %s",
                 esp_err_to_name(walkie_status));
    } else {
        badge_walkie_radio_resume();
    }
    render();

    const esp_timer_create_args_t timer_args = {
        .callback = idle_off_callback,
        .name = "badge_idle_off",
    };
    if (esp_timer_create(&timer_args, &s_idle_off_timer) != ESP_OK) {
        s_idle_off_timer = NULL;
        ESP_LOGW(TAG, "Screen-off timer unavailable; backlight stays on");
        return;
    }
    restart_idle_timer(BADGE_IDLE_DIM_SECONDS);
}

void wilderness_badge_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (s_show_wechat_qr) {
        if (ev == BSP_BTN_CLICK) {
            s_show_wechat_qr = false;
            render();
        }
        return;
    }

    badge_walkie_event_t walkie_event;
    switch (ev) {
    case BSP_BTN_PRESS: walkie_event = BADGE_WALKIE_EVENT_PRESS; break;
    case BSP_BTN_RELEASE: walkie_event = BADGE_WALKIE_EVENT_RELEASE; break;
    case BSP_BTN_DOUBLE: walkie_event = BADGE_WALKIE_EVENT_DOUBLE; break;
    case BSP_BTN_LONG: walkie_event = BADGE_WALKIE_EVENT_LONG; break;
    case BSP_BTN_CLICK:
    default: walkie_event = BADGE_WALKIE_EVENT_CLICK; break;
    }

    bool walkie_was_active = badge_walkie_logic_is_active(&s_walkie_logic);
    if (walkie_was_active || !badge_voice_logic_is_active(&s_voice_logic)) {
        badge_walkie_action_t walkie_actions = badge_walkie_logic_handle(
            &s_walkie_logic, (badge_walkie_key_t)btn, walkie_event);
        if (walkie_actions & BADGE_WALKIE_ACTION_ENTER) {
            s_show_wechat_qr = false;
            (void)badge_ble_mic_set_streaming(false);
            (void)badge_ble_mic_set_enabled(false);
            badge_walkie_radio_enter();
            restart_idle_timer(BADGE_VOICE_IDLE_DIM_SECONDS);
            ESP_LOGI(TAG, "Walkie-talkie mode entered");
        }
        if (walkie_actions & BADGE_WALKIE_ACTION_PTT_DOWN) {
            (void)badge_walkie_radio_set_talking(true);
        }
        if (walkie_actions & BADGE_WALKIE_ACTION_PTT_UP) {
            (void)badge_walkie_radio_set_talking(false);
        }
        if (walkie_actions & BADGE_WALKIE_ACTION_EXIT) {
            badge_walkie_radio_exit();
            s_page = 0;
            restart_idle_timer(BADGE_IDLE_DIM_SECONDS);
            ESP_LOGI(TAG, "Walkie-talkie mode exited");
        }
        if (walkie_actions & BADGE_WALKIE_ACTION_REDRAW) render();
        if (walkie_was_active ||
            (walkie_actions & BADGE_WALKIE_ACTION_ENTER)) return;
    }

    badge_voice_key_t voice_key = (badge_voice_key_t)btn;
    badge_voice_event_t voice_event;
    switch (ev) {
    case BSP_BTN_PRESS:
        voice_event = BADGE_VOICE_EVENT_PRESS;
        break;
    case BSP_BTN_RELEASE:
        voice_event = BADGE_VOICE_EVENT_RELEASE;
        break;
    case BSP_BTN_DOUBLE:
        voice_event = BADGE_VOICE_EVENT_DOUBLE;
        break;
    case BSP_BTN_LONG:
        voice_event = BADGE_VOICE_EVENT_LONG;
        break;
    case BSP_BTN_CLICK:
    default:
        voice_event = BADGE_VOICE_EVENT_CLICK;
        break;
    }

    bool voice_was_active = badge_voice_logic_is_active(&s_voice_logic);
    badge_voice_action_t actions = badge_voice_logic_handle(&s_voice_logic,
                                                             voice_key,
                                                             voice_event);
    if (actions & BADGE_VOICE_ACTION_ENTER) {
        s_show_wechat_qr = false;
        s_voice_sent = false;
        badge_walkie_radio_suspend();
        (void)badge_ble_mic_set_enabled(true);
        restart_idle_timer(BADGE_VOICE_IDLE_DIM_SECONDS);
        ESP_LOGI(TAG, "Voice input mode entered");
    }
    if (actions & BADGE_VOICE_ACTION_PTT_DOWN) {
        s_voice_sent = false;
        bool event_ok = badge_ble_mic_send_input_event(BADGE_BLE_INPUT_PTT_DOWN);
        bool audio_ok = badge_ble_mic_set_streaming(true);
        ESP_LOGI(TAG, "Badge PTT down: event=%d audio=%d", event_ok, audio_ok);
    }
    if (actions & BADGE_VOICE_ACTION_PTT_UP) {
        (void)badge_ble_mic_set_streaming(false);
        bool event_ok = badge_ble_mic_send_input_event(BADGE_BLE_INPUT_PTT_UP);
        ESP_LOGI(TAG, "Badge PTT up: event=%d", event_ok);
    }
    if (actions & BADGE_VOICE_ACTION_SEND) {
        s_voice_sent = badge_ble_mic_send_input_event(BADGE_BLE_INPUT_SEND);
        ESP_LOGI(TAG, "Badge send: event=%d", s_voice_sent);
    }
    if (actions & BADGE_VOICE_ACTION_EXIT) {
        (void)badge_ble_mic_set_streaming(false);
        (void)badge_ble_mic_set_enabled(false);
        badge_walkie_radio_resume();
        s_page = 0;
        s_voice_sent = false;
        restart_idle_timer(BADGE_IDLE_DIM_SECONDS);
        ESP_LOGI(TAG, "Voice input mode exited");
    }
    if (actions & BADGE_VOICE_ACTION_REDRAW) render();
    if (voice_was_active || (actions & BADGE_VOICE_ACTION_ENTER)) return;

    if (btn == BSP_BTN_OK && ev == BSP_BTN_LONG) {
        s_show_wechat_qr = true;
        render();
        return;
    }

    int next_page = s_page;
    if (ev == BSP_BTN_CLICK && btn == BSP_BTN_UP) {
        next_page = (s_page + PAGE_COUNT - 1) % PAGE_COUNT;
    } else if (ev == BSP_BTN_CLICK &&
               (btn == BSP_BTN_DOWN || btn == BSP_BTN_OK)) {
        next_page = (s_page + 1) % PAGE_COUNT;
    } else {
        return;
    }
    if (next_page != s_page) {
        s_page = next_page;
        render();
    }
}
