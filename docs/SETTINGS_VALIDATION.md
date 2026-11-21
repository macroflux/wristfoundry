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

New tests cover NVS save/reload with mocked reboot, failed commits, unchanged
writes, invalid versions, revision conflicts, settings validation, desktop draft
retention, default restoration, and builtin/unknown-icon rendering. Execution
in CI remains required; their presence alone does not establish passing results.

No new firmware or desktop binary has been installed on the user's watch/PC.
Physical reboot persistence, cross-interface changes, USB/display/radio behavior,
launcher interaction and measured power use remain pending. See
SETTINGS_UPDATE.md for the physical acceptance sequence.
