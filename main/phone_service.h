#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
typedef struct {
    bool advertising, connected, authenticated, active, busy;
    uint32_t seconds_remaining, passkey;
    int error;
} wf_phone_snapshot_t;
esp_err_t phone_service_init(void);
bool phone_service_discover(void);
void phone_service_stop(void);
void phone_service_get(wf_phone_snapshot_t *out);
bool phone_service_blocks_sleep(void);
bool phone_service_active(void);
void phone_service_activity(bool handshake);
int phone_service_read(uint8_t *data, size_t size);
bool phone_service_send(const void *data, size_t size);
uint32_t phone_service_generation(void);
