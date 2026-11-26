#include "watch_ui.h"

#include <inttypes.h>

#include "app_manager.h"
#include "ble_radar_app.h"
#include "ble_radar_service.h"
#include "phone_service.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "settings_app.h"
#include "ui_design.h"
#include "time_sync_service.h"

#define WRISTFOUNDRY_VERSION "0.1.0"

typedef struct {
    lv_obj_t *status_label;
    lv_obj_t *ble_label;
    lv_obj_t *network_label;
} system_app_context_t;

static const char *TAG = "watch_ui";
static lv_obj_t *s_watch_screen;
static lv_obj_t *s_time_label;
static lv_obj_t *s_date_label;
static lv_obj_t *s_battery_label;
static lv_obj_t *s_wifi_label;
static lv_obj_t *s_ble_label;
static lv_obj_t *s_sync_modal;
static lv_obj_t *s_sync_method_label;
static lv_obj_t *s_sync_status_label;
static lv_obj_t *s_sync_detail_label;
static lv_obj_t *s_sync_last_label;
static lv_obj_t *s_sync_close_button;
static lv_obj_t *s_sync_close_label;
static int64_t s_last_activity_us;
static system_app_context_t s_system_context;

static const char *reset_reason_name(esp_reset_reason_t reason)
{
    switch (reason) {
    case ESP_RST_POWERON: return "POWER";
    case ESP_RST_EXT: return "EXTERNAL";
    case ESP_RST_SW: return "SOFTWARE";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return "WATCHDOG";
    case ESP_RST_DEEPSLEEP: return "DEEP-SLEEP";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_USB: return "USB";
    case ESP_RST_JTAG: return "JTAG";
    case ESP_RST_PWR_GLITCH: return "PWR-GLITCH";
    case ESP_RST_CPU_LOCKUP: return "CPU-LOCKUP";
    default: return "UNKNOWN";
    }
}

static const char *wake_cause_name(esp_sleep_wakeup_cause_t cause)
{
    switch (cause) {
    case ESP_SLEEP_WAKEUP_GPIO: return "GPIO10";
    case ESP_SLEEP_WAKEUP_EXT1: return "EXT1";
    case ESP_SLEEP_WAKEUP_TIMER: return "TIMER";
    case ESP_SLEEP_WAKEUP_TOUCHPAD: return "TOUCHPAD";
    case ESP_SLEEP_WAKEUP_UNDEFINED: return "BOOT";
    default: return "OTHER";
    }
}

