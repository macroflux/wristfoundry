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
    lv_obj_t *ssid;
    lv_obj_t *password;
    lv_obj_t *timezone;
    lv_obj_t *keyboard;
    lv_obj_t *status;
} settings_context_t;

static settings_context_t s_settings;

static void back_event(lv_event_t *event)
{
    (void)event;
    watch_ui_note_activity();
    wf_app_manager_show_launcher();
}

static void field_event(lv_event_t *event)
{
    watch_ui_note_activity();
    lv_obj_t *field = lv_event_get_target(event);
    lv_keyboard_set_textarea(s_settings.keyboard, field);
    lv_obj_remove_flag(s_settings.keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_scroll_to_view(field, LV_ANIM_ON);
}

static void keyboard_event(lv_event_t *event)
{
    const lv_event_code_t code = lv_event_get_code(event);
    watch_ui_note_activity();
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        lv_keyboard_set_textarea(s_settings.keyboard, NULL);
        lv_obj_add_flag(s_settings.keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

static void save_event(lv_event_t *event)
{
    (void)event;
    watch_ui_note_activity();
    const esp_err_t result = network_service_save_configuration(
        lv_textarea_get_text(s_settings.ssid),
        lv_textarea_get_text(s_settings.password),
        lv_textarea_get_text(s_settings.timezone));
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
    const esp_err_t result = network_service_clear_configuration();
    if (result == ESP_OK) {
        lv_textarea_set_text(s_settings.ssid, "");
        lv_textarea_set_text(s_settings.password, "");
        lv_label_set_text(s_settings.status, "Saved network removed.");
    } else {
        lv_label_set_text_fmt(s_settings.status, "Clear failed: %s", esp_err_to_name(result));
    }
}

static void connect_event(lv_event_t *event)
{
    (void)event;
    watch_ui_note_activity();
    network_service_request_manual_connect();
    lv_label_set_text(s_settings.status, "Connection requested.");
}

static void sync_event(lv_event_t *event)
{
    (void)event;
    watch_ui_note_activity();
    const bool accepted = time_sync_service_request_auto();
    lv_label_set_text(s_settings.status,
                      accepted ? "NTP synchronization requested." : "Sync is busy; try again.");
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
    return field;
}

void settings_app_create(wf_app_t *app)
{
    app->screen = wf_ui_screen_create();
    wf_ui_header_create(app->screen, app->icon, app->name, "BACK", back_event, NULL);

    lv_obj_t *content = lv_obj_create(app->screen);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, WF_DISPLAY_WIDTH, WF_DISPLAY_HEIGHT - WF_HEADER_HEIGHT);
    lv_obj_align(content, LV_ALIGN_BOTTOM_MID, 0, 0);
    wf_ui_make_scrollable(content);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *security = wf_ui_card_create(content);
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

    s_settings.ssid = make_field(content, "WI-FI SSID", 32);
    s_settings.password = make_field(content, "WI-FI PASSWORD", 64);
    lv_textarea_set_password_mode(s_settings.password, true);
    s_settings.timezone = make_field(content, "POSIX TIMEZONE", 63);
    lv_textarea_set_placeholder_text(s_settings.timezone, "EST5EDT,M3.2.0,M11.1.0");

    lv_obj_t *row = lv_obj_create(content);
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

    lv_obj_t *sync = wf_ui_button_create(content, "SYNC TIME WITH NTP", sync_event, NULL);
    lv_obj_set_size(sync, 362, 56);
    s_settings.status = wf_ui_label_create(content, "Ready.", &lv_font_montserrat_14,
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
    lv_textarea_set_text(s_settings.ssid,
                         network.credentials_present ? network.ssid : "");
    if (network.timezone[0] != '\0' && strcmp(network.timezone, "NOT SET") != 0) {
        lv_textarea_set_text(s_settings.timezone, network.timezone);
    }
}

void settings_app_hide(wf_app_t *app)
{
    (void)app;
    if (s_settings.keyboard != NULL) lv_obj_add_flag(s_settings.keyboard, LV_OBJ_FLAG_HIDDEN);
}

void settings_app_update(wf_app_t *app, const void *model)
{
    (void)app;
    (void)model;
}

void settings_app_destroy(wf_app_t *app)
{
    if (app->screen != NULL) {
        lv_obj_delete(app->screen);
        app->screen = NULL;
    }
}
