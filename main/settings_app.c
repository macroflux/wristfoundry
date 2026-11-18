#include "settings_app.h"

#include <stdio.h>
#include <string.h>
#include "esp_err.h"
#include "lvgl.h"
#include "network_service.h"
#include "time_sync_service.h"
#include "ui_design.h"
#include "watch_ui.h"

typedef struct {
    const char *label;
    const char *posix;
} timezone_option_t;

static const timezone_option_t s_timezone_options[] = {
    {"US Eastern (UTC-5/-4 DST)", "EST5EDT,M3.2.0,M11.1.0"},
    {"US Central (UTC-6/-5 DST)", "CST6CDT,M3.2.0,M11.1.0"},
    {"US Mountain (UTC-7/-6 DST)", "MST7MDT,M3.2.0,M11.1.0"},
    {"US Pacific (UTC-8/-7 DST)", "PST8PDT,M3.2.0,M11.1.0"},
    {"Arizona (UTC-7)", "MST7"},
    {"Alaska (UTC-9/-8 DST)", "AKST9AKDT,M3.2.0,M11.1.0"},
    {"Hawaii (UTC-10)", "HST10"},
    {"UTC-12:00", "UTC12"}, {"UTC-11:00", "UTC11"},
    {"UTC-10:00", "UTC10"}, {"UTC-09:30", "UTC9:30"},
    {"UTC-09:00", "UTC9"}, {"UTC-08:00", "UTC8"},
    {"UTC-07:00", "UTC7"}, {"UTC-06:00", "UTC6"},
    {"UTC-05:00", "UTC5"}, {"UTC-04:00", "UTC4"},
    {"UTC-03:30", "UTC3:30"}, {"UTC-03:00", "UTC3"},
    {"UTC-02:00", "UTC2"}, {"UTC-01:00", "UTC1"},
    {"UTC+00:00", "UTC0"}, {"UTC+01:00", "UTC-1"},
    {"UTC+02:00", "UTC-2"}, {"UTC+03:00", "UTC-3"},
    {"UTC+03:30", "UTC-3:30"}, {"UTC+04:00", "UTC-4"},
    {"UTC+04:30", "UTC-4:30"}, {"UTC+05:00", "UTC-5"},
    {"UTC+05:30", "UTC-5:30"}, {"UTC+05:45", "UTC-5:45"},
    {"UTC+06:00", "UTC-6"}, {"UTC+06:30", "UTC-6:30"},
    {"UTC+07:00", "UTC-7"}, {"UTC+08:00", "UTC-8"},
    {"UTC+08:45", "UTC-8:45"}, {"UTC+09:00", "UTC-9"},
    {"UTC+09:30", "UTC-9:30"}, {"UTC+10:00", "UTC-10"},
    {"UTC+10:30", "UTC-10:30"}, {"UTC+11:00", "UTC-11"},
    {"UTC+12:00", "UTC-12"}, {"UTC+12:45", "UTC-12:45"},
    {"UTC+13:00", "UTC-13"}, {"UTC+14:00", "UTC-14"},
};

#define TIMEZONE_OPTION_COUNT (sizeof(s_timezone_options) / sizeof(s_timezone_options[0]))
#define TIMEZONE_OPTION_TEXT_SIZE 1536
#define SCAN_OPTION_TEXT_SIZE (NETWORK_SCAN_MAX_RESULTS * 56)

typedef struct {
    lv_obj_t *content;
    lv_obj_t *scan_picker;
    lv_obj_t *ssid;
    lv_obj_t *password;
    lv_obj_t *timezone_picker;
    lv_obj_t *keyboard;
    lv_obj_t *status;
    uint32_t scan_generation;
    size_t scan_count;
    network_scan_result_t scan_results[NETWORK_SCAN_MAX_RESULTS];
} settings_context_t;

static settings_context_t s_settings;
static char s_timezone_option_text[TIMEZONE_OPTION_TEXT_SIZE];
static char s_scan_option_text[SCAN_OPTION_TEXT_SIZE];
static void hide_keyboard(void);

static const char *selected_timezone(void)
{
    const uint32_t selected = lv_dropdown_get_selected(s_settings.timezone_picker);
    return selected < TIMEZONE_OPTION_COUNT ? s_timezone_options[selected].posix : "UTC0";
}

static void select_timezone(const char *posix)
{
    uint32_t selected = 21; /* UTC+00:00 */
    for (uint32_t i = 0; i < TIMEZONE_OPTION_COUNT; ++i) {
        if (strcmp(s_timezone_options[i].posix, posix) == 0) {
            selected = i;
            break;
        }
    }
    lv_dropdown_set_selected(s_settings.timezone_picker, selected);
}

