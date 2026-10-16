# Architecture

The 0.1 baseline keeps the proven implementation compact while establishing
public boundaries that can become ESP-IDF components without changing app code.

| Boundary | Current files | Responsibility |
| --- | --- | --- |
| App runtime | `app_manager.*` | registration, lazy creation, routes, lifecycle, navigation stress test |
| UI system | `ui_design.*`, `watch_ui.*` | layout tokens, watch face, launcher integration, status UI |
| Board | `board_services.*` | PMIC, RTC, QMI8658, battery and board telemetry |
| Power | `main.c` | dim/off timers, radio shutdown, light sleep, GPIO wake, power-test modes |
| Settings/network | `settings_app.*`, `network_service.*` | encrypted credentials, on-demand Wi-Fi, NTP, radio leases |
| Time | `time_sync_service.*` | NTP orchestration and sync history; hardware RTC is the offline clock |
| Optional BLE | `ble_radio_owner.*`, `ble_radar_*` | exclusive BLE ownership and bounded scanning |

The target component layout is `wf_app`, `wf_ui`, `wf_board`,
`wf_board_waveshare`, `wf_power`, `wf_settings`, `wf_network`, `wf_time`, and an
optional `wf_ble`. That split is proposed, not yet completed.

Apps do not initialize radios or board drivers directly. Services own scarce
hardware and expose bounded operations. Display-off and light-sleep transitions
cancel time sync, turn Wi-Fi off, stop BLE, and wait for resource release.
