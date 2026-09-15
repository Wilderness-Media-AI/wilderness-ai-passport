#include "badge_walkie_radio.h"

#include "badge_adpcm.h"
#include "badge_audio_owner.h"
#include "bsp_audio.h"

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <stddef.h>
#include <stdatomic.h>
#include <string.h>

#define WALKIE_MAGIC 0x574D5031UL
#define WALKIE_ROOM_ID 0x574D0001UL
#define WALKIE_VERSION 2
#define WALKIE_CHANNEL 6
#define WALKIE_SAMPLES 400
#define WALKIE_ADPCM_BYTES (4 + WALKIE_SAMPLES / 2)
#define WALKIE_PEER_TIMEOUT_MS 3000
#define WALKIE_TALK_TIMEOUT_MS 700
#define WALKIE_CALL_BURST_MS 1500
#define WALKIE_CALL_REPEAT_MS 20
#define WALKIE_SESSION_IDLE_MS 30000
#define WALKIE_STANDBY_INTERVAL_MS 500
#define WALKIE_STANDBY_WINDOW_MS 40

typedef enum {
    WALKIE_PACKET_HELLO = 1,
    WALKIE_PACKET_TALK_START,
    WALKIE_PACKET_AUDIO,
    WALKIE_PACKET_TALK_STOP,
    WALKIE_PACKET_CALL,
    WALKIE_PACKET_CALL_ACK,
    WALKIE_PACKET_HANGUP,
} walkie_packet_type_t;

typedef struct {
    uint32_t magic;
    uint32_t room;
    uint16_t sequence;
    uint8_t version;
    uint8_t type;
    uint8_t flags;
    uint8_t payload_bytes;
    char sender[16];
    uint8_t payload[WALKIE_ADPCM_BYTES];
} __attribute__((packed)) walkie_packet_t;

typedef struct {
    uint8_t source[ESP_NOW_ETH_ALEN];
    uint16_t length;
    uint8_t data[ESP_NOW_MAX_DATA_LEN];
} walkie_rx_item_t;

typedef enum {
    WALKIE_COMMAND_RESUME = 1,
    WALKIE_COMMAND_SUSPEND,
    WALKIE_COMMAND_ENTER,
    WALKIE_COMMAND_EXIT,
    WALKIE_COMMAND_TALK_START,
    WALKIE_COMMAND_TALK_STOP,
} walkie_command_t;

_Static_assert(offsetof(walkie_packet_t, payload) + WALKIE_ADPCM_BYTES <=
                   ESP_NOW_MAX_DATA_LEN,
               "walkie audio packet exceeds ESP-NOW v1 payload");

static const char *TAG = "badge_walkie";
static const uint8_t s_broadcast[ESP_NOW_ETH_ALEN] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

static QueueHandle_t s_command_queue;
static StaticQueue_t s_command_queue_storage;
static uint8_t s_command_queue_bytes[8 * sizeof(walkie_command_t)];
static QueueHandle_t s_rx_queue;
static StaticQueue_t s_rx_queue_storage;
static uint8_t s_rx_queue_bytes[12 * sizeof(walkie_rx_item_t)];
static StackType_t s_worker_stack[6144 / sizeof(StackType_t)];
static StaticTask_t s_worker_storage;

static portMUX_TYPE s_status_lock = portMUX_INITIALIZER_UNLOCKED;
static badge_walkie_radio_status_t s_status;
static badge_walkie_status_callback_t s_status_callback;
static void *s_status_callback_context;
static char s_device_name[16] = "WILDERNESS";
static uint8_t s_local_mac[ESP_NOW_ETH_ALEN];
static uint16_t s_sequence;
static bool s_transport_active;
static bool s_wifi_initialized;
static bool s_now_initialized;
static bool s_force_awake;
static bool s_session_active;
static TickType_t s_last_peer_tick;
static TickType_t s_last_talk_tick;
static TickType_t s_last_activity_tick;
static TickType_t s_call_end_tick;
static atomic_uint_fast32_t s_callback_drops;

static int16_t s_pcm[WALKIE_SAMPLES] __attribute__((aligned(4)));
static uint8_t s_encoded[WALKIE_ADPCM_BYTES];

static void notify_status(void)
{
    if (s_status_callback) s_status_callback(s_status_callback_context);
}

static void set_state(badge_walkie_radio_state_t state)
{
    taskENTER_CRITICAL(&s_status_lock);
    bool changed = s_status.state != state;
    s_status.state = state;
    taskEXIT_CRITICAL(&s_status_lock);
    if (changed) notify_status();
}

