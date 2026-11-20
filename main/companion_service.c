#include "companion_service.h"
#include "companion_frame.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "cJSON.h"
#include "app_manager.h"
#include "board_services.h"
#include "network_service.h"
#include "time_sync_service.h"
#include "driver/usb_serial_jtag.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static SemaphoreHandle_t s_tx;
static SemaphoreHandle_t s_ui;
static wf_frame_t s_frame;
static char s_output[WF_RESPONSE_MAX];
static bool s_identified;
static int64_t s_last_contact;
static unsigned s_sequence;
static char s_ui_action[48];
static bool s_ui_pending;
static cJSON *s_ui_apps;

static void wipe(void *data, size_t n)
{
    volatile unsigned char *p = data;
    while (n--) *p++ = 0;
}

/* Never echo request text, including malformed/unknown commands. */
static void wipe_json(cJSON *item)
{
    if (!item) return;
    if (item->valuestring) wipe(item->valuestring, strlen(item->valuestring));
    cJSON *child;
    cJSON_ArrayForEach(child, item) wipe_json(child);
}

/* Console and protocol share one bounded, serialized TX path. Logs cannot
 * contain RS, CR or embedded LF and therefore cannot inject protocol frames. */
static int log_output(const char *format, va_list args)
{
    char line[512];
    int n = vsnprintf(line, sizeof(line), format, args);
    for (size_t i = 0; line[i]; ++i)
        if ((unsigned char)line[i] < 32) line[i] = ' ';
    if (xSemaphoreTake(s_tx, pdMS_TO_TICKS(20)) == pdTRUE) {
        usb_serial_jtag_write_bytes("# ", 2, 0);
        usb_serial_jtag_write_bytes(line, strlen(line), 0);
        usb_serial_jtag_write_bytes("\n", 1, 0);
        xSemaphoreGive(s_tx);
    }
    return n;
}

static void emit(cJSON *message)
{
    if (!message) return;
    cJSON_AddNumberToObject(message, "v", 1);
    if (cJSON_PrintPreallocated(message, s_output + 5,
                               sizeof(s_output) - 6, false)) {
        memcpy(s_output, WF_PREFIX, 5);
        size_t n = strlen(s_output);
        s_output[n++] = '\n';
        if (xSemaphoreTake(s_tx, pdMS_TO_TICKS(100)) == pdTRUE) {
            /* Partial TX is allowed to fail as a truncated frame; clients time
             * out and never acknowledge a partial or corrupt response. */
            usb_serial_jtag_write_bytes(s_output, n, pdMS_TO_TICKS(100));
            xSemaphoreGive(s_tx);
        }
    }
    cJSON_Delete(message);
}

static void error_reply(const char *id, const char *code)
{
    cJSON *r = cJSON_CreateObject();
    cJSON_AddStringToObject(r, "id", id ? id : "");
    cJSON *e = cJSON_AddObjectToObject(r, "error");
    cJSON_AddStringToObject(e, "code", code);
    emit(r);
}

static const char *string(cJSON *o, const char *key, size_t max)
{
    cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
    return cJSON_IsString(v) && strlen(v->valuestring) <= max ? v->valuestring : NULL;
}

/* Bound cJSON recursion before allocation/parsing. Raw controls and embedded
 * NUL are not valid protocol content; escaped JSON controls remain supported. */
static bool shallow_json(const char *text)
{
    unsigned depth = 0;
    bool quoted = false, escaped = false;
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        if (*p < 32) return false;
        if (escaped) { escaped = false; continue; }
        if (quoted && *p == '\\') { escaped = true; continue; }
        if (*p == '"') { quoted = !quoted; continue; }
        if (!quoted && (*p == '{' || *p == '[')) { if (++depth > 8) return false; }
        if (!quoted && (*p == '}' || *p == ']')) { if (!depth) return false; --depth; }
    }
    return !quoted && !depth;
}

static bool unique_keys(cJSON *o)
{
    cJSON *a;
    cJSON_ArrayForEach(a, o) {
        for (cJSON *b = a->next; b; b = b->next)
            if (a->string && b->string && !strcmp(a->string, b->string)) return false;
        if ((cJSON_IsObject(a) || cJSON_IsArray(a)) && !unique_keys(a)) return false;
    }
    return true;
}

