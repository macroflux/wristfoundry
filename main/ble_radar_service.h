#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

enum {
    BLE_RADAR_MAX_DEVICES = 24,
    BLE_RADAR_NAME_MAX = 31,
    BLE_RADAR_UUID16_MAX = 4,
    BLE_RADAR_SCAN_TIMEOUT_SECONDS = 30,
};

typedef enum {
    BLE_RADAR_OFF,
    BLE_RADAR_STARTING,
    BLE_RADAR_READY,
    BLE_RADAR_SCANNING,
    BLE_RADAR_STOPPING,
    BLE_RADAR_ERROR,
} ble_radar_state_t;

typedef struct {
    uint8_t address[6];
    uint8_t address_type;
    char address_text[18];
    char name[BLE_RADAR_NAME_MAX + 1];
    int8_t rssi;
    int16_t rssi_average;
    int8_t tx_power;
    bool tx_power_present;
    uint8_t event_type;
    uint16_t manufacturer_id;
    uint8_t manufacturer_data_length;
    bool manufacturer_present;
    uint16_t service_uuid16[BLE_RADAR_UUID16_MAX];
    uint8_t service_uuid16_count;
    uint32_t seen_count;
    int64_t first_seen_ms;
    int64_t last_seen_ms;
    bool stale;
} ble_radar_device_t;

typedef struct {
    ble_radar_state_t state;
    bool host_ready;
    bool controller_ready;
    bool scanning;
    uint8_t device_count;
    uint32_t advertisement_count;
    uint32_t dropped_count;
    uint32_t parse_error_count;
    uint32_t generation;
    uint32_t seconds_remaining;
    int last_error;
    char last_result[48];
    size_t internal_free_bytes;
    size_t internal_largest_bytes;
    ble_radar_device_t devices[BLE_RADAR_MAX_DEVICES];
} ble_radar_snapshot_t;

esp_err_t ble_radar_service_init(void);
bool ble_radar_service_start(void);
void ble_radar_service_stop(const char *reason);
void ble_radar_service_clear(void);
void ble_radar_service_get_snapshot(ble_radar_snapshot_t *snapshot);
bool ble_radar_service_is_active(void);
bool ble_radar_service_can_sleep(void);
const char *ble_radar_state_name(ble_radar_state_t state);
const char *ble_radar_address_type_name(uint8_t type);
const char *ble_radar_event_type_name(uint8_t type);
