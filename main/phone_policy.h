#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#define WF_PHONE_DISCOVERY_MS 60000LL
#define WF_PHONE_IDLE_MS 300000LL
#define WF_PHONE_HANDSHAKE_MS 45000LL
/* Only explicit user actions extend a session; polling/notifications do not. */
static inline bool wf_phone_expired(int64_t now, int64_t since, bool active)
{
    return now - since >= (active ? WF_PHONE_IDLE_MS : WF_PHONE_HANDSHAKE_MS);
}
static inline bool wf_phone_command_activity(const char *cmd)
{
    return strcmp(cmd, "hello") && strcmp(cmd, "device.get") &&
        strcmp(cmd, "settings.get") && strcmp(cmd, "phone.get") &&
        strcmp(cmd, "apps.list") && strcmp(cmd, "diagnostics.get");
}