static cJSON *snapshot(void)
{
    network_snapshot_t n;
    time_sync_snapshot_t t;
    board_health_t h;
    network_service_get_snapshot(&n);
    time_sync_service_get_snapshot(&t);
    board_services_read(&h);
    cJSON *r = cJSON_CreateObject();
    cJSON_AddNumberToObject(r, "sequence", ++s_sequence);
    cJSON_AddNumberToObject(r, "uptime_ms", (double)(esp_timer_get_time() / 1000));
    if (h.pmu_online && h.battery_present && h.battery_percent >= 0)
        cJSON_AddNumberToObject(r, "battery_percent", h.battery_percent);
    else cJSON_AddNullToObject(r, "battery_percent");
    if (h.pmu_online) cJSON_AddBoolToObject(r, "charging", h.charging);
    else cJSON_AddNullToObject(r, "charging");
    cJSON_AddBoolToObject(r, "rtc_online", h.rtc_online);
    cJSON_AddBoolToObject(r, "rtc_valid", h.rtc_valid);
    char rtc[24] = "";
    if (h.rtc_online && h.rtc_valid)
        snprintf(rtc, sizeof(rtc), "%04u-%02u-%02u %02u:%02u:%02u",
                 h.year, h.month, h.day, h.hour, h.minute, h.second);
    cJSON_AddStringToObject(r, "rtc_local", rtc);
    cJSON_AddStringToObject(r, "timezone", n.timezone);
    cJSON_AddStringToObject(r, "wifi_state", network_service_state_name(n.state));
    cJSON_AddStringToObject(r, "wifi_result", n.last_connect_result);
    cJSON_AddBoolToObject(r, "configured", n.credentials_present);
    cJSON_AddStringToObject(r, "persistence", n.credential_store_encrypted ? "encrypted_nvs" : "ram_only");
    cJSON_AddStringToObject(r, "ssid", n.ssid);
    cJSON_AddStringToObject(r, "ip", n.ip);
    cJSON_AddBoolToObject(r, "scan_busy", n.scan_in_progress);
    cJSON_AddNumberToObject(r, "scan_generation", n.scan_generation);
    cJSON_AddStringToObject(r, "scan_result", n.last_scan_result);
    cJSON *scan = cJSON_AddArrayToObject(r, "networks");
    for (size_t i = 0; i < n.scan_count; ++i) {
        cJSON *ap = cJSON_CreateObject();
        cJSON_AddStringToObject(ap, "ssid", n.scan_results[i].ssid);
        cJSON_AddNumberToObject(ap, "rssi", n.scan_results[i].rssi);
        cJSON_AddBoolToObject(ap, "secured", n.scan_results[i].secured);
        cJSON_AddItemToArray(scan, ap);
    }
    cJSON_AddBoolToObject(r, "sync_busy", t.active);
    cJSON_AddStringToObject(r, "sync_state", time_sync_state_name(t.state));
    cJSON_AddStringToObject(r, "sync_status", t.status);
    cJSON_AddStringToObject(r, "sync_detail", t.detail);
    cJSON_AddStringToObject(r, "time_source", t.last_source);
    cJSON_AddNumberToObject(r, "last_sync_epoch", (double)t.last_success_epoch);
    cJSON_AddNumberToObject(r, "free_heap", esp_get_free_heap_size());
    return r;
}

void companion_service_ui_tick(void)
{
    if (!s_ui || xSemaphoreTake(s_ui, 0) != pdTRUE) return;
    if (!s_ui_apps) {
        s_ui_apps = cJSON_CreateArray();
        for (size_t i = 0; i < wf_app_manager_count(); ++i) {
            wf_app_t *a = wf_app_manager_at(i);
            cJSON *o = cJSON_CreateObject();
            cJSON_AddStringToObject(o, "id", a->id);
            cJSON_AddStringToObject(o, "name", a->name);
            cJSON_AddStringToObject(o, "description", a->description);
            cJSON_AddItemToArray(s_ui_apps, o);
        }
    }
    if (s_ui_pending) {
        if (!strcmp(s_ui_action, "@home")) wf_app_manager_show_watch();
        else for (size_t i = 0; i < wf_app_manager_count(); ++i) {
            wf_app_t *a = wf_app_manager_at(i);
            if (!strcmp(a->id, s_ui_action)) { wf_app_manager_open(a); break; }
        }
        s_ui_pending = false;
    }
    xSemaphoreGive(s_ui);
}

