#include "companion_service.h"
#include <inttypes.h>
#include <stdio.h>

#include "board_services.h"
#include "ble_radar_service.h"
#include "ble_radio_owner.h"
#include "phone_service.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "driver/gpio.h"
#include "driver/usb_serial_jtag.h"
#include "esp_app_desc.h"
#include "esp_attr.h"
#include "esp_chip_info.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_netif.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "network_service.h"
#include "nvs_flash.h"
#include "settings_service.h"
#include "sdkconfig.h"
#include "watch_ui.h"
#include "time_sync_service.h"
#include "usb_sleep_policy.h"
#include "hal/usb_serial_jtag_ll.h"

#define WRISTFOUNDRY_VERSION "0.2.0"
#define HOME_APPS_GPIO GPIO_NUM_0
#define PWR_WAKE_GPIO GPIO_NUM_10
#define POWER_KEY_POLL_MS 20
#define HEALTH_POLL_TICKS (1000 / POWER_KEY_POLL_MS)
#define POWER_KEY_RELEASE_TICKS 3
#define ACTIVE_BRIGHTNESS_PERCENT 100
#define DIM_BRIGHTNESS_PERCENT 18
#define DIM_TIMEOUT_MS 10000
#define SLEEP_TIMEOUT_MS 25000
#define POWER_TEST_NORMAL 0
#define POWER_TEST_ACTIVE 1
#define POWER_TEST_DISPLAY_OFF 2
#define POWER_TEST_LIGHT_SLEEP 3
#define POWER_TEST_DEEP_SLEEP 4
#define POWER_TEST_WIFI_ACTIVE 5
#define POWER_TEST_SETTLE_MS 10000
#define WIFI_SCAN_PAUSE_MS 1000

#ifndef WRISTFOUNDRY_POWER_TEST_MODE
#define WRISTFOUNDRY_POWER_TEST_MODE POWER_TEST_NORMAL
#endif

static const char *TAG = "wristfoundry";
static lv_obj_t *power_value_label;
static lv_obj_t *rtc_value_label;
static lv_obj_t *imu_value_label;
static lv_obj_t *touch_status_label;
static lv_obj_t *sleep_status_label;
static uint32_t touch_count;
static bool screen_awake = true;
static bool screen_dimmed;
static int64_t lifecycle_activity_us;
static uint32_t light_sleep_count;
static int64_t total_light_sleep_us;
static int64_t last_light_sleep_us;
static esp_sleep_wakeup_cause_t last_wake_cause;
static volatile uint32_t power_key_edge_count;
static bool power_key_press_latched;
static uint8_t power_key_release_ticks;
static bool home_key_press_latched;
static uint8_t home_key_press_ticks;
static uint8_t home_key_release_ticks;
RTC_DATA_ATTR static uint32_t retained_boot_count;

static void enter_battery_light_sleep(void);

static bool usb_tether_blocks_sleep(void)
{
    bool present = true;
    const esp_err_t result = board_services_usb_power_present(&present);
    return wf_usb_blocks_sleep(result == ESP_OK, present);
}

static const char *display_state_name(void)
{
    if (!screen_awake) {
        return "OFF";
    }
    return screen_dimmed ? "DIM" : "ACTIVE";
}

static const char *power_test_mode_name(void)
{
    switch (WRISTFOUNDRY_POWER_TEST_MODE) {
    case POWER_TEST_ACTIVE:
        return "active";
    case POWER_TEST_DISPLAY_OFF:
        return "display-off";
    case POWER_TEST_LIGHT_SLEEP:
        return "light-sleep";
    case POWER_TEST_DEEP_SLEEP:
        return "deep-sleep-experimental";
    case POWER_TEST_WIFI_ACTIVE:
        return "wifi-active-scan";
    case POWER_TEST_NORMAL:
    default:
        return "normal";
    }
}

static const char *reset_reason_name(esp_reset_reason_t reason)
{
    switch (reason) {
    case ESP_RST_POWERON:
        return "POWER";
    case ESP_RST_EXT:
        return "EXTERNAL";
    case ESP_RST_SW:
        return "SOFTWARE";
    case ESP_RST_PANIC:
        return "PANIC";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:
        return "WATCHDOG";
    case ESP_RST_DEEPSLEEP:
        return "DEEP-SLEEP";
    case ESP_RST_BROWNOUT:
        return "BROWNOUT";
    case ESP_RST_USB:
        return "USB";
    case ESP_RST_JTAG:
        return "JTAG";
    case ESP_RST_PWR_GLITCH:
        return "PWR-GLITCH";
    case ESP_RST_CPU_LOCKUP:
        return "CPU-LOCKUP";
    case ESP_RST_SDIO:
        return "SDIO";
    case ESP_RST_EFUSE:
        return "EFUSE";
    case ESP_RST_UNKNOWN:
    default:
        return "UNKNOWN";
    }
}

static void touch_button_event(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    touch_count++;
    lv_label_set_text_fmt(touch_status_label, "TOUCH OK  #%" PRIu32, touch_count);
    ESP_LOGI(TAG, "Touch verified (#%" PRIu32 ")", touch_count);
}

