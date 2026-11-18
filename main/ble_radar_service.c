#include "ble_radar_service.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "esp_bt.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "ble_radio_owner.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "host/ble_hs_adv.h"
#include "host/util/util.h"
#include "nimble/ble.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

typedef enum {
    COMMAND_START,
    COMMAND_STOP,
} service_command_t;

static const char *TAG = "ble_radar";
static SemaphoreHandle_t s_lock;
static QueueHandle_t s_commands;
static ble_radar_snapshot_t s_snapshot;
static bool s_start_requested;
static bool s_port_initialized;
static int64_t s_scan_deadline_ms;
static uint8_t s_own_address_type;

static int64_t now_ms(void)
{
    return esp_timer_get_time() / 1000;
}

static void set_result_locked(ble_radar_state_t state, int error,
                              const char *result)
{
    s_snapshot.state = state;
    s_snapshot.last_error = error;
    snprintf(s_snapshot.last_result, sizeof(s_snapshot.last_result), "%s", result);
}

static void set_result(ble_radar_state_t state, int error, const char *result)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    set_result_locked(state, error, result);
    xSemaphoreGive(s_lock);
}

static int find_device_locked(const uint8_t address[6], uint8_t type)
{
    for (uint8_t i = 0; i < s_snapshot.device_count; ++i) {
        if (s_snapshot.devices[i].address_type == type &&
            memcmp(s_snapshot.devices[i].address, address, 6) == 0) {
            return i;
        }
    }
    return -1;
}

static int allocate_device_locked(int64_t timestamp_ms)
{
    if (s_snapshot.device_count < BLE_RADAR_MAX_DEVICES) {
        return s_snapshot.device_count++;
    }

    int oldest = -1;
    int64_t oldest_seen = timestamp_ms;
    for (uint8_t i = 0; i < s_snapshot.device_count; ++i) {
        if (timestamp_ms - s_snapshot.devices[i].last_seen_ms > 30000 &&
            s_snapshot.devices[i].last_seen_ms < oldest_seen) {
            oldest = i;
            oldest_seen = s_snapshot.devices[i].last_seen_ms;
        }
    }
    if (oldest < 0) {
        s_snapshot.dropped_count++;
    }
    return oldest;
}

static void update_device(const struct ble_gap_disc_desc *disc)
{
    struct ble_hs_adv_fields fields = {0};
    const int parsed = ble_hs_adv_parse_fields(&fields, disc->data, disc->length_data);
    const int64_t timestamp_ms = now_ms();

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_snapshot.advertisement_count++;
    if (parsed != 0) {
        s_snapshot.parse_error_count++;
    }

    int index = find_device_locked(disc->addr.val, disc->addr.type);
    if (index < 0) {
        index = allocate_device_locked(timestamp_ms);
        if (index < 0) {
            xSemaphoreGive(s_lock);
            return;
        }
        memset(&s_snapshot.devices[index], 0, sizeof(s_snapshot.devices[index]));
        memcpy(s_snapshot.devices[index].address, disc->addr.val, 6);
        s_snapshot.devices[index].address_type = disc->addr.type;
        s_snapshot.devices[index].first_seen_ms = timestamp_ms;
        snprintf(s_snapshot.devices[index].address_text,
                 sizeof(s_snapshot.devices[index].address_text),
                 "%02X:%02X:%02X:%02X:%02X:%02X",
                 disc->addr.val[5], disc->addr.val[4], disc->addr.val[3],
                 disc->addr.val[2], disc->addr.val[1], disc->addr.val[0]);
    }

    ble_radar_device_t *device = &s_snapshot.devices[index];
    device->event_type = disc->event_type;
    device->last_seen_ms = timestamp_ms;
    device->stale = false;
    device->seen_count++;
    device->rssi = disc->rssi;
    if (device->seen_count == 1) {
        device->rssi_average = disc->rssi;
    } else {
        device->rssi_average = (int16_t)((device->rssi_average * 3 + disc->rssi) / 4);
    }

    if (parsed == 0) {
        if (fields.name != NULL && fields.name_len > 0) {
            const size_t length = fields.name_len > BLE_RADAR_NAME_MAX ?
                                  BLE_RADAR_NAME_MAX : fields.name_len;
            for (size_t i = 0; i < length; ++i) {
                const uint8_t character = fields.name[i];
                device->name[i] = character >= 0x20 && character <= 0x7e ?
                                  (char)character : '?';
            }
            device->name[length] = '\0';
        }
        device->tx_power_present = fields.tx_pwr_lvl_is_present;
        device->tx_power = fields.tx_pwr_lvl;
        if (fields.mfg_data != NULL && fields.mfg_data_len >= 2) {
            device->manufacturer_present = true;
            device->manufacturer_id = (uint16_t)fields.mfg_data[0] |
                                      ((uint16_t)fields.mfg_data[1] << 8);
            device->manufacturer_data_length = fields.mfg_data_len;
        }
        device->service_uuid16_count = fields.num_uuids16 > BLE_RADAR_UUID16_MAX ?
                                       BLE_RADAR_UUID16_MAX : fields.num_uuids16;
        for (uint8_t i = 0; i < device->service_uuid16_count; ++i) {
            device->service_uuid16[i] = fields.uuids16[i].value;
        }
    }
    xSemaphoreGive(s_lock);
}

