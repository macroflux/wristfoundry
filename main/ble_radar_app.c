#include "ble_radar_app.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "ble_radar_service.h"
#include "esp_timer.h"
#include "ui_design.h"
#include "watch_ui.h"

enum {
    RADAR_VISIBLE_DEVICES = 12,
    RADAR_DIRECTIONS = 16,
};

typedef enum {
    VIEW_RADAR,
    VIEW_LIST,
} radar_view_t;

typedef struct {
    lv_obj_t *button;
    lv_obj_t *name;
    lv_obj_t *detail;
    uint8_t device_index;
} device_row_t;

typedef struct {
    bool visible;
    bool display_active;
    radar_view_t view;
    uint8_t sweep_phase;
    ble_radar_snapshot_t snapshot;
    lv_timer_t *timer;
    lv_obj_t *radar_button;
    lv_obj_t *list_button;
    lv_obj_t *status_label;
    lv_obj_t *radar_panel;
    lv_obj_t *list_panel;
    lv_obj_t *sweep_dots[3];
    lv_obj_t *device_dots[RADAR_VISIBLE_DEVICES];
    lv_obj_t *device_labels[RADAR_VISIBLE_DEVICES];
    device_row_t rows[BLE_RADAR_MAX_DEVICES];
    lv_obj_t *detail_panel;
    lv_obj_t *detail_label;
} ble_radar_app_context_t;

static ble_radar_app_context_t s_context;
static const int8_t s_direction_x[RADAR_DIRECTIONS] =
    {0, 38, 71, 92, 100, 92, 71, 38, 0, -38, -71, -92, -100, -92, -71, -38};
static const int8_t s_direction_y[RADAR_DIRECTIONS] =
    {-100, -92, -71, -38, 0, 38, 71, 92, 100, 92, 71, 38, 0, -38, -71, -92};

static void update_timer_policy(void)
{
    if (s_context.timer == NULL) return;
    if (s_context.visible && s_context.display_active &&
        s_context.snapshot.scanning && s_context.view == VIEW_RADAR) {
        lv_timer_resume(s_context.timer);
    } else {
        lv_timer_pause(s_context.timer);
    }
}

static void back_event(lv_event_t *event)
{
    (void)event;
    wf_ui_report_activity();
    wf_app_manager_show_launcher();
}

static uint32_t address_hash(const ble_radar_device_t *device)
{
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < sizeof(device->address); ++i) {
        hash = (hash ^ device->address[i]) * 16777619u;
    }
    return hash;
}

static int radius_for_rssi(int rssi)
{
    if (rssi >= -45) return 25;
    if (rssi <= -95) return 96;
    return 25 + ((-45 - rssi) * 71) / 50;
}

static int compare_indices(const ble_radar_snapshot_t *snapshot, uint8_t a, uint8_t b)
{
    const ble_radar_device_t *left = &snapshot->devices[a];
    const ble_radar_device_t *right = &snapshot->devices[b];
    if (left->stale != right->stale) return left->stale ? 1 : -1;
    if (left->rssi_average != right->rssi_average) {
        return left->rssi_average > right->rssi_average ? -1 : 1;
    }
    if (left->last_seen_ms != right->last_seen_ms) {
        return left->last_seen_ms > right->last_seen_ms ? -1 : 1;
    }
    return 0;
}

static void sorted_indices(const ble_radar_snapshot_t *snapshot, uint8_t *indices)
{
    for (uint8_t i = 0; i < snapshot->device_count; ++i) indices[i] = i;
    for (uint8_t i = 1; i < snapshot->device_count; ++i) {
        const uint8_t value = indices[i];
        int j = i - 1;
        while (j >= 0 && compare_indices(snapshot, indices[j], value) > 0) {
            indices[j + 1] = indices[j];
            --j;
        }
        indices[j + 1] = value;
    }
}