static uint8_t weekday_from_date(const board_health_t *health)
{
    static const int offsets[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    int year = health->year;
    if (health->month < 3) {
        year--;
    }
    return (uint8_t)((year + year / 4 - year / 100 + year / 400 +
                      offsets[health->month - 1] + health->day) % 7);
}

void watch_ui_note_activity(void)
{
    s_last_activity_us = esp_timer_get_time();
    phone_service_activity(false);
}

int64_t watch_ui_last_activity_us(void)
{
    return s_last_activity_us;
}

static void activity_event(lv_event_t *event)
{
    (void)event;
    watch_ui_note_activity();
}

static void show_launcher_event(lv_event_t *event)
{
    (void)event;
    watch_ui_note_activity();
    time_sync_service_cancel("APP TRANSITION");
    if (s_sync_modal != NULL) lv_obj_add_flag(s_sync_modal, LV_OBJ_FLAG_HIDDEN);
    wf_app_manager_show_launcher();
}

static void app_back_event(lv_event_t *event)
{
    (void)event;
    watch_ui_note_activity();
    wf_app_manager_show_launcher();
}

static void request_connect_event(lv_event_t *event)
{
    (void)event;
    watch_ui_note_activity();
    network_service_request_manual_connect();
}

static void sync_time_event(lv_event_t *event)
{
    (void)event;
    watch_ui_note_activity();
    if (s_sync_modal != NULL) {
        lv_obj_remove_flag(s_sync_modal, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_sync_modal);
    }
    (void)time_sync_service_request_auto();
}

static void close_sync_event(lv_event_t *event)
{
    (void)event;
    watch_ui_note_activity();
    time_sync_snapshot_t sync;
    time_sync_service_get_snapshot(&sync);
    if (sync.active) {
        time_sync_service_cancel("USER CANCELED");
    } else if (s_sync_modal != NULL) {
        lv_obj_add_flag(s_sync_modal, LV_OBJ_FLAG_HIDDEN);
    }
}

static void system_sync_time_event(lv_event_t *event)
{
    (void)event;
    watch_ui_note_activity();
    (void)time_sync_service_request_auto();
}

static void create_sync_modal(void)
{
    s_sync_modal = wf_ui_card_create(s_watch_screen);
    lv_obj_set_size(s_sync_modal, 370, 336);
    lv_obj_center(s_sync_modal);
    lv_obj_set_style_border_color(s_sync_modal, lv_color_hex(WF_COLOR_PRIMARY), 0);
    lv_obj_set_style_border_width(s_sync_modal, 2, 0);

    lv_obj_t *title = wf_ui_label_create(s_sync_modal, "SYNC TIME",
                                          &lv_font_montserrat_20, WF_COLOR_TEXT);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);
    s_sync_method_label = wf_ui_label_create(s_sync_modal, "AUTO",
                                              &lv_font_montserrat_14,
                                              WF_COLOR_PRIMARY);
    lv_obj_align(s_sync_method_label, LV_ALIGN_TOP_LEFT, 0, 38);
    s_sync_status_label = wf_ui_label_create(s_sync_modal, "Ready",
                                              &lv_font_montserrat_20,
                                              WF_COLOR_TEXT);
    lv_obj_set_width(s_sync_status_label, 330);
    lv_label_set_long_mode(s_sync_status_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_sync_status_label, LV_ALIGN_TOP_LEFT, 0, 72);
    s_sync_detail_label = wf_ui_label_create(
        s_sync_modal, "Wi-Fi NTP with hardware RTC persistence",
        &lv_font_montserrat_14, WF_COLOR_MUTED);
    lv_obj_set_width(s_sync_detail_label, 330);
    lv_label_set_long_mode(s_sync_detail_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_sync_detail_label, LV_ALIGN_TOP_LEFT, 0, 124);
    s_sync_last_label = wf_ui_label_create(s_sync_modal, "LAST / NEVER",
                                            &lv_font_montserrat_14,
                                            WF_COLOR_MUTED);
    lv_obj_set_width(s_sync_last_label, 330);
    lv_label_set_long_mode(s_sync_last_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_sync_last_label, LV_ALIGN_TOP_LEFT, 0, 212);
    s_sync_close_button = wf_ui_button_create(
        s_sync_modal, "CANCEL", close_sync_event, NULL);
    lv_obj_set_size(s_sync_close_button, 140, 52);
    lv_obj_align(s_sync_close_button, LV_ALIGN_BOTTOM_MID, 0, 0);
    s_sync_close_label = lv_obj_get_child(s_sync_close_button, 0);
    lv_obj_add_flag(s_sync_modal, LV_OBJ_FLAG_HIDDEN);
}

static void radio_off_event(lv_event_t *event)
{
    (void)event;
    watch_ui_note_activity();
    network_service_force_off("SYSTEM APP");
}

