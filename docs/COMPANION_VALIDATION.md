# Companion validation record

## Hardware preparation follow-up (2026-11-20)

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

Updated 2026-11-20. Implementation is published on
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

No merge, release publication, hardware flashing or eFuse provisioning was
performed. CI only builds/tests/packages and uploads review artifacts.
