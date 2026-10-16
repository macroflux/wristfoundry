# Roadmap

Each phase includes a clean build, physical hardware checks, sleep/off behavior,
and comparative battery-path power measurements.

## Phase 1 — sanitized foundation (current)

- independent Apache-2.0 repository and public documentation
- watch face, launcher, lifecycle/registration API, scrolling and back behavior
- System, Settings, and bounded BLE Radar examples
- on-watch LVGL keyboard provisioning
- encrypted-NVS-only credential persistence
- Wi-Fi/NTP primary time synchronization with PCF85063 RTC persistence
- no BLE phone-time feature
- existing dim/off/light-sleep and power-test modes

Acceptance still required: clean build, clean-device flash, keyboard usability,
real-AP NTP-to-RTC, battery sleep/wake regression, and exposure review.

## Phase 2 — stable app SDK

Split the monolith into `wf_*` components, add typed capabilities and testable
service interfaces, and ship a reaction-timer app with host/unit tests.

## Phase 3 — complete motion support

Calibrate accelerometer and gyroscope, add a tilt-ball example, then evaluate
GPIO21 motion/raise-to-wake with false-wake and energy measurements.

## Phase 4 — microSD and logging

Add card detect/mount/unmount, asset loading, explicit export, and opt-in rotating
logs. Never store credentials, tokens, raw audio, or stable BLE identifiers.
Test removal, corruption, full-card behavior, flush latency, and sleep current.

## Phase 5 — optional BLE package

Separate BLE from the core build, preserve shared ownership and bounded scans,
and add privacy filters. BLE remains optional and is not a time source.

## Phase 6 — audio and haptics

Validate both ES7210 microphone channels, ES8311 output, NS4150B amplifier control,
speaker, and GPIO18 motor. Ship local diagnostics with explicit microphone consent,
amp-off idle behavior, and bounded haptic duty cycles.

## Phase 7 — power closure

Characterize every rail and operating state on the battery path; optimize sensor
rates, radio windows, display brightness, SD idle behavior, audio shutdown, and
wake sources. Publish the method and measured budgets.

## Phase 8 — optional higher-level examples

Only after the hardware foundation is complete, consider provider-neutral voice
or network examples. Keep secrets secure and cloud services optional.

## Phase 9 — release

Run license/SBOM and secret scans, create reproducible artifacts, publish measured
hardware validation, tag the first release, and write the public launch material.
