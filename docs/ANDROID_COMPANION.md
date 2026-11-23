# Android BLE Companion (0.3.0 development)

The Android app shares the Windows Companion's Dart controller, framed protocol,
settings editor, time rules and app icon metadata. Windows continues to use USB.
This candidate is not released and has not passed phone/watch hardware acceptance.

## Connect

1. Watch: Settings opens section cards. Choose Phone connection, then Discover
   for 60 seconds. Stop BLE Radar first if it owns the radio.
2. Android: open Companion, tap Scan for watches, grant Nearby devices access,
   select the watch, and enter its current six-digit code in Android's pairing
   dialog. Older Android versions need foreground location permission for scanning.
3. Complete the identity/capability handshake. The display wakes and stays on
   while the authenticated companion session is active. PWR can still blank it.
4. The watch disconnects after five minutes without user interaction. Telemetry
   polling and status reads do not renew this deadline. Disconnect, cancellation,
   or expiry releases the BLE host/controller and resumes saved inactivity policy.

Discovery does not suspend normal display timeout, but blocks processor light
sleep until the window expires. A pending connection has a separate 45-second
pairing/handshake deadline. Disconnect never starts another discovery window.
Returning from a session gives the normal power profile a fresh inactivity interval.
Android closes active connections when backgrounded; it does not run a background
service or silently reconnect. A system pairing dialog may finish before that rule
applies. The firmware deadline remains the final bound if the app disappears.

## Pairing and transport

- GATT service `8c8e0001-6e9b-4b71-9e1d-42b47c574631`.
- Write-with-response RX `8c8e0002-6e9b-4b71-9e1d-42b47c574631` requires
  encrypted, authenticated BLE pairing. The phone uses Android's system passkey UI.
- Notify TX `8c8e0003-6e9b-4b71-9e1d-42b47c574631`; firmware sends only after
  authentication and subscription. Pin codes are displayed locally, never returned
  by the USB/BLE protocol or exported in diagnostics.
- Version-1 RS/WF1/JSON/LF frames span GATT chunks. Android requests MTU 185;
  serialized acknowledged writes respect negotiated MTU. Queues and frame sizes
  are bounded; overflow/failed notification terminates the session.
- No persistent watch bonds or new NVS/eFuse keys are introduced. Each session
  uses a fresh watch code. Some phones may retain an old pairing; forget it in
  Android Bluetooth settings and retry if pairing fails after a watch restart.
- The dispatcher serializes USB and BLE requests, keeping separate handshakes
  and frame buffers. Both interfaces use the same revision-checked settings service.
- `phone.get` reports status/countdown without the code. USB can issue
  `phone.discover` and `phone.disconnect`. BLE clients disconnect locally.
- `session.activity` is sent only for explicit app interaction, throttled to once
  per ten seconds. Mutating commands and local watch interaction also count.
  Repeated hello/status reads do not.

## Build and install for testing

Use Flutter 3.38.7, JDK 17 and an Android SDK. From the repository:

```powershell
python scripts/prepare_android.py
cd companion/desktop
flutter pub get
flutter analyze --fatal-infos
flutter test --reporter expanded --timeout 90s
flutter build apk --debug
```

Install `companion/desktop/build/app/outputs/flutter-apk/app-debug.apk` with
`adb install -r <path>` or transfer the APK and allow installation from that source.
The debug APK is for local testing. A distributable signed Android release needs
a separately managed signing identity; no signing secrets are committed.

The preparation script generates the pinned Flutter Android scaffold, copies
the reviewed native bridge from `companion/android-native/MainActivity.kt`, and
adds foreground Bluetooth permissions. Generated Android files are ignored.
CI also packages a debug APK and checksums. The existing stable release workflow
is unchanged; Android publication is intentionally deferred until hardware testing.

## Acceptance before release

- Install the APK on a real Android phone and application-only development firmware
  on the watch using the existing reviewed backup/update process. Never flash the
  secure profile as an ordinary update, erase NVS, or provision eFuses for this test.
- Observe discovery ending after 60 seconds, with no re-advertising after expiry,
  cancellation or disconnect. A second start while Radar owns the radio must fail.
- Reject incorrect pairing codes, missing subscription, unauthenticated commands,
  oversized frames and stale fragments. Retry after cancellation and watch reboot.
- Verify BLE settings save, watch/phone/Windows revision refresh and reboot
  persistence; verify time/timezone, Wi-Fi credentials and app/icon navigation.
- Confirm a connected/authenticated session wakes and holds the display. PWR
  still blanks it. A phone merely linked/pairing must not obtain an indefinite lease.
- Leave the app untouched while polling: disconnect by five minutes. Touch/use
  the app to extend it, then test phone Bluetooth off, app background and force-stop.
- Verify unplugged battery dim/sleep and Radar work after BLE cleanup. Confirm
  USB remains usable alongside BLE. Measure current before choosing wider defaults.
