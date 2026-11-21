#pragma once

#include <stddef.h>
#include <stdint.h>

#include "lvgl.h"

typedef struct wf_app wf_app_t;

typedef void (*wf_app_lifecycle_fn)(wf_app_t *app);
typedef void (*wf_app_update_fn)(wf_app_t *app, const void *model);

struct wf_app {
    const char *id;
    const char *name;
    const char *icon;
    const char *description;
    wf_app_lifecycle_fn create;
    wf_app_lifecycle_fn show;
    wf_app_lifecycle_fn hide;
    wf_app_update_fn update;
    wf_app_lifecycle_fn destroy;
    lv_obj_t *screen;
    void *context;
    /* Portable builtin icon name, not an LVGL glyph. Optional for old apps. */
    const char *icon_name;
};

typedef struct {
    uint32_t rounds;
    uint32_t objects_before;
    uint32_t objects_after;
    size_t heap_before;
    size_t heap_after;
    bool passed;
} wf_app_stress_result_t;

void wf_app_manager_init(lv_obj_t *watch_screen);
bool wf_app_manager_register(wf_app_t *app);
void wf_app_manager_finish_registration(void);
void wf_app_manager_show_watch(void);
void wf_app_manager_show_launcher(void);
void wf_app_manager_open(wf_app_t *app);
void wf_app_manager_global_button(void);
void wf_app_manager_update(const void *model);
const char *wf_app_manager_view_name(void);
size_t wf_app_manager_count(void);
wf_app_t *wf_app_manager_at(size_t index);
wf_app_stress_result_t wf_app_manager_stress_test(uint32_t rounds);
