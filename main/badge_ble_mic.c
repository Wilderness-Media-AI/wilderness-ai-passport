#include "badge_ble_mic.h"

#include "badge_adpcm.h"
#include "bsp_audio.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "host/ble_att.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_hs_mbuf.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs_flash.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define MIC_DEVICE_NAME "Wilderness Mic"
#define MIC_CTRL_UUID   0xA2B1
#define MIC_INPUT_UUID  0xA2B2
#define MIC_AUDIO_UUID  0xA2B3
#define MIC_PCM_BYTES   3200
#define MIC_CHUNK_HEADER_BYTES 2
#define MIC_CHUNK_LAST  0x80

static const char *TAG = "badge_ble_mic";

static const ble_uuid128_t s_service_uuid = BLE_UUID128_INIT(
    0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0xB0, 0xA2, 0x00, 0x00);

static uint16_t s_ctrl_handle;
static uint16_t s_input_handle;
static uint16_t s_audio_handle;
static volatile uint16_t s_connection = BLE_HS_CONN_HANDLE_NONE;
static volatile uint16_t s_input_connection = BLE_HS_CONN_HANDLE_NONE;
static volatile uint16_t s_audio_connection = BLE_HS_CONN_HANDLE_NONE;
static volatile bool s_stream_requested;
static uint8_t s_address_type;
static uint8_t s_block_sequence;
static SemaphoreHandle_t s_stream_signal;
static StaticSemaphore_t s_stream_signal_storage;
static QueueHandle_t s_input_queue;
static StaticQueue_t s_input_queue_storage;
static uint8_t s_input_queue_bytes[8];
static struct ble_gap_event_listener s_gap_listener;

static int16_t s_pcm[MIC_PCM_BYTES / sizeof(int16_t)] __attribute__((aligned(4)));
static uint8_t s_adpcm[BADGE_ADPCM_BYTES];
static uint8_t s_notify_buffer[256];
static StackType_t s_audio_stack[4096 / sizeof(StackType_t)];
static StaticTask_t s_audio_task_storage;
static StackType_t s_input_stack[2048 / sizeof(StackType_t)];
static StaticTask_t s_input_task_storage;

static void start_advertising(void);

bool badge_ble_mic_is_ready(void)
{
    return s_audio_connection != BLE_HS_CONN_HANDLE_NONE &&
           s_input_connection != BLE_HS_CONN_HANDLE_NONE;
}

bool badge_ble_mic_set_streaming(bool start)
{
    if (start && s_audio_connection == BLE_HS_CONN_HANDLE_NONE) return false;
    s_stream_requested = start;
    if (start && s_stream_signal) xSemaphoreGive(s_stream_signal);
    return !start || s_audio_connection != BLE_HS_CONN_HANDLE_NONE;
}

bool badge_ble_mic_send_input_event(badge_ble_input_event_t event)
{
    if (!s_input_queue || s_input_connection == BLE_HS_CONN_HANDLE_NONE) return false;
    uint8_t value = (uint8_t)event;
    return xQueueSend(s_input_queue, &value, 0) == pdTRUE;
}

static int control_access(uint16_t connection, uint16_t attribute,
                          struct ble_gatt_access_ctxt *context, void *argument)
{
    (void)connection;
    (void)attribute;
    (void)argument;
    if (context->op != BLE_GATT_ACCESS_OP_WRITE_CHR ||
        OS_MBUF_PKTLEN(context->om) != 1) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    uint8_t command = 0xff;
    if (os_mbuf_copydata(context->om, 0, 1, &command) != 0 || command > 1) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    (void)badge_ble_mic_set_streaming(command == 1);
    ESP_LOGI(TAG, "Mac microphone request: %s", command == 1 ? "start" : "stop");
    return 0;
}

static int notify_access(uint16_t connection, uint16_t attribute,
                         struct ble_gatt_access_ctxt *context, void *argument)
{
    (void)connection;
    (void)attribute;
    (void)context;
    (void)argument;
    return BLE_ATT_ERR_UNLIKELY;
}

static const struct ble_gatt_svc_def s_gatt_services[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &s_service_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = BLE_UUID16_DECLARE(MIC_CTRL_UUID),
                .access_cb = control_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
                .val_handle = &s_ctrl_handle,
            },
            {
                .uuid = BLE_UUID16_DECLARE(MIC_INPUT_UUID),
                .access_cb = notify_access,
                .flags = BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_input_handle,
            },
            {
                .uuid = BLE_UUID16_DECLARE(MIC_AUDIO_UUID),
                .access_cb = notify_access,
                .flags = BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_audio_handle,
            },
            { 0 },
        },
    },
    { 0 },
};

