#pragma once
#include "esp_err.h"

/* Single USB input owner. Never called from the LVGL task. */
esp_err_t companion_service_init(void);
/* Called by the existing hardware-health owner, under the display lock. */
void companion_service_ui_tick(void);