static void create_health_card(lv_obj_t *screen, int y, const char *title,
                               lv_obj_t **value_label)
{
    lv_obj_t *card = lv_obj_create(screen);
    lv_obj_set_size(card, 360, 88);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_set_style_radius(card, 18, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x0B141B), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x173845), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_pad_all(card, 10, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *heading = lv_label_create(card);
    lv_label_set_text(heading, title);
    lv_obj_set_style_text_color(heading, lv_color_hex(0x49E6FF), 0);
    lv_obj_align(heading, LV_ALIGN_TOP_LEFT, 0, 0);

    *value_label = lv_label_create(card);
    lv_label_set_text(*value_label, "CHECKING...");
    lv_obj_set_style_text_color(*value_label, lv_color_hex(0xE8F0F7), 0);
    lv_obj_set_style_text_font(*value_label, &lv_font_montserrat_20, 0);
    lv_obj_align(*value_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);
}

static void create_health_ui(void)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x05070B), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "WRISTFOUNDRY");
    lv_obj_set_style_text_color(title, lv_color_hex(0x49E6FF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 22);

    lv_obj_t *version = lv_label_create(screen);
    lv_label_set_text_fmt(version, "v" WRISTFOUNDRY_VERSION "  /  BOOT: %s",
                          reset_reason_name(esp_reset_reason()));
    lv_obj_set_style_text_color(version, lv_color_hex(0x98A6B8), 0);
    lv_obj_align(version, LV_ALIGN_TOP_MID, 0, 62);

    create_health_card(screen, 92, "POWER / BATTERY", &power_value_label);
    create_health_card(screen, 188, "REAL-TIME CLOCK", &rtc_value_label);
    create_health_card(screen, 284, "MOTION SENSOR", &imu_value_label);

    sleep_status_label = lv_label_create(screen);
    lv_label_set_text(sleep_status_label, "SLEEP: READY");
    lv_obj_set_style_text_color(sleep_status_label, lv_color_hex(0x98A6B8), 0);
    lv_obj_align(sleep_status_label, LV_ALIGN_TOP_MID, 0, 384);

    lv_obj_t *button = lv_button_create(screen);
    lv_obj_set_size(button, 320, 68);
    lv_obj_align(button, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_radius(button, 22, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x0F3540), LV_PART_MAIN);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x147A91), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(button, lv_color_hex(0x49E6FF), 0);
    lv_obj_set_style_border_width(button, 2, 0);
    lv_obj_add_event_cb(button, touch_button_event, LV_EVENT_CLICKED, NULL);

    touch_status_label = lv_label_create(button);
    lv_label_set_text(touch_status_label, "TOUCH TO VERIFY");
    lv_obj_set_style_text_color(touch_status_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(touch_status_label, &lv_font_montserrat_20, 0);
    lv_obj_center(touch_status_label);
}

static void update_health_ui(const board_health_t *health)
{
    if (!health->pmu_online) {
        lv_label_set_text(power_value_label, "AXP2101 OFFLINE");
    } else if (!health->battery_present) {
        lv_label_set_text_fmt(power_value_label, "BATTERY NOT PRESENT\nUSB: %s",
                              health->usb_present ? "CONNECTED" : "DISCONNECTED");
    } else if (health->battery_percent >= 0) {
        lv_label_set_text_fmt(power_value_label, "%u mV  /  %d%%\nUSB: %s  %s",
                              health->battery_mv, health->battery_percent,
                              health->usb_present ? "YES" : "NO",
                              health->charging ? "CHARGING" : "");
    } else {
        lv_label_set_text_fmt(power_value_label, "%u mV  /  GAUGE N/A\nUSB: %s  %s",
                              health->battery_mv, health->usb_present ? "YES" : "NO",
                              health->charging ? "CHARGING" : "");
    }

    if (!health->rtc_online) {
        lv_label_set_text(rtc_value_label, "PCF85063 OFFLINE");
    } else if (!health->rtc_valid) {
        lv_label_set_text(rtc_value_label, "RTC ONLINE\nTIME NOT SET");
    } else {
        lv_label_set_text_fmt(rtc_value_label, "%02u:%02u:%02u\n%04u-%02u-%02u",
                              health->hour, health->minute, health->second,
                              health->year, health->month, health->day);
    }

    if (!health->imu_online) {
        lv_label_set_text(imu_value_label, "QMI8658 OFFLINE");
    } else {
        lv_label_set_text_fmt(imu_value_label, "QMI8658 ID %02X\nX %+d  Y %+d  Z %+d mg",
                              health->imu_who_am_i,
                              (int)health->accel_x_mg, (int)health->accel_y_mg,
                              (int)health->accel_z_mg);
    }
}

static void set_screen_awake(bool awake)
{
    if (awake == screen_awake && !(awake && screen_dimmed)) {
        return;
    }
    wf_settings_snapshot_t settings;
    settings_service_get(&settings);
    if (!awake && WRISTFOUNDRY_POWER_TEST_MODE == POWER_TEST_NORMAL &&
        !settings.values.radio_finish) {
        time_sync_service_cancel("DISPLAY OFF");
        network_service_force_off("DISPLAY OFF");
        ble_radar_service_stop("DISPLAY OFF");
    }
    if (!bsp_display_lock(1000)) {
        ESP_LOGW(TAG, "Power-button action skipped: LVGL lock timed out");
        return;
    }

    esp_err_t result = awake
                           ? bsp_display_brightness_set(ACTIVE_BRIGHTNESS_PERCENT)
                           : bsp_display_backlight_off();
    if (result == ESP_OK) {
        screen_awake = awake;
        companion_service_set_display_awake(awake);
        screen_dimmed = false;
        if (awake && WRISTFOUNDRY_POWER_TEST_MODE == POWER_TEST_NORMAL) {
            watch_ui_note_activity();
            lifecycle_activity_us = watch_ui_last_activity_us();
        }
        ESP_LOGI(TAG, "PWR button: AMOLED %s", awake ? "awake" : "sleeping");
    } else {
        ESP_LOGW(TAG, "Could not set AMOLED state: %s", esp_err_to_name(result));
    }
    bsp_display_unlock();
}

