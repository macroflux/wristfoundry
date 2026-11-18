#include "network_service.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "board_services.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"
#include "time_sync_service.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#define CONNECT_TIMEOUT_MS 15000
#define RETRY_INTERVAL_MS 2000
#define MAX_CONNECT_ATTEMPTS 3
#define IDLE_RADIO_TIMEOUT_MS 15000
#define NTP_TIMEOUT_MS 12000
#define SERVICE_TICK_MS 100
#define PROVISION_LINE_MAX 520
#define WIFI_SCAN_RECORD_LIMIT 24

static const char *TAG = "network";
static SemaphoreHandle_t s_lock;
static network_snapshot_t s_snapshot;
static char s_password[65];
static bool s_nvs_ready;
static esp_netif_t *s_sta_netif;
static bool s_wifi_initialized;
static bool s_wifi_started;
static bool s_manual_lease;
static bool s_sync_lease;
static bool s_sync_requested;
static bool s_sync_cancel_requested;
static bool s_start_requested;
static bool s_scan_requested;
static int64_t s_connect_started_us;
static int64_t s_next_retry_us;
static int64_t s_idle_deadline_us;
static int64_t s_radio_started_us;
static int64_t s_online_started_us;
static uint8_t s_connect_attempts;
static wifi_ap_record_t s_scan_records[WIFI_SCAN_RECORD_LIMIT];

static esp_err_t ensure_wifi_initialized(void);

static void lock(void) { xSemaphoreTake(s_lock, portMAX_DELAY); }
static void unlock(void) { xSemaphoreGive(s_lock); }

static void secure_zero(void *buffer, size_t length)
{
    volatile unsigned char *cursor = buffer;
    while (length-- > 0) *cursor++ = 0;
}

const char *network_service_state_name(network_state_t state)
{
    switch (state) {
    case NETWORK_STATE_CONNECTING: return "CONNECTING";
    case NETWORK_STATE_ONLINE: return "ONLINE";
    case NETWORK_STATE_ERROR: return "ERROR";
    case NETWORK_STATE_OFF:
    default: return "OFF";
    }
}

static void set_state_locked(network_state_t state, const char *result)
{
    if (s_snapshot.state != state) {
        ESP_LOGI(TAG, "State %s -> %s", network_service_state_name(s_snapshot.state),
                 network_service_state_name(state));
        s_snapshot.state = state;
    }
    if (result != NULL) {
        strlcpy(s_snapshot.last_connect_result, result,
                sizeof(s_snapshot.last_connect_result));
    }
}

static esp_err_t load_configuration(void)
{
    nvs_handle_t handle;
    esp_err_t result = nvs_open("network", NVS_READONLY, &handle);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        strlcpy(s_snapshot.timezone, "NOT SET", sizeof(s_snapshot.timezone));
        strlcpy(s_snapshot.last_connect_result, "NOT PROVISIONED",
                sizeof(s_snapshot.last_connect_result));
        return ESP_OK;
    }
    if (result != ESP_OK) {
        return result;
    }

    size_t ssid_length = sizeof(s_snapshot.ssid);
    size_t password_length = sizeof(s_password);
    size_t timezone_length = sizeof(s_snapshot.timezone);
    result = nvs_get_str(handle, "ssid", s_snapshot.ssid, &ssid_length);
    if (result == ESP_OK) {
        result = nvs_get_str(handle, "password", s_password, &password_length);
    }
    if (result == ESP_OK) {
        result = nvs_get_str(handle, "timezone", s_snapshot.timezone,
                             &timezone_length);
    }
    nvs_close(handle);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        secure_zero(s_password, sizeof(s_password));
        memset(s_snapshot.ssid, 0, sizeof(s_snapshot.ssid));
        strlcpy(s_snapshot.timezone, "NOT SET", sizeof(s_snapshot.timezone));
        strlcpy(s_snapshot.last_connect_result, "NOT PROVISIONED",
                sizeof(s_snapshot.last_connect_result));
        return ESP_OK;
    }
    if (result == ESP_OK && s_snapshot.ssid[0] != '\0' &&
        s_snapshot.timezone[0] != '\0') {
        s_snapshot.credentials_present = true;
        strlcpy(s_snapshot.last_connect_result, "READY",
                sizeof(s_snapshot.last_connect_result));
        setenv("TZ", s_snapshot.timezone, 1);
        tzset();
        ESP_LOGI(TAG, "Saved encrypted network configuration loaded");
        return ESP_OK;
    }

    secure_zero(s_password, sizeof(s_password));
    memset(s_snapshot.ssid, 0, sizeof(s_snapshot.ssid));
    strlcpy(s_snapshot.timezone, "NOT SET", sizeof(s_snapshot.timezone));
    strlcpy(s_snapshot.last_connect_result, "INCOMPLETE CONFIG",
            sizeof(s_snapshot.last_connect_result));
    return result == ESP_OK ? ESP_ERR_INVALID_STATE : result;
}

