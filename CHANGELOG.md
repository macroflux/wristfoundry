# Changelog

All notable changes are documented here. WristFoundry follows Semantic
Versioning once a version is published.

## Unreleased - 0.3.0 candidate

- Fix Android BLE pairing-probe framing that could silently discard the first
  handshake; Bluetooth timeout guidance now identifies the failing stage.
- Shared compact watch artwork on the Android welcome screen, plus WF branding
  in Android launcher/adaptive icons, Windows executable icon and both headers.
- Android Flutter Companion with explicit BLE scanning, system passkey pairing,
  shared settings, network/clock controls, app icons/navigation and diagnostics.
- Settings section cards on watch, Windows and Android: Power & display, Wi-Fi,
  Time & timezone, and Phone connection.
- Watch-initiated 60-second discovery, authenticated/encrypted control, display
  hold during active companion sessions, and five-minute user-idle disconnect.
  Polling does not extend sessions. PWR can blank the display; disconnect restores
  the saved power profile. Radar and phone sessions share exclusive radio ownership.
- Debug APK packaging in CI. Hardware pairing, reconnect and battery acceptance
  are pending; this candidate is not published. See docs/ANDROID_COMPANION.md.

## 0.2.0 - 2026-11-21

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
- Firmware/companion version: 0.2.0. See `docs/SETTINGS_UPDATE.md` for
  application-only firmware updates, complete Windows runtime installation,
  and rollback instructions. Existing compatible installations retain NVS.
- Install both matching components, close older Companion instances, and
  reconnect. Settings saved from either interface use the same watch storage.
- Local hardware operation and settings saving were confirmed by the project
  owner. Detailed power measurements and the full radio/sleep matrix remain
  unverified; see `docs/SETTINGS_VALIDATION.md`.

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