static void input_task(void *argument)
{
    (void)argument;
    uint8_t event;
    for (;;) {
        if (xQueueReceive(s_input_queue, &event, portMAX_DELAY) != pdTRUE) continue;
        uint16_t connection = s_input_connection;
        if (connection == BLE_HS_CONN_HANDLE_NONE) continue;
        struct os_mbuf *packet = ble_hs_mbuf_from_flat(&event, sizeof(event));
        int rc = packet ? ble_gatts_notify_custom(connection, s_input_handle, packet)
                        : BLE_HS_ENOMEM;
        if (rc != 0) {
            ESP_LOGW(TAG, "Input event %u notification failed: %d", event, rc);
        }
    }
}

static bool notify_fragment(const void *data, size_t length)
{
    TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(100);
    while (s_audio_connection != BLE_HS_CONN_HANDLE_NONE) {
        struct os_mbuf *packet = ble_hs_mbuf_from_flat(data, length);
        if (packet) {
            int rc = ble_gatts_notify_custom(s_audio_connection, s_audio_handle, packet);
            if (rc == 0) {
                /* Pace consecutive fragments so the controller TX queue does
                 * not receive a four-notification burst every 100 ms. */
                vTaskDelay(pdMS_TO_TICKS(2));
                return true;
            }
            if (rc != BLE_HS_ENOMEM && rc != BLE_HS_EBUSY && rc != BLE_HS_EAGAIN) {
                return false;
            }
        }
        if ((int32_t)(xTaskGetTickCount() - deadline) >= 0) return false;
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    return false;
}

static bool notify_adpcm_block(const uint8_t *block, size_t length)
{
    uint16_t mtu = ble_att_mtu(s_audio_connection);
    if (mtu < BLE_ATT_MTU_DFLT) mtu = BLE_ATT_MTU_DFLT;
    size_t body_capacity = (size_t)mtu - 3 - MIC_CHUNK_HEADER_BYTES;
    if (body_capacity > sizeof(s_notify_buffer) - MIC_CHUNK_HEADER_BYTES) {
        body_capacity = sizeof(s_notify_buffer) - MIC_CHUNK_HEADER_BYTES;
    }
    uint8_t sequence = s_block_sequence++;
    size_t offset = 0;
    uint8_t fragment = 0;
    while (offset < length) {
        size_t bytes = length - offset;
        if (bytes > body_capacity) bytes = body_capacity;
        bool last = offset + bytes == length;
        s_notify_buffer[0] = sequence;
        s_notify_buffer[1] = fragment | (last ? MIC_CHUNK_LAST : 0);
        memcpy(s_notify_buffer + MIC_CHUNK_HEADER_BYTES, block + offset, bytes);
        if (!notify_fragment(s_notify_buffer, bytes + MIC_CHUNK_HEADER_BYTES)) return false;
        offset += bytes;
        ++fragment;
    }
    return true;
}

static void audio_task(void *argument)
{
    (void)argument;
    badge_adpcm_state_t encoder;
    for (;;) {
        xSemaphoreTake(s_stream_signal, portMAX_DELAY);
        if (!s_stream_requested) continue;
        if (bsp_audio_init() != ESP_OK || bsp_audio_set_format(16000, 16, 1) != ESP_OK) {
            ESP_LOGE(TAG, "Microphone initialization failed");
            s_stream_requested = false;
            continue;
        }
        badge_adpcm_reset(&encoder);
        ESP_LOGI(TAG, "Microphone streaming started: 16 kHz mono IMA-ADPCM");
        while (s_stream_requested && s_audio_connection != BLE_HS_CONN_HANDLE_NONE) {
            if (bsp_audio_read(s_pcm, sizeof(s_pcm)) != ESP_OK) {
                ESP_LOGE(TAG, "Microphone read failed");
                break;
            }
            size_t encoded = badge_adpcm_encode(&encoder, s_pcm,
                                                 BADGE_ADPCM_SAMPLES,
                                                 s_adpcm, sizeof(s_adpcm));
            if (encoded != BADGE_ADPCM_BYTES || !notify_adpcm_block(s_adpcm, encoded)) {
                ESP_LOGW(TAG, "Audio block dropped");
            }
        }
        s_stream_requested = false;
        ESP_LOGI(TAG, "Microphone streaming stopped");
    }
}

static int gap_event(struct ble_gap_event *event, void *argument)
{
    (void)argument;
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            s_connection = event->connect.conn_handle;
            ESP_LOGI(TAG, "Mac connected (handle %u)", s_connection);
            (void)ble_gap_set_prefered_le_phy(s_connection,
                                              BLE_GAP_LE_PHY_2M_MASK,
                                              BLE_GAP_LE_PHY_2M_MASK, 0);
            (void)ble_gattc_exchange_mtu(s_connection, NULL, NULL);
        } else {
            start_advertising();
        }
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        s_stream_requested = false;
        s_connection = BLE_HS_CONN_HANDLE_NONE;
        s_input_connection = BLE_HS_CONN_HANDLE_NONE;
        s_audio_connection = BLE_HS_CONN_HANDLE_NONE;
        ESP_LOGI(TAG, "Mac disconnected; advertising restarted");
        start_advertising();
        break;
    case BLE_GAP_EVENT_SUBSCRIBE:
        if (event->subscribe.attr_handle == s_input_handle) {
            s_input_connection = event->subscribe.cur_notify
                ? event->subscribe.conn_handle : BLE_HS_CONN_HANDLE_NONE;
            ESP_LOGI(TAG, "Input notifications %s",
                     event->subscribe.cur_notify ? "enabled" : "disabled");
        } else if (event->subscribe.attr_handle == s_audio_handle) {
            s_audio_connection = event->subscribe.cur_notify
                ? event->subscribe.conn_handle : BLE_HS_CONN_HANDLE_NONE;
            ESP_LOGI(TAG, "Audio notifications %s",
                     event->subscribe.cur_notify ? "enabled" : "disabled");
        }
        break;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        if (s_connection == BLE_HS_CONN_HANDLE_NONE) start_advertising();
        break;
    default:
        break;
    }
    return 0;
}