static void timezone_event(lv_event_t *event)
{
    (void)event;
    watch_ui_note_activity();
    hide_keyboard();
}

static void hide_keyboard(void)
{
    if (s_settings.keyboard == NULL) return;
    lv_keyboard_set_textarea(s_settings.keyboard, NULL);
    lv_obj_add_flag(s_settings.keyboard, LV_OBJ_FLAG_HIDDEN);
    if (s_settings.content != NULL) {
        lv_obj_set_height(s_settings.content,
                          WF_DISPLAY_HEIGHT - WF_HEADER_HEIGHT);
        lv_obj_align(s_settings.content, LV_ALIGN_BOTTOM_MID, 0, 0);
    }
}

static void back_event(lv_event_t *event)
{
    (void)event;
    watch_ui_note_activity();
    hide_keyboard();
    wf_app_manager_show_launcher();
}

static void field_event(lv_event_t *event)
{
    watch_ui_note_activity();
    lv_obj_t *field = lv_event_get_target(event);
    lv_keyboard_set_textarea(s_settings.keyboard, field);
    lv_obj_remove_flag(s_settings.keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_height(s_settings.content,
                      WF_DISPLAY_HEIGHT - WF_HEADER_HEIGHT - 218);
    lv_obj_align(s_settings.content, LV_ALIGN_TOP_MID, 0, WF_HEADER_HEIGHT);
    lv_obj_update_layout(s_settings.content);
    lv_obj_scroll_to_view(field, LV_ANIM_ON);
}

static void field_defocused_event(lv_event_t *event)
{
    (void)event;
    hide_keyboard();
}

static void content_event(lv_event_t *event)
{
    if (lv_event_get_target(event) == s_settings.content) {
        watch_ui_note_activity();
        hide_keyboard();
    }
}

static void keyboard_event(lv_event_t *event)
{
    const lv_event_code_t code = lv_event_get_code(event);
    watch_ui_note_activity();
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        hide_keyboard();
    }
}

static void save_event(lv_event_t *event)
{
    (void)event;
    watch_ui_note_activity();
    hide_keyboard();
    const esp_err_t result = network_service_save_configuration(
        lv_textarea_get_text(s_settings.ssid),
        lv_textarea_get_text(s_settings.password),
        selected_timezone());
    if (result == ESP_OK) {
        lv_label_set_text(
            s_settings.status,
            network_service_credentials_are_encrypted()
                ? "Saved securely. Tap CONNECT or SYNC TIME."
                : "Ready until reboot. Tap CONNECT or SYNC TIME.");
        lv_textarea_set_text(s_settings.password, "");
    } else if (result == ESP_ERR_INVALID_STATE) {
        lv_label_set_text(s_settings.status,
                          "Save blocked: encrypted credential storage is unavailable.");
    } else {
        lv_label_set_text_fmt(s_settings.status, "Save failed: %s", esp_err_to_name(result));
    }
}

static void clear_event(lv_event_t *event)
{
    (void)event;
    watch_ui_note_activity();
    hide_keyboard();
    const esp_err_t result = network_service_clear_configuration();
    if (result == ESP_OK) {
        lv_textarea_set_text(s_settings.ssid, "");
        lv_textarea_set_text(s_settings.password, "");
        select_timezone("UTC0");
        lv_label_set_text(s_settings.status, "Saved network removed.");
    } else {
        lv_label_set_text_fmt(s_settings.status, "Clear failed: %s", esp_err_to_name(result));
    }
}

static void connect_event(lv_event_t *event)
{
    (void)event;
    watch_ui_note_activity();
    hide_keyboard();
    network_service_request_manual_connect();
    lv_label_set_text(s_settings.status, "Connection requested.");
}

static void sync_event(lv_event_t *event)
{
    (void)event;
    watch_ui_note_activity();
    hide_keyboard();
    const bool accepted = time_sync_service_request_auto();
    lv_label_set_text(s_settings.status,
                      accepted ? "NTP synchronization requested." : "Sync is busy; try again.");
}

static void scan_event(lv_event_t *event)
{
    (void)event;
    watch_ui_note_activity();
    hide_keyboard();
    const bool accepted = network_service_request_scan();
    lv_label_set_text(s_settings.status,
                      accepted ? "Scanning nearby Wi-Fi networks..." :
                                 "Wi-Fi is busy; try scanning again shortly.");
}