static void create_watch_face(void)
{
    s_watch_screen = wf_ui_screen_create();

    lv_obj_t *brand = wf_ui_label_create(s_watch_screen, "WRISTFOUNDRY",
                                          &lv_font_montserrat_14, WF_COLOR_PRIMARY);
    lv_obj_set_style_text_letter_space(brand, 3, 0);
    lv_obj_align(brand, LV_ALIGN_TOP_MID, 0, 36);

    s_time_label = wf_ui_label_create(s_watch_screen, "--:--",
                                       &lv_font_montserrat_32, WF_COLOR_TEXT);
    lv_obj_set_style_transform_scale_x(s_time_label, 320, 0);
    lv_obj_set_style_transform_scale_y(s_time_label, 320, 0);
    lv_obj_align(s_time_label, LV_ALIGN_TOP_MID, 0, 72);

    s_date_label = wf_ui_label_create(s_watch_screen, "TIME NOT SET",
                                       &lv_font_montserrat_20, WF_COLOR_MUTED);
    lv_obj_align(s_date_label, LV_ALIGN_TOP_MID, 0, 139);

    lv_obj_t *power_card = wf_ui_card_create(s_watch_screen);
    lv_obj_set_size(power_card, 362, 128);
    lv_obj_align(power_card, LV_ALIGN_TOP_MID, 0, 194);

    lv_obj_t *power_heading = wf_ui_label_create(power_card, "POWER",
                                                  &lv_font_montserrat_14, WF_COLOR_MUTED);
    lv_obj_align(power_heading, LV_ALIGN_TOP_LEFT, 0, 0);
    s_battery_label = wf_ui_label_create(power_card, "BATTERY --",
                                          &lv_font_montserrat_20, WF_COLOR_TEXT);
    lv_obj_align(s_battery_label, LV_ALIGN_TOP_LEFT, 0, 30);
    lv_obj_t *sync = wf_ui_button_create(power_card, "SYNC",
                                          sync_time_event, NULL);
    lv_obj_set_size(sync, 86, 48);
    lv_obj_align(sync, LV_ALIGN_TOP_RIGHT, 0, -4);
    s_wifi_label = wf_ui_label_create(power_card, "WI-FI OFF",
                                       &lv_font_montserrat_14, WF_COLOR_MUTED);
    lv_obj_align(s_wifi_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    s_ble_label = wf_ui_label_create(power_card, "BLE OFF",
                                      &lv_font_montserrat_14, WF_COLOR_MUTED);
    lv_obj_align(s_ble_label, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    lv_obj_t *apps = wf_ui_button_create(s_watch_screen, "OPEN APPS",
                                          show_launcher_event, NULL);
    lv_obj_set_size(apps, 362, 66);
    lv_obj_align(apps, LV_ALIGN_BOTTOM_MID, 0, -38);
    create_sync_modal();
}

static void app_noop(wf_app_t *app)
{
    (void)app;
}

static void app_destroy(wf_app_t *app)
{
    if (app->screen != NULL) {
        lv_obj_delete(app->screen);
        app->screen = NULL;
    }
}

static void system_create(wf_app_t *app)
{
    app->screen = wf_ui_screen_create();
    wf_ui_header_create(app->screen, app->icon, app->name, "BACK",
                        app_back_event, NULL);

    lv_obj_t *content = lv_obj_create(app->screen);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, WF_DISPLAY_WIDTH, WF_DISPLAY_HEIGHT - WF_HEADER_HEIGHT);
    lv_obj_align(content, LV_ALIGN_BOTTOM_MID, 0, 0);
    wf_ui_make_scrollable(content);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *telemetry = wf_ui_card_create(content);
    lv_obj_set_width(telemetry, 362);
    lv_obj_set_height(telemetry, 278);
    lv_obj_set_flex_flow(telemetry, LV_FLEX_FLOW_COLUMN);
    wf_ui_label_create(telemetry, "DEVICE TELEMETRY",
                       &lv_font_montserrat_14, WF_COLOR_PRIMARY);
    s_system_context.status_label = wf_ui_label_create(
        telemetry, "READING HARDWARE...", &lv_font_montserrat_14, WF_COLOR_TEXT);
    lv_obj_set_width(s_system_context.status_label, 328);
    lv_obj_set_style_text_line_space(s_system_context.status_label, 4, 0);

    lv_obj_t *ble = wf_ui_card_create(content);
    lv_obj_set_width(ble, 362);
    lv_obj_set_height(ble, 176);
    lv_obj_set_flex_flow(ble, LV_FLEX_FLOW_COLUMN);
    wf_ui_label_create(ble, "BLUETOOTH LE", &lv_font_montserrat_14, WF_COLOR_PRIMARY);
    s_system_context.ble_label = wf_ui_label_create(
        ble, "BLE OFF", &lv_font_montserrat_14, WF_COLOR_TEXT);
    lv_obj_set_width(s_system_context.ble_label, 328);
    lv_obj_set_style_text_line_space(s_system_context.ble_label, 4, 0);

    lv_obj_t *network = wf_ui_card_create(content);
    lv_obj_set_width(network, 362);
    lv_obj_set_height(network, 304);
    lv_obj_set_flex_flow(network, LV_FLEX_FLOW_COLUMN);
    wf_ui_label_create(network, "NETWORK", &lv_font_montserrat_14, WF_COLOR_PRIMARY);
    s_system_context.network_label = wf_ui_label_create(
        network, "READING NETWORK...", &lv_font_montserrat_14, WF_COLOR_TEXT);
    lv_obj_set_width(s_system_context.network_label, 328);
    lv_obj_set_style_text_line_space(s_system_context.network_label, 4, 0);

    lv_obj_t *actions = lv_obj_create(content);
    lv_obj_remove_style_all(actions);
    lv_obj_set_size(actions, 362, WF_TOUCH_TARGET);
    lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(actions, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *connect = wf_ui_button_create(actions, "CONNECT",
                                             request_connect_event, NULL);
    lv_obj_set_width(connect, 112);
    lv_obj_t *sync = wf_ui_button_create(actions, "SYNC TIME",
                                          system_sync_time_event, NULL);
    lv_obj_set_width(sync, 112);
    lv_obj_t *off = wf_ui_button_create(actions, "RADIO OFF", radio_off_event, NULL);
    lv_obj_set_width(off, 112);
}

static void system_update(wf_app_t *app, const void *data)
{
    (void)app;
    const watch_ui_model_t *model = data;
    if (model == NULL || s_system_context.status_label == NULL) {
        return;
    }
    const board_health_t *health = &model->health;
    const network_snapshot_t *network = &model->network;
    const ble_radar_snapshot_t *ble = model->ble;

    lv_label_set_text_fmt(
        s_system_context.status_label,
        "RTC       %s  %02u:%02u:%02u\n"
        "BATTERY   %u mV  %d%%\n"
        "POWER     %s%s%s\n"
        "MOTION    %+d  %+d  %+d mg\n"
        "IMU       %s  ID 0x%02X\n"
        "RESET     %s\n"
        "WAKE      %s\n"
        "UPTIME    %" PRId64 " s\n"
        "DISPLAY   %s / %s\n"
        "SLEEP     %.1f s  (#%" PRIu32 ")\n"
        "RADIOS    WIFI %s  BLE %s",
        health->rtc_valid ? "OK" : "UNSET", health->hour, health->minute,
        health->second, health->battery_mv, health->battery_percent,
        health->usb_present ? "USB" : "BATTERY",
        health->charging ? " / CHARGING" : "",
        health->battery_present ? "" : " / NO CELL",
        (int)health->accel_x_mg, (int)health->accel_y_mg,
        (int)health->accel_z_mg, health->imu_online ? "OK" : "OFFLINE",
        health->imu_who_am_i, reset_reason_name(esp_reset_reason()),
        wake_cause_name(model->wake_cause), esp_timer_get_time() / 1000000,
        model->display_state, wf_app_manager_view_name(),
        model->last_light_sleep_us / 1000000.0, model->light_sleep_count,
        network_service_state_name(network->state), ble->scanning ? "SCAN" :
        (ble->host_ready ? "READY" : "OFF"));

    lv_label_set_text_fmt(
        s_system_context.ble_label,
        "STATE     %s\n"
        "SCAN      %s  /  %" PRIu32 " s LEFT\n"
        "DEVICES   %u / %u  ADV %" PRIu32 "\n"
        "HOST/CTL  %s / %s\n"
        "ERROR     %d  %s\n"
        "MEMORY    %u KB  /  %u KB LARGEST",
        ble_radar_state_name(ble->state), ble->scanning ? "ACTIVE" : "IDLE",
        ble->seconds_remaining, ble->device_count, BLE_RADAR_MAX_DEVICES,
        ble->advertisement_count, ble->host_ready ? "READY" : "OFF",
        ble->controller_ready ? "READY" : "OFF", ble->last_error, ble->last_result,
        (unsigned)(ble->internal_free_bytes / 1024),
        (unsigned)(ble->internal_largest_bytes / 1024));

    lv_label_set_text_fmt(
        s_system_context.network_label,
        "STATE     %s  /  LEASES %" PRIu32 "\n"
        "CONFIG    %s\n"
        "SSID      %s\n"
        "IP/RSSI   %s  /  %d dBm\n"
        "RESULT    %s\n"
        "RADIO     %.1f s  /  %.1f s total\n"
        "TIMEZONE  %s\n"
        "LAST SYNC %s\n"
        "SYNC AT   %s\n"
        "AUTO      %s / %s",
        network_service_state_name(network->state), network->lease_count,
        network->credentials_present ? "SAVED" : "NOT PROVISIONED",
        network->credentials_present ? network->ssid : "-",
        network->ip[0] != '\0' ? network->ip : "-", network->rssi,
        network->last_connect_result,
        network->current_radio_active_ms / 1000.0,
        network->cumulative_radio_active_ms / 1000.0,
        network->timezone, network->last_sync_result,
        network->last_sync_local[0] != '\0' ? network->last_sync_local : "NEVER",
        time_sync_state_name(model->sync.state), model->sync.last_source);
}

static wf_app_t s_apps[] = {
    {
        .id = "SYSTEM", .name = "SYSTEM", .icon = "SY", .icon_name = "system",
        .description = "Hardware, power, sleep, reset, motion, and network diagnostics.",
        .create = system_create, .show = app_noop, .hide = app_noop,
        .update = system_update, .destroy = app_destroy, .context = &s_system_context,
    },
    {
        .id = "SETTINGS", .name = "SETTINGS", .icon = "SE", .icon_name = "settings",
        .description = "Secure on-watch Wi-Fi provisioning and NTP time synchronization.",
        .create = settings_app_create, .show = settings_app_show,
        .hide = settings_app_hide, .update = settings_app_update,
        .destroy = settings_app_destroy,
    },
    {
        .id = "BLE_RADAR", .name = "BLE RADAR", .icon = "BR", .icon_name = "radar",
        .description = "Bounded, time-limited nearby Bluetooth LE advertisement scanning.",
        .create = ble_radar_app_create, .show = ble_radar_app_show,
        .hide = ble_radar_app_hide, .update = ble_radar_app_update,
        .destroy = ble_radar_app_destroy,
    },
};

void watch_ui_create(void)
{
    wf_ui_design_init();
    wf_ui_set_activity_callback(activity_event);
    create_watch_face();
    wf_app_manager_init(s_watch_screen);
    for (size_t i = 0; i < sizeof(s_apps) / sizeof(s_apps[0]); ++i) {
        if (!wf_app_manager_register(&s_apps[i])) {
            ESP_LOGE(TAG, "Could not register app %s", s_apps[i].id);
        }
    }
    wf_app_manager_finish_registration();
    const wf_app_stress_result_t stress = wf_app_manager_stress_test(50);
    ESP_LOGI(TAG, "UI foundation: %u apps, navigation stress %s",
             (unsigned)wf_app_manager_count(), stress.passed ? "PASS" : "FAIL");
    watch_ui_note_activity();
}

void watch_ui_handle_home_button(void)
{
    watch_ui_note_activity();
    time_sync_service_cancel("GPIO0 HOME/APPS");
    if (s_sync_modal != NULL) lv_obj_add_flag(s_sync_modal, LV_OBJ_FLAG_HIDDEN);
    wf_app_manager_global_button();
}

const char *watch_ui_view_name(void)
{
    return wf_app_manager_view_name();
}

void watch_ui_update(const board_health_t *health, const char *display_state,
                     esp_sleep_wakeup_cause_t wake_cause,
                     uint32_t light_sleep_count, int64_t last_light_sleep_us,
                     const network_snapshot_t *network,
                     const ble_radar_snapshot_t *ble,
                     const time_sync_snapshot_t *sync)
{
    static const char *weekdays[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    static const char *months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                   "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

    if (health->rtc_valid && health->month >= 1 && health->month <= 12) {
        lv_label_set_text_fmt(s_time_label, "%02u:%02u", health->hour, health->minute);
        lv_label_set_text_fmt(s_date_label, "%s  /  %s %u",
                              weekdays[weekday_from_date(health)],
                              months[health->month - 1], health->day);
    } else {
        lv_label_set_text(s_time_label, "--:--");
        lv_label_set_text(s_date_label, health->rtc_online ? "TIME NOT SET" : "RTC OFFLINE");
    }

    if (!health->pmu_online) {
        lv_label_set_text(s_battery_label, "POWER UNAVAILABLE");
    } else if (!health->battery_present) {
        lv_label_set_text(s_battery_label, health->usb_present ? "USB POWER" : "NO BATTERY");
    } else if (health->battery_percent >= 0) {
        lv_label_set_text_fmt(s_battery_label, "%d%%  /  %s", health->battery_percent,
                              health->charging ? "CHARGING" :
                              (health->usb_present ? "USB" : "BATTERY"));
    } else {
        lv_label_set_text_fmt(s_battery_label, "%u mV  /  %s", health->battery_mv,
                              health->usb_present ? "USB" : "BATTERY");
    }

    lv_label_set_text_fmt(s_wifi_label, "WI-FI %s",
                          network_service_state_name(network->state));
    lv_obj_set_style_text_color(s_wifi_label,
        lv_color_hex(network->radio_active ? WF_COLOR_PRIMARY : WF_COLOR_MUTED), 0);
    lv_label_set_text(s_ble_label, ble->scanning ? "BLE SCAN" :
                      (ble->host_ready ? "BLE READY" : "BLE OFF"));
    lv_obj_set_style_text_color(s_ble_label,
        lv_color_hex(ble->scanning ? WF_COLOR_PRIMARY : WF_COLOR_MUTED), 0);

    lv_label_set_text(s_sync_method_label, sync->method);
    lv_label_set_text(s_sync_status_label, sync->status);
    lv_label_set_text(s_sync_detail_label, sync->detail);
    lv_label_set_text_fmt(s_sync_last_label, "LAST / %s\n%s",
                          sync->last_source[0] ? sync->last_source : "NEVER",
                          sync->last_local[0] ? sync->last_local : "NO SUCCESS YET");
    lv_label_set_text(s_sync_close_label, sync->active ? "CANCEL" : "DISMISS");
    lv_obj_set_style_text_color(s_sync_status_label,
        lv_color_hex(sync->state == TIME_SYNC_SUCCESS ? WF_COLOR_SUCCESS :
                     sync->state == TIME_SYNC_FAILED ? WF_COLOR_WARNING :
                     WF_COLOR_TEXT), 0);

    watch_ui_model_t model = {
        .health = *health,
        .display_state = display_state,
        .wake_cause = wake_cause,
        .light_sleep_count = light_sleep_count,
        .last_light_sleep_us = last_light_sleep_us,
        .network = *network,
        .ble = ble,
        .sync = *sync,
    };
    wf_app_manager_update(&model);
}
