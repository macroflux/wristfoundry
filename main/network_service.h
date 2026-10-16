#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

typedef enum {
    NETWORK_STATE_OFF,
    NETWORK_STATE_CONNECTING,
    NETWORK_STATE_ONLINE,
    NETWORK_STATE_ERROR,
} network_state_t;

typedef struct {
    network_state_t state;
    bool credentials_present;
    bool radio_active;
    bool sync_in_progress;
    uint32_t lease_count;
    char ssid[33];
    char ip[16];
    char timezone[64];
    int8_t rssi;
    char last_connect_result[32];
    uint32_t current_connection_ms;
    uint64_t current_radio_active_ms;
    uint64_t cumulative_radio_active_ms;
    bool last_sync_succeeded;
    uint32_t sync_generation;
    int64_t last_sync_epoch;
    char last_sync_source[16];
    char last_sync_local[24];
    char last_sync_result[32];
} network_snapshot_t;

esp_err_t network_service_init(void);
esp_err_t network_service_save_configuration(const char *ssid,
                                              const char *password,
                                              const char *timezone);
esp_err_t network_service_clear_configuration(void);
bool network_service_credentials_are_encrypted(void);
esp_err_t network_service_acquire(const char *owner);
void network_service_release(const char *owner);
void network_service_force_off(const char *reason);
void network_service_request_manual_connect(void);
void network_service_release_manual_connect(void);
bool network_service_request_time_sync(void);
void network_service_cancel_time_sync(void);
void network_service_get_snapshot(network_snapshot_t *snapshot);
const char *network_service_state_name(network_state_t state);
