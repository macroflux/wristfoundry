# Companion validation record

## Hardware preparation follow-up (2026-11-20)

### User-run development flash and successful companion use

After automated attempts could not open the serial port, the user ran the
prepared development flash command in their own PowerShell and reported that
it wrote successfully. The prepared images were from revision `14eb948` and
the checksummed development-only artifact described below. The agent did not
observe that external flash output or perform a post-flash binary readback.

The user then reported that the real companion works and that time setting,
timezone selection and clock synchronization work. These are user-observed
hardware results, not CI/demo results. The automated direct protocol probe
still could not open the port, so independently captured startup markers,
RTC readback comparison and timed USB screen-off/unplug/reconnect acceptance
remain pending. No Wi-Fi credentials have been supplied or configured here.

The requested follow-up suppresses automatic display dim/off on confirmed USB
power, in addition to the CPU light-sleep guard already in `14eb948`. USB attach
wakes the display; unplug starts a fresh battery inactivity interval. Manual
PWR blanking remains available. The companion reports the revised policy.
This display-timeout change has not yet been flashed or hardware-validated.

Final tethered-display revision:
`c1de44d95941b1eb7d7345ebad651f9248def1a2`. Its
[development-only run](https://github.com/macroflux/wristfoundry/actions/runs/44810966121)
passed the C framing/power-policy test, full ESP-IDF 5.5.5 build, development
configuration assertions and artifact upload; the secure job was skipped.
Downloaded ZIP and all image SHA-256 values match the artifact manifest, the
partition table matches the existing baseline, and the application contains
the new USB display policy and direct wake markers. Esptool validated its
internal image checksum/hash. The [app-only review](COMPANION_USB_AWAKE_REVIEW.md)
records exact image hash, offset, update command and rollback to the user-tested
companion firmware. The new application has not been flashed; no additional
hardware results are claimed for it.

The user approved applying the `c1de44d` app-only update. The pinned native USB
device and all image checksums were rechecked, and no running companion or
Python/serial-tool process was found. The automated esptool attempt again
received Windows access denied when opening the port and exited with code 2
before writing. The prepared app-only command was handed back for the user's
own PowerShell, which previously succeeded for `14eb948`. This automated
attempt did not alter the running firmware or NVS; applying and observing the
new display behavior remain pending.

### Earlier automated flash attempts: blocked before writing

The user approved the development flash and subsequent hardware tests. The
image checksums and pinned USB identity were checked again; the selected native
USB candidate remained available on the same port. No running companion,
Python serial monitor, or common flashing process was found. Esptool 4.12.0
could not open the selected port: Windows returned PermissionError 13 / access
denied (OS error 5), and esptool exited with code 2 before chip synchronization
or any flash write. A busy port and other OS/device access problems cannot yet
be distinguished. No image was written, no backup was restored, and no NVS or
eFuse operation occurred. Startup, real protocol/dashboard, RTC readback and
USB sleep/reconnect tests therefore remain unperformed on this build. The next
step is waking/reconnecting the existing firmware and rechecking serial access;
the user's development-flash approval remains in effect.

User observations: the Windows app launches; native USB produced Code 43 while
the watch was asleep. Waking the watch and reconnecting exposed a serial port
and a selectable candidate, but the protocol handshake timed out. The user
restored backup firmware and has not flashed companion-enabled firmware.
These observations do not establish a desktop protocol defect.

The existing private recovery set was located and verified: 16 contiguous
2 MiB chunks cover 32 MiB, all matching the recorded SHA-256 manifest. Binary
inspection found the legacy provisioning task marker and no v1 frame-prefix or
companion-service marker. The matching legacy source tree has no companion
implementation. This supports the expected protocol mismatch; a byte-for-byte
readback of the currently running device has not been performed. No backup,
credential-bearing contents, or device identifiers are included here.

The normal-mode branch includes companion_service in CMake and initializes it
after network/time services with ESP_ERROR_CHECK. A non-secret startup marker
now explicitly reports protocol v1 ready. Native USB Serial/JTAG console and
desktop framing agree on RS + WF1 + JSON + LF, protocol version 1.

The sleep follow-up centralizes a VBUS guard at every manual sleep entry,
including the PWR-release path, and rechecks before sleeping. VBUS protection
does not depend on successful host enumeration. PMU read failure fails awake.
The optional IDF automatic-sleep connection guard is requested in defaults;
it is conditional on PM being enabled and does not replace the manual guard.
Normal battery mode still uses light sleep, with a one-second timer checking
VBUS while display, touch, and radios remain asleep/off. D+ is detached during
sleep and restored on exit so USB attach can enumerate after wake. USB attach
keeps the AMOLED off. This adds periodic battery wake work: current impact and
Windows re-enumeration are not yet measured or hardware-verified.

Local full build remains blocked during ESP-IDF tool activation by Windows
error 623; local C syntax checks pass. A development-only manual CI option was
added so hardware preparation can build without running the secure profile.
New firmware has not been flashed. Real startup, handshake/dashboard, RTC
readback, screen-off USB and unplug/reconnect acceptance remain pending explicit
flash approval and physical testing. Wi-Fi configuration requires locally
provided credentials and has not been tested.

Prepared implementation revision: `14eb94856fe815bf8a0c7a3714c00a76fc984359`.
The [development-only run](https://github.com/macroflux/wristfoundry/actions/runs/44805589926)
passed its C framing/sleep-policy tests, full ESP-IDF 5.5.5 build, development
configuration assertions, checksum generation and upload. The encrypted-NVS
job was explicitly skipped. The artifact ZIP digest and all three image
checksums were verified locally. Esptool validated application and bootloader
image checksums/hashes. The binary contains the v1 prefix and startup-ready
marker; its partition table exactly matches the backup table. This is build
evidence, not successful physical startup. See [flash review](COMPANION_FLASH_REVIEW.md).

Local Python contract execution was blocked by a missing jsonschema module;
the local host-C executable could not link in this environment. The four
firmware C syntax checks passed, and the host-C test passed in the run above.
Earlier desktop/schema results below remain tied to their stated revisions.

## Initial CI record (before user hardware testing)

Recorded 2026-11-20. Implementation is published on
`feat/wristfoundry-companion` in [draft PR #1](https://github.com/macroflux/wristfoundry/pull/1).
Base: `5b910a02e6301694d0dfcc8ccff71f334d13f12f`.

## Observed automated results

The [Companion run for source 81b28a6](https://github.com/macroflux/wristfoundry/actions/runs/44780694614)
completed successfully on Windows Server 2022 with Flutter 3.38.7 / Dart 3.10.7.

| Check | Observed result |
| --- | --- |
| Flutter analyzer, fatal infos enabled | Passed; no issues found |
| Flutter tests | All 23 passed |
| Windows x64 release build | Passed, including the custom native USB bridge |
| Full-runtime ZIP packaging and SHA-256 manifest | Passed; CI artifact uploaded |
| Python schema/contract checks | All 3 passed |
| Host C framing/timezone/power-policy checks | Passed with warnings treated as errors |
| Public safety checks | Passed |

The source was formatted by CI before analysis and compilation. The generated
dependency lockfile is retained with this review evidence. The downloadable
Windows artifact belongs to the run above, not to a published release.

The [firmware run for source 81b28a6](https://github.com/macroflux/wristfoundry/actions/runs/44780694644)
failed on an RTC snapshot `snprintf` format-truncation warning treated as an
error. Commit `ead0dadb2c96391fe58b2c75898909926cc150aa` increases that buffer
to accommodate the full storage-type ranges. Its
[firmware verification run](https://github.com/macroflux/wristfoundry/actions/runs/44781590197)
passed both ESP-IDF 5.5.5 development and encrypted-NVS builds, their security
configuration assertions, checksums, and artifact uploads. The development run
also generated its CycloneDX SBOM. The
[Companion rerun at ead0dad](https://github.com/macroflux/wristfoundry/actions/runs/44781590208)
also passed Windows analysis, all 23 tests, native release compilation, packaging,
and the Python/C contract checks. These are the validated implementation revision;
the following evidence commit adds documentation, screenshots, and the exact
resolved lockfile only.

## Rendered UI evidence

[Companion UI review](COMPANION_UI_REVIEW.md) includes seven actual Flutter
widget renders at 1440 x 1000: Connect with multiple candidates, dashboard,
Wi-Fi, time, supported settings, compiled apps, and disconnected state. They
were captured from source `81b28a6` using fixture discovery and the labeled
demo transport. No watch was connected or changed by the screenshot tests.

The original CI artifact contains nine PNGs. Two are excluded from the gallery:
the initial no-device frame painted incompletely, and the diagnostics report
used box glyphs because Consolas was not loaded into the widget-test renderer.
The analyzer and UI tests passed, but these screenshots do not establish native
Windows font rendering or clean-PC acceptance.

## Test coverage and limits

The actual firmware framing parser is exercised for exact bounds, raw NUL,
oversized-frame recovery and CRLF handling. Contract checks validate JSON Schema
fixtures, cross-language timezone rules and single-reader ownership. Flutter
protocol tests cover fragments/coalescing, malformed/oversized input, mismatched
and stale-session IDs, unsupported versions, timeout, disconnect and pending
bounds. Controller tests cover candidates, hotplug, unplug during save, stale
snapshots, capability gating, busy ports, wrong identity, retry/cancel, selected
device reconnection and RTC error replies. UI tests exercise acknowledged forms,
password exclusion from diagnostics, compiled-app labels and live-state clearing.

Local ESP-IDF activation is blocked by Windows error 623 (illegal system DLL
relocation). Local Clang syntax checks passed for all four changed firmware C
files using ESP-IDF headers and cached configuration; these are not a complete
firmware build. Flutter/Dart are absent from the local PATH, so the desktop
results above come from CI, not a claimed local SDK build.

## Hardware and distribution acceptance still required

All physical USB, DTR/RTS/reset behavior, multiple-watch selection, Wi-Fi,
credential persistence, RTC/NTP, app navigation, display-off handling, reconnect
and power/current behavior remain **hardware-unverified**. A clean Windows PC
without the SDK has not been tested. Complete the physical acceptance checklist
in [COMPANION.md](COMPANION.md) before treating this as release-ready.

Brightness/sleep setters and individual app installation are unavailable by
design. Existing local-wall-time RTC ambiguity through an offline DST transition
remains documented. USB identity is a compatibility check, not cryptographic
authentication; credentials use the local USB link and the existing firmware
storage policy.

No merge, release publication or eFuse provisioning was performed. CI only
builds/tests/packages and uploads review artifacts. The subsequent user-run
development flash and user-reported results are recorded at the top.