static void set_local_talking(bool talking)
{
    taskENTER_CRITICAL(&s_status_lock);
    bool changed = s_status.local_talking != talking;
    s_status.local_talking = talking;
    taskEXIT_CRITICAL(&s_status_lock);
    if (changed) notify_status();
}

static void set_remote(const char *name, bool talking)
{
    taskENTER_CRITICAL(&s_status_lock);
    bool changed = s_status.remote_talking != talking ||
                   strncmp(s_status.peer_name, name,
                           sizeof(s_status.peer_name)) != 0;
    s_status.remote_talking = talking;
    strncpy(s_status.peer_name, name, sizeof(s_status.peer_name) - 1);
    s_status.peer_name[sizeof(s_status.peer_name) - 1] = '\0';
    if (s_status.state == BADGE_WALKIE_RADIO_CALLING ||
        s_status.state == BADGE_WALKIE_RADIO_SEARCHING) {
        s_status.state = BADGE_WALKIE_RADIO_READY;
        changed = true;
    }
    taskEXIT_CRITICAL(&s_status_lock);
    if (changed) notify_status();
}

static void set_session_status(bool active, bool incoming,
                               badge_walkie_radio_state_t state,
                               const char *peer_name)
{
    taskENTER_CRITICAL(&s_status_lock);
    memset(&s_status, 0, sizeof(s_status));
    s_status.state = state;
    s_status.session_active = active;
    s_status.incoming_call = incoming;
    if (peer_name) {
        strncpy(s_status.peer_name, peer_name,
                sizeof(s_status.peer_name) - 1);
    }
    taskEXIT_CRITICAL(&s_status_lock);
    notify_status();
}

static bool send_packet(walkie_packet_type_t type, const uint8_t *payload,
                        size_t payload_bytes)
{
    if (!s_transport_active || payload_bytes > WALKIE_ADPCM_BYTES) return false;
    walkie_packet_t packet = {
        .magic = WALKIE_MAGIC,
        .room = WALKIE_ROOM_ID,
        .sequence = s_sequence++,
        .version = WALKIE_VERSION,
        .type = (uint8_t)type,
        .flags = s_status.local_talking ? 1U : 0U,
        .payload_bytes = (uint8_t)payload_bytes,
    };
    strncpy(packet.sender, s_device_name, sizeof(packet.sender) - 1);
    if (payload_bytes) memcpy(packet.payload, payload, payload_bytes);
    size_t bytes = offsetof(walkie_packet_t, payload) + payload_bytes;
    esp_err_t err = esp_now_send(s_broadcast, (const uint8_t *)&packet, bytes);
    if (err != ESP_OK) {
        taskENTER_CRITICAL(&s_status_lock);
        ++s_status.dropped_frames;
        taskEXIT_CRITICAL(&s_status_lock);
        return false;
    }
    return true;
}

static void receive_callback(const esp_now_recv_info_t *info,
                             const uint8_t *data, int data_length)
{
    if (!info || !data || data_length <= 0 ||
        data_length > ESP_NOW_MAX_DATA_LEN || !s_rx_queue) return;
    walkie_rx_item_t item = { .length = (uint16_t)data_length };
    memcpy(item.source, info->src_addr, ESP_NOW_ETH_ALEN);
    memcpy(item.data, data, (size_t)data_length);
    if (xQueueSend(s_rx_queue, &item, 0) != pdTRUE) {
        atomic_fetch_add(&s_callback_drops, 1);
    }
}

static bool packet_valid(const walkie_rx_item_t *item,
                         const walkie_packet_t **packet)
{
    if (item->length < offsetof(walkie_packet_t, payload)) return false;
    const walkie_packet_t *candidate = (const walkie_packet_t *)item->data;
    if (candidate->magic != WALKIE_MAGIC || candidate->room != WALKIE_ROOM_ID ||
        candidate->version != WALKIE_VERSION ||
        candidate->payload_bytes > WALKIE_ADPCM_BYTES ||
        item->length != offsetof(walkie_packet_t, payload) +
                            candidate->payload_bytes) {
        return false;
    }
    *packet = candidate;
    return true;
}