static void update_radar_objects(void)
{
    uint8_t indices[BLE_RADAR_MAX_DEVICES] = {0};
    sorted_indices(&s_context.snapshot, indices);
    const uint8_t visible = s_context.snapshot.device_count > RADAR_VISIBLE_DEVICES ?
                            RADAR_VISIBLE_DEVICES : s_context.snapshot.device_count;
    for (uint8_t i = 0; i < RADAR_VISIBLE_DEVICES; ++i) {
        if (i >= visible) {
            lv_obj_add_flag(s_context.device_dots[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s_context.device_labels[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        const ble_radar_device_t *device = &s_context.snapshot.devices[indices[i]];
        const uint32_t hash = address_hash(device);
        const uint8_t direction = hash % RADAR_DIRECTIONS;
        const int radius = radius_for_rssi(device->rssi_average);
        const int x = (s_direction_x[direction] * radius) / 100;
        const int y = (s_direction_y[direction] * radius) / 100;
        lv_obj_remove_flag(s_context.device_dots[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(s_context.device_labels[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(s_context.device_dots[i], 177 + x, 108 + y);
        lv_obj_set_pos(s_context.device_labels[i], 187 + x, 101 + y);
        lv_label_set_text_fmt(s_context.device_labels[i], "%d", device->rssi_average);
        lv_obj_set_style_bg_color(s_context.device_dots[i],
            lv_color_hex(device->stale ? WF_COLOR_MUTED : WF_COLOR_SUCCESS), 0);
    }
}

static void timer_event(lv_timer_t *timer)
{
    (void)timer;
    if (!s_context.visible || !s_context.display_active ||
        !s_context.snapshot.scanning || s_context.view != VIEW_RADAR) {
        return;
    }
    s_context.sweep_phase = (s_context.sweep_phase + 1) % RADAR_DIRECTIONS;
    for (uint8_t i = 0; i < 3; ++i) {
        const uint8_t phase = (s_context.sweep_phase + RADAR_DIRECTIONS - i) % RADAR_DIRECTIONS;
        const int radius = 38 + i * 24;
        lv_obj_set_pos(s_context.sweep_dots[i],
            179 + (s_direction_x[phase] * radius) / 100,
            110 + (s_direction_y[phase] * radius) / 100);
        lv_obj_set_style_opa(s_context.sweep_dots[i], LV_OPA_COVER - i * 70, 0);
    }
}

static void set_view(radar_view_t view)
{
    s_context.view = view;
    if (view == VIEW_RADAR) {
        lv_obj_remove_flag(s_context.radar_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_context.list_panel, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_context.radar_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(s_context.list_panel, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_style_border_color(s_context.radar_button,
        lv_color_hex(view == VIEW_RADAR ? WF_COLOR_PRIMARY : WF_COLOR_BORDER), 0);
    lv_obj_set_style_border_color(s_context.list_button,
        lv_color_hex(view == VIEW_LIST ? WF_COLOR_PRIMARY : WF_COLOR_BORDER), 0);
    update_timer_policy();
}

static void radar_view_event(lv_event_t *event)
{
    (void)event;
    wf_ui_report_activity();
    set_view(VIEW_RADAR);
}

static void list_view_event(lv_event_t *event)
{
    (void)event;
    wf_ui_report_activity();
    set_view(VIEW_LIST);
}

static void start_event(lv_event_t *event)
{
    (void)event;
    wf_ui_report_activity();
    ble_radar_service_start();
}

static void stop_event(lv_event_t *event)
{
    (void)event;
    wf_ui_report_activity();
    ble_radar_service_stop("USER");
}

static void clear_event(lv_event_t *event)
{
    (void)event;
    wf_ui_report_activity();
    ble_radar_service_clear();
}

static void close_detail_event(lv_event_t *event)
{
    (void)event;
    wf_ui_report_activity();
    lv_obj_add_flag(s_context.detail_panel, LV_OBJ_FLAG_HIDDEN);
}

static void row_event(lv_event_t *event)
{
    device_row_t *row = lv_event_get_user_data(event);
    wf_ui_report_activity();
    if (row == NULL || row->device_index >= s_context.snapshot.device_count) return;
    const ble_radar_device_t *device = &s_context.snapshot.devices[row->device_index];
    const int64_t timestamp_ms = esp_timer_get_time() / 1000;
    const int64_t first_age_seconds = (timestamp_ms - device->first_seen_ms) / 1000;
    const int64_t last_age_seconds = (timestamp_ms - device->last_seen_ms) / 1000;
    char uuids[48] = "NONE";
    char tx_power[24] = "NOT ADVERTISED";
    char manufacturer[32] = "NOT ADVERTISED";
    if (device->tx_power_present) {
        snprintf(tx_power, sizeof(tx_power), "%d dBm", device->tx_power);
    }
    if (device->manufacturer_present) {
        snprintf(manufacturer, sizeof(manufacturer), "0x%04X / %u BYTES",
                 device->manufacturer_id, device->manufacturer_data_length);
    }
    if (device->service_uuid16_count > 0) {
        size_t used = 0;
        uuids[0] = '\0';
        for (uint8_t i = 0; i < device->service_uuid16_count; ++i) {
            used += snprintf(uuids + used, sizeof(uuids) - used, "%s%04X",
                             i == 0 ? "" : ", ", device->service_uuid16[i]);
            if (used >= sizeof(uuids)) break;
        }
    }
    lv_label_set_text_fmt(
        s_context.detail_label,
        "%s\n\nADDRESS\n%s\n%s\n\nSIGNAL\n%d dBm  /  AVG %d dBm\nSEEN %" PRIu32 " TIMES\nFIRST %" PRId64 "s / LAST %" PRId64 "s\n\nADVERTISEMENT\n%s\nTX POWER  %s\nMFR DATA  %s\nUUID16    %s",
        device->name[0] ? device->name : "UNNAMED DEVICE",
        device->address_text, ble_radar_address_type_name(device->address_type),
        device->rssi, device->rssi_average, device->seen_count,
        first_age_seconds < 0 ? 0 : first_age_seconds,
        last_age_seconds < 0 ? 0 : last_age_seconds,
        ble_radar_event_type_name(device->event_type),
        tx_power, manufacturer, uuids);
    lv_obj_remove_flag(s_context.detail_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_context.detail_panel);
}

static void create_radar_panel(lv_obj_t *screen)
{
    s_context.radar_panel = wf_ui_card_create(screen);
    lv_obj_set_size(s_context.radar_panel, 362, 264);
    lv_obj_align(s_context.radar_panel, LV_ALIGN_TOP_MID, 0, 144);
    lv_obj_clear_flag(s_context.radar_panel, LV_OBJ_FLAG_SCROLLABLE);

    for (int diameter = 64; diameter <= 192; diameter += 64) {
        lv_obj_t *ring = lv_obj_create(s_context.radar_panel);
        lv_obj_remove_style_all(ring);
        lv_obj_set_size(ring, diameter, diameter);
        lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(ring, 1, 0);
        lv_obj_set_style_border_color(ring, lv_color_hex(WF_COLOR_BORDER), 0);
        lv_obj_center(ring);
    }
    lv_obj_t *axis_h = lv_obj_create(s_context.radar_panel);
    lv_obj_remove_style_all(axis_h);
    lv_obj_set_size(axis_h, 212, 1);
    lv_obj_set_style_bg_color(axis_h, lv_color_hex(WF_COLOR_BORDER), 0);
    lv_obj_set_style_bg_opa(axis_h, LV_OPA_COVER, 0);
    lv_obj_center(axis_h);
    lv_obj_t *axis_v = lv_obj_create(s_context.radar_panel);
    lv_obj_remove_style_all(axis_v);
    lv_obj_set_size(axis_v, 1, 212);
    lv_obj_set_style_bg_color(axis_v, lv_color_hex(WF_COLOR_BORDER), 0);
    lv_obj_set_style_bg_opa(axis_v, LV_OPA_COVER, 0);
    lv_obj_center(axis_v);

    for (uint8_t i = 0; i < 3; ++i) {
        s_context.sweep_dots[i] = lv_obj_create(s_context.radar_panel);
        lv_obj_remove_style_all(s_context.sweep_dots[i]);
        lv_obj_set_size(s_context.sweep_dots[i], 8, 8);
        lv_obj_set_style_radius(s_context.sweep_dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(s_context.sweep_dots[i], lv_color_hex(WF_COLOR_PRIMARY), 0);
        lv_obj_set_style_bg_opa(s_context.sweep_dots[i], LV_OPA_COVER, 0);
    }
    for (uint8_t i = 0; i < RADAR_VISIBLE_DEVICES; ++i) {
        s_context.device_dots[i] = lv_obj_create(s_context.radar_panel);
        lv_obj_remove_style_all(s_context.device_dots[i]);
        lv_obj_set_size(s_context.device_dots[i], 10, 10);
        lv_obj_set_style_radius(s_context.device_dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(s_context.device_dots[i], LV_OPA_COVER, 0);
        s_context.device_labels[i] = wf_ui_label_create(
            s_context.radar_panel, "", &lv_font_montserrat_14, WF_COLOR_TEXT);
        lv_obj_add_flag(s_context.device_dots[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_context.device_labels[i], LV_OBJ_FLAG_HIDDEN);
    }
}

static void create_list_panel(lv_obj_t *screen)
{
    s_context.list_panel = wf_ui_card_create(screen);
    lv_obj_set_size(s_context.list_panel, 362, 264);
    lv_obj_align(s_context.list_panel, LV_ALIGN_TOP_MID, 0, 144);
    wf_ui_make_scrollable(s_context.list_panel);
    lv_obj_set_style_pad_left(s_context.list_panel, 8, 0);
    lv_obj_set_style_pad_right(s_context.list_panel, 8, 0);
    lv_obj_set_style_pad_top(s_context.list_panel, 8, 0);
    lv_obj_set_flex_flow(s_context.list_panel, LV_FLEX_FLOW_COLUMN);
    for (uint8_t i = 0; i < BLE_RADAR_MAX_DEVICES; ++i) {
        device_row_t *row = &s_context.rows[i];
        row->button = lv_obj_create(s_context.list_panel);
        lv_obj_remove_style_all(row->button);
        lv_obj_set_size(row->button, 326, 62);
        lv_obj_set_style_bg_color(row->button, lv_color_hex(WF_COLOR_PANEL_ALT), 0);
        lv_obj_set_style_bg_opa(row->button, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(row->button, 12, 0);
        lv_obj_add_flag(row->button, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row->button, row_event, LV_EVENT_CLICKED, row);
        row->name = wf_ui_label_create(row->button, "", &lv_font_montserrat_14, WF_COLOR_TEXT);
        lv_obj_align(row->name, LV_ALIGN_TOP_LEFT, 10, 8);
        row->detail = wf_ui_label_create(row->button, "", &lv_font_montserrat_14, WF_COLOR_MUTED);
        lv_obj_align(row->detail, LV_ALIGN_BOTTOM_LEFT, 10, -7);
        lv_obj_add_flag(row->button, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_add_flag(s_context.list_panel, LV_OBJ_FLAG_HIDDEN);
}

static void create_detail_panel(lv_obj_t *screen)
{
    s_context.detail_panel = wf_ui_card_create(screen);
    lv_obj_set_size(s_context.detail_panel, 370, 364);
    lv_obj_center(s_context.detail_panel);
    lv_obj_set_style_border_color(s_context.detail_panel, lv_color_hex(WF_COLOR_PRIMARY), 0);
    lv_obj_set_style_border_width(s_context.detail_panel, 2, 0);
    s_context.detail_label = wf_ui_label_create(
        s_context.detail_panel, "", &lv_font_montserrat_14, WF_COLOR_TEXT);
    lv_obj_set_width(s_context.detail_label, 330);
    lv_label_set_long_mode(s_context.detail_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_context.detail_label, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_t *close = wf_ui_button_create(s_context.detail_panel, "CLOSE", close_detail_event, NULL);
    lv_obj_set_size(close, 110, 48);
    lv_obj_align(close, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(s_context.detail_panel, LV_OBJ_FLAG_HIDDEN);
}

void ble_radar_app_create(wf_app_t *app)
{
    memset(&s_context, 0, sizeof(s_context));
    app->context = &s_context;
    app->screen = wf_ui_screen_create();
    wf_ui_header_create(app->screen, app->icon, app->name, "BACK", back_event, NULL);

    s_context.radar_button = wf_ui_button_create(app->screen, "RADAR", radar_view_event, NULL);
    lv_obj_set_size(s_context.radar_button, 86, 50);
    lv_obj_align(s_context.radar_button, LV_ALIGN_TOP_LEFT, 24, 86);
    s_context.list_button = wf_ui_button_create(app->screen, "LIST", list_view_event, NULL);
    lv_obj_set_size(s_context.list_button, 86, 50);
    lv_obj_align(s_context.list_button, LV_ALIGN_TOP_LEFT, 120, 86);
    s_context.status_label = wf_ui_label_create(
        app->screen, "OFF", &lv_font_montserrat_14, WF_COLOR_MUTED);
    lv_obj_set_width(s_context.status_label, 174);
    lv_obj_set_style_text_align(s_context.status_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(s_context.status_label, LV_ALIGN_TOP_RIGHT, -24, 103);

    create_radar_panel(app->screen);
    create_list_panel(app->screen);

    lv_obj_t *start = wf_ui_button_create(app->screen, "START", start_event, NULL);
    lv_obj_set_size(start, 110, 54);
    lv_obj_align(start, LV_ALIGN_BOTTOM_LEFT, 24, -20);
    lv_obj_t *stop = wf_ui_button_create(app->screen, "STOP", stop_event, NULL);
    lv_obj_set_size(stop, 110, 54);
    lv_obj_align(stop, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_t *clear = wf_ui_button_create(app->screen, "CLEAR", clear_event, NULL);
    lv_obj_set_size(clear, 110, 54);
    lv_obj_align(clear, LV_ALIGN_BOTTOM_RIGHT, -24, -20);
    create_detail_panel(app->screen);

    s_context.timer = lv_timer_create(timer_event, 100, NULL);
    lv_timer_pause(s_context.timer);
    set_view(VIEW_RADAR);
}

void ble_radar_app_show(wf_app_t *app)
{
    (void)app;
    s_context.visible = true;
    update_timer_policy();
}

void ble_radar_app_hide(wf_app_t *app)
{
    (void)app;
    s_context.visible = false;
    if (s_context.timer != NULL) lv_timer_pause(s_context.timer);
    if (s_context.detail_panel != NULL) lv_obj_add_flag(s_context.detail_panel, LV_OBJ_FLAG_HIDDEN);
    ble_radar_service_stop("APP EXIT");
}

void ble_radar_app_update(wf_app_t *app, const void *data)
{
    (void)app;
    const watch_ui_model_t *model = data;
    if (model == NULL || s_context.status_label == NULL) return;
    if (model->ble == NULL) return;
    s_context.snapshot = *model->ble;
    s_context.display_active = strcmp(model->display_state, "ACTIVE") == 0;
    update_timer_policy();
    lv_label_set_text_fmt(s_context.status_label, "%s  %u  %" PRIu32 "s",
        ble_radar_state_name(model->ble->state), model->ble->device_count,
        model->ble->seconds_remaining);
    lv_obj_set_style_text_color(s_context.status_label,
        lv_color_hex(model->ble->scanning ? WF_COLOR_SUCCESS : WF_COLOR_MUTED), 0);
    update_radar_objects();

    uint8_t indices[BLE_RADAR_MAX_DEVICES] = {0};
    sorted_indices(model->ble, indices);
    const int64_t timestamp_ms = esp_timer_get_time() / 1000;
    for (uint8_t i = 0; i < BLE_RADAR_MAX_DEVICES; ++i) {
        device_row_t *row = &s_context.rows[i];
        if (i >= model->ble->device_count) {
            lv_obj_add_flag(row->button, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        const uint8_t device_index = indices[i];
        const ble_radar_device_t *device = &model->ble->devices[device_index];
        row->device_index = device_index;
        lv_obj_remove_flag(row->button, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text_fmt(row->name, "%s%s",
            device->name[0] ? device->name : "UNNAMED", device->stale ? "  /  STALE" : "");
        const int64_t age_seconds = (timestamp_ms - device->last_seen_ms) / 1000;
        lv_label_set_text_fmt(row->detail, "%d dBm   %s   %" PRId64 "s ago",
            device->rssi_average, device->address_text, age_seconds < 0 ? 0 : age_seconds);
    }
}

void ble_radar_app_destroy(wf_app_t *app)
{
    ble_radar_service_stop("DESTROY");
    if (s_context.timer != NULL) {
        lv_timer_delete(s_context.timer);
        s_context.timer = NULL;
    }
    if (app->screen != NULL) {
        lv_obj_delete(app->screen);
        app->screen = NULL;
    }
}
