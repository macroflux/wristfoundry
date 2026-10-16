#pragma once

#include "lvgl.h"

enum {
    WF_DISPLAY_WIDTH = 410,
    WF_DISPLAY_HEIGHT = 502,
    WF_SAFE_TOP = 10,
    WF_SPACE_XS = 6,
    WF_SPACE_SM = 10,
    WF_SPACE_MD = 16,
    WF_SPACE_LG = 22,
    WF_HEADER_HEIGHT = 82,
    WF_TOUCH_TARGET = 54,
    WF_RADIUS = 18,
};

#define WF_COLOR_BACKGROUND 0x03070B
#define WF_COLOR_PANEL      0x0B1820
#define WF_COLOR_PANEL_ALT  0x102832
#define WF_COLOR_PRIMARY    0x56DBF4
#define WF_COLOR_TEXT       0xEAF2F7
#define WF_COLOR_MUTED      0x8296A1
#define WF_COLOR_BORDER     0x214652
#define WF_COLOR_SUCCESS    0x63E6A5
#define WF_COLOR_WARNING    0xFFCB69

typedef void (*wf_ui_action_cb_t)(lv_event_t *event);

void wf_ui_design_init(void);
void wf_ui_set_activity_callback(wf_ui_action_cb_t callback);
void wf_ui_report_activity(void);
lv_obj_t *wf_ui_screen_create(void);
lv_obj_t *wf_ui_header_create(lv_obj_t *parent, const char *icon,
                              const char *title, const char *action_text,
                              wf_ui_action_cb_t action_cb, void *user_data);
lv_obj_t *wf_ui_button_create(lv_obj_t *parent, const char *text,
                              wf_ui_action_cb_t action_cb, void *user_data);
lv_obj_t *wf_ui_card_create(lv_obj_t *parent);
lv_obj_t *wf_ui_label_create(lv_obj_t *parent, const char *text,
                             const lv_font_t *font, uint32_t color);
void wf_ui_make_scrollable(lv_obj_t *object);