static esp_err_t set_full_power(bool enabled)
{
    if (!s_transport_active) return ESP_ERR_INVALID_STATE;
    if (enabled) {
        if (!s_force_awake) {
            ESP_RETURN_ON_ERROR(esp_wifi_force_wakeup_acquire(), TAG,
                                "Wi-Fi force wake failed");
            s_force_awake = true;
        }
        return esp_now_set_wake_window(UINT16_MAX);
    }

    esp_err_t err = esp_now_set_wake_window(WALKIE_STANDBY_WINDOW_MS);
    if (s_force_awake) {
        esp_wifi_force_wakeup_release();
        s_force_awake = false;
    }
    return err;
}

static esp_err_t audio_start(void)
{
    for (int attempts = 0; attempts < 30; ++attempts) {
        if (badge_audio_owner_claim(BADGE_AUDIO_OWNER_WALKIE)) break;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    if (badge_audio_owner_current() != BADGE_AUDIO_OWNER_WALKIE) {
        ESP_LOGE(TAG, "Audio is still owned by another mode");
        return ESP_ERR_TIMEOUT;
    }
    esp_err_t err = bsp_audio_init();
    if (err == ESP_OK) err = bsp_audio_set_format(16000, 16, 1);
    if (err != ESP_OK) {
        badge_audio_owner_release(BADGE_AUDIO_OWNER_WALKIE);
        return err;
    }
    bsp_audio_set_volume(70);
    return ESP_OK;
}

static void audio_stop(void)
{
    if (badge_audio_owner_current() != BADGE_AUDIO_OWNER_WALKIE) return;
    (void)bsp_audio_close();
    badge_audio_owner_release(BADGE_AUDIO_OWNER_WALKIE);
}

static esp_err_t transport_init(void)
{
    if (s_transport_active) return ESP_OK;
    set_state(BADGE_WALKIE_RADIO_STARTING);

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) goto fail;
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) goto fail;

    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&wifi_config);
    if (err != ESP_OK) goto fail;
    s_wifi_initialized = true;
    if ((err = esp_wifi_set_storage(WIFI_STORAGE_RAM)) != ESP_OK) goto fail;
    if ((err = esp_wifi_set_mode(WIFI_MODE_STA)) != ESP_OK) goto fail;
    if ((err = esp_wifi_start()) != ESP_OK) goto fail;
    if ((err = esp_wifi_set_ps(WIFI_PS_MIN_MODEM)) != ESP_OK) goto fail;
    if ((err = esp_wifi_set_channel(WALKIE_CHANNEL,
                                    WIFI_SECOND_CHAN_NONE)) != ESP_OK) goto fail;
    if ((err = esp_wifi_connectionless_module_set_wake_interval(
             WALKIE_STANDBY_INTERVAL_MS)) != ESP_OK) goto fail;
    if ((err = esp_read_mac(s_local_mac, ESP_MAC_WIFI_STA)) != ESP_OK) goto fail;

    if ((err = esp_now_init()) != ESP_OK) goto fail;
    s_now_initialized = true;
    if ((err = esp_now_register_recv_cb(receive_callback)) != ESP_OK) goto fail;
    esp_now_peer_info_t peer = {
        .channel = WALKIE_CHANNEL,
        .ifidx = WIFI_IF_STA,
        .encrypt = false,
    };
    memcpy(peer.peer_addr, s_broadcast, ESP_NOW_ETH_ALEN);
    if ((err = esp_now_add_peer(&peer)) != ESP_OK) goto fail;
    if ((err = esp_now_set_wake_window(WALKIE_STANDBY_WINDOW_MS)) != ESP_OK) {
        goto fail;
    }

    s_transport_active = true;
    set_session_status(false, false, BADGE_WALKIE_RADIO_STANDBY, NULL);
    ESP_LOGI(TAG,
             "ESP-NOW standby ready: channel=%d interval=%dms window=%dms heap=%lu",
             WALKIE_CHANNEL, WALKIE_STANDBY_INTERVAL_MS,
             WALKIE_STANDBY_WINDOW_MS,
             (unsigned long)esp_get_free_heap_size());
    return ESP_OK;

fail:
    ESP_LOGE(TAG, "ESP-NOW standby init failed: %s", esp_err_to_name(err));
    if (s_now_initialized) {
        esp_now_unregister_recv_cb();
        (void)esp_now_deinit();
        s_now_initialized = false;
    }
    if (s_wifi_initialized) {
        (void)esp_wifi_stop();
        (void)esp_wifi_deinit();
        s_wifi_initialized = false;
    }
    s_transport_active = false;
    set_session_status(false, false, BADGE_WALKIE_RADIO_ERROR, NULL);
    return err;
}

