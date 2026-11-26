#pragma once
#include <stdbool.h>
#include <string.h>

/* USB must not override the display-off radio and navigation policy. */
static inline bool wf_command_needs_awake(const char *command)
{
    return !strcmp(command, "wifi.scan") || !strcmp(command, "wifi.connect") ||
           !strcmp(command, "ntp.sync") || !strcmp(command, "apps.open") ||
           !strcmp(command, "apps.home") || !strcmp(command, "phone.discover");
}
