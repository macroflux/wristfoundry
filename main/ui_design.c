#include "ui_design.h"

static lv_style_t s_screen_style;
static lv_style_t s_card_style;
static lv_style_t s_button_style;
static lv_style_t s_button_pressed_style;
static bool s_initialized;
static wf_ui_action_cb_t s_activity_callback;

void wf_ui_set_activity_callback(wf_ui_action_cb_t callback)
{
    s_activity_callback = callback;
}

void wf_ui_report_activity(void)
{
    if (s_activity_callback != NULL) {
        s_activity_callback(NULL);
    }
}

static void scroll_activity_event(lv_event_t *event)
{
    (void)event;
    wf_ui_report_activity();
}

void wf_ui_design_init(void)
{
    if (s_initialized) {
        return;
    }

    lv_style_init(&s_screen_style);
    lv_style_set_bg_color(&s_screen_style, lv_color_hex(WF_COLOR_BACKGROUND));
    lv_style_set_bg_opa(&s_screen_style, LV_OPA_COVER);
    lv_style_set_text_color(&s_screen_style, lv_color_hex(WF_COLOR_TEXT));
    lv_style_set_border_width(&s_screen_style, 0);
    lv_style_set_pad_all(&s_screen_style, 0);

    lv_style_init(&s_card_style);
    lv_style_set_bg_color(&s_card_style, lv_color_hex(WF_COLOR_PANEL));
    lv_style_set_bg_opa(&s_card_style, LV_OPA_COVER);
    lv_style_set_border_color(&s_card_style, lv_color_hex(WF_COLOR_BORDER));
    lv_style_set_border_width(&s_card_style, 1);
    lv_style_set_radius(&s_card_style, WF_RADIUS);
    lv_style_set_pad_all(&s_card_style, WF_SPACE_MD);

    lv_style_init(&s_button_style);
    lv_style_set_bg_color(&s_button_style, lv_color_hex(WF_COLOR_PANEL_ALT));
    lv_style_set_bg_opa(&s_button_style, LV_OPA_COVER);
    lv_style_set_border_color(&s_button_style, lv_color_hex(WF_COLOR_BORDER));
    lv_style_set_border_width(&s_button_style, 1);
    lv_style_set_radius(&s_button_style, WF_RADIUS);
    lv_style_set_text_color(&s_button_style, lv_color_hex(WF_COLOR_TEXT));

    lv_style_init(&s_button_pressed_style);
    lv_style_set_bg_color(&s_button_pressed_style, lv_color_hex(0x165269));
    lv_style_set_border_color(&s_button_pressed_style, lv_color_hex(WF_COLOR_PRIMARY));

    s_initialized = true;
}

lv_obj_t *wf_ui_screen_create(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(screen);
    lv_obj_add_style(screen, &s_screen_style, 0);
    lv_obj_set_size(screen, WF_DISPLAY_WIDTH, WF_DISPLAY_HEIGHT);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    if (s_activity_callback != NULL) {
        lv_obj_add_event_cb(screen, s_activity_callback, LV_EVENT_PRESSED, NULL);
    }
    return screen;
}

lv_obj_t *wf_ui_label_create(lv_obj_t *parent, const char *text,
                             const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    if (font != NULL) {
        lv_obj_set_style_text_font(label, font, 0);
    }
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    return label;
}

lv_obj_t *wf_ui_header_create(lv_obj_t *parent, const char *icon,
                              const char *title, const char *action_text,
                              wf_ui_action_cb_t action_cb, void *user_data)
{
    lv_obj_t *header = lv_obj_create(parent);
    lv_obj_remove_style_all(header);
    lv_obj_set_size(header, WF_DISPLAY_WIDTH, WF_HEADER_HEIGHT);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(WF_COLOR_BACKGROUND), 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);

    lv_obj_t *icon_label = wf_ui_label_create(header, icon, &lv_font_montserrat_20,
                                               WF_COLOR_PRIMARY);
    lv_obj_set_width(icon_label, 42);
    lv_obj_set_style_text_align(icon_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(icon_label, LV_ALIGN_LEFT_MID, WF_SPACE_LG, WF_SAFE_TOP / 2);

    lv_obj_t *title_label = wf_ui_label_create(header, title, &lv_font_montserrat_20,
                                                WF_COLOR_TEXT);
    lv_obj_align(title_label, LV_ALIGN_LEFT_MID, 70, WF_SAFE_TOP / 2);

    if (action_text != NULL && action_cb != NULL) {
        lv_obj_t *action = lv_button_create(header);
        lv_obj_remove_style_all(action);
        lv_obj_add_style(action, &s_button_style, 0);
        lv_obj_add_style(action, &s_button_pressed_style, LV_STATE_PRESSED);
        lv_obj_set_size(action, 82, WF_TOUCH_TARGET);
        lv_obj_align(action, LV_ALIGN_RIGHT_MID, -WF_SPACE_MD, WF_SAFE_TOP / 2);
        lv_obj_add_event_cb(action, action_cb, LV_EVENT_CLICKED, user_data);
        lv_obj_t *label = wf_ui_label_create(action, action_text, &lv_font_montserrat_14,
                                              WF_COLOR_TEXT);
        lv_obj_center(label);
    }
    return header;
}

lv_obj_t *wf_ui_button_create(lv_obj_t *parent, const char *text,
                              wf_ui_action_cb_t action_cb, void *user_data)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_add_style(button, &s_button_style, 0);
    lv_obj_add_style(button, &s_button_pressed_style, LV_STATE_PRESSED);
    lv_obj_set_height(button, WF_TOUCH_TARGET);
    if (action_cb != NULL) {
        lv_obj_add_event_cb(button, action_cb, LV_EVENT_CLICKED, user_data);
    }
    lv_obj_t *label = wf_ui_label_create(button, text, &lv_font_montserrat_14,
                                          WF_COLOR_TEXT);
    lv_obj_center(label);
    return button;
}

lv_obj_t *wf_ui_card_create(lv_obj_t *parent)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_add_style(card, &s_card_style, 0);
    return card;
}

void wf_ui_make_scrollable(lv_obj_t *object)
{
    lv_obj_add_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(object, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(object, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_bg_opa(object, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(object, 0, 0);
    lv_obj_set_style_pad_left(object, WF_SPACE_LG, 0);
    lv_obj_set_style_pad_right(object, WF_SPACE_LG, 0);
    lv_obj_set_style_pad_top(object, WF_SPACE_SM, 0);
    lv_obj_set_style_pad_bottom(object, WF_SPACE_LG, 0);
    lv_obj_set_style_pad_row(object, WF_SPACE_SM, 0);
    lv_obj_add_event_cb(object, scroll_activity_event, LV_EVENT_SCROLL_BEGIN, NULL);
}
