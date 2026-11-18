# Changelog

All notable changes are documented here. WristFoundry follows Semantic
Versioning once a version is published.

## 0.1.0 — release candidate

- Introduced the reusable app lifecycle and scrolling launcher.
- Added responsive LVGL watch UI, navigation, dim/off, and wake behavior.
- Added PMIC, battery, RTC, accelerometer, and board diagnostics.
- Added on-watch Wi-Fi entry, bounded radio ownership, NTP synchronization,
  RTC writeback, and offline RTC timekeeping.
- Added optional bounded BLE advertisement scanning without BLE time sync.
- Added a development profile with persistent credentials disabled.
- Added an optional HMAC-backed encrypted-NVS profile with permanent eFuse
  provisioning warnings.
- Added CI builds, release packaging, checksums, safety scans, and an SBOM.

Physical acceptance of the public release candidate remains documented in
`docs/RELEASE_CHECKLIST.md`.