static void scan_picker_event(lv_event_t *event)
{
    (void)event;
    watch_ui_note_activity();
    hide_keyboard();
    const uint32_t selected = lv_dropdown_get_selected(s_settings.scan_picker);
    if (selected >= s_settings.scan_count) return;
    lv_textarea_set_text(s_settings.ssid, s_settings.scan_results[selected].ssid);
    lv_label_set_text_fmt(s_settings.status, "Selected %s. Enter its passkey.",
                          s_settings.scan_results[selected].ssid);
}

static void update_scan_results(const network_snapshot_t *network)
{
    if (network->scan_generation == s_settings.scan_generation) return;
    s_settings.scan_generation = network->scan_generation;
    s_settings.scan_count = network->scan_count;
    memcpy(s_settings.scan_results, network->scan_results,
           sizeof(s_settings.scan_results));

    if (network->scan_count == 0) {
        lv_dropdown_set_options(s_settings.scan_picker, "No networks found");
        lv_obj_add_state(s_settings.scan_picker, LV_STATE_DISABLED);
        lv_label_set_text_fmt(s_settings.status, "Wi-Fi scan: %s.",
                              network->last_scan_result);
        return;
    }

    memset(s_scan_option_text, 0, sizeof(s_scan_option_text));
    size_t used = 0;
    for (size_t i = 0; i < network->scan_count; ++i) {
        const network_scan_result_t *entry = &network->scan_results[i];
        const int written = snprintf(s_scan_option_text + used,
                                     sizeof(s_scan_option_text) - used,
                                     "%s  (%d dBm, %s)%s", entry->ssid,
                                     entry->rssi, entry->secured ? "secured" : "open",
                                     i + 1 < network->scan_count ? "\n" : "");
        if (written < 0 ||
            (size_t)written >= sizeof(s_scan_option_text) - used) break;
        used += (size_t)written;
    }
    lv_dropdown_set_options(s_settings.scan_picker, s_scan_option_text);
    lv_obj_remove_state(s_settings.scan_picker, LV_STATE_DISABLED);
    lv_label_set_text_fmt(s_settings.status, "Found %u networks. Tap one to select it.",
                          (unsigned)network->scan_count);
}