static cJSON *hello(void)
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char identity[24];
    snprintf(identity, sizeof(identity), "wf-%02x%02x%02x%02x%02x%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    cJSON *r = cJSON_CreateObject();
    cJSON_AddStringToObject(r, "product", "WristFoundry");
    cJSON_AddStringToObject(r, "device_id", identity);
    cJSON_AddStringToObject(r, "model", "Waveshare ESP32-S3 AMOLED 2.06");
    cJSON_AddStringToObject(r, "firmware", esp_app_get_description()->version);
    const char *caps[] = {"device.get", "wifi.scan", "wifi.configure", "wifi.connect",
        "wifi.disconnect", "time.set", "timezone.set", "ntp.sync", "ntp.cancel",
        "settings.get", "apps.list", "apps.open", "apps.home", "diagnostics.get"};
    cJSON_AddItemToObject(r, "capabilities", cJSON_CreateStringArray(caps, sizeof(caps)/sizeof(caps[0])));
    cJSON_AddBoolToObject(r, "app_packages", false);
    cJSON_AddNumberToObject(r, "request_max", WF_REQUEST_MAX);
    return r;
}

static void dispatch(char *line)
{
    if (strncmp(line, WF_PREFIX, 5)) return;
    if (!shallow_json(line + 5)) { error_reply(NULL, "malformed"); return; }
    const char *end = NULL;
    cJSON *q = cJSON_ParseWithOpts(line + 5, &end, true);
    if (!q || !cJSON_IsObject(q) || !unique_keys(q)) {
        wipe_json(q); cJSON_Delete(q); error_reply(NULL, "malformed"); return;
    }
    const char *id = string(q, "id", 48);
    const char *cmd = string(q, "cmd", 40);
    cJSON *v = cJSON_GetObjectItemCaseSensitive(q, "v");
    cJSON *p = cJSON_GetObjectItemCaseSensitive(q, "params");
    const char *error = NULL;
    cJSON *result = NULL;
    if (!id || !id[0] || !cmd || !cJSON_IsObject(p)) error = "invalid_argument";
    else if (!cJSON_IsNumber(v) || v->valuedouble != 1) error = "unsupported_version";
    else if (!strcmp(cmd, "hello")) { result = hello(); s_identified = true; }
    else if (!s_identified) error = "handshake_required";
    else if (!strcmp(cmd, "device.get") || !strcmp(cmd, "diagnostics.get")) result = snapshot();
    else if (!strcmp(cmd, "settings.get")) {
        result = cJSON_CreateObject();
        cJSON_AddStringToObject(result, "policy", "10s dim / 25s off; radios stop at display off");
        cJSON_AddBoolToObject(result, "brightness_set", false);
        cJSON_AddBoolToObject(result, "sleep_set", false);
    } else if (!strcmp(cmd, "wifi.scan")) {
        if (!network_service_request_scan()) error = "busy";
    } else if (!strcmp(cmd, "wifi.connect")) {
        network_snapshot_t n;
        network_service_get_snapshot(&n);
        if (!n.credentials_present) error = "not_configured";
        else network_service_request_manual_connect();
    } else if (!strcmp(cmd, "wifi.disconnect")) network_service_release_manual_connect();
    else if (!strcmp(cmd, "ntp.sync")) {
        if (!time_sync_service_request_auto()) error = "busy";
    } else if (!strcmp(cmd, "ntp.cancel")) time_sync_service_cancel("COMPANION");
    else if (!strcmp(cmd, "wifi.configure")) {
        const char *ssid = string(p, "ssid", 32);
        const char *password = string(p, "password", 64);
        network_snapshot_t n;
        network_service_get_snapshot(&n);
        esp_err_t e = ssid && password ? network_service_save_configuration(ssid, password,
            !strcmp(n.timezone, "NOT SET") ? "UTC0" : n.timezone) : ESP_ERR_INVALID_ARG;
        if (e != ESP_OK) error = e == ESP_ERR_INVALID_ARG ? "invalid_argument" : "storage_unavailable";
    } else if (!strcmp(cmd, "timezone.set")) {
        const char *zone = string(p, "timezone", 63);
        esp_err_t e = zone ? network_service_set_timezone(zone) : ESP_ERR_INVALID_ARG;
        if (e != ESP_OK) error = e == ESP_ERR_INVALID_ARG ? "invalid_argument" : e == ESP_ERR_INVALID_STATE ? "busy" : "rtc_or_storage_failed";
    } else if (!strcmp(cmd, "time.set")) {
        cJSON *epoch = cJSON_GetObjectItemCaseSensitive(p, "epoch_utc");
        esp_err_t e = cJSON_IsNumber(epoch) && epoch->valuedouble >= 1704067200.0 &&
            epoch->valuedouble <= 4102444799.0 && epoch->valuedouble == (double)(int64_t)epoch->valuedouble
                ? time_sync_service_set_utc((int64_t)epoch->valuedouble, "USB COMPANION") : ESP_ERR_INVALID_ARG;
        if (e != ESP_OK) error = e == ESP_ERR_INVALID_ARG ? "invalid_argument" : e == ESP_ERR_INVALID_STATE ? "busy" : "rtc_write_failed";
    } else if (!strcmp(cmd, "apps.list")) {
        xSemaphoreTake(s_ui, portMAX_DELAY);
        if (s_ui_apps) result = cJSON_Duplicate(s_ui_apps, true); else error = "busy";
        xSemaphoreGive(s_ui);
    } else if (!strcmp(cmd, "apps.open") || !strcmp(cmd, "apps.home")) {
        const char *app = !strcmp(cmd, "apps.home") ? "@home" : string(p, "app_id", 47);
        bool found = app && !strcmp(app, "@home");
        xSemaphoreTake(s_ui, portMAX_DELAY);
        cJSON *a;
        cJSON_ArrayForEach(a, s_ui_apps) {
            const char *aid = string(a, "id", 47);
            if (app && aid && !strcmp(aid, app)) found = true;
        }
        if (!found) error = "invalid_argument";
        else if (s_ui_pending) error = "busy";
        else { strlcpy(s_ui_action, app, sizeof(s_ui_action)); s_ui_pending = true; }
        xSemaphoreGive(s_ui);
        if (!error) {
            int64_t deadline = esp_timer_get_time() + 2000000;
            for (;;) {
                vTaskDelay(pdMS_TO_TICKS(20));
                xSemaphoreTake(s_ui, portMAX_DELAY);
                bool pending = s_ui_pending;
                if (esp_timer_get_time() >= deadline && pending) { s_ui_pending = false; error = "ui_unavailable"; }
                xSemaphoreGive(s_ui);
                if (!pending || error) break;
            }
        }
    } else error = "unsupported_command";
    if (error) { cJSON_Delete(result); error_reply(id, error); }
    else {
        cJSON *r = cJSON_CreateObject();
        cJSON_AddStringToObject(r, "id", id);
        if (!result) { result = cJSON_CreateObject(); cJSON_AddStringToObject(result, "status", "accepted"); }
        cJSON_AddItemToObject(r, "result", result);
        emit(r);
        s_last_contact = esp_timer_get_time();
    }
    wipe_json(q);
    cJSON_Delete(q);
}