static void set_sleep_status(const char *status)
{
    if (sleep_status_label == NULL) {
        return;
    }
    if (!bsp_display_lock(1000)) {
        ESP_LOGW(TAG, "Could not update sleep status: LVGL lock timed out");
        return;
    }
    lv_label_set_text(sleep_status_label, status);
    bsp_display_unlock();
}

static void log_power_state(const char *state, const char *event)
{
    board_health_t health;
    board_services_read(&health);
    if (health.rtc_valid) {
        ESP_LOGI(TAG,
                 "POWER_STATE mode=%s state=%s event=%s uptime_ms=%" PRId64
                 " rtc=%04u-%02u-%02uT%02u:%02u:%02u battery_mv=%u usb_power=%s",
                 power_test_mode_name(), state, event, esp_timer_get_time() / 1000,
                 health.year, health.month, health.day, health.hour, health.minute,
                 health.second, health.battery_mv, health.usb_present ? "yes" : "no");
    } else {
        ESP_LOGI(TAG,
                 "POWER_STATE mode=%s state=%s event=%s uptime_ms=%" PRId64
                 " rtc=unavailable battery_mv=%u usb_power=%s",
                 power_test_mode_name(), state, event, esp_timer_get_time() / 1000,
                 health.battery_mv, health.usb_present ? "yes" : "no");
    }
}

static void IRAM_ATTR power_key_isr(void *argument)
{
    (void)argument;
    __atomic_fetch_add(&power_key_edge_count, 1, __ATOMIC_RELAXED);
}

static void process_power_key_events(void)
{
    const uint32_t edges =
        __atomic_exchange_n(&power_key_edge_count, 0, __ATOMIC_ACQ_REL);

    if (power_key_press_latched) {
        if (gpio_get_level(PWR_WAKE_GPIO) == 0) {
            if (++power_key_release_ticks >= POWER_KEY_RELEASE_TICKS) {
                power_key_press_latched = false;
                power_key_release_ticks = 0;
                const bool usb_guard_active = usb_tether_blocks_sleep();
                if (!screen_awake && !usb_guard_active) {
                    ESP_LOGI(TAG, "PWR released without USB host: entering sleep immediately");
                    enter_battery_light_sleep();
                }
            }
        } else {
            power_key_release_ticks = 0;
        }
        return;
    }

    if (edges == 0) {
        return;
    }

    power_key_press_latched = true;
    power_key_release_ticks = 0;
    ESP_LOGI(TAG, "GPIO10 PWR press latched: one AMOLED toggle");
    if (screen_awake) {
        set_sleep_status(usb_tether_blocks_sleep()
                             ? "SLEEP BLOCKED: VBUS"
                             : "SLEEP: ARMED");
    }
    set_screen_awake(!screen_awake);
}

static void process_home_key(void)
{
    if (gpio_get_level(HOME_APPS_GPIO) == 0) {
        home_key_release_ticks = 0;
        if (!home_key_press_latched && ++home_key_press_ticks >= POWER_KEY_RELEASE_TICKS) {
            home_key_press_latched = true;
            home_key_press_ticks = 0;
            if (bsp_display_lock(1000)) {
                watch_ui_handle_home_button();
                ESP_LOGI(TAG, "GPIO0 HOME/APPS pressed: view=%s", watch_ui_view_name());
                bsp_display_unlock();
            } else {
                ESP_LOGW(TAG, "GPIO0 HOME/APPS ignored: LVGL lock timed out");
            }
        }
    } else {
        home_key_press_ticks = 0;
        if (home_key_press_latched &&
            ++home_key_release_ticks >= POWER_KEY_RELEASE_TICKS) {
            home_key_press_latched = false;
            home_key_release_ticks = 0;
        }
    }
}

static void show_sleep_result(int64_t duration_us)
{
    if (sleep_status_label == NULL) {
        return;
    }
    if (!bsp_display_lock(1000)) {
        ESP_LOGW(TAG, "Could not show light-sleep result: LVGL lock timed out");
        return;
    }
    lv_label_set_text_fmt(sleep_status_label, "SLEEP: %.1f s / LOW POWER",
                          duration_us / 1000000.0);
    /* The LVGL task can still be waiting on its pre-sleep timeout immediately
       after lvgl_port_resume(). Render synchronously so the AMOLED controller
       receives this frame before brightness is restored. */
    lv_refr_now(NULL);
    bsp_display_unlock();
}

static bool set_touch_input_enabled(bool enabled)
{
    if (!bsp_display_lock(1000)) {
        ESP_LOGW(TAG, "Could not %s touch input: LVGL lock timed out",
                 enabled ? "resume" : "suspend");
        return false;
    }

    lv_indev_t *touch = bsp_display_get_input_dev();
    if (touch != NULL) {
        lv_indev_enable(touch, enabled);
    }
    bsp_display_unlock();
    return touch != NULL;
}

