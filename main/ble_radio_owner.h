#pragma once

#include <stdbool.h>

#include "esp_err.h"

typedef enum {
    BLE_RADIO_OWNER_NONE,
    BLE_RADIO_OWNER_RADAR,
} ble_radio_owner_t;

esp_err_t ble_radio_owner_init(void);
bool ble_radio_owner_claim(ble_radio_owner_t owner);
void ble_radio_owner_release(ble_radio_owner_t owner);
ble_radio_owner_t ble_radio_owner_current(void);
const char *ble_radio_owner_name(ble_radio_owner_t owner);
