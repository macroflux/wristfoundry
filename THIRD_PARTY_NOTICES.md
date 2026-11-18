# Third-party notices

The ESP-IDF Component Manager resolves the exact dependency graph during a
build. Before every release, regenerate the lock file and license inventory.

Resolved dependencies currently declared by `dependencies.lock`:

| Component | Version constraint | License verified in upstream package |
| --- | --- | --- |
| Espressif ESP-IDF | `>=5.5.5,<5.6.0` | Apache-2.0 |
| Espressif CMake Utilities | `0.5.3` | Apache-2.0 |
| Espressif Codec Device | `1.5.11` | Apache-2.0 |
| Espressif I/O Expander | `1.2.1` | Apache-2.0 |
| Espressif LCD Panel I/O Additions | `1.0.1~1` | Apache-2.0 |
| Espressif LCD Touch | `1.2.1` | Apache-2.0 |
| Espressif FT5x06 Touch | `1.1.1` | Apache-2.0 |
| Espressif LVGL Port | `2.9.0` | Apache-2.0 |
| Waveshare ESP32-S3-Touch-AMOLED-2.06 BSP | `2.0.0` | Apache-2.0 |
| Waveshare SH8601 LCD driver | `2.0.0` | Apache-2.0 |
| Waveshare QMI8658 component | `1.0.0` | Apache-2.0 |
| LVGL | `9.5.0` | MIT |

License identifiers above were checked against the resolved component packages.
LVGL may compile optional bundled libraries under their own permissive notices;
the generated release SBOM is the authoritative build-specific inventory. No
generated `managed_components` directory is distributed. Dependency sources are
fetched by ESP-IDF for each public build.