static void start_advertising(void)
{
    struct ble_hs_adv_fields fields = { 0 };
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)MIC_DEVICE_NAME;
    fields.name_len = strlen(MIC_DEVICE_NAME);
    fields.name_is_complete = 1;
    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Advertising data failed: %d", rc);
        return;
    }

    struct ble_hs_adv_fields response = { 0 };
    response.uuids128 = (ble_uuid128_t *)&s_service_uuid;
    response.num_uuids128 = 1;
    response.uuids128_is_complete = 1;
    rc = ble_gap_adv_rsp_set_fields(&response);
    if (rc != 0) {
        ESP_LOGE(TAG, "Scan response failed: %d", rc);
        return;
    }

    struct ble_gap_adv_params parameters = { 0 };
    parameters.conn_mode = BLE_GAP_CONN_MODE_UND;
    parameters.disc_mode = BLE_GAP_DISC_MODE_GEN;
    parameters.itvl_min = BLE_GAP_ADV_ITVL_MS(100);
    parameters.itvl_max = BLE_GAP_ADV_ITVL_MS(150);
    rc = ble_gap_adv_start(s_address_type, NULL, BLE_HS_FOREVER,
                           &parameters, NULL, NULL);
    ESP_LOGI(TAG, "Advertising %s (rc=%d)", MIC_DEVICE_NAME, rc);
}

static void host_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    if (rc == 0) rc = ble_hs_id_infer_auto(0, &s_address_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "BLE address setup failed: %d", rc);
        return;
    }
    start_advertising();
}

static void host_task(void *argument)
{
    (void)argument;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t badge_ble_mic_init(void)
{
    // NimBLE logs every notification at INFO. Audio streaming emits dozens of
    // notifications per second, so those logs can saturate the 115200-baud
    // console and delay control writes behind queued audio traffic.
    esp_log_level_set("NimBLE", ESP_LOG_WARN);

    esp_err_t error = nvs_flash_init();
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "NVS initialization failed: %s", esp_err_to_name(error));
        return error;
    }
    error = nimble_port_init();
    if (error != ESP_OK) return error;

    s_stream_signal = xSemaphoreCreateBinaryStatic(&s_stream_signal_storage);
    if (!s_stream_signal) return ESP_ERR_NO_MEM;
    s_input_queue = xQueueCreateStatic(sizeof(s_input_queue_bytes), sizeof(uint8_t),
                                      s_input_queue_bytes, &s_input_queue_storage);
    if (!s_input_queue) return ESP_ERR_NO_MEM;
    if (!xTaskCreateStatic(audio_task, "badge_mic", 4096, NULL, 6,
                           s_audio_stack, &s_audio_task_storage)) {
        return ESP_ERR_NO_MEM;
    }
    if (!xTaskCreateStatic(input_task, "badge_input", 2048, NULL, 6,
                           s_input_stack, &s_input_task_storage)) {
        return ESP_ERR_NO_MEM;
    }

    ble_hs_cfg.sync_cb = host_sync;
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_gap_device_name_set(MIC_DEVICE_NAME);
    int rc = ble_gatts_count_cfg(s_gatt_services);
    if (rc != 0) ESP_LOGE(TAG, "GATT count failed: %d", rc);
    if (rc == 0) {
        rc = ble_gatts_add_svcs(s_gatt_services);
        if (rc != 0) ESP_LOGE(TAG, "GATT registration failed: %d", rc);
    }
    if (rc == 0) {
        rc = ble_gap_event_listener_register(&s_gap_listener, gap_event, NULL);
        if (rc != 0) ESP_LOGE(TAG, "GAP listener failed: %d", rc);
    }
    if (rc != 0) {
        ESP_LOGE(TAG, "GATT setup failed: %d", rc);
        return ESP_FAIL;
    }

    nimble_port_freertos_init(host_task);
    ESP_LOGI(TAG, "BLE microphone ready; badge input and Mac CTRL are available");
    return ESP_OK;
}
