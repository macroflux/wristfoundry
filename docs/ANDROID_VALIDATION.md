# Android/BLE candidate validation

Source version 0.3.0, unreleased. Stable 0.2.0 is unchanged. CI packages are
available and the Android APK is installed for UI review. The watch update
and physical Bluetooth acceptance remain pending.

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

## CI and UI review at source 92bb70c

- Firmware CI run 45240521222 and Companion run 45240521200 completed successfully.
  Both firmware profiles and the Windows/Android build jobs passed.
- Downloaded Android and development firmware artifacts were checked against
  their GitHub SHA-256 digests and internal manifests. The development partition
  table is unchanged from 0.2.0. Application SHA-256:
  `3c5d5b3543c4ee989c1afd1b9ba5d141dbb49544e66ecd728c6dc8a6c0377863`.
- ADB installed and opened Android version 0.3.0 (versionCode 3). The owner
  reviewed Demo mode and approved its appearance, requesting the shared watch
  graphic on the opening screen and a compact layout.
- The welcome layout now reuses the Windows watch illustration. Tests verify
  zero welcome scroll extent and reachable scan/demo actions at 384x832 and
  360x640 logical pixels. All 29 Flutter tests passed; analysis passed.
- WF branding was added to both Flutter headers, five Android launcher densities,
  a padded adaptive foreground and a seven-size Windows ICO. Android resource XML,
  generated manifest/overlay and all ICO image sizes were verified. The revised
  app packages still need CI rebuilding and installation before phone review.
- A known-device COM7 flash attempt failed at port opening with Access denied.
  No flash write occurred. An app-only manual update script was prepared locally.

## Remaining physical acceptance

Hardware follow-up: owner confirmed the d68479f Android build connects and reads
telemetry, app descriptors and diagnostics. Owner also reported a narrow power
dropdown overflow, unclear scan feedback/default UTC, and watch navigation
freezing while connected with a reported NO_MEM message. The candidate addresses
the mobile presentation/routing issues, cleans up the default Wi-Fi netif after
driver-init failure and exports internal free/largest/minimum heap plus PSRAM
free bytes. The navigation freeze is NOT established as fixed; capture the new
heap diagnostics before/after scan and navigation, and verify navigation in a
fresh session without scanning Wi-Fi. These new changes are not installed yet.

The owner reported a real BLE handshake timeout after entering the watch code.
Source inspection found that the native RS-only pairing probe remained in the
firmware line buffer, causing the following hello frame to be discarded. The
candidate now probes with LF and adds an LF boundary before BLE requests for
compatibility with the previous native bridge. Flutter regression coverage and
analysis passed (31 tests); a C framing regression is added for CI execution.
The corrected APK still needs building/installing and a physical retry. This
fix does not require changing the watch protocol or flashing new firmware.

Run the sequence in ANDROID_COMPANION.md on the real watch and phone. Pairing
codes, encrypted GATT, MTU fragmentation, fresh discovery after disconnect,
the five-minute timeout despite polling, display/PWR behavior, full stack release,
Radar coexistence, USB coexistence and actual battery use remain unverified.
Do not publish 0.3.0 until local hardware acceptance is reported and reviewed.
