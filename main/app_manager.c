#include "app_manager.h"

#include <inttypes.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "ui_design.h"

#define WF_MAX_APPS 10

typedef enum {
    WF_ROUTE_WATCH,
    WF_ROUTE_LAUNCHER,
    WF_ROUTE_APP,
} wf_route_t;

static const char *TAG = "app_manager";
static wf_app_t *s_apps[WF_MAX_APPS];
static size_t s_app_count;
static lv_obj_t *s_watch_screen;
static lv_obj_t *s_launcher_screen;
static lv_obj_t *s_launcher_grid;
static wf_app_t *s_current_app;
static wf_route_t s_route = WF_ROUTE_WATCH;

static void launcher_home_event(lv_event_t *event)
{
    (void)event;
    wf_ui_report_activity();
    wf_app_manager_show_watch();
}

static void launcher_app_event(lv_event_t *event)
{
    wf_app_t *app = lv_event_get_user_data(event);
    wf_ui_report_activity();
    wf_app_manager_open(app);
}

static void load_screen(lv_obj_t *screen)
{
    if (screen == NULL) {
        return;
    }
    lv_screen_load(screen);
    lv_obj_invalidate(screen);
}

static void create_launcher(void)
{
    s_launcher_screen = wf_ui_screen_create();
    wf_ui_header_create(s_launcher_screen, "WF", "APPS", "WATCH",
                        launcher_home_event, NULL);

    s_launcher_grid = lv_obj_create(s_launcher_screen);
    lv_obj_remove_style_all(s_launcher_grid);
    lv_obj_set_size(s_launcher_grid, WF_DISPLAY_WIDTH, WF_DISPLAY_HEIGHT - WF_HEADER_HEIGHT);
    lv_obj_align(s_launcher_grid, LV_ALIGN_BOTTOM_MID, 0, 0);
    wf_ui_make_scrollable(s_launcher_grid);
    lv_obj_set_flex_flow(s_launcher_grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(s_launcher_grid, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(s_launcher_grid, WF_SPACE_SM, 0);

    for (size_t i = 0; i < s_app_count; ++i) {
        wf_app_t *app = s_apps[i];
        lv_obj_t *tile = wf_ui_card_create(s_launcher_grid);
        lv_obj_set_size(tile, 174, 118);
        lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(tile, launcher_app_event, LV_EVENT_CLICKED, app);

        lv_obj_t *icon = wf_ui_label_create(tile, app->icon, &lv_font_montserrat_20,
                                             WF_COLOR_PRIMARY);
        lv_obj_align(icon, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_t *name = wf_ui_label_create(tile, app->name, &lv_font_montserrat_14,
                                             WF_COLOR_TEXT);
        lv_obj_set_width(name, 142);
        lv_label_set_long_mode(name, LV_LABEL_LONG_WRAP);
        lv_obj_align(name, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    }
}

void wf_app_manager_init(lv_obj_t *watch_screen)
{
    s_watch_screen = watch_screen;
    s_launcher_screen = NULL;
    s_launcher_grid = NULL;
    s_current_app = NULL;
    s_app_count = 0;
    s_route = WF_ROUTE_WATCH;
}

bool wf_app_manager_register(wf_app_t *app)
{
    if (app == NULL || app->id == NULL || app->name == NULL || app->icon == NULL ||
        app->create == NULL || s_app_count >= WF_MAX_APPS) {
        return false;
    }
    for (size_t i = 0; i < s_app_count; ++i) {
        if (strcmp(s_apps[i]->id, app->id) == 0) {
            return false;
        }
    }
    s_apps[s_app_count++] = app;
    return true;
}

void wf_app_manager_finish_registration(void)
{
    create_launcher();
    wf_app_manager_show_watch();
}

void wf_app_manager_show_watch(void)
{
    if (s_current_app != NULL && s_current_app->hide != NULL) {
        s_current_app->hide(s_current_app);
    }
    s_current_app = NULL;
    s_route = WF_ROUTE_WATCH;
    load_screen(s_watch_screen);
}

void wf_app_manager_show_launcher(void)
{
    if (s_current_app != NULL && s_current_app->hide != NULL) {
        s_current_app->hide(s_current_app);
    }
    s_current_app = NULL;
    s_route = WF_ROUTE_LAUNCHER;
    load_screen(s_launcher_screen);
}

void wf_app_manager_open(wf_app_t *app)
{
    if (app == NULL) {
        return;
    }
    if (s_current_app != NULL && s_current_app != app && s_current_app->hide != NULL) {
        s_current_app->hide(s_current_app);
    }
    if (app->screen == NULL) {
        app->create(app);
    }
    if (app->screen == NULL) {
        ESP_LOGE(TAG, "App %s did not create a screen", app->id);
        return;
    }
    s_current_app = app;
    s_route = WF_ROUTE_APP;
    load_screen(app->screen);
    if (app->show != NULL) {
        app->show(app);
    }
}

void wf_app_manager_global_button(void)
{
    if (s_route == WF_ROUTE_LAUNCHER) {
        wf_app_manager_show_watch();
    } else {
        wf_app_manager_show_launcher();
    }
}

void wf_app_manager_update(const void *model)
{
    for (size_t i = 0; i < s_app_count; ++i) {
        if (s_apps[i]->screen != NULL && s_apps[i]->update != NULL) {
            s_apps[i]->update(s_apps[i], model);
        }
    }
}

const char *wf_app_manager_view_name(void)
{
    if (s_route == WF_ROUTE_WATCH) {
        return "WATCH";
    }
    if (s_route == WF_ROUTE_LAUNCHER) {
        return "LAUNCHER";
    }
    return s_current_app != NULL ? s_current_app->id : "APP";
}

size_t wf_app_manager_count(void)
{
    return s_app_count;
}

wf_app_t *wf_app_manager_at(size_t index)
{
    return index < s_app_count ? s_apps[index] : NULL;
}

static uint32_t count_tree(lv_obj_t *root)
{
    if (root == NULL) {
        return 0;
    }
    uint32_t count = 1;
    const uint32_t children = lv_obj_get_child_count(root);
    for (uint32_t i = 0; i < children; ++i) {
        count += count_tree(lv_obj_get_child(root, i));
    }
    return count;
}

static uint32_t count_owned_objects(void)
{
    uint32_t count = count_tree(s_watch_screen) + count_tree(s_launcher_screen);
    for (size_t i = 0; i < s_app_count; ++i) {
        count += count_tree(s_apps[i]->screen);
    }
    return count;
}

wf_app_stress_result_t wf_app_manager_stress_test(uint32_t rounds)
{
    enum { HEAP_NOISE_TOLERANCE_BYTES = 64 };
    wf_app_stress_result_t result = {.rounds = rounds};

    /* Warm every lazy app once, then measure only repeat navigation. */
    wf_app_manager_show_launcher();
    for (size_t i = 0; i < s_app_count; ++i) {
        wf_app_manager_open(s_apps[i]);
        wf_app_manager_show_launcher();
    }
    wf_app_manager_show_watch();

    result.objects_before = count_owned_objects();
    result.heap_before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    for (uint32_t round = 0; round < rounds; ++round) {
        wf_app_manager_show_launcher();
        for (size_t i = 0; i < s_app_count; ++i) {
            wf_app_manager_open(s_apps[i]);
            wf_app_manager_show_launcher();
        }
        wf_app_manager_show_watch();
    }
    result.objects_after = count_owned_objects();
    result.heap_after = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    const size_t heap_loss = result.heap_before > result.heap_after ?
                             result.heap_before - result.heap_after : 0;
    result.passed = result.objects_before == result.objects_after &&
                    heap_loss <= HEAP_NOISE_TOLERANCE_BYTES;
    ESP_LOGI(TAG,
             "NAV_STRESS rounds=%" PRIu32 " apps=%u objects=%" PRIu32 "->%" PRIu32
             " internal_heap=%u->%u loss=%u tolerance=%u result=%s",
             rounds, (unsigned)s_app_count, result.objects_before, result.objects_after,
             (unsigned)result.heap_before, (unsigned)result.heap_after,
             (unsigned)heap_loss, HEAP_NOISE_TOLERANCE_BYTES,
             result.passed ? "PASS" : "FAIL");
    return result;
}