static int gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    if (event->type == BLE_GAP_EVENT_DISC) {
        update_device(&event->disc);
    } else if (event->type == BLE_GAP_EVENT_DISC_COMPLETE) {
        xSemaphoreTake(s_lock, portMAX_DELAY);
        s_snapshot.scanning = false;
        s_scan_deadline_ms = 0;
        set_result_locked(BLE_RADAR_READY, event->disc_complete.reason,
                          event->disc_complete.reason == 0 ? "SCAN COMPLETE" : "SCAN STOPPED");
        const uint8_t device_count = s_snapshot.device_count;
        const uint32_t advertisement_count = s_snapshot.advertisement_count;
        xSemaphoreGive(s_lock);
        ESP_LOGI(TAG, "scan ended: reason=%d devices=%u advertisements=%" PRIu32,
                 event->disc_complete.reason, device_count, advertisement_count);
        service_command_t command = COMMAND_STOP;
        xQueueSend(s_commands, &command, 0);
    }
    return 0;
}

static void begin_scan(void)
{
    struct ble_gap_disc_params parameters = {0};
    parameters.passive = 0;
    parameters.filter_duplicates = 0;
    parameters.itvl = 160;   /* 100 ms */
    parameters.window = 48;  /* 30 ms */

    const int rc = ble_hs_id_infer_auto(0, &s_own_address_type);
    if (rc != 0) {
        set_result(BLE_RADAR_ERROR, rc, "ADDRESS SETUP FAILED");
        return;
    }
    const int scan_rc = ble_gap_disc(s_own_address_type,
                                     BLE_RADAR_SCAN_TIMEOUT_SECONDS * 1000,
                                     &parameters, gap_event, NULL);
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (scan_rc == 0) {
        s_snapshot.scanning = true;
        s_snapshot.generation++;
        s_scan_deadline_ms = now_ms() + BLE_RADAR_SCAN_TIMEOUT_SECONDS * 1000;
        set_result_locked(BLE_RADAR_SCANNING, 0, "SCANNING");
        ESP_LOGI(TAG, "scan started: generation=%" PRIu32 " timeout=%us",
                 s_snapshot.generation, BLE_RADAR_SCAN_TIMEOUT_SECONDS);
    } else {
        set_result_locked(BLE_RADAR_ERROR, scan_rc, "SCAN START FAILED");
    }
    xSemaphoreGive(s_lock);
}

static void host_sync(void)
{
    const int rc = ble_hs_util_ensure_addr(0);
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_snapshot.host_ready = rc == 0;
    s_snapshot.controller_ready = rc == 0;
    if (rc != 0) {
        set_result_locked(BLE_RADAR_ERROR, rc, "HOST SYNC FAILED");
    } else {
        set_result_locked(BLE_RADAR_READY, 0, "READY");
    }
    const bool should_scan = s_start_requested && rc == 0;
    xSemaphoreGive(s_lock);
    if (should_scan) {
        begin_scan();
    }
}