bool network_service_credentials_are_encrypted(void)
{
#if CONFIG_NVS_ENCRYPTION
    return s_nvs_ready;
#else
    return false;
#endif
}

esp_err_t network_service_save_configuration(const char *ssid,
                                              const char *password,
                                              const char *timezone)
{
    if (s_lock == NULL) return ESP_ERR_INVALID_STATE;
    if (ssid == NULL || password == NULL || timezone == NULL || ssid[0] == '\0' ||
        timezone[0] == '\0' || strlen(ssid) > 32 || strlen(password) > 64 ||
        strlen(timezone) >= sizeof(s_snapshot.timezone)) {
        return ESP_ERR_INVALID_ARG;
    }
#if CONFIG_NVS_ENCRYPTION
    if (!s_nvs_ready) {
        ESP_LOGE(TAG, "Refusing to persist Wi-Fi credentials because encrypted NVS is unavailable");
        return ESP_ERR_INVALID_STATE;
    }
#endif

    network_service_force_off("SETTINGS UPDATE");
#if CONFIG_NVS_ENCRYPTION
    nvs_handle_t handle = 0;
    esp_err_t result = nvs_open("network", NVS_READWRITE, &handle);
    if (result == ESP_OK) result = nvs_set_str(handle, "ssid", ssid);
    if (result == ESP_OK) result = nvs_set_str(handle, "password", password);
    if (result == ESP_OK) result = nvs_set_str(handle, "timezone", timezone);
    if (result == ESP_OK) result = nvs_commit(handle);
    if (handle != 0) nvs_close(handle);
    if (result != ESP_OK) return result;
#endif

    lock();
    strlcpy(s_snapshot.ssid, ssid, sizeof(s_snapshot.ssid));
    strlcpy(s_password, password, sizeof(s_password));
    strlcpy(s_snapshot.timezone, timezone, sizeof(s_snapshot.timezone));
    s_snapshot.credentials_present = true;
    set_state_locked(NETWORK_STATE_OFF, "READY");
    unlock();
    setenv("TZ", timezone, 1);
    tzset();
    ESP_LOGI(TAG, "%s network configuration accepted",
             network_service_credentials_are_encrypted() ? "Encrypted persistent" : "Volatile");
    return ESP_OK;
}

esp_err_t network_service_clear_configuration(void)
{
    if (s_lock == NULL) return ESP_ERR_INVALID_STATE;
    network_service_force_off("SETTINGS CLEAR");
#if CONFIG_NVS_ENCRYPTION
    if (!s_nvs_ready) return ESP_ERR_INVALID_STATE;
    nvs_handle_t handle = 0;
    esp_err_t result = nvs_open("network", NVS_READWRITE, &handle);
    if (result == ESP_OK) result = nvs_erase_all(handle);
    if (result == ESP_OK) result = nvs_commit(handle);
    if (handle != 0) nvs_close(handle);
    if (result != ESP_OK) return result;
#endif
    lock();
    secure_zero(s_password, sizeof(s_password));
    memset(s_snapshot.ssid, 0, sizeof(s_snapshot.ssid));
    strlcpy(s_snapshot.timezone, "NOT SET", sizeof(s_snapshot.timezone));
    s_snapshot.credentials_present = false;
    set_state_locked(NETWORK_STATE_OFF, "NOT PROVISIONED");
    unlock();
    return ESP_OK;
}

static void wifi_event(void *argument, esp_event_base_t base, int32_t id, void *data)
{
    (void)argument;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *event = data;
        lock();
        if (s_snapshot.state == NETWORK_STATE_ONLINE) {
            ESP_LOGW(TAG, "Disconnected: reason=%d; reconnect governed by active lease",
                     event->reason);
        }
        s_snapshot.ip[0] = '\0';
        s_snapshot.rssi = 0;
        if (s_snapshot.lease_count > 0 && s_wifi_started &&
            s_snapshot.state == NETWORK_STATE_ONLINE) {
            set_state_locked(NETWORK_STATE_CONNECTING, "DISCONNECTED");
            s_connect_started_us = esp_timer_get_time();
            s_next_retry_us = s_connect_started_us;
            s_connect_attempts = 0;
        }
        unlock();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = data;
        lock();
        snprintf(s_snapshot.ip, sizeof(s_snapshot.ip), IPSTR, IP2STR(&event->ip_info.ip));
        s_online_started_us = esp_timer_get_time();
        set_state_locked(NETWORK_STATE_ONLINE, "CONNECTED");
        ESP_LOGI(TAG, "Connected to configured network: duration_ms=%" PRId64,
                 (s_online_started_us - s_connect_started_us) / 1000);
        unlock();
    }
}