static void session_stop(bool notify_peer)
{
    if (s_session_active && notify_peer && s_transport_active) {
        set_local_talking(false);
        (void)send_packet(WALKIE_PACKET_TALK_STOP, NULL, 0);
        (void)send_packet(WALKIE_PACKET_HANGUP, NULL, 0);
        (void)send_packet(WALKIE_PACKET_HANGUP, NULL, 0);
    }
    s_session_active = false;
    audio_stop();
    if (s_transport_active) {
        (void)set_full_power(false);
        set_session_status(false, false, BADGE_WALKIE_RADIO_STANDBY, NULL);
        ESP_LOGI(TAG, "Radio session ended; low-power standby resumed");
    }
    s_last_peer_tick = 0;
    s_last_talk_tick = 0;
    s_last_activity_tick = 0;
    s_call_end_tick = 0;
    if (s_rx_queue) xQueueReset(s_rx_queue);
}

static void transport_deinit(void)
{
    if (s_session_active) session_stop(true);
    if (s_force_awake) {
        esp_wifi_force_wakeup_release();
        s_force_awake = false;
    }
    s_transport_active = false;
    if (s_now_initialized) {
        esp_now_unregister_recv_cb();
        (void)esp_now_deinit();
        s_now_initialized = false;
    }
    if (s_wifi_initialized) {
        (void)esp_wifi_stop();
        (void)esp_wifi_deinit();
        s_wifi_initialized = false;
    }
    set_session_status(false, false, BADGE_WALKIE_RADIO_OFF, NULL);
    if (s_rx_queue) xQueueReset(s_rx_queue);
    ESP_LOGI(TAG, "ESP-NOW standby suspended");
}

static esp_err_t session_start(bool incoming, const char *peer_name)
{
    esp_err_t err = transport_init();
    if (err != ESP_OK) return err;
    if (s_session_active) return ESP_OK;
    if ((err = set_full_power(true)) != ESP_OK) return err;
    if ((err = audio_start()) != ESP_OK) {
        (void)set_full_power(false);
        return err;
    }

    TickType_t now = xTaskGetTickCount();
    s_session_active = true;
    s_last_activity_tick = now;
    s_last_peer_tick = incoming ? now : 0;
    s_last_talk_tick = 0;
    s_call_end_tick = incoming ? 0 : now + pdMS_TO_TICKS(WALKIE_CALL_BURST_MS);
    set_session_status(true, incoming,
                       incoming ? BADGE_WALKIE_RADIO_READY :
                                  BADGE_WALKIE_RADIO_CALLING,
                       peer_name);
    ESP_LOGI(TAG, "%s radio session started",
             incoming ? "Incoming" : "Outgoing");
    return ESP_OK;
}

static void process_received(const walkie_rx_item_t *item)
{
    const walkie_packet_t *packet;
    if (!packet_valid(item, &packet) ||
        memcmp(item->source, s_local_mac, ESP_NOW_ETH_ALEN) == 0) return;

    char peer_name[sizeof(packet->sender) + 1];
    memcpy(peer_name, packet->sender, sizeof(packet->sender));
    peer_name[sizeof(packet->sender)] = '\0';
    TickType_t now = xTaskGetTickCount();

    if (packet->type == WALKIE_PACKET_CALL) {
        if (!s_session_active && session_start(true, peer_name) != ESP_OK) {
            return;
        }
        s_last_peer_tick = now;
        set_remote(peer_name, false);
        (void)send_packet(WALKIE_PACKET_CALL_ACK, NULL, 0);
        (void)send_packet(WALKIE_PACKET_CALL_ACK, NULL, 0);
        return;
    }
    if (!s_session_active) return;
    if (packet->type == WALKIE_PACKET_HANGUP) {
        ESP_LOGI(TAG, "%s ended the radio session", peer_name);
        session_stop(false);
        return;
    }

    s_last_peer_tick = now;
    if (packet->type == WALKIE_PACKET_CALL_ACK) {
        set_remote(peer_name, false);
        return;
    }

    bool remote_talking = packet->type == WALKIE_PACKET_TALK_START ||
                          packet->type == WALKIE_PACKET_AUDIO ||
                          (packet->type == WALKIE_PACKET_HELLO &&
                           (packet->flags & 1U));
    if (packet->type == WALKIE_PACKET_TALK_STOP) remote_talking = false;
    if (packet->type == WALKIE_PACKET_TALK_START ||
        packet->type == WALKIE_PACKET_TALK_STOP ||
        packet->type == WALKIE_PACKET_AUDIO) {
        s_last_activity_tick = now;
    }
    if (remote_talking) s_last_talk_tick = now;

    bool local_talking;
    taskENTER_CRITICAL(&s_status_lock);
    local_talking = s_status.local_talking;
    taskEXIT_CRITICAL(&s_status_lock);

    /* Simultaneous PTT has a deterministic winner: the lower station MAC. */
    if (local_talking && remote_talking) {
        if (memcmp(item->source, s_local_mac, ESP_NOW_ETH_ALEN) < 0) {
            set_local_talking(false);
            (void)send_packet(WALKIE_PACKET_TALK_STOP, NULL, 0);
            (void)send_packet(WALKIE_PACKET_TALK_STOP, NULL, 0);
            local_talking = false;
        } else {
            return;
        }
    }

    set_remote(peer_name, remote_talking);
    if (packet->type == WALKIE_PACKET_AUDIO && !local_talking &&
        packet->payload_bytes == WALKIE_ADPCM_BYTES &&
        badge_adpcm_decode(packet->payload, packet->payload_bytes,
                           WALKIE_SAMPLES, s_pcm, WALKIE_SAMPLES) ==
            WALKIE_SAMPLES) {
        if (bsp_audio_write(s_pcm, sizeof(s_pcm)) == ESP_OK) {
            taskENTER_CRITICAL(&s_status_lock);
            ++s_status.received_frames;
            taskEXIT_CRITICAL(&s_status_lock);
        } else {
            taskENTER_CRITICAL(&s_status_lock);
            ++s_status.dropped_frames;
            taskEXIT_CRITICAL(&s_status_lock);
        }
    }
}

