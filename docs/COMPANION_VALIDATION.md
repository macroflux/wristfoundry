# Companion validation record

Base: `5b910a02e6301694d0dfcc8ccff71f334d13f12f` (2026-11-20).

## Implemented, not yet hardware-verified

Flutter UI/controller, Win32 serial bridge, versioned framed protocol, separate
demo transport, firmware dispatcher, shared Wi-Fi/time APIs, UI-owner app
actions, allowlisted diagnostics, Windows runtime packaging, tests and CI.

## Local environment evidence

- Repository restored byte-for-byte from the connected source after Git network
  cloning failed; original Git objects are available from the local public repo.
- `scripts/Build.ps1`: blocked during ESP-IDF 5.5.5 export. Installed cross tools
  report Windows error 623 (illegal system DLL relocation). No firmware build,
  flash or eFuse operation completed from this attempt.
- Flutter/Dart are not on PATH. Windows analyze/test/release compilation and
  rendered UI capture must be verified by CI or a configured SDK environment.
- GitHub connector branch creation returned HTTP 403; publication is being
  attempted through the existing authorized Desktop connection. This record
  must be updated with the actual final result.

## Automated checks provided

`tests/companion_frame_test.c` exercises the actual firmware incremental parser
and timezone whitelist, including exact bounds, oversized-frame recovery and
CRLF handling. Python contract tests validate JSON Schema fixtures, cross-language
timezone rules and single-reader ownership. Flutter protocol tests cover
fragments/coalescing, malformed/oversized input, ID mismatch, unsupported version,
timeout, disconnect and pending bounds. Controller tests cover candidate counts,
hotplug, unplug during save, stale snapshots, capability gating, busy ports,
wrong identity, retry/cancel, selected-device reconnection and RTC error replies.

`test/ui_test.dart` renders the actual Flutter widgets at 1440x1000 and writes
Welcome, multiple-device, demo-dashboard, Wi-Fi, Time and disconnected PNGs.
These are software-rendered demo/fixture captures, not photographs or evidence
of a live hardware connection. CI retains them as review artifacts even if a
later native release build fails. Screenshots are not yet claimed captured in
this local environment.

The existing firmware development/secure CI is retained. The added Companion
workflow runs schema/C tests, Windows analysis/tests/captures/release packaging.
No workflow flashes a device, provisions eFuses, merges or publishes a release.

## Release gates

Passing compilation and simulated tests are not physical acceptance. Complete
the separate USB, radio, RTC, app, sleep and clean-PC checks in `COMPANION.md`.