static void enter_battery_light_sleep(void)
{
    /* All manual callers, including the PWR-release and power-test paths. */
    if (usb_tether_blocks_sleep() || phone_service_blocks_sleep()) return;
    if (WRISTFOUNDRY_POWER_TEST_MODE == POWER_TEST_NORMAL) {
        wf_settings_snapshot_t preferences;
        settings_service_get(&preferences);
        network_snapshot_t network;
        network_service_get_snapshot(&network);
        if (preferences.values.radio_finish &&
            (network.radio_active || !ble_radar_service_can_sleep() ||
             !time_sync_service_can_sleep())) return;
        time_sync_service_cancel("LIGHT SLEEP");
        network_service_force_off("LIGHT SLEEP");
        ble_radar_service_stop("LIGHT SLEEP");
        if (!ble_radar_service_can_sleep() || !time_sync_service_can_sleep()) {
            ESP_LOGI(TAG, "Light sleep deferred while radio resources release");
            return;
        }
    }
    if (gpio_get_level(PWR_WAKE_GPIO) != 0) {
        ESP_LOGW(TAG, "Light sleep deferred: GPIO10 power-key signal is already high");
        return;
    }

    esp_err_t result = gpio_wakeup_enable(PWR_WAKE_GPIO, GPIO_INTR_HIGH_LEVEL);
    if (result == ESP_OK) {
        result = esp_sleep_enable_gpio_wakeup();
    }
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Could not arm GPIO10 light-sleep wake: %s", esp_err_to_name(result));
        set_screen_awake(true);
        return;
    }

    /* A sleeping S3 cannot enumerate native USB. Poll only VBUS once a second
     * while remaining in the low-power state between checks. No display/radio
     * wake on these timer ticks. Measurement modes retain their original sleep. */
    const bool poll_usb = WRISTFOUNDRY_POWER_TEST_MODE == POWER_TEST_NORMAL;
    if (poll_usb && esp_sleep_enable_timer_wakeup(1000000) != ESP_OK) {
        gpio_wakeup_disable(PWR_WAKE_GPIO);
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
        ESP_LOGE(TAG, "Could not arm battery USB-attach polling; sleep deferred");
        return;
    }

    board_health_t before_sleep;
    board_services_read(&before_sleep);

    /* lvgl_port_stop() only stops LVGL's tick timer; its task can still poll
       input devices. Disable touch while holding the LVGL lock so no I2C touch
       transaction can remain in flight when light sleep starts. */
    const bool touch_suspended = set_touch_input_enabled(false);
    const uint32_t suspended_services = board_services_suspend_for_light_sleep();
    const bool lvgl_suspended = lvgl_port_stop() == ESP_OK;
    ESP_LOGI(TAG,
             "Low-power preparation: IMU=%s PMU_ADC=%s TOUCH=%s LVGL=%s battery=%umV",
             (suspended_services & BOARD_SLEEP_SUSPENDED_IMU) ? "SUSPENDED" : "ACTIVE",
             (suspended_services & BOARD_SLEEP_SUSPENDED_PMU_ADC) ? "SUSPENDED" : "ACTIVE",
             touch_suspended ? "SUSPENDED" : "ACTIVE",
             lvgl_suspended ? "SUSPENDED" : "ACTIVE", before_sleep.battery_mv);

    const int64_t sleep_started_us = esp_timer_get_time();
    ESP_LOGI(TAG, "Entering light sleep on battery; short-press PWR to wake (GPIO10 high)");
    gpio_intr_disable(PWR_WAKE_GPIO);
    /* Drop D+ before sleeping so a later attachment is enumerated only after
     * the CPU/PHY is awake. Restore the pad on every exit, including errors. */
    usb_serial_jtag_ll_phy_enable_pad(false);
    bool usb_wake = false;
    esp_sleep_wakeup_cause_t wake_cause = ESP_SLEEP_WAKEUP_UNDEFINED;
    do {
        if (usb_tether_blocks_sleep()) {
            usb_wake = true;
            result = ESP_OK;
            break;
        }
        result = esp_light_sleep_start();
        wake_cause = esp_sleep_get_wakeup_cause();
    } while (poll_usb && result == ESP_OK && wake_cause == ESP_SLEEP_WAKEUP_TIMER &&
             gpio_get_level(PWR_WAKE_GPIO) == 0);
    usb_serial_jtag_ll_phy_enable_pad(true);
    if (poll_usb) esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    const int64_t sleep_duration_us = esp_timer_get_time() - sleep_started_us;

    gpio_wakeup_disable(PWR_WAKE_GPIO);
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
    board_services_resume_from_light_sleep(suspended_services);

    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Light sleep failed: %s", esp_err_to_name(result));
        if (lvgl_suspended) {
            (void)lvgl_port_resume();
        }
        if (touch_suspended) {
            (void)set_touch_input_enabled(true);
        }
        __atomic_store_n(&power_key_edge_count, 0, __ATOMIC_RELEASE);
        gpio_intr_enable(PWR_WAKE_GPIO);
        set_screen_awake(true);
        return;
    }

    light_sleep_count++;
    total_light_sleep_us += sleep_duration_us;
    last_light_sleep_us = sleep_duration_us;
    last_wake_cause = wake_cause;
    ESP_LOGI(TAG,
             "Light-sleep wake: cause=%d interval=%.3f s count=%" PRIu32
             " total=%.3f s",
             wake_cause, sleep_duration_us / 1000000.0, light_sleep_count,
             total_light_sleep_us / 1000000.0);

    /* AXP2101 reports the same short press after release. Consume that event so
       one physical press wakes the screen instead of immediately blanking it. */
    for (int attempt = 0; attempt < 20 && gpio_get_level(PWR_WAKE_GPIO) != 0; attempt++) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    (void)board_services_poll_power_button();
    __atomic_store_n(&power_key_edge_count, 0, __ATOMIC_RELEASE);
    power_key_press_latched = false;
    power_key_release_ticks = 0;
    gpio_intr_enable(PWR_WAKE_GPIO);

    board_health_t after_sleep;
    board_services_read(&after_sleep);
    if (lvgl_suspended) {
        esp_err_t lvgl_result = lvgl_port_resume();
        if (lvgl_result != ESP_OK) {
            ESP_LOGW(TAG, "Could not resume LVGL timer: %s", esp_err_to_name(lvgl_result));
        }
    }
    if (touch_suspended) {
        (void)set_touch_input_enabled(true);
    }
    ESP_LOGI(TAG,
             "Low-power restoration complete: IMU=%s PMU_ADC=%s TOUCH=%s LVGL=%s "
             "battery=%umV->%umV",
             (suspended_services & BOARD_SLEEP_SUSPENDED_IMU) ? "RESTORED" : "UNCHANGED",
             (suspended_services & BOARD_SLEEP_SUSPENDED_PMU_ADC) ? "RESTORED" : "UNCHANGED",
             touch_suspended ? "RESTORED" : "UNCHANGED",
             lvgl_suspended ? "RESTORED" : "UNCHANGED", before_sleep.battery_mv,
             after_sleep.battery_mv);

    if (usb_wake) {
        ESP_LOGI(TAG, "VBUS attached: USB restored; waking tethered display");
        set_screen_awake(true);
        return;
    }
    show_sleep_result(sleep_duration_us);
    /* The forced refresh above completes while brightness is still zero, so
       wake reveals the new result rather than the pre-sleep frame. */
    vTaskDelay(pdMS_TO_TICKS(20));
    set_screen_awake(true);
}