static esp_err_t ensure_wifi_initialized(void)
{
    if (s_wifi_initialized) {
        return ESP_OK;
    }
    esp_err_t result = esp_netif_init();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) return result;
    result = esp_event_loop_create_default();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) return result;
    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (s_sta_netif == NULL) return ESP_ERR_NO_MEM;

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    init.nvs_enable = false;
    result = esp_wifi_init(&init);
    if (result != ESP_OK) return result;
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                wifi_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                wifi_event, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    s_wifi_initialized = true;
    return ESP_OK;
}

static esp_err_t start_radio(void)
{
    if (s_wifi_started) return ESP_OK;
    esp_err_t result = ensure_wifi_initialized();
    if (result != ESP_OK) return result;

    wifi_config_t config = {0};
    strlcpy((char *)config.sta.ssid, s_snapshot.ssid, sizeof(config.sta.ssid));
    strlcpy((char *)config.sta.password, s_password, sizeof(config.sta.password));
    config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    config.sta.pmf_cfg.capable = true;
    config.sta.pmf_cfg.required = false;
    result = esp_wifi_set_config(WIFI_IF_STA, &config);
    secure_zero(&config, sizeof(config));
    ESP_RETURN_ON_ERROR(result, TAG, "Station configuration failed");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "Radio start failed");

    s_wifi_started = true;
    s_snapshot.radio_active = true;
    s_radio_started_us = esp_timer_get_time();
    s_connect_started_us = s_radio_started_us;
    s_next_retry_us = s_radio_started_us;
    s_connect_attempts = 0;
    set_state_locked(NETWORK_STATE_CONNECTING, "STARTED");
    ESP_LOGI(TAG, "Radio active interval started; leases=%" PRIu32,
             s_snapshot.lease_count);
    return ESP_OK;
}

static void stop_radio_locked(const char *reason)
{
    if (!s_wifi_started) {
        if (s_snapshot.state != NETWORK_STATE_ERROR) {
            set_state_locked(NETWORK_STATE_OFF, reason);
        }
        return;
    }
    (void)esp_wifi_disconnect();
    esp_err_t result = esp_wifi_stop();
    const int64_t interval_us = esp_timer_get_time() - s_radio_started_us;
    s_snapshot.cumulative_radio_active_ms += (uint64_t)(interval_us / 1000);
    s_snapshot.current_radio_active_ms = 0;
    s_snapshot.current_connection_ms = 0;
    s_snapshot.radio_active = false;
    s_wifi_started = false;
    s_snapshot.ip[0] = '\0';
    s_snapshot.rssi = 0;
    set_state_locked(NETWORK_STATE_OFF, reason);
    ESP_LOGI(TAG,
             "Radio active interval ended: reason=%s duration_ms=%" PRId64
             " cumulative_ms=%" PRIu64 " stop=%s",
             reason, interval_us / 1000, s_snapshot.cumulative_radio_active_ms,
             esp_err_to_name(result));
}

esp_err_t network_service_acquire(const char *owner)
{
    if (s_lock == NULL) return ESP_ERR_INVALID_STATE;
    lock();
    if (!s_snapshot.credentials_present) {
        set_state_locked(NETWORK_STATE_ERROR, "NO CREDENTIALS");
        unlock();
        ESP_LOGW(TAG, "Lease denied for %s: no saved configuration",
                 owner != NULL ? owner : "unnamed");
        return ESP_ERR_NOT_FOUND;
    }
    s_snapshot.lease_count++;
    s_idle_deadline_us = 0;
    if (!s_wifi_started) s_start_requested = true;
    ESP_LOGI(TAG, "Lease acquired: owner=%s count=%" PRIu32,
             owner != NULL ? owner : "unnamed", s_snapshot.lease_count);
    unlock();
    return ESP_OK;
}

void network_service_release(const char *owner)
{
    if (s_lock == NULL) return;
    lock();
    if (s_snapshot.lease_count > 0) s_snapshot.lease_count--;
    if (s_snapshot.lease_count == 0 && s_wifi_started) {
        s_idle_deadline_us = esp_timer_get_time() + IDLE_RADIO_TIMEOUT_MS * 1000LL;
    }
    ESP_LOGI(TAG, "Lease released: owner=%s count=%" PRIu32 " idle_timeout_ms=%d",
             owner != NULL ? owner : "unnamed", s_snapshot.lease_count,
             IDLE_RADIO_TIMEOUT_MS);
    unlock();
}