static void host_reset(int reason)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_snapshot.host_ready = false;
    s_snapshot.scanning = false;
    set_result_locked(BLE_RADAR_ERROR, reason, "HOST RESET");
    xSemaphoreGive(s_lock);
}

static void host_task(void *parameter)
{
    (void)parameter;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static void stop_stack(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_start_requested = false;
    const bool was_scanning = s_snapshot.scanning;
    s_snapshot.scanning = false;
    set_result_locked(BLE_RADAR_STOPPING, 0, "RELEASING BLE");
    xSemaphoreGive(s_lock);

    if (was_scanning) {
        ble_gap_disc_cancel();
    }
    if (s_port_initialized) {
        const int rc = nimble_port_stop();
        if (rc == 0) {
            nimble_port_deinit();
            s_port_initialized = false;
        } else {
            set_result(BLE_RADAR_ERROR, rc, "HOST STOP FAILED");
            return;
        }
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_snapshot.host_ready = false;
    s_snapshot.controller_ready = false;
    s_scan_deadline_ms = 0;
    set_result_locked(BLE_RADAR_OFF, 0, "OFF");
    xSemaphoreGive(s_lock);
    ble_radio_owner_release(BLE_RADIO_OWNER_RADAR);
    ESP_LOGI(TAG, "BLE host and controller released");
}

static void start_stack(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_start_requested = true;
    if (s_snapshot.scanning || s_snapshot.state == BLE_RADAR_STARTING ||
        s_snapshot.state == BLE_RADAR_STOPPING) {
        xSemaphoreGive(s_lock);
        return;
    }
    set_result_locked(BLE_RADAR_STARTING, 0, "STARTING HOST");
    xSemaphoreGive(s_lock);

    if (!s_port_initialized) {
        if (!ble_radio_owner_claim(BLE_RADIO_OWNER_RADAR)) {
            set_result(BLE_RADAR_ERROR, ESP_ERR_INVALID_STATE,
                       "BLE BUSY: ANOTHER OWNER");
            return;
        }
        const int rc = nimble_port_init();
        if (rc != ESP_OK) {
            ble_radio_owner_release(BLE_RADIO_OWNER_RADAR);
            set_result(BLE_RADAR_ERROR, rc, "HOST INIT FAILED");
            return;
        }
        s_port_initialized = true;
        ble_hs_cfg.reset_cb = host_reset;
        ble_hs_cfg.sync_cb = host_sync;
        nimble_port_freertos_init(host_task);
    } else {
        begin_scan();
    }
}

static void service_task(void *parameter)
{
    (void)parameter;
    service_command_t command;
    for (;;) {
        if (xQueueReceive(s_commands, &command, pdMS_TO_TICKS(250)) == pdTRUE) {
            if (command == COMMAND_START) {
                start_stack();
            } else {
                stop_stack();
            }
        }

        xSemaphoreTake(s_lock, portMAX_DELAY);
        const int64_t timestamp_ms = now_ms();
        for (uint8_t i = 0; i < s_snapshot.device_count; ++i) {
            s_snapshot.devices[i].stale =
                timestamp_ms - s_snapshot.devices[i].last_seen_ms > 10000;
        }
        xSemaphoreGive(s_lock);
    }
}

esp_err_t ble_radar_service_init(void)
{
    if (s_lock != NULL) {
        return ESP_OK;
    }
    s_lock = xSemaphoreCreateMutex();
    s_commands = xQueueCreate(4, sizeof(service_command_t));
    if (s_lock == NULL || s_commands == NULL) {
        return ESP_ERR_NO_MEM;
    }
    memset(&s_snapshot, 0, sizeof(s_snapshot));
    s_snapshot.state = BLE_RADAR_OFF;
    snprintf(s_snapshot.last_result, sizeof(s_snapshot.last_result), "OFF");
    if (xTaskCreate(service_task, "ble_radar", 4096, NULL, 5, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "service ready: max_devices=%u timeout=%us active_scan=100/30ms",
             BLE_RADAR_MAX_DEVICES, BLE_RADAR_SCAN_TIMEOUT_SECONDS);
    return ESP_OK;
}

bool ble_radar_service_start(void)
{
    if (s_commands == NULL) {
        return false;
    }
    service_command_t command = COMMAND_START;
    return xQueueSend(s_commands, &command, 0) == pdTRUE;
}

void ble_radar_service_stop(const char *reason)
{
    if (s_commands == NULL) {
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_start_requested = false;
    if (reason != NULL && s_snapshot.state != BLE_RADAR_OFF) {
        snprintf(s_snapshot.last_result, sizeof(s_snapshot.last_result), "STOP: %.40s", reason);
    }
    xSemaphoreGive(s_lock);
    service_command_t command = COMMAND_STOP;
    xQueueSend(s_commands, &command, 0);
}

void ble_radar_service_clear(void)
{
    if (s_lock == NULL) {
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    memset(s_snapshot.devices, 0, sizeof(s_snapshot.devices));
    s_snapshot.device_count = 0;
    s_snapshot.advertisement_count = 0;
    s_snapshot.dropped_count = 0;
    s_snapshot.parse_error_count = 0;
    xSemaphoreGive(s_lock);
}

void ble_radar_service_get_snapshot(ble_radar_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return;
    }
    memset(snapshot, 0, sizeof(*snapshot));
    if (s_lock == NULL) {
        snapshot->state = BLE_RADAR_OFF;
        snprintf(snapshot->last_result, sizeof(snapshot->last_result), "NOT INITIALIZED");
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *snapshot = s_snapshot;
    if (s_snapshot.scanning && s_scan_deadline_ms > 0) {
        const int64_t remaining = s_scan_deadline_ms - now_ms();
        snapshot->seconds_remaining = remaining > 0 ? (uint32_t)((remaining + 999) / 1000) : 0;
    }
    xSemaphoreGive(s_lock);
    snapshot->internal_free_bytes = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    snapshot->internal_largest_bytes = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
}

bool ble_radar_service_is_active(void)
{
    if (s_lock == NULL) return false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    const bool active = s_snapshot.scanning;
    xSemaphoreGive(s_lock);
    return active;
}

bool ble_radar_service_can_sleep(void)
{
    if (s_lock == NULL) return true;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    const bool can_sleep = s_snapshot.state == BLE_RADAR_OFF ||
                           s_snapshot.state == BLE_RADAR_ERROR;
    xSemaphoreGive(s_lock);
    return can_sleep;
}

const char *ble_radar_state_name(ble_radar_state_t state)
{
    switch (state) {
    case BLE_RADAR_OFF: return "OFF";
    case BLE_RADAR_STARTING: return "STARTING";
    case BLE_RADAR_READY: return "READY";
    case BLE_RADAR_SCANNING: return "SCANNING";
    case BLE_RADAR_STOPPING: return "STOPPING";
    case BLE_RADAR_ERROR: return "ERROR";
    default: return "UNKNOWN";
    }
}

const char *ble_radar_address_type_name(uint8_t type)
{
    switch (type) {
    case BLE_ADDR_PUBLIC: return "PUBLIC";
    case BLE_ADDR_RANDOM: return "RANDOM";
    case BLE_ADDR_PUBLIC_ID: return "PUBLIC ID";
    case BLE_ADDR_RANDOM_ID: return "RANDOM ID";
    default: return "UNKNOWN";
    }
}

const char *ble_radar_event_type_name(uint8_t type)
{
    switch (type) {
    case BLE_HCI_ADV_RPT_EVTYPE_ADV_IND: return "CONNECTABLE";
    case BLE_HCI_ADV_RPT_EVTYPE_DIR_IND: return "DIRECTED";
    case BLE_HCI_ADV_RPT_EVTYPE_SCAN_IND: return "SCANNABLE";
    case BLE_HCI_ADV_RPT_EVTYPE_NONCONN_IND: return "NONCONNECT";
    case BLE_HCI_ADV_RPT_EVTYPE_SCAN_RSP: return "SCAN RESPONSE";
    default: return "ADVERTISEMENT";
    }
}
