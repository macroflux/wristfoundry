#pragma once
#include <stdbool.h>
#include <stdint.h>

#define WF_SETTINGS_VERSION 1
#define WF_RADIO_GRACE_SECONDS 30
typedef struct {
    uint32_t version;
    uint32_t dim_seconds;
    uint32_t off_seconds;
    uint32_t usb_display; /* 0: follow timeout, 1: stay on */
    uint32_t radio_finish; /* 0: stop, 1: finish bounded work */
} wf_settings_t;

static inline wf_settings_t wf_settings_defaults(void)
{
    return (wf_settings_t){WF_SETTINGS_VERSION, 10, 25, 1, 0};
}
static inline bool wf_settings_valid(const wf_settings_t *s)
{
    return s && s->version == WF_SETTINGS_VERSION &&
        s->dim_seconds >= 5 && s->dim_seconds <= 300 &&
        s->off_seconds >= 10 && s->off_seconds <= 600 &&
        s->off_seconds > s->dim_seconds &&
        s->usb_display <= 1 && s->radio_finish <= 1;
}

static inline bool wf_settings_stop_radios(bool finish_bounded, int64_t now,
                                          int64_t deadline, bool work_pending)
{
    return !finish_bounded || !work_pending || now >= deadline;
}
