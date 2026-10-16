#include "board_services.h"

#include <stdio.h>
#include <string.h>

#include "bsp/esp-bsp.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "qmi8658.h"

#define AXP2101_ADDRESS          0x34
#define AXP2101_STATUS1          0x00
#define AXP2101_STATUS2          0x01
#define AXP2101_CHIP_ID          0x03
#define AXP2101_ADC_CONTROL      0x30
#define AXP2101_BATTERY_VOLTAGE  0x34
#define AXP2101_IRQ_ENABLE2      0x41
#define AXP2101_IRQ_STATUS2      0x49
#define AXP2101_BATTERY_PERCENT  0xA4
#define AXP2101_PKEY_SHORT_MASK  (1U << 3)

#define PCF85063_ADDRESS         0x51
#define PCF85063_TIME_REGISTER   0x04

#define I2C_TIMEOUT_MS           100
#define I2C_SPEED_HZ             400000

static const char *TAG = "board_services";
static i2c_master_dev_handle_t s_pmu;
static i2c_master_dev_handle_t s_rtc;
static qmi8658_dev_t s_imu;
static bool s_pmu_ready;
static bool s_rtc_ready;
static bool s_imu_ready;
static uint8_t s_imu_who_am_i;
static uint8_t s_saved_adc_control;

static void read_rtc(board_health_t *health);

static esp_err_t add_i2c_device(i2c_master_bus_handle_t bus, uint8_t address,
                                i2c_master_dev_handle_t *device)
{
    const i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = I2C_SPEED_HZ,
    };
    return i2c_master_bus_add_device(bus, &config, device);
}

static esp_err_t read_registers(i2c_master_dev_handle_t device, uint8_t start,
                                uint8_t *data, size_t length)
{
    return i2c_master_transmit_receive(device, &start, 1, data, length, I2C_TIMEOUT_MS);
}

static esp_err_t read_register(i2c_master_dev_handle_t device, uint8_t reg, uint8_t *value)
{
    return read_registers(device, reg, value, 1);
}

static esp_err_t write_register(i2c_master_dev_handle_t device, uint8_t reg, uint8_t value)
{
    const uint8_t data[] = {reg, value};
    return i2c_master_transmit(device, data, sizeof(data), I2C_TIMEOUT_MS);
}

static uint8_t bcd_to_binary(uint8_t value)
{
    return (uint8_t)(((value >> 4) * 10) + (value & 0x0F));
}

static uint8_t binary_to_bcd(uint8_t value)
{
    return (uint8_t)(((value / 10) << 4) | (value % 10));
}

static void init_pmu(i2c_master_bus_handle_t bus)
{
    uint8_t chip_id = 0;
    uint8_t adc_control = 0;
    uint8_t irq_enable = 0;

    if (add_i2c_device(bus, AXP2101_ADDRESS, &s_pmu) != ESP_OK ||
        read_register(s_pmu, AXP2101_CHIP_ID, &chip_id) != ESP_OK) {
        ESP_LOGW(TAG, "AXP2101 not detected");
        return;
    }

    s_pmu_ready = true;
    ESP_LOGI(TAG, "AXP2101 ready (chip ID 0x%02X)", chip_id);

    if (read_register(s_pmu, AXP2101_ADC_CONTROL, &adc_control) == ESP_OK) {
        adc_control |= (1U << 0) | (1U << 2);
        adc_control &= ~(1U << 1);
        if (write_register(s_pmu, AXP2101_ADC_CONTROL, adc_control) != ESP_OK) {
            ESP_LOGW(TAG, "Could not configure AXP2101 ADC channels");
        }
    }

    if (read_register(s_pmu, AXP2101_IRQ_ENABLE2, &irq_enable) == ESP_OK) {
        irq_enable |= AXP2101_PKEY_SHORT_MASK;
        if (write_register(s_pmu, AXP2101_IRQ_ENABLE2, irq_enable) != ESP_OK) {
            ESP_LOGW(TAG, "Could not enable AXP2101 short-press interrupt");
        }
    }
    write_register(s_pmu, AXP2101_IRQ_STATUS2, AXP2101_PKEY_SHORT_MASK);
}

static void init_rtc(i2c_master_bus_handle_t bus)
{
    uint8_t control = 0;
    if (add_i2c_device(bus, PCF85063_ADDRESS, &s_rtc) == ESP_OK &&
        read_register(s_rtc, 0x00, &control) == ESP_OK) {
        s_rtc_ready = true;
        ESP_LOGI(TAG, "PCF85063 RTC ready");
    } else {
        ESP_LOGW(TAG, "PCF85063 RTC not detected");
    }
}

