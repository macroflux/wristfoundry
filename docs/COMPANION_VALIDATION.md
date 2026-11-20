# Companion validation record

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
