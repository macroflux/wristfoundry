#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "board_services.h"
#include "ble_radar_service.h"
#include "esp_sleep.h"
#include "network_service.h"
#include "time_sync_service.h"

typedef struct {
    board_health_t health;
    const char *display_state;
    esp_sleep_wakeup_cause_t wake_cause;
    uint32_t light_sleep_count;
    int64_t last_light_sleep_us;
    network_snapshot_t network;
    const ble_radar_snapshot_t *ble;
    time_sync_snapshot_t sync;
} watch_ui_model_t;

void watch_ui_create(void);
void watch_ui_note_activity(void);
int64_t watch_ui_last_activity_us(void);
const char *watch_ui_view_name(void);
void watch_ui_handle_home_button(void);

void watch_ui_update(const board_health_t *health, const char *display_state,
                     esp_sleep_wakeup_cause_t wake_cause,
                     uint32_t light_sleep_count, int64_t last_light_sleep_us,
                     const network_snapshot_t *network,
                     const ble_radar_snapshot_t *ble,
                     const time_sync_snapshot_t *sync);