void network_service_force_off(const char *reason)
{
    if (s_lock == NULL) return;
    lock();
    s_manual_lease = false;
    s_sync_lease = false;
    s_sync_requested = false;
    s_sync_cancel_requested = true;
    s_start_requested = false;
    s_scan_requested = false;
    if (s_snapshot.scan_in_progress) (void)esp_wifi_scan_stop();
    s_snapshot.scan_in_progress = false;
    s_snapshot.sync_in_progress = false;
    s_snapshot.lease_count = 0;
    stop_radio_locked(reason != NULL ? reason : "FORCED OFF");
    unlock();
}

void network_service_request_manual_connect(void)
{
    if (s_lock == NULL) return;
    lock();
    const bool acquire = !s_manual_lease;
    if (acquire) s_manual_lease = true;
    unlock();
    if (acquire && network_service_acquire("manual-ui") != ESP_OK) {
        lock(); s_manual_lease = false; unlock();
    }
}

void network_service_release_manual_connect(void)
{
    if (s_lock == NULL) return;
    lock();
    const bool release = s_manual_lease;
    s_manual_lease = false;
    unlock();
    if (release) network_service_release("manual-ui");
}

bool network_service_request_scan(void)
{
    if (s_lock == NULL) return false;
    lock();
    if (s_scan_requested || s_snapshot.scan_in_progress || s_wifi_started ||
        s_start_requested || s_snapshot.lease_count != 0) {
        unlock();
        return false;
    }
    s_scan_requested = true;
    strlcpy(s_snapshot.last_scan_result, "REQUESTED",
            sizeof(s_snapshot.last_scan_result));
    unlock();
    return true;
}

bool network_service_request_time_sync(void)
{
    if (s_lock == NULL) return false;
    lock();
    if (s_sync_requested || s_snapshot.sync_in_progress) {
        unlock();
        return false;
    }
    s_snapshot.sync_generation++;
    s_sync_requested = true;
    s_sync_cancel_requested = false;
    s_sync_lease = true;
    strlcpy(s_snapshot.last_sync_result, "REQUESTED",
            sizeof(s_snapshot.last_sync_result));
    unlock();
    if (network_service_acquire("ntp-sync") != ESP_OK) {
        lock();
        s_sync_requested = false;
        s_sync_lease = false;
        s_snapshot.last_sync_succeeded = false;
        strlcpy(s_snapshot.last_sync_result, "NO NETWORK CONFIG",
                sizeof(s_snapshot.last_sync_result));
        unlock();
        return false;
    }
    return true;
}

void network_service_cancel_time_sync(void)
{
    if (s_lock == NULL) return;
    lock();
    const bool release_now = s_sync_lease && !s_snapshot.sync_in_progress;
    s_sync_cancel_requested = true;
    s_sync_requested = false;
    if (release_now) s_sync_lease = false;
    s_snapshot.last_sync_succeeded = false;
    strlcpy(s_snapshot.last_sync_result, "CANCELED",
            sizeof(s_snapshot.last_sync_result));
    unlock();
    if (release_now) {
        network_service_release("ntp-sync");
        lock();
        if (s_snapshot.lease_count == 0) stop_radio_locked("SYNC CANCELED");
        unlock();
    }
}

void network_service_get_snapshot(network_snapshot_t *snapshot)
{
    if (snapshot == NULL) return;
    if (s_lock == NULL) {
        memset(snapshot, 0, sizeof(*snapshot));
        return;
    }
    lock();
    *snapshot = s_snapshot;
    const int64_t now = esp_timer_get_time();
    if (s_wifi_started) {
        snapshot->current_radio_active_ms = (uint64_t)((now - s_radio_started_us) / 1000);
    }
    if (s_snapshot.state == NETWORK_STATE_ONLINE) {
        snapshot->current_connection_ms = (uint32_t)((now - s_online_started_us) / 1000);
        wifi_ap_record_t ap;
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) snapshot->rssi = ap.rssi;
    }
    unlock();
}

static void sanitize_scan_ssid(const uint8_t *source, char *destination,
                               size_t destination_size)
{
    size_t index = 0;
    while (index + 1 < destination_size && index < 32 && source[index] != '\0') {
        const unsigned char value = source[index];
        destination[index] = (value >= 0x20 && value <= 0x7e) ? (char)value : '?';
        index++;
    }
    destination[index] = '\0';
}

