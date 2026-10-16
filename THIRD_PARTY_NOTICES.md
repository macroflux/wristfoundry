# Third-party notices

The ESP-IDF Component Manager resolves the exact dependency graph during a
build. Before every release, regenerate the lock file and license inventory.

Direct dependencies currently declared by this project:

| Component | Version constraint | License verified in upstream package |
| --- | --- | --- |
| Espressif ESP-IDF | `>=5.5.5,<5.6.0` | Apache-2.0 |
| Waveshare ESP32-S3-Touch-AMOLED-2.06 BSP | `2.0.0` | Apache-2.0 |
| Waveshare QMI8658 component | `1.0.0` | Apache-2.0 |
| LVGL | `9.5.0` | MIT |

No generated `managed_components` directory is distributed from the private
source. Dependency sources are fetched by ESP-IDF for each public build.
