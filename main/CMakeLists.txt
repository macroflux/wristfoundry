idf_component_register(
    SRCS "main.c" "board_services.c" "network_service.c" "ui_design.c"
         "app_manager.c" "watch_ui.c" "ble_radio_owner.c"
         "ble_radar_service.c" "ble_radar_app.c" "settings_app.c"
         "time_sync_service.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_app_format esp_driver_gpio esp_driver_i2c
                  esp_driver_usb_serial_jtag esp_event esp_netif esp_system
                  esp_timer esp_wifi espressif__esp_lvgl_port heap lwip nvs_flash spi_flash bt
)

if(NOT DEFINED WRISTFOUNDRY_POWER_TEST_MODE)
    set(WRISTFOUNDRY_POWER_TEST_MODE 0)
endif()

if(NOT WRISTFOUNDRY_POWER_TEST_MODE MATCHES "^[0-5]$")
    message(FATAL_ERROR "WRISTFOUNDRY_POWER_TEST_MODE must be 0..5")
endif()

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    WRISTFOUNDRY_POWER_TEST_MODE=${WRISTFOUNDRY_POWER_TEST_MODE}
)
