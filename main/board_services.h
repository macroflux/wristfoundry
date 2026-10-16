#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct {
    bool pmu_online;
    bool battery_present;
    bool usb_present;
    bool charging;
    uint16_t battery_mv;
    int battery_percent;

    bool rtc_online;
    bool rtc_valid;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;

    bool imu_online;
    uint8_t imu_who_am_i;
    float accel_x_mg;
    float accel_y_mg;
    float accel_z_mg;
} board_health_t;

enum {
    BOARD_SLEEP_SUSPENDED_IMU = 1U << 0,
    BOARD_SLEEP_SUSPENDED_PMU_ADC = 1U << 1,
};

esp_err_t board_services_init(void);
esp_err_t board_services_set_rtc_from_build_if_unset(const char *build_date,
                                                      const char *build_time,
                                                      bool *was_set);
esp_err_t board_services_set_rtc_local(uint16_t year, uint8_t month, uint8_t day,
                                       uint8_t hour, uint8_t minute, uint8_t second);
bool board_services_poll_power_button(void);
esp_err_t board_services_usb_power_present(bool *present);
uint32_t board_services_suspend_for_light_sleep(void);
void board_services_resume_from_light_sleep(uint32_t suspended_flags);
void board_services_read(board_health_t *health);