static void finish_scan(esp_err_t result, wifi_ap_record_t *records,
                        uint16_t record_count)
{
    lock();
    memset(s_snapshot.scan_results, 0, sizeof(s_snapshot.scan_results));
    s_snapshot.scan_count = 0;
    if (result == ESP_OK) {
        for (uint16_t i = 0;
             i < record_count && s_snapshot.scan_count < NETWORK_SCAN_MAX_RESULTS;
             ++i) {
            char ssid[33];
            sanitize_scan_ssid(records[i].ssid, ssid, sizeof(ssid));
            if (ssid[0] == '\0') continue;
            bool duplicate = false;
            for (size_t j = 0; j < s_snapshot.scan_count; ++j) {
                if (strcmp(s_snapshot.scan_results[j].ssid, ssid) == 0) {
                    duplicate = true;
                    break;
                }
            }
            if (duplicate) continue;
            network_scan_result_t *entry =
                &s_snapshot.scan_results[s_snapshot.scan_count++];
            strlcpy(entry->ssid, ssid, sizeof(entry->ssid));
            entry->rssi = records[i].rssi;
            entry->secured = records[i].authmode != WIFI_AUTH_OPEN;
        }
        strlcpy(s_snapshot.last_scan_result,
                s_snapshot.scan_count > 0 ? "COMPLETE" : "NO NETWORKS",
                sizeof(s_snapshot.last_scan_result));
    } else {
        strlcpy(s_snapshot.last_scan_result, esp_err_to_name(result),
                sizeof(s_snapshot.last_scan_result));
    }
    s_snapshot.scan_generation++;
    s_snapshot.scan_in_progress = false;
    unlock();
}

static void perform_wifi_scan(void)
{
    memset(s_scan_records, 0, sizeof(s_scan_records));
    uint16_t record_count = WIFI_SCAN_RECORD_LIMIT;
    esp_err_t result = ensure_wifi_initialized();
    if (result == ESP_OK) result = esp_wifi_start();
    if (result == ESP_OK) {
        lock();
        s_wifi_started = true;
        s_snapshot.radio_active = true;
        s_radio_started_us = esp_timer_get_time();
        unlock();
        const wifi_scan_config_t config = {
            .show_hidden = false,
            .scan_type = WIFI_SCAN_TYPE_ACTIVE,
            .scan_time.active = {.min = 80, .max = 180},
        };
        result = esp_wifi_scan_start(&config, true);
        if (result == ESP_OK) {
            result = esp_wifi_scan_get_ap_records(&record_count, s_scan_records);
        }
    }

    lock();
    if (s_wifi_started) {
        (void)esp_wifi_stop();
        const int64_t interval_us = esp_timer_get_time() - s_radio_started_us;
        s_snapshot.cumulative_radio_active_ms += (uint64_t)(interval_us / 1000);
        s_snapshot.current_radio_active_ms = 0;
        s_snapshot.radio_active = false;
        s_wifi_started = false;
        ESP_LOGI(TAG, "Wi-Fi scan radio interval ended: duration_ms=%" PRId64,
                 interval_us / 1000);
    }
    unlock();
    finish_scan(result, s_scan_records, result == ESP_OK ? record_count : 0);
}

