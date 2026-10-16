#include "ble_radio_owner.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static SemaphoreHandle_t s_lock;
static ble_radio_owner_t s_owner;

esp_err_t ble_radio_owner_init(void)
{
    if (s_lock != NULL) return ESP_OK;
    s_lock = xSemaphoreCreateMutex();
    return s_lock != NULL ? ESP_OK : ESP_ERR_NO_MEM;
}

bool ble_radio_owner_claim(ble_radio_owner_t owner)
{
    if (s_lock == NULL || owner == BLE_RADIO_OWNER_NONE) return false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    const bool claimed = s_owner == BLE_RADIO_OWNER_NONE || s_owner == owner;
    if (claimed) s_owner = owner;
    xSemaphoreGive(s_lock);
    return claimed;
}

void ble_radio_owner_release(ble_radio_owner_t owner)
{
    if (s_lock == NULL) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_owner == owner) s_owner = BLE_RADIO_OWNER_NONE;
    xSemaphoreGive(s_lock);
}

ble_radio_owner_t ble_radio_owner_current(void)
{
    if (s_lock == NULL) return BLE_RADIO_OWNER_NONE;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    const ble_radio_owner_t owner = s_owner;
    xSemaphoreGive(s_lock);
    return owner;
}

const char *ble_radio_owner_name(ble_radio_owner_t owner)
{
    switch (owner) {
    case BLE_RADIO_OWNER_RADAR: return "RADAR";
    case BLE_RADIO_OWNER_NONE:
    default: return "NONE";
    }
}
