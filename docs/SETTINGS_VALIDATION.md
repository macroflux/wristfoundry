# Settings and icon candidate validation

Candidate source version: 0.2.0. No tag, release publication or hardware flash.

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

No new firmware or desktop binary has been installed on the user's watch/PC.
Physical reboot persistence, cross-interface changes, USB/display/radio behavior,
launcher interaction and measured power use remain pending. See
SETTINGS_UPDATE.md for the physical acceptance sequence.