static void service_task(void *arg)
{
    (void)arg;
    unsigned char bytes[128];
    int64_t next_event = 0;
    for (;;) {
        bool connected = usb_serial_jtag_is_connected();
        if (!connected || (s_identified && esp_timer_get_time() - s_last_contact > 15000000)) {
            if (s_identified) network_service_release_manual_connect();
            s_identified = false;
            wipe(&s_frame, sizeof(s_frame));
        }
        int count = usb_serial_jtag_read_bytes(bytes, sizeof(bytes), pdMS_TO_TICKS(50));
        for (int i = 0; i < count; ++i) {
            int state = wf_frame_push(&s_frame, bytes[i]);
            if (state == 1) dispatch(s_frame.data);
            if (state) wipe(s_frame.data, sizeof(s_frame.data));
            if (state == -1) error_reply(NULL, "frame_too_large");
        }
        wipe(bytes, sizeof(bytes));
        if (s_identified && esp_timer_get_time() >= next_event) {
            cJSON *e = cJSON_CreateObject();
            cJSON_AddStringToObject(e, "event", "device.status");
            cJSON_AddItemToObject(e, "data", snapshot());
            emit(e);
            next_event = esp_timer_get_time() + 2000000;
        }
    }
}

esp_err_t companion_service_init(void)
{
    s_tx = xSemaphoreCreateMutex();
    s_ui = xSemaphoreCreateMutex();
    if (!s_tx || !s_ui) return ESP_ERR_NO_MEM;
    const usb_serial_jtag_driver_config_t config = {.tx_buffer_size = 8192, .rx_buffer_size = 2048};
    usb_serial_jtag_driver_config_t mutable_config = config;
    esp_err_t e = usb_serial_jtag_driver_install(&mutable_config);
    if (e != ESP_OK) return e;
    esp_log_set_vprintf(log_output);
    return xTaskCreate(service_task, "companion", 8192, NULL, 3, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
