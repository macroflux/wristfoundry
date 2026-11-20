# WristFoundry Companion

The official Flutter Windows companion lives in `companion/desktop`. It uses
the same cyan/charcoal instrument design as `main/ui_design.h`, a connection-first
welcome view, a device dashboard, and explicit, acknowledged service actions.
This contribution is a **draft, hardware-unverified implementation** until the
physical acceptance checks below pass. Build/test evidence is recorded in
`COMPANION_VALIDATION.md`; do not infer passing builds from the presence of CI.

## Build and run (Windows x64)

Install Flutter 3.38.7 (Dart 3.10) from Flutter's official SDK archive and Visual
Studio's **Desktop development with C++** workload with the Windows SDK. This
repository includes the Windows runner; do not regenerate it with `flutter
create`, which would overwrite the native USB bridge integration.

```powershell
cd companion/desktop
flutter doctor -v
flutter pub get
dart format lib test
flutter analyze --fatal-infos
flutter test --reporter expanded
flutter run -d windows
flutter build windows --release
../../scripts/Package-Companion.ps1
```

Start normally with no watch attached. Use **Try demo** for a visibly labeled
simulated watch. Demo uses the same framed client over a separate mock transport
and never opens a serial port. Its example readings are not hardware evidence.

CI builds a full runtime ZIP with the EXE, Flutter engine DLL, data/assets and
licenses. The serial bridge uses Windows SetupAPI/Win32 directly, so no separate
libserialport DLL must be distributed. Keep the whole directory together.
An unsigned portable build may require the supported Microsoft Visual C++ x64
redistributable. No installer, signing certificate, release or store submission
is created by the workflow.

## Connect and recover

Use a USB **data** cable. Discovery enumerates metadata every two seconds; it
does not open ports. Native Espressif USB Serial/JTAG devices are prioritized;
other ports require **Show other serial ports** and explicit selection. A port
label is not WristFoundry identity. Select a device, connect, then wait for
version/identity validation and the initial snapshot before changing anything.

With several watches present, select one. An unplug clears live values, disables
all watch actions, cancels pending calls, and clears password input. The last
device is labeled last-seen. Optional auto-reconnect only targets that same USB
identity and validated firmware identity. Navigation survives reconnect; fresh
state must load again. Preferences are currently session-only; no passwords or
device IDs are written to desktop configuration files.

Close `idf.py monitor`, serial terminals and other tools if a port is busy.
Retry after reconnecting a working data cable. Wrong-device, incompatible
version and handshake-timeout errors remain on the welcome screen. For old
firmware, use the repository's documented development build/flash workflow in
`INSTALL.md` as a **separate deliberate hardware operation**. Companion never
flashes or provisions eFuses. Do not use secure-profile flashing as routine setup.

The native bridge does not issue a break or bootloader reset sequence and keeps
DTR/RTS disabled. Initial driver line state at open must still be tested on the
physical native USB hardware. Ambiguous third-party USB/UART adapters are not
claimed reset-safe.

## Shared services and boundaries

`companion_service` is the sole serial reader, replacing the old text/hex Wi-Fi
provisioning console. Firmware RX and dispatch run in their own lower-priority
task, not LVGL. Existing network service functions retain radio leases, bounded
scan/connect/NTP work and display-off cancellation. App listing/navigation are
passed to the existing health/UI owner under the display lock. UI actions time
out and cancel their pending slot if the owner is unavailable. The companion
does not create its own Wi-Fi driver, RTC driver or navigation stack.

The development profile retains RAM-only credentials. Secure builds retain
encrypted NVS and the existing eFuse boundary. USB itself is not encrypted or
cryptographically authenticated: use a trusted PC and a physically controlled
watch. Never enable verbose native driver/request dumps when handling secrets.
Firmware wipes raw command buffers and cJSON strings, but Dart/Windows use
managed/native copies and do not promise forensic memory erasure. Export uses
a strict allowlist and a 100-event history, excluding raw logs, SSIDs, IPs,
device IDs, ports and request arguments. Copy/export is user initiated.

## Time behavior

PC sync sends `DateTime.now().toUtc()` seconds. Manual entry explicitly selects
the source timezone and converts using rules for the **entered date**. US and
European DST gaps and repeated hours are rejected; use UTC to disambiguate.
The v1 catalog is UTC, US Eastern/Central/Mountain/Pacific, Arizona, London and
Central Europe. It does not claim worldwide IANA coverage. Rules are versioned
source code and need updating if jurisdictions change their rules.

The firmware's existing RTC stores local wall time. UTC writes first update the
RTC through the board service and only then set system time; an RTC failure
is not a successful sync. Timezone changes are independent of passwords and
reject while network NTP is pending. With established system UTC they rewrite
local RTC time. Before UTC is established, timezone selection applies at the
next explicit PC/manual/NTP time set. NTP still uses the existing time-sync
owner and reports RTC writeback failure in its device status.

Local-wall-time RTC storage inherits an ambiguity through an offline DST
transition. Continuous UTC RTC storage/boot migration is a future framework
change; this PR does not claim to resolve that existing design limitation.

## Capabilities and app model

The dashboard routes to Wi-Fi, time, settings, apps and diagnostics. Firmware
advertises only implemented commands. Brightness and sleep timing remain
compile-time policy with setters unavailable. Apps are compiled descriptors;
**individual installation/removal is unavailable** until a package runtime
exists. Extension names and prerequisites are documented in `protocol/README.md`.

## Physical acceptance (not performed by CI)

- Attach one and multiple native USB watches; verify no reset on explicit open,
  cancel/retry, busy-port and permission errors; disconnect during every write.
- Verify identity and version rejection using another serial device; reconnect
  only the selected watch, including a changed COM number and USB ID collision.
- Verify actual Wi-Fi scanning, Unicode SSID byte limits, open/WPA networks,
  wrong passwords, connect result, display-off cancellation and lease release.
- Test RAM-only reboot loss and encrypted-NVS persistence independently, using
  already authorized/provisioned test hardware; never provision eFuses from UI.
- Compare PC/manual UTC, RTC readback, NTP-to-RTC success/failure and offline
  continuity. Test DST boundaries and clarify local-RTC transition limitation.
- Open each compiled app with watch controls and companion concurrently; test
  display-off navigation, wake, radio ownership and LVGL responsiveness.
- Measure active, screen-off and battery/sleep current with companion connected,
  unplugged and reconnected. RX waits at most 50 ms; status/heartbeat cadence is
  two seconds, with no permanent new radio lease. Numeric power impact remains
  unmeasured. Confirm existing battery light-sleep behavior is preserved.
- Inspect the full portable ZIP on a clean Windows x64 PC without an SDK. Verify
  runtime assets, labels, text scaling, narrow windows, and report redaction.

See [protocol](../protocol/README.md), [power](POWER.md), and
[secure storage](SECURE_STORAGE.md).
