# Settings and icon validation

Version: 0.2.0. The initial preparation record below is historical; the latest
CI and hardware results are recorded at the end.

Local checks on 2026-11-21:

- ESP-Clang syntax checks passed for settings_service.c, settings_app.c,
  app_manager.c, watch_ui.c, companion_service.c, network_service.c,
  time_sync_service.c and main.c, using ESP-IDF headers and cached configuration.
  This is not a complete compile/link or firmware build.
- Release-note extraction/version-drift test passed.
- `git diff --check` passed.
- Full local ESP-IDF activation failed with Windows error 623 (illegal system
  DLL relocation) while checking the installed compiler tools.
- Flutter/Dart are unavailable on PATH. Local Windows C++ compiler tools are
  also absent; the host C tests could not be linked/run locally.
- Protocol Python test discovery was blocked by missing `jsonschema` locally.
- Initial CI submission was blocked: the connected GitHub integration denies tree
  writes (403), and local GitHub CLI authentication fails (401). A normal Git
  fetch works with the Windows certificate backend, but authenticated push and
  PR creation did not succeed from this task. The user subsequently pushed the
  branch and opened draft PR #2 from their own PowerShell.

## CI at source 42de547

[Firmware run](https://github.com/macroflux/wristfoundry/actions/runs/44980328189)
passed development and encrypted-NVS builds, security assertions, public safety
checks and artifact uploads. Secure firmware was built only, not flashed.

[Companion run](https://github.com/macroflux/wristfoundry/actions/runs/44980328220)
passed Python contracts, C framing/radio policy tests, production settings-service
tests with mocked NVS, and Flutter analysis. During the widget tests, review
identified a fake-clock deadlock in new settings-test initialization. Commit
173a8aa moves delayed demo initialization into tester.runAsync. A rerun is
required before claiming the Flutter suite and Windows package are validated.

The development artifact ZIP was downloaded locally and its SHA-256 matched
the GitHub artifact digest:
`c2e049e72810dad2f4ea5151f07bb20498a8c2967ac87c6c1b89ee6743e32640`.

New tests cover NVS save/reload with mocked reboot, failed commits, unchanged
writes, invalid versions, revision conflicts, settings validation, desktop draft
retention, default restoration, and builtin/unknown-icon rendering. Execution
in CI remains required; their presence alone does not establish passing results.

At that initial checkpoint, no new firmware or desktop binary had been installed.
Physical reboot persistence, cross-interface changes, USB/display/radio behavior,
launcher interaction and measured power use remain pending. See
SETTINGS_UPDATE.md for the physical acceptance sequence.

## Release acceptance, 2026-11-21

- At source f0825f15a44dbc131976e07e5af5e7c56dc8725b, firmware run
  [44981861617](https://github.com/macroflux/wristfoundry/actions/runs/44981861617)
  produced development and secure build artifacts. The secure image was not flashed.
- Companion run
  [44981861623](https://github.com/macroflux/wristfoundry/actions/runs/44981861623)
  passed contract/native tests, Flutter analysis/tests, and Windows release
  build/packaging after the fake-clock test fix.
- Downloaded artifact digests and internal package checksums were verified.
  Development partition-table bytes match the earlier USB-awake build.
  Application SHA-256:
  `ac9721fa7a06a148ed23ce77be100f1fbb426208271318c134e99e24637b020f`.
- Installed the complete Companion 0.2.0+2 runtime locally, preserving the old
  runtime. An older running instance initially displayed the old controls;
  reopening the updated EXE resolved this.
- The owner's screenshot shows real COM7 connection, watch firmware 0.2.0,
  Companion 0.2, and enabled Power & display controls including Save to watch.
- Following the save/cross-interface/restart-persistence test instructions, the
  owner reported "perfect, works awesome" and approved release. This is owner
  acceptance, not an independently captured RTC/NVS readback or exhaustive test log.
- Measured battery current, exhaustive radio deadline/sleep/reconnect behavior,
  and all negative-path hardware checks remain unverified. Automated tests cover
  mocked NVS failures, revisions and bounded policy logic.
- Release approval is recorded; publication status must be checked on GitHub.
