/* Exercise production persistence service with an in-memory NVS fault model.
 * This does not establish physical flash/power-loss behavior or locking. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../main/settings_service.h"
#include "nvs.h"

static wf_settings_t stored, staged;
static bool present, fail_commit;
static unsigned commits;
esp_err_t nvs_open(const char *name, int mode, nvs_handle_t *h)
{ assert(!strcmp(name, "preferences")); (void)mode; *h = 1; return ESP_OK; }
esp_err_t nvs_get_blob(nvs_handle_t h, const char *key, void *out, size_t *size)
{
    (void)h; assert(!strcmp(key, "power_v1"));
    if (!present) return ESP_ERR_NVS_NOT_FOUND;
    assert(*size >= sizeof(stored)); memcpy(out, &stored, sizeof(stored));
    *size = sizeof(stored); return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h, const char *key, const void *value, size_t size)
{ (void)h; assert(!strcmp(key, "power_v1")); assert(size == sizeof(staged)); memcpy(&staged, value, size); return ESP_OK; }
esp_err_t nvs_commit(nvs_handle_t h)
{ (void)h; ++commits; if (fail_commit) return ESP_FAIL; stored = staged; present = true; return ESP_OK; }
void nvs_close(nvs_handle_t h) { (void)h; }

int main(void)
{
    assert(settings_service_init(true) == ESP_OK);
    wf_settings_snapshot_t s;
    settings_service_get(&s);
    assert(s.values.dim_seconds == 10 && !s.saved);
    wf_settings_t edit = s.values;
    edit.dim_seconds = 20; edit.off_seconds = 50;
    assert(settings_service_save(&edit, s.revision) == ESP_OK);
    settings_service_get(&s);
    assert(s.saved && s.revision == 2 && commits == 1);
    assert(settings_service_save(&edit, s.revision) == ESP_OK && commits == 1);
    assert(settings_service_save(&edit, 1) == ESP_ERR_INVALID_STATE);
    edit.dim_seconds = edit.off_seconds;
    assert(settings_service_save(&edit, s.revision) == ESP_ERR_INVALID_ARG);
    edit.dim_seconds = 30;
    fail_commit = true;
    assert(settings_service_save(&edit, s.revision) == ESP_FAIL);
    settings_service_get(&s);
    assert(s.revision == 2 && s.values.dim_seconds == 20);
    fail_commit = false;
    assert(settings_service_init(true) == ESP_OK); /* simulated reboot */
    settings_service_get(&s);
    assert(s.saved && s.values.dim_seconds == 20 && s.values.off_seconds == 50);
    stored.version = 999;
    assert(settings_service_init(true) == ESP_OK);
    settings_service_get(&s);
    assert(s.values.version == 1 && s.values.dim_seconds == 10 && !s.saved);
    assert(settings_service_init(false) == ESP_OK);
    settings_service_get(&s);
    edit = wf_settings_defaults();
    assert(settings_service_save(&edit, s.revision) == ESP_ERR_NOT_SUPPORTED);
    puts("settings persistence/failure/conflict tests passed");
    return 0;
}
