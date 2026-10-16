# Hardware support matrix

Target: Waveshare ESP32-S3-Touch-AMOLED-2.06. “Verified” means evidence exists in
the extracted code and/or prior physical testing; it does not mean every public
0.1 path has completed release acceptance.

| Capability | Evidence/status | Public plan |
| --- | --- | --- |
| 410 x 502 AMOLED | Verified through first-party BSP and physical UI use | Keep responsive safe-area layouts; reconcile SH8601/CO5300 naming before release |
| FT3168 touch | Verified in launcher/navigation | Validate keyboard edge targets under beveled cover |
| GPIO0 Home/Apps | Verified | Retain global navigation behavior |
| AXP2101 PMIC/power key | Verified telemetry and events | Add current/rail diagnostics where the BSP safely exposes them |
| PCF85063 RTC | Verified driver access | NTP sets RTC; RTC remains offline source |
| QMI8658 accelerometer | Verified identity/readings | Add calibrated motion service and sample app |
| QMI8658 gyroscope | Present in board/BSP, not exercised by this firmware | Add and validate in IMU phase |
| Wi-Fi | Provisioning and on-demand service implemented | Hardware-validate on-watch keyboard and real-AP NTP |
| BLE | Bounded advertisement scan verified | Optional module only; never a time source |
| microSD, GPIO1/2/3 | Exposed by board/BSP; not implemented here | Add service, mount/unmount policy, assets and privacy-safe rotating logs |
| ES7210 dual microphones | Shown by board docs/BSP; not implemented here | Capture diagnostics only after privacy and power gates |
| ES8311 + NS4150B speaker/amp, amp GPIO46 | Shown by board docs/BSP; not implemented here | Tone/playback diagnostic with hard amp-off sleep policy |
| Vibration motor, GPIO18 | Shown by board docs; not implemented here | Haptic service with duration/duty limits |
| QMI8658 INT1, GPIO21 | Shown by schematic; wake path not implemented | Motion/raise-to-wake experiment with false-wake measurement |
| USB Serial/JTAG | Verified build/monitor path | Document recovery and diagnostic commands |

Wi-Fi credentials are never stored on microSD. SD is intended for replaceable
assets, explicit exports, screenshots, and size-limited diagnostic/sensor logs.
Logging is off by default and must flush/unmount before sleep.