static void init_imu(i2c_master_bus_handle_t bus)
{
    esp_err_t result = qmi8658_init(&s_imu, bus, QMI8658_ADDRESS_HIGH);
    if (result == ESP_OK) {
        result = qmi8658_get_who_am_i(&s_imu, &s_imu_who_am_i);
    }
    if (result == ESP_OK) {
        result = qmi8658_set_accel_range(&s_imu, QMI8658_ACCEL_RANGE_4G);
    }
    if (result == ESP_OK) {
        result = qmi8658_set_accel_odr(&s_imu, QMI8658_ACCEL_ODR_62_5HZ);
    }
    if (result == ESP_OK) {
        qmi8658_set_accel_unit_mg(&s_imu, true);
        result = qmi8658_enable_accel(&s_imu, true);
    }

    if (result == ESP_OK) {
        s_imu_ready = true;
        ESP_LOGI(TAG, "QMI8658 ready (WHO_AM_I 0x%02X)", s_imu_who_am_i);
    } else {
        ESP_LOGW(TAG, "QMI8658 initialization failed: %s", esp_err_to_name(result));
    }
}

esp_err_t board_services_init(void)
{
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (bus == NULL) {
        ESP_LOGE(TAG, "BSP I2C bus is unavailable");
        return ESP_ERR_INVALID_STATE;
    }

    init_pmu(bus);
    init_rtc(bus);
    init_imu(bus);
    return ESP_OK;
}

static int month_from_abbreviation(const char *month)
{
    static const char *months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
    };
    for (int index = 0; index < 12; index++) {
        if (strcmp(month, months[index]) == 0) {
            return index + 1;
        }
    }
    return 0;
}

