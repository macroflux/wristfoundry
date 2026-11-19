# WristFoundry

WristFoundry is an open-source ESP-IDF and LVGL starter framework for the
Waveshare ESP32-S3-Touch-AMOLED-2.06 watch. It provides a working watch face,
scrolling launcher, app lifecycle, hardware telemetry, power-aware display and
radio behavior, on-watch Wi-Fi provisioning, NTP time synchronization backed by
the hardware RTC, and an optional bounded BLE scanner example.

**[Read the Phase One build story](https://rethinkingtheworld.com/wristfoundry-esp32-smartwatch-framework-phase-one/)**
for the project background, hardware bring-up, framework design, power lessons,
and the path to this first public release candidate.

This repository is a clean public extraction. It has its own history and does
not contain factory images, private product apps, device backups, generated
build trees, saved credentials, or private BLE features.

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

The public development profile has also been built, flashed, and revalidated on
the physical watch. Wi-Fi scanning and selection, keyboard entry and dismissal,
timezone selection, navigation stress, and the display dim/off lifecycle pass.
Successful NTP against a real access point and secure-profile credential
persistence remain hardware acceptance tests for the first tagged release.

## Security boundary

Persistent Wi-Fi credentials are stored only in encrypted NVS. The development
profile holds them in RAM until reboot, and WristFoundry never stores them on
the SD card. The secure profile uses the ESP32-S3 HMAC peripheral. Its first boot can
automatically and permanently provision the configured eFuse key block. This
is intentional but irreversible. Read [Secure storage](docs/SECURE_STORAGE.md)
before building or flashing it. Secure firmware is not included in public
release bundles.

BLE is optional and is not a clock source. Time synchronization is Wi-Fi/NTP
first, with the PCF85063 keeping time while offline.

## Build

Requirements: ESP-IDF 5.5.5 and PowerShell 7 or Windows PowerShell 5.1.

```powershell
.\scripts\Build.ps1
```

See [installation and flashing](docs/INSTALL.md) for source builds, release
bundles, recovery preparation, and the separate secure-profile workflow.

The normal development build keeps Wi-Fi credentials in RAM until reboot. It
never persists or loads them and does not provision security eFuses. After
inspecting eFuses and selecting a key block, build the secure profile as
documented in `docs/SECURE_STORAGE.md`.

## CI/CD

Pull requests and pushes to `main` build both the development and encrypted-NVS
profiles with ESP-IDF 5.5.5, assert the secure configuration, scan for private
material, and retain checksummed firmware artifacts. Semantic version tags build
a normal-profile release bundle and create a draft GitHub release for human
approval. Secure-profile binaries are never placed in public release bundles.
See [CI/CD policy](docs/CI_CD.md).

## Add an app

Define a `wf_app_t`, implement the lifecycle callbacks you need, and register it
before `wf_app_manager_finish_registration()`. See [App development](docs/APP_DEVELOPMENT.md).

## Project status

This is the 0.1.0 release candidate. Hardware support is expanded in
measured phases so each feature has a power budget and sleep policy. See the
[roadmap](docs/ROADMAP.md), [hardware matrix](docs/HARDWARE.md), and
[power policy](docs/POWER.md). The remaining release gate is documented in the
[release checklist](docs/RELEASE_CHECKLIST.md).

Licensed under Apache-2.0. See `LICENSE`, `NOTICE`, and
`THIRD_PARTY_NOTICES.md`.