static void update_inactivity_lifecycle(void)
{
    static int64_t next_usb_check_us;
    static bool tethered;
    static uint32_t settings_revision;
    static int64_t radio_deadline;
    static bool radios_stopped;
    static bool phone_was_active;
    wf_settings_snapshot_t settings;
    settings_service_get(&settings);
    const int64_t now_us = esp_timer_get_time();
    const bool phone_active = phone_service_active();
    if (phone_active && !phone_was_active) set_screen_awake(true);
    if (phone_active || phone_was_active) lifecycle_activity_us = now_us;
    phone_was_active = phone_active;
    if (settings.revision != settings_revision) {
        settings_revision = settings.revision;
        lifecycle_activity_us = now_us;
        if (screen_awake) set_screen_awake(true);
    }
    if (now_us >= next_usb_check_us) {
        bool vbus = false;
        const esp_err_t result = board_services_usb_power_present(&vbus);
        const bool attached = wf_usb_keeps_display_awake(result == ESP_OK, vbus);
        if (attached && !tethered) {
            ESP_LOGI(TAG, "USB power attached: display inactivity timeout suspended");
            set_screen_awake(true);
        }
        tethered = attached;
        next_usb_check_us = now_us + 1000000;
    }
    /* Radio grace is a hard maximum, not permission for an indefinite lease.
     * Closing the display blocks new radio operations through the companion. */
    if (screen_awake) { radio_deadline = 0; radios_stopped = false; }
    else if (!radios_stopped) {
        if (!radio_deadline) radio_deadline = now_us +
            (settings.values.radio_finish ? WF_RADIO_GRACE_SECONDS * 1000000LL : 0);
        network_snapshot_t network;
        time_sync_snapshot_t sync;
        network_service_get_snapshot(&network);
        time_sync_service_get_snapshot(&sync);
        const bool pending = network.scan_in_progress || network.sync_in_progress ||
            network.state == NETWORK_STATE_CONNECTING || sync.active ||
            !ble_radar_service_can_sleep();
        if (wf_settings_stop_radios(settings.values.radio_finish, now_us,
                                   radio_deadline, pending)) {
            time_sync_service_cancel("DISPLAY OFF");
            network_service_force_off("DISPLAY OFF");
            ble_radar_service_stop("DISPLAY OFF");
            radios_stopped = true;
        }
    }
    if (phone_active || (tethered && settings.values.usb_display)) {
        /* The PWR button can still explicitly blank the screen. Automatic
         * dim/off is paused; unplug starts a fresh battery idle interval. */
        lifecycle_activity_us = now_us;
        return;
    }
    const int64_t activity_us = watch_ui_last_activity_us();
    if (activity_us > lifecycle_activity_us) {
        lifecycle_activity_us = activity_us;
        if (screen_dimmed || !screen_awake) {
            ESP_LOGI(TAG, "Touch activity: %s -> ACTIVE", display_state_name());
            set_screen_awake(true);
        }
    }

    if (!screen_awake) {
        return;
    }

    const int64_t idle_ms = (esp_timer_get_time() - lifecycle_activity_us) / 1000;
    if (idle_ms >= settings.values.off_seconds * 1000LL) {
        ESP_LOGI(TAG, "Inactivity -> DISPLAY OFF");
        set_screen_awake(false);
    } else if (idle_ms >= settings.values.dim_seconds * 1000LL && !screen_dimmed) {
        if (!bsp_display_lock(1000)) {
            ESP_LOGW(TAG, "Could not dim display: LVGL lock timed out");
            return;
        }
        esp_err_t result = bsp_display_brightness_set(DIM_BRIGHTNESS_PERCENT);
        bsp_display_unlock();
        if (result == ESP_OK) {
            screen_dimmed = true;
            ESP_LOGI(TAG, "Inactivity -> DIM (%d%%)", DIM_BRIGHTNESS_PERCENT);
        }
    }
}

