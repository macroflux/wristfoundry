#pragma once
#include "esp_err.h"
#include "settings_policy.h"

typedef struct {
    wf_settings_t values;
    uint32_t revision;
    bool storage_ready;
    bool saved;
} wf_settings_snapshot_t;

esp_err_t settings_service_init(bool storage_ready);
void settings_service_get(wf_settings_snapshot_t *out);
/* expected_revision protects edits from the other interface. No RAM change on
 * failed commit; all values are stored together in one versioned NVS blob. */
esp_err_t settings_service_save(const wf_settings_t *values, uint32_t expected_revision);