static void worker_task(void *context)
{
    (void)context;
    badge_adpcm_state_t encoder;
    TickType_t last_hello = 0;
    TickType_t last_call = 0;
    for (;;) {
        walkie_command_t command;
        TickType_t wait = s_transport_active ? pdMS_TO_TICKS(20) : portMAX_DELAY;
        if (xQueueReceive(s_command_queue, &command, wait) == pdTRUE) {
            if (command == WALKIE_COMMAND_RESUME && !s_transport_active) {
                (void)transport_init();
            } else if (command == WALKIE_COMMAND_SUSPEND) {
                transport_deinit();
            } else if (command == WALKIE_COMMAND_ENTER) {
                if (session_start(false, NULL) != ESP_OK) {
                    session_stop(false);
                    set_state(BADGE_WALKIE_RADIO_ERROR);
                }
                last_call = 0;
                last_hello = 0;
            } else if (command == WALKIE_COMMAND_EXIT) {
                session_stop(true);
            } else if (command == WALKIE_COMMAND_TALK_START &&
                       s_session_active) {
                badge_walkie_radio_status_t snapshot;
                badge_walkie_radio_get_status(&snapshot);
                if (snapshot.state == BADGE_WALKIE_RADIO_READY &&
                    !snapshot.remote_talking) {
                    badge_adpcm_reset(&encoder);
                    set_local_talking(true);
                    s_last_activity_tick = xTaskGetTickCount();
                    (void)send_packet(WALKIE_PACKET_TALK_START, NULL, 0);
                }
            } else if (command == WALKIE_COMMAND_TALK_STOP &&
                       s_session_active) {
                set_local_talking(false);
                s_last_activity_tick = xTaskGetTickCount();
                (void)send_packet(WALKIE_PACKET_TALK_STOP, NULL, 0);
            }
        }
        if (!s_transport_active) continue;

        walkie_rx_item_t received;
        while (xQueueReceive(s_rx_queue, &received, 0) == pdTRUE) {
            process_received(&received);
        }
        if (!s_session_active) continue;

        badge_walkie_radio_status_t snapshot;
        badge_walkie_radio_get_status(&snapshot);
        if (snapshot.local_talking) {
            if (bsp_audio_read(s_pcm, sizeof(s_pcm)) == ESP_OK &&
                badge_adpcm_encode(&encoder, s_pcm, WALKIE_SAMPLES,
                                   s_encoded, sizeof(s_encoded)) ==
                    WALKIE_ADPCM_BYTES) {
                (void)send_packet(WALKIE_PACKET_AUDIO, s_encoded,
                                  sizeof(s_encoded));
            } else {
                taskENTER_CRITICAL(&s_status_lock);
                ++s_status.dropped_frames;
                taskEXIT_CRITICAL(&s_status_lock);
            }
        }

        TickType_t now = xTaskGetTickCount();
        if (snapshot.state == BADGE_WALKIE_RADIO_CALLING &&
            now < s_call_end_tick &&
            (last_call == 0 ||
             now - last_call >= pdMS_TO_TICKS(WALKIE_CALL_REPEAT_MS))) {
            (void)send_packet(WALKIE_PACKET_CALL, NULL, 0);
            last_call = now;
        } else if (snapshot.state == BADGE_WALKIE_RADIO_CALLING &&
                   now >= s_call_end_tick) {
            set_state(BADGE_WALKIE_RADIO_SEARCHING);
        }
        if (last_hello == 0 || now - last_hello >= pdMS_TO_TICKS(1000)) {
            (void)send_packet(WALKIE_PACKET_HELLO, NULL, 0);
            last_hello = now;
        }

        uint32_t callback_drops =
            (uint32_t)atomic_exchange(&s_callback_drops, 0);
        if (callback_drops) {
            taskENTER_CRITICAL(&s_status_lock);
            s_status.dropped_frames += callback_drops;
            taskEXIT_CRITICAL(&s_status_lock);
        }
        if (s_last_talk_tick &&
            now - s_last_talk_tick >= pdMS_TO_TICKS(WALKIE_TALK_TIMEOUT_MS) &&
            snapshot.remote_talking) {
            set_remote(snapshot.peer_name, false);
        }
        if (s_last_peer_tick &&
            now - s_last_peer_tick >= pdMS_TO_TICKS(WALKIE_PEER_TIMEOUT_MS)) {
            taskENTER_CRITICAL(&s_status_lock);
            bool changed = s_status.state == BADGE_WALKIE_RADIO_READY;
            s_status.state = BADGE_WALKIE_RADIO_SEARCHING;
            s_status.remote_talking = false;
            s_status.peer_name[0] = '\0';
            taskEXIT_CRITICAL(&s_status_lock);
            s_last_peer_tick = 0;
            if (changed) notify_status();
        }
        if (s_last_activity_tick &&
            now - s_last_activity_tick >=
                pdMS_TO_TICKS(WALKIE_SESSION_IDLE_MS)) {
            ESP_LOGI(TAG, "Radio session idle timeout");
            session_stop(true);
        }
    }
}

