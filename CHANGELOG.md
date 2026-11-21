# Changelog

All notable changes are documented here. WristFoundry follows Semantic
Versioning once a version is published.

## Unreleased — 0.2.0 candidate

### Added
- Shared power/display preferences in the watch Settings app and Windows
  Companion: USB display mode, dim/off timeouts, bounded radio completion,
  explicit save and restore defaults.
- Nonsecret preferences in a versioned internal-NVS record, with defaults on
  absent/invalid data, revision conflict detection, and no writes for unchanged
  saved values. Wi-Fi credential security/persistence remains unchanged.
- Settings landing page with separate Wi-Fi and Time/timezone sections.
- Portable builtin icon metadata in `apps.list`, watch launcher symbols and
  companion icons, with initials fallback for existing/unknown apps.

### Compatibility and updating
- USB protocol remains v1 with additive capabilities. New companion controls
  require `settings.set`; old firmware keeps its read-only policy view.
- No SD card, partition change, NVS erase, or extra eFuse key is required.
- USB communication remains available independently of display timeout. Radio
  completion is limited to 30 seconds after display-off; it is not an always-on mode.
- Proposed firmware/companion version: 0.2.0. Not tagged or released. See
  `docs/SETTINGS_UPDATE.md` for local acceptance and update/rollback instructions.

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
