#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef enum {
    TIME_SYNC_IDLE,
    TIME_SYNC_WIFI,
    TIME_SYNC_SUCCESS,
    TIME_SYNC_FAILED,
    TIME_SYNC_CANCELED,
} time_sync_state_t;

typedef struct {
    time_sync_state_t state;
    bool active;
    bool wifi_attempted;
    uint32_t generation;
    int64_t last_success_epoch;
    int32_t last_drift_seconds;
    char method[24];
    char status[48];
    char detail[72];
    char last_source[20];
    char last_local[24];
} time_sync_snapshot_t;

esp_err_t time_sync_service_init(bool persist_history);
bool time_sync_service_request_auto(void);
esp_err_t time_sync_service_set_utc(int64_t epoch, const char *source);
void time_sync_service_cancel(const char *reason);
void time_sync_service_get_snapshot(time_sync_snapshot_t *snapshot);
bool time_sync_service_can_sleep(void);
const char *time_sync_state_name(time_sync_state_t state);
