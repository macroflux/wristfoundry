# USB awake application update review

Prepared 2026-11-20; **not flashed**. This application-only update requires
confirmation before writing. The user reports the earlier 14eb948 firmware
connects successfully and supports time/timezone synchronization.

Revision: `c1de44d95941b1eb7d7345ebad651f9248def1a2`, ESP-IDF 5.5.5, normal power mode 0,
development profile. [Development-only CI](https://github.com/macroflux/wristfoundry/actions/runs/44810966121)
passed compilation, C framing/power-policy tests, development configuration
assertions and artifact upload. The secure-profile job was skipped.

## Behavior

- Confirmed USB power wakes the display and suspends automatic dim/off.
- The processor and USB remain available while tethered.
- PWR can still deliberately blank the display and stop radios.
- Unplug starts a fresh battery idle interval with normal dim/off/light sleep.
- USB attachment from battery sleep restores the display directly.

PMU read errors do not hold the screen at full brightness on battery. Battery
VBUS polling/current impact and reconnect timing still need physical measurement.
This new display policy has not yet been tested on the watch.

## Exact write

| Image | Offset | Bytes | SHA-256 |
| --- | --- | --- | --- |
| wristfoundry.bin | 0x10000 | 1591456 | cb488ad80f327ad9e341fe51a509eafea4c22c538911c45504090eb4a5ac9f7f |

All artifact checksums were verified locally. The partition table matches the
earlier development image and the original backup. This update writes only
application sectors; bootloader, partition table, NVS, PHY data and eFuses are
not written. No erase-all, security provisioning, or secure profile is used.

The local ignored `build-hardware/FLASH-USB-AWAKE-COMMAND.txt` contains the exact
command with the extracted build path. At preparation the pinned USB candidate
was COM7; recheck it and close Companion/serial tools before running. The
command is:

```powershell
# Working directory: build-hardware/development-c1de44d/build
& 'C:\Espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe' -m esptool --chip esp32s3 --port COM7 --baud 460800 --before default_reset --after hard_reset write_flash --flash_mode keep --flash_freq keep --flash_size keep 0x10000 wristfoundry.bin
```

## Working-firmware rollback

The user-tested 14eb948 application is retained locally at
`build-hardware/development/build/wristfoundry.bin`, SHA-256
`92f8d48b7426eb7d855d44e2e3a695409441ea7563f07337a5aa9902c19ede9c`.
The separate `ROLLBACK-WORKING-COMPANION-COMMAND.txt` writes that application at
0x10000 only, preserving the companion-enabled baseline and NVS. It is for
recovery if needed, not a planned final restore. The original verified full
backup and full-flash recovery command remain untouched.

## Hardware check after applying

Reconnect the real companion, confirm the revised USB policy under Settings,
and leave the watch untouched on USB for over 30 seconds. It should stay bright
and connected. Unplug and check dim/off on battery, then reconnect while asleep
and confirm display wake and a fresh protocol handshake. Check deliberate PWR
blanking separately. Record user observations separately from automated evidence.