static lv_obj_t *make_field(lv_obj_t *parent, const char *label, uint32_t max_length)
{
    wf_ui_label_create(parent, label, &lv_font_montserrat_14, WF_COLOR_MUTED);
    lv_obj_t *field = lv_textarea_create(parent);
    lv_obj_set_width(field, 350);
    lv_obj_set_height(field, 52);
    lv_textarea_set_one_line(field, true);
    lv_textarea_set_max_length(field, max_length);
    lv_obj_add_event_cb(field, field_event, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(field, field_event, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(field, field_defocused_event, LV_EVENT_DEFOCUSED, NULL);
    return field;
}

void settings_app_create(wf_app_t *app)
{
    app->screen = wf_ui_screen_create();
    wf_ui_header_create(app->screen, app->icon, app->name, "BACK", back_event, NULL);

    s_settings.content = lv_obj_create(app->screen);
    lv_obj_remove_style_all(s_settings.content);
    lv_obj_set_size(s_settings.content, WF_DISPLAY_WIDTH,
                    WF_DISPLAY_HEIGHT - WF_HEADER_HEIGHT);
    lv_obj_align(s_settings.content, LV_ALIGN_BOTTOM_MID, 0, 0);
    wf_ui_make_scrollable(s_settings.content);
    lv_obj_set_flex_flow(s_settings.content, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_event_cb(s_settings.content, content_event, LV_EVENT_CLICKED, NULL);

    lv_obj_t *security = wf_ui_card_create(s_settings.content);
    lv_obj_set_size(security, 362, 86);
    lv_obj_t *security_label = wf_ui_label_create(
        security,
        network_service_credentials_are_encrypted() ?
            "CREDENTIAL STORE / ENCRYPTED NVS" :
            "CREDENTIAL STORE / VOLATILE (CLEARED ON REBOOT)",
        &lv_font_montserrat_14,
        network_service_credentials_are_encrypted() ? WF_COLOR_PRIMARY : WF_COLOR_WARNING);
    lv_obj_set_width(security_label, 326);
    lv_label_set_long_mode(security_label, LV_LABEL_LONG_WRAP);

    lv_obj_t *scan = wf_ui_button_create(s_settings.content, "SCAN WI-FI NETWORKS",
                                         scan_event, NULL);
    lv_obj_set_size(scan, 362, 52);
    s_settings.scan_picker = lv_dropdown_create(s_settings.content);
    lv_obj_set_size(s_settings.scan_picker, 350, 52);
    lv_dropdown_set_options(s_settings.scan_picker, "Tap SCAN WI-FI NETWORKS");
    lv_obj_add_state(s_settings.scan_picker, LV_STATE_DISABLED);
    lv_obj_add_event_cb(s_settings.scan_picker, scan_picker_event,
                        LV_EVENT_VALUE_CHANGED, NULL);

    s_settings.ssid = make_field(s_settings.content, "WI-FI SSID", 32);
    s_settings.password = make_field(s_settings.content, "WI-FI PASSWORD", 64);
    lv_textarea_set_password_mode(s_settings.password, true);
    wf_ui_label_create(s_settings.content, "TIME ZONE", &lv_font_montserrat_14,
                       WF_COLOR_MUTED);
    s_settings.timezone_picker = lv_dropdown_create(s_settings.content);
    lv_obj_set_size(s_settings.timezone_picker, 350, 52);
    memset(s_timezone_option_text, 0, sizeof(s_timezone_option_text));
    size_t timezone_used = 0;
    for (size_t i = 0; i < TIMEZONE_OPTION_COUNT; ++i) {
        const int written = snprintf(s_timezone_option_text + timezone_used,
                                     sizeof(s_timezone_option_text) - timezone_used,
                                     "%s%s", s_timezone_options[i].label,
                                     i + 1 < TIMEZONE_OPTION_COUNT ? "\n" : "");
        if (written < 0 ||
            (size_t)written >= sizeof(s_timezone_option_text) - timezone_used) break;
        timezone_used += (size_t)written;
    }
    lv_dropdown_set_options(s_settings.timezone_picker, s_timezone_option_text);
    select_timezone("UTC0");
    lv_obj_add_event_cb(s_settings.timezone_picker, timezone_event,
                        LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(s_settings.timezone_picker, timezone_event,
                        LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *row = lv_obj_create(s_settings.content);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, 362, 58);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *save = wf_ui_button_create(
        row, network_service_credentials_are_encrypted() ? "SAVE" : "USE ONCE",
        save_event, NULL);
    lv_obj_set_width(save, 112);
    lv_obj_t *clear = wf_ui_button_create(row, "CLEAR", clear_event, NULL);
    lv_obj_set_width(clear, 112);
    lv_obj_t *connect = wf_ui_button_create(row, "CONNECT", connect_event, NULL);
    lv_obj_set_width(connect, 112);

    lv_obj_t *sync = wf_ui_button_create(s_settings.content, "SYNC TIME WITH NTP", sync_event, NULL);
    lv_obj_set_size(sync, 362, 56);
    s_settings.status = wf_ui_label_create(s_settings.content, "Ready.", &lv_font_montserrat_14,
                                            WF_COLOR_MUTED);
    lv_obj_set_width(s_settings.status, 350);
    lv_label_set_long_mode(s_settings.status, LV_LABEL_LONG_WRAP);

    s_settings.keyboard = lv_keyboard_create(app->screen);
    lv_obj_set_size(s_settings.keyboard, WF_DISPLAY_WIDTH, 218);
    lv_obj_align(s_settings.keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(s_settings.keyboard, keyboard_event, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(s_settings.keyboard, keyboard_event, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(s_settings.keyboard, keyboard_event, LV_EVENT_CANCEL, NULL);
    lv_obj_add_flag(s_settings.keyboard, LV_OBJ_FLAG_HIDDEN);
}

void settings_app_show(wf_app_t *app)
{
    (void)app;
    network_snapshot_t network;
    network_service_get_snapshot(&network);
    update_scan_results(&network);
    lv_textarea_set_text(s_settings.ssid,
                         network.credentials_present ? network.ssid : "");
    if (network.timezone[0] != '\0' && strcmp(network.timezone, "NOT SET") != 0) {
        select_timezone(network.timezone);
    }
}

void settings_app_hide(wf_app_t *app)
{
    (void)app;
    hide_keyboard();
}

void settings_app_update(wf_app_t *app, const void *model)
{
    (void)app;
    (void)model;
    network_snapshot_t network;
    network_service_get_snapshot(&network);
    if (network.scan_in_progress) {
        lv_label_set_text(s_settings.status, "Scanning nearby Wi-Fi networks...");
    }
    update_scan_results(&network);
}

void settings_app_destroy(wf_app_t *app)
{
    if (app->screen != NULL) {
        lv_obj_delete(app->screen);
        app->screen = NULL;
    }
    memset(&s_settings, 0, sizeof(s_settings));
}