static void perform_ntp_sync(void)
{
    lock();
    s_sync_requested = false;
    s_snapshot.sync_in_progress = true;
    strlcpy(s_snapshot.last_sync_result, "IN PROGRESS",
            sizeof(s_snapshot.last_sync_result));
    unlock();

    ESP_LOGI(TAG, "NTP synchronization started: server=pool.ntp.org timeout_ms=%d",
             NTP_TIMEOUT_MS);
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_err_t result = esp_netif_sntp_init(&config);
    if (result == ESP_OK) {
        const int64_t deadline = esp_timer_get_time() + NTP_TIMEOUT_MS * 1000LL;
        result = ESP_ERR_TIMEOUT;
        while (esp_timer_get_time() < deadline) {
            lock();
            const bool canceled = s_sync_cancel_requested;
            unlock();
            if (canceled) {
                result = ESP_ERR_INVALID_STATE;
                break;
            }
            const esp_err_t wait_result =
                esp_netif_sntp_sync_wait(pdMS_TO_TICKS(250));
            if (wait_result == ESP_OK) {
                result = ESP_OK;
                break;
            }
        }
        esp_netif_sntp_deinit();
    }

    char local_text[24] = "NEVER";
    esp_err_t rtc_result = ESP_FAIL;
    if (result == ESP_OK) {
        time_t now;
        time(&now);
        if (now < 1704067200LL || now > 4102444799LL) {
            result = ESP_ERR_INVALID_RESPONSE;
        }
    }
    if (result == ESP_OK) {
        time_t now;
        time(&now);
        struct tm local;
        localtime_r(&now, &local);
        rtc_result = board_services_set_rtc_local((uint16_t)(local.tm_year + 1900),
                                                   (uint8_t)(local.tm_mon + 1),
                                                   (uint8_t)local.tm_mday,
                                                   (uint8_t)local.tm_hour,
                                                   (uint8_t)local.tm_min,
                                                   (uint8_t)local.tm_sec);
        strftime(local_text, sizeof(local_text), "%Y-%m-%d %H:%M:%S", &local);
    }

    lock();
    s_snapshot.sync_in_progress = false;
    s_snapshot.last_sync_succeeded = result == ESP_OK && rtc_result == ESP_OK;
    if (result != ESP_OK) {
        snprintf(s_snapshot.last_sync_result, sizeof(s_snapshot.last_sync_result),
                 "NTP %s", esp_err_to_name(result));
    } else if (rtc_result != ESP_OK) {
        snprintf(s_snapshot.last_sync_result, sizeof(s_snapshot.last_sync_result),
                 "RTC %s", esp_err_to_name(rtc_result));
    } else {
        strlcpy(s_snapshot.last_sync_result, "SUCCESS",
                sizeof(s_snapshot.last_sync_result));
        strlcpy(s_snapshot.last_sync_source, "WI-FI NTP",
                sizeof(s_snapshot.last_sync_source));
        s_snapshot.last_sync_epoch = time(NULL);
        strlcpy(s_snapshot.last_sync_local, local_text,
                sizeof(s_snapshot.last_sync_local));
    }
    unlock();
    ESP_LOGI(TAG, "NTP result=%s local=%s RTC update=%s",
             esp_err_to_name(result), local_text, esp_err_to_name(rtc_result));

    lock();
    const bool release = s_sync_lease;
    s_sync_lease = false;
    const bool canceled = s_sync_cancel_requested;
    unlock();
    if (release) {
        network_service_release("ntp-sync");
        if (canceled) {
            lock();
            if (s_snapshot.lease_count == 0) stop_radio_locked("SYNC CANCELED");
            unlock();
        }
    }
}

static int hex_value(char value)
{
    if (value >= '0' && value <= '9') return value - '0';
    value = (char)tolower((unsigned char)value);
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    return -1;
}

static bool decode_hex(const char *input, char *output, size_t output_size)
{
    const size_t length = strlen(input);
    if ((length & 1U) != 0 || length / 2 >= output_size) return false;
    for (size_t index = 0; index < length; index += 2) {
        const int high = hex_value(input[index]);
        const int low = hex_value(input[index + 1]);
        if (high < 0 || low < 0) return false;
        output[index / 2] = (char)((high << 4) | low);
    }
    output[length / 2] = '\0';
    return true;
}

