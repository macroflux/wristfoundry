# WristFoundry

## WristFoundry 0.2.0

Persistent power/display preferences and shared app icons are included.
The watch and Companion expose USB display behavior, dim/off timeouts and
bounded radio shutdown through one settings service. Preferences use internal
NVS; no SD card or extra eFuse key is needed. Wi-Fi credential policy is unchanged.
App identity/icon metadata is reported by the watch and rendered on both UIs.

See the [changelog](CHANGELOG.md), [settings update guide](docs/SETTINGS_UPDATE.md),
and [latest release](https://github.com/macroflux/wristfoundry/releases/latest)
for version notes and matching firmware/Windows packages with checksums.
Watch the repository's Releases for updates. Close older Companion instances
before launching an updated runtime.

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

Manual CI runs can select `development_only` for hardware preparation; this
skips the secure-profile job. The Companion workflow separately checks the
protocol and builds/tests/packages the Flutter Windows application.

## Add an app

Define a `wf_app_t`, implement the lifecycle callbacks you need, and register it
before `wf_app_manager_finish_registration()`. See [App development](docs/APP_DEVELOPMENT.md).

## Windows companion (draft)

The official [WristFoundry Companion](docs/COMPANION.md) in
`companion/desktop` provides a Flutter Windows Welcome / Connect flow, USB
identity/capability handshake, device dashboard, Wi-Fi, UTC/RTC time setup,
timezone/NTP, supported settings, compiled app controls and redacted diagnostics.
Try the clearly labeled demo mode without hardware. Applications are compiled
into watch firmware: individual app installation is unavailable. Firmware
flashing is a separate, deliberate operation outside the companion.

### Run on Windows

Download the full Windows x64 runtime ZIP from a successful
[Companion workflow run](https://github.com/macroflux/wristfoundry/actions/workflows/companion.yml),
extract it, and run `wristfoundry_companion.exe`. Keep the EXE,
`flutter_windows.dll`, and `data` directory together. Flutter is not required
on the user's PC; the unsigned portable build may require Microsoft's Visual
C++ x64 redistributable. This draft is not a signed installer or published release.
Source-build requirements and commands are in [Companion setup](docs/COMPANION.md).

Connect the watch with a USB data cable, select its port, and connect explicitly.
The watch must run companion-enabled firmware from this branch. Older firmware
can expose a COM port yet time out because it does not implement the protocol.
USB enumeration errors such as Windows Code 43 occur before the handshake;
the older sleeping firmware required waking/reconnecting to expose its port.

### Tethered power behavior

The latest development implementation keeps the processor/USB available and
pauses automatic display dim/off on confirmed USB power. Attachment wakes the
display; unplugging starts a fresh battery inactivity interval. PWR can still
deliberately blank the display and stop radios. Battery light sleep checks for
USB attachment once a second; its additional current impact is not yet measured.

### Validation and review

The user reports successfully flashing development revision `14eb948`, connecting
the real companion, and using time/timezone synchronization. The subsequent
stay-awake display update, revision `c1de44d`, passes the development build and
C power-policy tests; applying it and validating the new display behavior on
the watch are not yet confirmed. Automated serial access on the development PC
returns access denied, while the user's own PowerShell flash succeeded.

Windows analysis, 23 Flutter tests, release compilation and runtime packaging
passed at the revisions recorded in the [validation record](docs/COMPANION_VALIDATION.md).
Physical RTC readback comparison, systematic unplug/reconnect tests, Wi-Fi/NTP,
clean-PC distribution testing and power measurements remain acceptance work.
The companion remains under review in [draft PR #1](https://github.com/macroflux/wristfoundry/pull/1)
and has not been merged to `main`.

- [Rendered UI gallery](docs/COMPANION_UI_REVIEW.md): actual Flutter renders
  using explicitly labeled demo data and fixture discovery.
- [App-only USB-awake update review](docs/COMPANION_USB_AWAKE_REVIEW.md): exact
  image checksum, write offset, preparation and rollback details.
- [Companion setup and physical acceptance](docs/COMPANION.md).

## Project status

The current framework version is 0.2.0. Hardware support is expanded in
measured phases so each feature has a power budget and sleep policy. See the
[roadmap](docs/ROADMAP.md), [hardware matrix](docs/HARDWARE.md), and
[power policy](docs/POWER.md). The remaining release gate is documented in the
[release checklist](docs/RELEASE_CHECKLIST.md).

Licensed under Apache-2.0. See `LICENSE`, `NOTICE`, and
`THIRD_PARTY_NOTICES.md`.
