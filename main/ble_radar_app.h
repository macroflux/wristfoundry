#pragma once

#include "app_manager.h"

void ble_radar_app_create(wf_app_t *app);
void ble_radar_app_show(wf_app_t *app);
void ble_radar_app_hide(wf_app_t *app);
void ble_radar_app_update(wf_app_t *app, const void *model);
void ble_radar_app_destroy(wf_app_t *app);
