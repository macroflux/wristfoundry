#include "time_sync_service.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "network_service.h"
#include "nvs.h"

typedef enum { COMMAND_START, COMMAND_CANCEL } sync_command_t;
static const char *TAG = "time_sync";
static SemaphoreHandle_t s_lock;
static QueueHandle_t s_commands;
static time_sync_snapshot_t s_snapshot;
static uint32_t s_network_generation;
static int64_t s_last_request_ms;
static bool s_persist_history;
static bool s_start_pending;
static bool s_start_canceled;

static int64_t now_ms(void) { return esp_timer_get_time() / 1000; }
static void lock(void) { xSemaphoreTake(s_lock, portMAX_DELAY); }
static void unlock(void) { xSemaphoreGive(s_lock); }

static void save_success(const char *source, const char *local, int64_t epoch)
{
    if (!s_persist_history) return;
    nvs_handle_t handle;
    if (nvs_open("time_sync", NVS_READWRITE, &handle) != ESP_OK) return;
    esp_err_t result = nvs_set_str(handle, "source", source);
    if (result == ESP_OK) result = nvs_set_str(handle, "local", local);
    if (result == ESP_OK) result = nvs_set_i64(handle, "epoch", epoch);
    if (result == ESP_OK) result = nvs_commit(handle);
    nvs_close(handle);
    if (result != ESP_OK) ESP_LOGW(TAG, "persist failed: %s", esp_err_to_name(result));
}

static void load_success(void)
{
    if (!s_persist_history) return;
    nvs_handle_t handle;
    if (nvs_open("time_sync", NVS_READONLY, &handle) != ESP_OK) return;
    size_t source_len = sizeof(s_snapshot.last_source);
    size_t local_len = sizeof(s_snapshot.last_local);
    (void)nvs_get_str(handle, "source", s_snapshot.last_source, &source_len);
    (void)nvs_get_str(handle, "local", s_snapshot.last_local, &local_len);
    (void)nvs_get_i64(handle, "epoch", &s_snapshot.last_success_epoch);
    nvs_close(handle);
}

static void fail_sync(const char *status, const char *detail)
{
    network_service_cancel_time_sync();
    lock();
    s_snapshot.active = false;
    s_snapshot.state = TIME_SYNC_FAILED;
    strlcpy(s_snapshot.status, status, sizeof(s_snapshot.status));
    strlcpy(s_snapshot.detail, detail, sizeof(s_snapshot.detail));
    unlock();
}

static void begin_sync(void)
{
    network_snapshot_t network;
    network_service_get_snapshot(&network);
    lock();
    s_snapshot.generation++;
    s_snapshot.wifi_attempted = network.credentials_present;
    s_snapshot.active = network.credentials_present;
    s_snapshot.state = network.credentials_present ? TIME_SYNC_WIFI : TIME_SYNC_FAILED;
    strlcpy(s_snapshot.method, "WI-FI / NTP", sizeof(s_snapshot.method));
    strlcpy(s_snapshot.status, network.credentials_present ?
            "Connecting to Wi-Fi" : "Wi-Fi not configured", sizeof(s_snapshot.status));
    strlcpy(s_snapshot.detail, network.credentials_present ?
            "Using configured network, then pool.ntp.org" :
            "Add credentials in Settings; BLE time sync is unsupported",
            sizeof(s_snapshot.detail));
    unlock();
    if (!network.credentials_present) return;
    if (!network_service_request_time_sync()) {
        fail_sync("NTP request rejected", "Network service is busy; try again");
        return;
    }
    network_service_get_snapshot(&network);
    s_network_generation = network.sync_generation;
}

static void cancel_sync(const char *reason)
{
    network_service_cancel_time_sync();
    lock();
    s_snapshot.active = false;
    s_snapshot.state = TIME_SYNC_CANCELED;
    strlcpy(s_snapshot.status, "Canceled", sizeof(s_snapshot.status));
    strlcpy(s_snapshot.detail, reason != NULL ? reason : "User canceled",
            sizeof(s_snapshot.detail));
    unlock();
}

static void poll_wifi(void)
{
    network_snapshot_t network;
    network_service_get_snapshot(&network);
    lock();
    if (network.sync_generation != s_network_generation) { unlock(); return; }
    snprintf(s_snapshot.status, sizeof(s_snapshot.status), "%s",
             network.sync_in_progress ? "Synchronizing with NTP" :
             network.state == NETWORK_STATE_CONNECTING ? "Connecting to Wi-Fi" :
             network.last_sync_result);
    snprintf(s_snapshot.detail, sizeof(s_snapshot.detail), "%s / %s",
             network_service_state_name(network.state), network.last_sync_result);
    const bool success = network.last_sync_succeeded;
    const bool pending = network.sync_in_progress ||
                         strcmp(network.last_sync_result, "REQUESTED") == 0;
    unlock();
    if (success) {
        lock();
        s_snapshot.active = false;
        s_snapshot.state = TIME_SYNC_SUCCESS;
        s_snapshot.last_success_epoch = network.last_sync_epoch;
        strlcpy(s_snapshot.last_source, "WI-FI NTP", sizeof(s_snapshot.last_source));
        strlcpy(s_snapshot.last_local, network.last_sync_local, sizeof(s_snapshot.last_local));
        strlcpy(s_snapshot.status, "Time synchronized", sizeof(s_snapshot.status));
        snprintf(s_snapshot.detail, sizeof(s_snapshot.detail), "NTP / %s", network.last_sync_local);
        unlock();
        save_success("WI-FI NTP", network.last_sync_local, network.last_sync_epoch);
    } else if (!pending && strcmp(network.last_sync_result, "NEVER") != 0) {
        fail_sync("NTP synchronization failed", network.last_sync_result);
    }
}