static void health_task(void *argument)
{
    (void)argument;
    uint32_t poll_cycle = 0;
    uint32_t health_cycle = 0;
    bool usb_sleep_guard_reported = false;

    while (true) {
        if (WRISTFOUNDRY_POWER_TEST_MODE == POWER_TEST_NORMAL) {
            process_power_key_events();
            process_home_key();
            update_inactivity_lifecycle();
        }

        if ((poll_cycle++ % HEALTH_POLL_TICKS) == 0) {
            board_health_t health;
            board_services_read(&health);
            const bool usb_host_connected = usb_serial_jtag_is_connected();
            const bool usb_guard_active = usb_tether_blocks_sleep();

            if (WRISTFOUNDRY_POWER_TEST_MODE == POWER_TEST_NORMAL &&
                !screen_awake && health.pmu_online) {
                if (usb_guard_active) {
                    if (!usb_sleep_guard_reported) {
                        ESP_LOGI(TAG,
                                 "VBUS present or unreadable: keeping native serial/recovery active; "
                                 "AMOLED remains off without light sleep");
                        usb_sleep_guard_reported = true;
                    }
                } else if (ble_radar_service_can_sleep() &&
                           time_sync_service_can_sleep()) {
                    usb_sleep_guard_reported = false;
                    enter_battery_light_sleep();
                }
            } else if (screen_awake) {
                usb_sleep_guard_reported = false;
            }

            if (screen_awake) {
                if (bsp_display_lock(1000)) {
                    if (WRISTFOUNDRY_POWER_TEST_MODE == POWER_TEST_NORMAL) {
                        network_snapshot_t network;
                        ble_radar_snapshot_t ble;
                        time_sync_snapshot_t sync;
                        network_service_get_snapshot(&network);
                        ble_radar_service_get_snapshot(&ble);
                        time_sync_service_get_snapshot(&sync);
                        companion_service_ui_tick();
                        watch_ui_update(&health, display_state_name(), last_wake_cause,
                                        light_sleep_count, last_light_sleep_us,
                                        &network, &ble, &sync);
                    } else {
                        update_health_ui(&health);
                    }
                    bsp_display_unlock();
                } else {
                    ESP_LOGW(TAG, "Skipped health-screen update: LVGL lock timed out");
                }
            }

            if ((health_cycle++ % 10) == 0) {
                ESP_LOGI(TAG,
                         "Health: AXP=%s BAT=%s %umV %d%% USB_POWER=%s USB_HOST=%s "
                         "RTC=%s IMU=%s "
                         "ACC=(%.0f,%.0f,%.0f)mg DISPLAY=%s VIEW=%s",
                         health.pmu_online ? "OK" : "FAIL",
                         health.battery_present ? "YES" : "NO", health.battery_mv,
                         health.battery_percent, health.usb_present ? "YES" : "NO",
                         usb_host_connected ? "YES" : "NO",
                         health.rtc_online ? (health.rtc_valid ? "OK" : "UNSET") : "FAIL",
                         health.imu_online ? "OK" : "FAIL", health.accel_x_mg,
                         health.accel_y_mg, health.accel_z_mg,
                         display_state_name(),
                         WRISTFOUNDRY_POWER_TEST_MODE == POWER_TEST_NORMAL
                             ? watch_ui_view_name()
                             : "POWER-TEST");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(POWER_KEY_POLL_MS));
    }
}

static bool wait_for_battery_measurement(void)
{
    set_sleep_status("TEST: UNPLUG USB TO START");
    log_power_state(power_test_mode_name(), "armed-waiting-for-usb-disconnect");
    while (true) {
        board_health_t health;
        board_services_read(&health);
        if (health.pmu_online && !health.usb_present &&
            !usb_serial_jtag_is_connected()) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

static esp_err_t start_wifi_active_scan(void)
{
    esp_err_t result = esp_netif_init();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
        return result;
    }
    result = esp_event_loop_create_default();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
        return result;
    }

    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    config.nvs_enable = false;
    ESP_RETURN_ON_ERROR(esp_wifi_init(&config), TAG, "Wi-Fi init failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG,
                        "Wi-Fi RAM storage failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG,
                        "Wi-Fi station mode failed");
    return esp_wifi_start();
}

static void power_test_task(void *argument)
{
    (void)argument;

    if (WRISTFOUNDRY_POWER_TEST_MODE == POWER_TEST_NORMAL) {
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGW(TAG,
             "POWER CHARACTERIZATION IMAGE: mode=%s; short PWR presses are ignored; "
             "long PWR and ROM recovery remain available",
             power_test_mode_name());
    log_power_state(power_test_mode_name(), "boot-ready");

    if (WRISTFOUNDRY_POWER_TEST_MODE == POWER_TEST_ACTIVE) {
        set_sleep_status("TEST: ACTIVE / AMOLED ON");
        log_power_state("active-amoled-on", "steady");
        vTaskDelete(NULL);
        return;
    }

    if (WRISTFOUNDRY_POWER_TEST_MODE == POWER_TEST_WIFI_ACTIVE) {
        set_sleep_status("TEST: WIFI ACTIVE SCAN");
        esp_err_t result = start_wifi_active_scan();
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "Wi-Fi active-scan test failed: %s", esp_err_to_name(result));
            set_sleep_status("TEST: WIFI INIT FAILED");
            vTaskDelete(NULL);
            return;
        }
        log_power_state("wifi-active-scan", "radio-started");
        const wifi_scan_config_t scan_config = {
            .show_hidden = true,
            .scan_type = WIFI_SCAN_TYPE_ACTIVE,
            .scan_time.active = {
                .min = 100,
                .max = 120,
            },
        };
        while (true) {
            const int64_t scan_started_us = esp_timer_get_time();
            result = esp_wifi_scan_start(&scan_config, true);
            uint16_t ap_count = 0;
            if (result == ESP_OK) {
                result = esp_wifi_scan_get_ap_num(&ap_count);
                (void)esp_wifi_clear_ap_list();
            }
            ESP_LOGI(TAG,
                     "WIFI_TEST event=active-scan-complete result=%s aps=%u duration_ms=%" PRId64,
                     esp_err_to_name(result), ap_count,
                     (esp_timer_get_time() - scan_started_us) / 1000);
            vTaskDelay(pdMS_TO_TICKS(WIFI_SCAN_PAUSE_MS));
        }
    }

    if (WRISTFOUNDRY_POWER_TEST_MODE == POWER_TEST_DISPLAY_OFF) {
        set_sleep_status("TEST: DISPLAY OFF IN 10 S");
        vTaskDelay(pdMS_TO_TICKS(POWER_TEST_SETTLE_MS));
        set_screen_awake(false);
        log_power_state("display-brightness-zero-mcu-awake", "steady");
        vTaskDelete(NULL);
        return;
    }

    if (!wait_for_battery_measurement()) {
        vTaskDelete(NULL);
        return;
    }

    if (WRISTFOUNDRY_POWER_TEST_MODE == POWER_TEST_LIGHT_SLEEP) {
        set_screen_awake(false);
        log_power_state("gpio10-light-sleep", "enter");
        enter_battery_light_sleep();
        set_sleep_status("TEST: LIGHT-SLEEP WOKE");
        log_power_state("gpio10-light-sleep", "wake-complete");
        vTaskDelete(NULL);
        return;
    }

    if (WRISTFOUNDRY_POWER_TEST_MODE == POWER_TEST_DEEP_SLEEP) {
        if (gpio_get_level(PWR_WAKE_GPIO) != 0) {
            ESP_LOGE(TAG, "Deep sleep refused: GPIO10 is already high");
            set_sleep_status("TEST: RELEASE PWR / REBOOT");
            vTaskDelete(NULL);
            return;
        }
        esp_err_t result = esp_sleep_enable_ext1_wakeup_io(
            1ULL << PWR_WAKE_GPIO, ESP_EXT1_WAKEUP_ANY_HIGH);
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "Deep sleep wake setup failed: %s", esp_err_to_name(result));
            set_sleep_status("TEST: DEEP-SLEEP UNSUPPORTED");
            vTaskDelete(NULL);
            return;
        }

        set_screen_awake(false);
        const bool touch_suspended = set_touch_input_enabled(false);
        const uint32_t suspended_services = board_services_suspend_for_light_sleep();
        const bool lvgl_suspended = lvgl_port_stop() == ESP_OK;
        ESP_LOGI(TAG,
                 "Deep-sleep preparation: IMU=%s PMU_ADC=%s TOUCH=%s LVGL=%s",
                 (suspended_services & BOARD_SLEEP_SUSPENDED_IMU) ? "SUSPENDED" : "ACTIVE",
                 (suspended_services & BOARD_SLEEP_SUSPENDED_PMU_ADC) ? "SUSPENDED" : "ACTIVE",
                 touch_suspended ? "SUSPENDED" : "ACTIVE",
                 lvgl_suspended ? "SUSPENDED" : "ACTIVE");
        log_power_state("gpio10-deep-sleep-experimental", "enter");
        esp_deep_sleep_start();
    }

    vTaskDelete(NULL);
}