static uint8_t weekday_from_date(int year, int month, int day)
{
    static const int offsets[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (month < 3) {
        year--;
    }
    return (uint8_t)((year + year / 4 - year / 100 + year / 400 +
                      offsets[month - 1] + day) % 7);
}

esp_err_t board_services_set_rtc_from_build_if_unset(const char *build_date,
                                                      const char *build_time,
                                                      bool *was_set)
{
    if (was_set != NULL) {
        *was_set = false;
    }
    if (!s_rtc_ready || build_date == NULL || build_time == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    board_health_t current;
    memset(&current, 0, sizeof(current));
    read_rtc(&current);
    if (current.rtc_valid) {
        return ESP_OK;
    }

    char month_text[4] = {0};
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    if (sscanf(build_date, "%3s %d %d", month_text, &day, &year) != 3 ||
        sscanf(build_time, "%d:%d:%d", &hour, &minute, &second) != 3) {
        return ESP_ERR_INVALID_ARG;
    }
    month = month_from_abbreviation(month_text);
    if (year < 2000 || year > 2099 || month == 0 || day < 1 || day > 31 ||
        hour < 0 || hour > 23 || minute < 0 || minute > 59 ||
        second < 0 || second > 59) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t data[] = {
        PCF85063_TIME_REGISTER,
        binary_to_bcd((uint8_t)second),
        binary_to_bcd((uint8_t)minute),
        binary_to_bcd((uint8_t)hour),
        binary_to_bcd((uint8_t)day),
        binary_to_bcd(weekday_from_date(year, month, day)),
        binary_to_bcd((uint8_t)month),
        binary_to_bcd((uint8_t)(year - 2000)),
    };
    esp_err_t result = i2c_master_transmit(s_rtc, data, sizeof(data), I2C_TIMEOUT_MS);
    if (result == ESP_OK) {
        if (was_set != NULL) {
            *was_set = true;
        }
        ESP_LOGI(TAG, "RTC initialized from build time: %04d-%02d-%02d %02d:%02d:%02d",
                 year, month, day, hour, minute, second);
    }
    return result;
}

esp_err_t board_services_set_rtc_local(uint16_t year, uint8_t month, uint8_t day,
                                       uint8_t hour, uint8_t minute, uint8_t second)
{
    if (!s_rtc_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    if (year < 2000 || year > 2099 || month < 1 || month > 12 ||
        day < 1 || day > 31 || hour > 23 || minute > 59 || second > 59) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t data[] = {
        PCF85063_TIME_REGISTER,
        binary_to_bcd(second),
        binary_to_bcd(minute),
        binary_to_bcd(hour),
        binary_to_bcd(day),
        binary_to_bcd(weekday_from_date(year, month, day)),
        binary_to_bcd(month),
        binary_to_bcd((uint8_t)(year - 2000)),
    };
    return i2c_master_transmit(s_rtc, data, sizeof(data), I2C_TIMEOUT_MS);
}

bool board_services_poll_power_button(void)
{
    uint8_t status = 0;
    if (!s_pmu_ready || read_register(s_pmu, AXP2101_IRQ_STATUS2, &status) != ESP_OK ||
        (status & AXP2101_PKEY_SHORT_MASK) == 0) {
        return false;
    }

    write_register(s_pmu, AXP2101_IRQ_STATUS2, AXP2101_PKEY_SHORT_MASK);
    return true;
}

esp_err_t board_services_usb_power_present(bool *present)
{
    if (!s_pmu_ready || present == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t status1 = 0;
    uint8_t status2 = 0;
    esp_err_t result = read_register(s_pmu, AXP2101_STATUS1, &status1);
    if (result == ESP_OK) {
        result = read_register(s_pmu, AXP2101_STATUS2, &status2);
    }
    if (result == ESP_OK) {
        *present = (status1 & (1U << 5)) != 0 && (status2 & (1U << 3)) == 0;
    }
    return result;
}

uint32_t board_services_suspend_for_light_sleep(void)
{
    uint32_t suspended = 0;

    if (s_imu_ready) {
        esp_err_t result = qmi8658_enable_accel(&s_imu, false);
        if (result == ESP_OK) {
            suspended |= BOARD_SLEEP_SUSPENDED_IMU;
        } else {
            ESP_LOGW(TAG, "Could not suspend QMI8658 accelerometer: %s",
                     esp_err_to_name(result));
        }
    }

    if (s_pmu_ready &&
        read_register(s_pmu, AXP2101_ADC_CONTROL, &s_saved_adc_control) == ESP_OK) {
        const uint8_t sleep_adc_control =
            s_saved_adc_control & (uint8_t)~((1U << 0) | (1U << 2));
        esp_err_t result = write_register(s_pmu, AXP2101_ADC_CONTROL,
                                          sleep_adc_control);
        if (result == ESP_OK) {
            suspended |= BOARD_SLEEP_SUSPENDED_PMU_ADC;
        } else {
            ESP_LOGW(TAG, "Could not suspend AXP2101 battery/VBUS ADC: %s",
                     esp_err_to_name(result));
        }
    }

    return suspended;
}

void board_services_resume_from_light_sleep(uint32_t suspended_flags)
{
    if ((suspended_flags & BOARD_SLEEP_SUSPENDED_PMU_ADC) != 0) {
        esp_err_t result = write_register(s_pmu, AXP2101_ADC_CONTROL,
                                          s_saved_adc_control);
        if (result != ESP_OK) {
            ESP_LOGW(TAG, "Could not restore AXP2101 ADC configuration: %s",
                     esp_err_to_name(result));
        }
    }

    if ((suspended_flags & BOARD_SLEEP_SUSPENDED_IMU) != 0) {
        esp_err_t result = qmi8658_enable_accel(&s_imu, true);
        if (result != ESP_OK) {
            ESP_LOGW(TAG, "Could not resume QMI8658 accelerometer: %s",
                     esp_err_to_name(result));
        }
    }
}

static void read_power(board_health_t *health)
{
    uint8_t status1 = 0;
    uint8_t status2 = 0;
    uint8_t voltage[2] = {0};
    uint8_t percent = 0;

    if (!s_pmu_ready || read_register(s_pmu, AXP2101_STATUS1, &status1) != ESP_OK ||
        read_register(s_pmu, AXP2101_STATUS2, &status2) != ESP_OK) {
        return;
    }

    health->pmu_online = true;
    health->battery_present = (status1 & (1U << 3)) != 0;
    health->usb_present = (status1 & (1U << 5)) != 0 && (status2 & (1U << 3)) == 0;
    health->charging = (status2 >> 5) == 1;

    if (health->battery_present &&
        read_registers(s_pmu, AXP2101_BATTERY_VOLTAGE, voltage, sizeof(voltage)) == ESP_OK) {
        health->battery_mv = (uint16_t)(((voltage[0] & 0x1F) << 8) | voltage[1]);
    }
    if (health->battery_present &&
        read_register(s_pmu, AXP2101_BATTERY_PERCENT, &percent) == ESP_OK && percent <= 100) {
        health->battery_percent = percent;
    }
}

static void read_rtc(board_health_t *health)
{
    uint8_t data[7] = {0};
    if (!s_rtc_ready ||
        read_registers(s_rtc, PCF85063_TIME_REGISTER, data, sizeof(data)) != ESP_OK) {
        return;
    }

    health->rtc_online = true;
    health->second = bcd_to_binary(data[0] & 0x7F);
    health->minute = bcd_to_binary(data[1] & 0x7F);
    health->hour = bcd_to_binary(data[2] & 0x3F);
    health->day = bcd_to_binary(data[3] & 0x3F);
    health->month = bcd_to_binary(data[5] & 0x1F);
    health->year = (uint16_t)(2000 + bcd_to_binary(data[6]));
    health->rtc_valid = (data[0] & 0x80) == 0 && health->second < 60 &&
                        health->minute < 60 && health->hour < 24 &&
                        health->day >= 1 && health->day <= 31 &&
                        health->month >= 1 && health->month <= 12;
}

static void read_imu(board_health_t *health)
{
    if (!s_imu_ready) {
        return;
    }

    health->imu_who_am_i = s_imu_who_am_i;
    if (qmi8658_read_accel(&s_imu, &health->accel_x_mg, &health->accel_y_mg,
                           &health->accel_z_mg) == ESP_OK) {
        health->imu_online = true;
    }
}

void board_services_read(board_health_t *health)
{
    if (health == NULL) {
        return;
    }

    memset(health, 0, sizeof(*health));
    health->battery_percent = -1;
    read_power(health);
    read_rtc(health);
    read_imu(health);
}