static void service_task(void *arg)
{
    (void)arg;
    sync_command_t command;
    for (;;) {
        if (xQueueReceive(s_commands, &command, pdMS_TO_TICKS(200)) == pdTRUE) {
            if (command == COMMAND_START) {
                lock();
                bool canceled = s_start_canceled;
                s_start_pending = false;
                s_start_canceled = false;
                unlock();
                if (!canceled) begin_sync();
            } else cancel_sync("USER CANCELED");
        }
        lock();
        const bool poll = s_snapshot.active && s_snapshot.state == TIME_SYNC_WIFI;
        unlock();
        if (poll) poll_wifi();
    }
}

esp_err_t time_sync_service_init(bool persist_history)
{
    if (s_lock != NULL) return ESP_OK;
    s_persist_history = persist_history;
    s_lock = xSemaphoreCreateMutex();
    s_commands = xQueueCreate(3, sizeof(sync_command_t));
    if (s_lock == NULL || s_commands == NULL) return ESP_ERR_NO_MEM;
    memset(&s_snapshot, 0, sizeof(s_snapshot));
    s_snapshot.state = TIME_SYNC_IDLE;
    strlcpy(s_snapshot.method, "WI-FI / NTP", sizeof(s_snapshot.method));
    strlcpy(s_snapshot.status, "Ready", sizeof(s_snapshot.status));
    strlcpy(s_snapshot.detail, "NTP first; hardware RTC keeps time offline", sizeof(s_snapshot.detail));
    strlcpy(s_snapshot.last_source, "NEVER", sizeof(s_snapshot.last_source));
    load_success();
    if (xTaskCreate(service_task, "time_sync", 4096, NULL, 5, NULL) != pdPASS) return ESP_ERR_NO_MEM;
    ESP_LOGI(TAG, "service ready: Wi-Fi/NTP with hardware RTC; history=%s",
             s_persist_history ? "encrypted NVS" : "volatile");
    return ESP_OK;
}

bool time_sync_service_request_auto(void)
{
    if (s_commands == NULL) return false;
    lock();
    const int64_t now = now_ms();
    if (s_snapshot.active || s_start_pending || now - s_last_request_ms < 3000) { unlock(); return false; }
    s_last_request_ms = now;
    s_start_pending = true;
    s_start_canceled = false;
    s_snapshot.active = true;
    unlock();
    const sync_command_t command = COMMAND_START;
    bool queued = xQueueSend(s_commands, &command, 0) == pdTRUE;
    if (!queued) { lock(); s_start_pending = false; s_snapshot.active = false; unlock(); }
    return queued;
}

esp_err_t time_sync_service_set_utc(int64_t epoch, const char *source)
{
    if (!s_lock || !source) return ESP_ERR_INVALID_STATE;
    lock();
    if (s_snapshot.active || s_start_pending) { unlock(); return ESP_ERR_INVALID_STATE; }
    /* Reserve the existing sync owner while the shared network/RTC API writes. */
    s_snapshot.active = true;
    s_snapshot.state = TIME_SYNC_IDLE;
    unlock();
    esp_err_t result = network_service_set_time_utc(epoch);
    char local_text[24] = "";
    if (result == ESP_OK) {
        time_t now = (time_t)epoch;
        struct tm local;
        localtime_r(&now, &local);
        strftime(local_text, sizeof(local_text), "%Y-%m-%d %H:%M:%S", &local);
    }
    lock();
    s_snapshot.active = false;
    s_snapshot.generation++;
    s_snapshot.state = result == ESP_OK ? TIME_SYNC_SUCCESS : TIME_SYNC_FAILED;
    strlcpy(s_snapshot.method, source, sizeof(s_snapshot.method));
    strlcpy(s_snapshot.status, result == ESP_OK ? "Time set / RTC updated" : "Time set failed", sizeof(s_snapshot.status));
    strlcpy(s_snapshot.detail, result == ESP_OK ? source : esp_err_to_name(result), sizeof(s_snapshot.detail));
    if (result == ESP_OK) {
        s_snapshot.last_success_epoch = epoch;
        strlcpy(s_snapshot.last_source, source, sizeof(s_snapshot.last_source));
        strlcpy(s_snapshot.last_local, local_text, sizeof(s_snapshot.last_local));
    }
    unlock();
    if (result == ESP_OK) save_success(source, local_text, epoch);
    return result;
}

void time_sync_service_cancel(const char *reason)
{
    if (s_commands == NULL) return;
    lock();
    const bool active = s_snapshot.active;
    if (s_start_pending) s_start_canceled = true;
    unlock();
    if (!active) return;
    network_service_cancel_time_sync();
    const sync_command_t command = COMMAND_CANCEL;
    xQueueSend(s_commands, &command, 0);
    (void)reason;
}

void time_sync_service_get_snapshot(time_sync_snapshot_t *snapshot)
{
    if (snapshot == NULL) return;
    memset(snapshot, 0, sizeof(*snapshot));
    if (s_lock == NULL) return;
    lock(); *snapshot = s_snapshot; unlock();
}

bool time_sync_service_can_sleep(void)
{
    if (s_lock == NULL) return true;
    lock(); const bool inactive = !s_snapshot.active && !s_start_pending; unlock();
    return inactive;
}

const char *time_sync_state_name(time_sync_state_t state)
{
    switch (state) {
    case TIME_SYNC_WIFI: return "WI-FI";
    case TIME_SYNC_SUCCESS: return "SUCCESS";
    case TIME_SYNC_FAILED: return "FAILED";
    case TIME_SYNC_CANCELED: return "CANCELED";
    case TIME_SYNC_IDLE:
    default: return "IDLE";
    }
}
