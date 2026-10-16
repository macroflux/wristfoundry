# WristFoundry

WristFoundry is an open-source ESP-IDF and LVGL starter framework for the
Waveshare ESP32-S3-Touch-AMOLED-2.06 watch. It provides a working watch face,
scrolling launcher, app lifecycle, hardware telemetry, power-aware display and
radio behavior, on-watch Wi-Fi provisioning, NTP time synchronization backed by
the hardware RTC, and an optional bounded BLE scanner example.

This repository is a clean public extraction. It has its own history and does
not contain factory images, private product apps, device backups, generated
build trees, saved credentials, or BLE phone-time code.

## What is verified

The extracted baseline comes from firmware exercised on physical hardware:

- 410 x 502 AMOLED display and FT3168 touch input
- scrolling launcher, app navigation, back behavior, dim/off lifecycle
- GPIO0 Home/Apps input and AXP2101 power-key events
- battery operation, charging/USB/status telemetry, and GPIO10 light-sleep wake
- PCF85063 hardware RTC access
- QMI8658 identity and accelerometer readings
- Wi-Fi provisioning service and NTP-to-RTC implementation
- bounded BLE advertisement scanning

The public repository itself must be rebuilt and revalidated on a clean device
before its first release. Successful NTP against a real access point and the
on-watch keyboard flow remain hardware acceptance tests.

## Security boundary

Wi-Fi credentials are stored only in NVS. WristFoundry refuses to persist them
unless ESP-IDF NVS encryption is enabled. It never stores credentials on the SD
card. The secure profile uses the ESP32-S3 HMAC peripheral and requires an
explicit, irreversible eFuse decision; read [Secure storage](docs/SECURE_STORAGE.md)
before flashing it.

BLE is optional and is not a clock source. Time synchronization is Wi-Fi/NTP
first, with the PCF85063 keeping time while offline.

## Build

Requirements: ESP-IDF 5.5.5 and PowerShell 7 or Windows PowerShell 5.1.

```powershell
.\scripts\Build.ps1
```

The normal development build can run without encrypted NVS, but Settings will
block credential saving. After inspecting eFuses and selecting a key block,
build the secure profile as documented in `docs/SECURE_STORAGE.md`.

## Add an app

Define a `wf_app_t`, implement the lifecycle callbacks you need, and register it
before `wf_app_manager_finish_registration()`. See [App development](docs/APP_DEVELOPMENT.md).

## Project status

This is a pre-release foundation, version 0.1.0. Hardware support is expanded in
measured phases so each feature has a power budget and sleep policy. See the
[roadmap](docs/ROADMAP.md), [hardware matrix](docs/HARDWARE.md), and
[power policy](docs/POWER.md).

Licensed under Apache-2.0. See `LICENSE`, `NOTICE`, and
`THIRD_PARTY_NOTICES.md`.