esp_err_t badge_walkie_radio_init(const char *device_name,
                                  badge_walkie_status_callback_t callback,
                                  void *callback_context)
{
    if (s_command_queue) return ESP_OK;
    if (device_name && device_name[0]) {
        strncpy(s_device_name, device_name, sizeof(s_device_name) - 1);
        s_device_name[sizeof(s_device_name) - 1] = '\0';
    }
    s_status_callback = callback;
    s_status_callback_context = callback_context;
    s_command_queue = xQueueCreateStatic(8, sizeof(walkie_command_t),
                                         s_command_queue_bytes,
                                         &s_command_queue_storage);
    s_rx_queue = xQueueCreateStatic(12, sizeof(walkie_rx_item_t),
                                    s_rx_queue_bytes, &s_rx_queue_storage);
    if (!s_command_queue || !s_rx_queue) return ESP_ERR_NO_MEM;
    if (!xTaskCreateStatic(worker_task, "walkie",
                           sizeof(s_worker_stack) / sizeof(s_worker_stack[0]),
                           NULL, 5, s_worker_stack, &s_worker_storage)) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static void queue_command(walkie_command_t command)
{
    if (s_command_queue) (void)xQueueSend(s_command_queue, &command, 0);
}

void badge_walkie_radio_enter(void)
{
    queue_command(WALKIE_COMMAND_ENTER);
}

void badge_walkie_radio_exit(void)
{
    queue_command(WALKIE_COMMAND_EXIT);
}

void badge_walkie_radio_resume(void)
{
    queue_command(WALKIE_COMMAND_RESUME);
}

void badge_walkie_radio_suspend(void)
{
    queue_command(WALKIE_COMMAND_SUSPEND);
}

bool badge_walkie_radio_set_talking(bool talking)
{
    badge_walkie_radio_status_t status;
    badge_walkie_radio_get_status(&status);
    if (talking && (!status.session_active ||
                    status.state != BADGE_WALKIE_RADIO_READY ||
                    status.remote_talking)) {
        return false;
    }
    queue_command(talking ? WALKIE_COMMAND_TALK_START :
                            WALKIE_COMMAND_TALK_STOP);
    return true;
}

void badge_walkie_radio_get_status(badge_walkie_radio_status_t *status)
{
    if (!status) return;
    taskENTER_CRITICAL(&s_status_lock);
    *status = s_status;
    taskEXIT_CRITICAL(&s_status_lock);
}
