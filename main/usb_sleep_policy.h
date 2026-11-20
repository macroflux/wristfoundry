#pragma once
#include <stdbool.h>

/* VBUS protects enumeration too, before the host starts sending SOF packets.
 * An unreadable PMU fails awake; a confirmed unplug permits battery sleep. */
static inline bool wf_usb_blocks_sleep(bool vbus_read_ok, bool vbus_present)
{
    return !vbus_read_ok || vbus_present;
}
