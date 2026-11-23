# Android/BLE candidate validation

Source version 0.3.0, unreleased. Stable 0.2.0 is unchanged. No new firmware
has been flashed and no installable APK or Windows candidate is claimed here.

## Local checks

- Flutter 3.38.7 / Dart 3.10.7 analysis passed with no issues.
- All 28 Flutter tests passed, including Settings section navigation, narrow
  phone layout, existing settings save/revision conflict tests, framing and USB
  controller regression tests. Desktop UI review captures were generated.
- Four Python protocol/release tests passed, including new zero-argument BLE
  control fixtures and rejection of extra parameters.
- Native MainActivity.kt compiled with the available Kotlin 1.9.24 compiler,
  Android API 35 headers and cached Flutter embedding. This checks native source
  compilation, not Android GATT behavior or the complete Gradle application.
- ESP-Clang syntax checks passed against installed ESP-IDF 5.5.5 headers and
  generated development configuration for phone_service.c, companion_service.c,
  settings_app.c, main.c, watch_ui.c and ble_radio_owner.c.
- `git diff --check` passed. No partition/security provisioning changes.

## Environment limitations

The local ESP GCC executable fails with Windows error 623 (illegal system DLL
relocation), so complete firmware compile/link must run in CI. The local Flutter
SDK was prepared in ignored build output; its dependencies/test engine were
verified/downloaded to permit analysis and tests here.

APK assembly reached Gradle but cannot download its distribution in this task:
Java reports `SocketException: Permission denied: connect`. An isolated Gradle
cache was used, preserving the owner's normal cache. Android CI prepares the
pinned Flutter scaffold and native bridge, runs checks and builds a debug APK
with checksums. Windows packaging and both firmware profiles also need CI.
Native C policy tests are included in CI; their presence is not a local execution
result. Secure firmware must only be compiled, never routinely flashed.

## Physical acceptance pending

Run the sequence in ANDROID_COMPANION.md on the real watch and phone. Pairing
codes, encrypted GATT, MTU fragmentation, fresh discovery after disconnect,
the five-minute timeout despite polling, display/PWR behavior, full stack release,
Radar coexistence, USB coexistence and actual battery use remain unverified.
Do not publish 0.3.0 until local hardware acceptance is reported and reviewed.
