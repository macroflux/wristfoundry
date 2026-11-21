#include "settings_service.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"

static SemaphoreHandle_t s_mutex;
static wf_settings_snapshot_t s_state;

esp_err_t settings_service_init(bool storage_ready)
{
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) return ESP_ERR_NO_MEM;
    s_state = (wf_settings_snapshot_t){.values = wf_settings_defaults(),
        .revision = 1, .storage_ready = storage_ready};
    if (!storage_ready) return ESP_OK;
    nvs_handle_t handle;
    esp_err_t err = nvs_open("preferences", NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (err != ESP_OK) { s_state.storage_ready = false; return ESP_OK; }
    wf_settings_t loaded;
    size_t size = sizeof(loaded);
    err = nvs_get_blob(handle, "power_v1", &loaded, &size);
    nvs_close(handle);
    if (err == ESP_OK && size == sizeof(loaded) && wf_settings_valid(&loaded)) {
        s_state.values = loaded;
        s_state.saved = true;
    }
    /* Unknown/corrupt versions keep safe RAM defaults; never erase NVS. */
    return ESP_OK;
}

void settings_service_get(wf_settings_snapshot_t *out)
{
    if (!out) return;
    if (!s_mutex) {
        *out = (wf_settings_snapshot_t){.values = wf_settings_defaults(), .revision = 1};
        return;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *out = s_state;
    xSemaphoreGive(s_mutex);
}

esp_err_t settings_service_save(const wf_settings_t *values, uint32_t expected_revision)
{
    if (!wf_settings_valid(values)) return ESP_ERR_INVALID_ARG;
    if (!s_mutex) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    esp_err_t err = ESP_OK;
    if (expected_revision != s_state.revision) err = ESP_ERR_INVALID_STATE;
    else if (!s_state.storage_ready) err = ESP_ERR_NOT_SUPPORTED;
    else if (!s_state.saved || memcmp(values, &s_state.values, sizeof(*values))) {
        nvs_handle_t handle;
        err = nvs_open("preferences", NVS_READWRITE, &handle);
        if (err == ESP_OK) {
            err = nvs_set_blob(handle, "power_v1", values, sizeof(*values));
            if (err == ESP_OK) err = nvs_commit(handle);
            nvs_close(handle);
        }
        if (err == ESP_OK) {
            s_state.values = *values;
            s_state.saved = true;
            ++s_state.revision;
        }
    }
    xSemaphoreGive(s_mutex);
    return err;
}