static void log_startup_diagnostics(void)
{
    esp_chip_info_t chip_info;
    uint32_t flash_size = 0;
    const esp_app_desc_t *app = esp_app_get_description();

    esp_chip_info(&chip_info);
    esp_err_t flash_result = esp_flash_get_size(NULL, &flash_size);

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "WRISTFOUNDRY v%s watch-face time sync", WRISTFOUNDRY_VERSION);
    ESP_LOGI(TAG, "Firmware: %s (%s)", app->project_name, app->version);
    ESP_LOGI(TAG, "ESP-IDF: %s", esp_get_idf_version());
    ESP_LOGI(TAG, "Chip: ESP32-S3, %d cores, revision v%d.%d",
             chip_info.cores, chip_info.revision / 100, chip_info.revision % 100);
    if (flash_result == ESP_OK) {
        ESP_LOGI(TAG, "Flash: %" PRIu32 " MB", flash_size / (1024 * 1024));
    } else {
        ESP_LOGW(TAG, "Flash size query failed: %s", esp_err_to_name(flash_result));
    }
    ESP_LOGI(TAG, "PSRAM available: %u bytes",
             (unsigned int)heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
    ESP_LOGI(TAG, "Reset reason: %s (%d)", reset_reason_name(esp_reset_reason()),
             esp_reset_reason());
    ESP_LOGI(TAG, "Wake cause: %d; retained boot count: %" PRIu32,
             esp_sleep_get_wakeup_cause(), retained_boot_count);
    ESP_LOGI(TAG, "Default lifecycle (saved preferences override): DIM at %d ms (%d%%) -> OFF at %d ms",
             DIM_TIMEOUT_MS, DIM_BRIGHTNESS_PERCENT, SLEEP_TIMEOUT_MS);
    ESP_LOGI(TAG, "Motion wake: deferred; schematic QMI8658 INT1=GPIO21, GPIO10 retained");
    ESP_LOGI(TAG, "Power-test build mode: %s (%d)", power_test_mode_name(),
             WRISTFOUNDRY_POWER_TEST_MODE);
    ESP_LOGI(TAG, "========================================");
}