static void provisioning_task(void *argument)
{
    (void)argument;
    char line[PROVISION_LINE_MAX];
    setvbuf(stdin, NULL, _IONBF, 0);
    while (true) {
        if (fgets(line, sizeof(line), stdin) == NULL) {
            clearerr(stdin);
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        char *newline = strpbrk(line, "\r\n");
        if (newline != NULL) *newline = '\0';
        if (strcmp(line, "WFNET STATUS") == 0) {
            lock();
            const int64_t now = esp_timer_get_time();
            const uint64_t current_radio_ms = s_wifi_started
                                                  ? (uint64_t)((now - s_radio_started_us) / 1000)
                                                  : 0;
            printf("WFNET_ACK STATUS configured=%s encrypted=%s "
                   "state=%s radio=%s leases=%" PRIu32 " radio_ms=%" PRIu64
                   " total_radio_ms=%" PRIu64 " connect_result=%s sync_result=%s\n",
                   s_snapshot.credentials_present ? "yes" : "no",
                   s_snapshot.credential_store_encrypted ? "yes" : "no",
                   network_service_state_name(s_snapshot.state),
                   s_wifi_started ? "on" : "off", s_snapshot.lease_count,
                   current_radio_ms, s_snapshot.cumulative_radio_active_ms,
                   s_snapshot.last_connect_result, s_snapshot.last_sync_result);
            unlock();
            fflush(stdout);
            continue;
        }
        if (strcmp(line, "WFNET CONNECT") == 0) {
            network_service_request_manual_connect();
            printf("WFNET_ACK CONNECT result=REQUESTED\n");
            fflush(stdout);
            continue;
        }
        if (strcmp(line, "WFNET SYNC") == 0) {
            network_service_request_time_sync();
            printf("WFNET_ACK SYNC result=REQUESTED\n");
            fflush(stdout);
            continue;
        }
        if (strcmp(line, "WFNET OFF") == 0) {
            network_service_force_off("USB RADIO OFF");
            printf("WFNET_ACK OFF result=ESP_OK\n");
            fflush(stdout);
            continue;
        }
        if (strcmp(line, "CTIME AUTO") == 0) {
            const bool requested = time_sync_service_request_auto();
            printf("CTIME_ACK AUTO result=%s\n", requested ? "REQUESTED" : "BUSY");
            fflush(stdout);
            continue;
        }
        if (strcmp(line, "CTIME STATUS") == 0) {
            time_sync_snapshot_t sync;
            time_sync_service_get_snapshot(&sync);
            printf("CTIME_ACK STATUS state=%s active=%s method=%s status=%s "
                   "last_source=%s last_local=%s\n",
                   time_sync_state_name(sync.state), sync.active ? "yes" : "no",
                   sync.method, sync.status,
                   sync.last_source, sync.last_local[0] ? sync.last_local : "-");
            fflush(stdout);
            continue;
        }
        if (strcmp(line, "CTIME CANCEL") == 0) {
            time_sync_service_cancel("USB CANCELED");
            printf("CTIME_ACK CANCEL result=OK\n");
            fflush(stdout);
            continue;
        }
        if (strcmp(line, "WFNET CLEAR") == 0) {
            esp_err_t result = network_service_clear_configuration();
            printf("WFNET_ACK CLEAR result=%s\n", esp_err_to_name(result));
            fflush(stdout);
            continue;
        }
        if (strncmp(line, "WFNET SET ", 10) == 0) {
            char *ssid_hex = line + 10;
            char *password_hex = strchr(ssid_hex, ' ');
            char *timezone_hex = password_hex != NULL ? strchr(password_hex + 1, ' ') : NULL;
            if (password_hex == NULL || timezone_hex == NULL) {
                printf("WFNET_ACK SET result=INVALID_FORMAT\n");
                fflush(stdout);
                secure_zero(line, sizeof(line));
                continue;
            }
            *password_hex++ = '\0';
            *timezone_hex++ = '\0';
            char ssid[33], password[65], timezone[64];
            const bool password_ok = strcmp(password_hex, "-") == 0
                                         ? (password[0] = '\0', true)
                                         : decode_hex(password_hex, password,
                                                      sizeof(password));
            if (!decode_hex(ssid_hex, ssid, sizeof(ssid)) || !password_ok ||
                !decode_hex(timezone_hex, timezone, sizeof(timezone)) ||
                ssid[0] == '\0' || timezone[0] == '\0') {
                printf("WFNET_ACK SET result=INVALID_VALUE\n");
                fflush(stdout);
                secure_zero(password, sizeof(password));
                secure_zero(line, sizeof(line));
                continue;
            }
            esp_err_t result = network_service_save_configuration(ssid, password, timezone);
            secure_zero(password, sizeof(password));
            secure_zero(line, sizeof(line));
            printf("WFNET_ACK SET result=%s\n", esp_err_to_name(result));
            fflush(stdout);
        }
    }
    vTaskDelete(NULL);
}

static void service_task(void *argument)
{
    (void)argument;
    while (true) {
        const int64_t now = esp_timer_get_time();
        bool run_sync = false;
        bool run_scan = false;
        lock();
        if (s_scan_requested && !s_wifi_started) {
            s_scan_requested = false;
            s_snapshot.scan_in_progress = true;
            strlcpy(s_snapshot.last_scan_result, "SCANNING",
                    sizeof(s_snapshot.last_scan_result));
            run_scan = true;
        }
        if (!run_scan && s_start_requested && !s_wifi_started) {
            s_start_requested = false;
            esp_err_t result = start_radio();
            if (result != ESP_OK) {
                s_snapshot.lease_count = 0;
                s_manual_lease = false;
                s_sync_lease = false;
                s_sync_requested = false;
                set_state_locked(NETWORK_STATE_ERROR, esp_err_to_name(result));
                ESP_LOGE(TAG, "Radio start failed: %s", esp_err_to_name(result));
            }
        }
        if (s_snapshot.state == NETWORK_STATE_CONNECTING && s_wifi_started) {
            if ((now - s_connect_started_us) / 1000 >= CONNECT_TIMEOUT_MS ||
                s_connect_attempts >= MAX_CONNECT_ATTEMPTS) {
                ESP_LOGW(TAG, "Connect failed: attempts=%u duration_ms=%" PRId64,
                         s_connect_attempts, (now - s_connect_started_us) / 1000);
                set_state_locked(NETWORK_STATE_ERROR, "CONNECT TIMEOUT");
                if (s_sync_requested || s_sync_lease) {
                    s_snapshot.last_sync_succeeded = false;
                    strlcpy(s_snapshot.last_sync_result, "CONNECT FAILED",
                            sizeof(s_snapshot.last_sync_result));
                }
                s_snapshot.lease_count = 0;
                s_manual_lease = false;
                s_sync_lease = false;
                s_sync_requested = false;
                stop_radio_locked("CONNECT TIMEOUT");
                set_state_locked(NETWORK_STATE_ERROR, "CONNECT TIMEOUT");
            } else if (now >= s_next_retry_us) {
                s_connect_attempts++;
                s_next_retry_us = now + RETRY_INTERVAL_MS * 1000LL;
                ESP_LOGI(TAG, "Connect attempt %u/%u", s_connect_attempts,
                         MAX_CONNECT_ATTEMPTS);
                esp_err_t result = esp_wifi_connect();
                if (result != ESP_OK) {
                    strlcpy(s_snapshot.last_connect_result, esp_err_to_name(result),
                            sizeof(s_snapshot.last_connect_result));
                }
            }
        }
        if (s_snapshot.state == NETWORK_STATE_ONLINE && s_sync_requested &&
            !s_snapshot.sync_in_progress) {
            run_sync = true;
        }
        if (s_wifi_started && s_snapshot.lease_count == 0 &&
            s_idle_deadline_us != 0 && now >= s_idle_deadline_us) {
            stop_radio_locked("IDLE TIMEOUT");
            s_idle_deadline_us = 0;
        }
        unlock();
        if (run_scan) perform_wifi_scan();
        if (run_sync) perform_ntp_sync();
        vTaskDelay(pdMS_TO_TICKS(SERVICE_TICK_MS));
    }
}

esp_err_t network_service_init(bool nvs_ready)
{
    s_nvs_ready = nvs_ready;
    s_lock = xSemaphoreCreateMutex();
    if (s_lock == NULL) return ESP_ERR_NO_MEM;
    usb_serial_jtag_driver_config_t usb_config = {
        .tx_buffer_size = 2048,
        .rx_buffer_size = 1024,
    };
    esp_err_t usb_result = usb_serial_jtag_driver_install(&usb_config);
    if (usb_result == ESP_OK || usb_result == ESP_ERR_INVALID_STATE) {
        usb_serial_jtag_vfs_use_driver();
        usb_serial_jtag_vfs_set_rx_line_endings(ESP_LINE_ENDINGS_CRLF);
        usb_serial_jtag_vfs_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);
    } else {
        ESP_LOGW(TAG, "USB provisioning input unavailable: %s",
                 esp_err_to_name(usb_result));
    }
    strlcpy(s_snapshot.last_sync_result, "NEVER",
            sizeof(s_snapshot.last_sync_result));
    strlcpy(s_snapshot.last_sync_source, "NONE",
            sizeof(s_snapshot.last_sync_source));
    strlcpy(s_snapshot.last_scan_result, "NOT RUN",
            sizeof(s_snapshot.last_scan_result));
    s_snapshot.credential_store_ready = s_nvs_ready;
    s_snapshot.credential_store_encrypted = network_service_credentials_are_encrypted();
    if (s_snapshot.credential_store_encrypted) {
        esp_err_t result = load_configuration();
        if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG, "Encrypted network configuration unavailable: %s",
                     esp_err_to_name(result));
        }
    } else {
        strlcpy(s_snapshot.timezone, "NOT SET", sizeof(s_snapshot.timezone));
        strlcpy(s_snapshot.last_connect_result, "VOLATILE ONLY",
                sizeof(s_snapshot.last_connect_result));
        ESP_LOGI(TAG, "Persistent Wi-Fi credentials are disabled; provisioning is RAM-only");
    }
    if (xTaskCreate(service_task, "network_service", 6144, NULL, 5, NULL) != pdPASS ||
        xTaskCreate(provisioning_task, "wifi_provision", 6144, NULL, 3, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG,
             "Service ready: configured=%s policy=on-demand retries=%d timeout_ms=%d idle_ms=%d",
             s_snapshot.credentials_present ? "yes" : "no", MAX_CONNECT_ATTEMPTS,
             CONNECT_TIMEOUT_MS, IDLE_RADIO_TIMEOUT_MS);
    return ESP_OK;
}