void app_main(void)
{
    if (esp_reset_reason() != ESP_RST_DEEPSLEEP) {
        retained_boot_count = 0;
    }
    retained_boot_count++;
    last_wake_cause = esp_sleep_get_wakeup_cause();
    log_startup_diagnostics();
    ESP_LOGI(TAG, "Starting Waveshare AMOLED display and FT3168 touch");

    lv_display_t *display = bsp_display_start();
    if (display == NULL) {
        ESP_LOGE(TAG, "BSP display/touch initialization failed");
        return;
    }

    ESP_ERROR_CHECK(bsp_display_brightness_set(ACTIVE_BRIGHTNESS_PERCENT));
    ESP_ERROR_CHECK(board_services_init());

    const gpio_config_t power_key_config = {
        .pin_bit_mask = 1ULL << PWR_WAKE_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&power_key_config));
    const gpio_config_t home_key_config = {
        .pin_bit_mask = 1ULL << HOME_APPS_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&home_key_config));
    /* The BSP touch driver installs the shared GPIO ISR service while the
       display starts. Reinstalling it here is harmless but emits an error-level
       diagnostic on every clean boot. */
    ESP_ERROR_CHECK(gpio_isr_handler_add(PWR_WAKE_GPIO, power_key_isr, NULL));
    ESP_LOGI(TAG, "PWR wake signal: GPIO10 idle=%d, wake level=HIGH", gpio_get_level(PWR_WAKE_GPIO));
    ESP_LOGI(TAG,
             "HOME/APPS key: GPIO0 active=LOW idle=%d; ROM BOOT strap remains available during reset",
             gpio_get_level(HOME_APPS_GPIO));

    bool rtc_was_set = false;
    esp_err_t rtc_result = board_services_set_rtc_from_build_if_unset(
        __DATE__, __TIME__, &rtc_was_set);
    if (rtc_result != ESP_OK) {
        ESP_LOGW(TAG, "RTC setup skipped: %s", esp_err_to_name(rtc_result));
    } else if (rtc_was_set) {
        ESP_LOGI(TAG, "RTC was unset and has been initialized from the build timestamp");
    }
    log_power_state("boot", "services-ready");

    if (!bsp_display_lock(1000)) {
        ESP_LOGE(TAG, "Could not acquire LVGL lock");
        return;
    }

    if (WRISTFOUNDRY_POWER_TEST_MODE == POWER_TEST_NORMAL) {
        watch_ui_create();
        lifecycle_activity_us = watch_ui_last_activity_us();
    } else {
        create_health_ui();
    }
    bsp_display_unlock();

    /* Let LVGL/BSP reserve its DMA-capable display resources before network
       task stacks consume internal RAM. The radio remains off during init. */
    if (WRISTFOUNDRY_POWER_TEST_MODE == POWER_TEST_NORMAL) {
#if CONFIG_NVS_ENCRYPTION && CONFIG_NVS_SEC_KEY_PROTECT_USING_HMAC
        ESP_LOGW(TAG, "SECURE STORAGE PROFILE: first boot may permanently provision the configured HMAC eFuse key block");
        esp_err_t nvs_result = nvs_flash_init();
        if (nvs_result != ESP_OK) {
            ESP_LOGW(TAG, "NVS unavailable; credentials cannot be loaded: %s",
                     esp_err_to_name(nvs_result));
        }
#else
        const esp_err_t nvs_result = nvs_flash_init();
        ESP_LOGI(TAG, "Development profile: NVS for nonsecret preferences only; Wi-Fi credentials remain RAM-only");
#endif
        ESP_ERROR_CHECK(settings_service_init(nvs_result == ESP_OK));
        ESP_ERROR_CHECK(ble_radio_owner_init());
        ESP_ERROR_CHECK(ble_radar_service_init());
        ESP_ERROR_CHECK(phone_service_init());
        ESP_ERROR_CHECK(network_service_init(nvs_result == ESP_OK));
        ESP_ERROR_CHECK(time_sync_service_init(
            network_service_credentials_are_encrypted()));
        ESP_ERROR_CHECK(companion_service_init());
        ESP_LOGI(TAG, "Companion USB protocol v1 ready (normal mode)");
    }

    ESP_LOGI(TAG, "AMOLED ready: %ld x %ld",
             (long)lv_display_get_horizontal_resolution(display),
             (long)lv_display_get_vertical_resolution(display));
    ESP_LOGI(TAG, "Touch input: %s",
             bsp_display_get_input_dev() != NULL ? "ready" : "not available");

    BaseType_t task_result = xTaskCreate(health_task, "hardware_health", 8192, NULL, 4, NULL);
    if (task_result != pdPASS) {
        ESP_LOGE(TAG, "Could not start hardware-health task");
        return;
    }
    task_result = xTaskCreate(power_test_task, "power_test", 4096, NULL, 3, NULL);
    if (task_result != pdPASS) {
        ESP_LOGE(TAG, "Could not start power-characterization task");
        return;
    }
    ESP_LOGI(TAG, "UI / watch-face time sync ready");
}
