# Companion development flash review

Prepared 2026-11-20; **not flashed**. Approval is required before writing.

Source revision: `14eb94856fe815bf8a0c7a3714c00a76fc984359`, normal power mode 0,
ESP-IDF 5.5.5, ESP32-S3, development profile. The
[development-only CI run](https://github.com/macroflux/wristfoundry/actions/runs/44805589926)
passed. The secure-profile job was skipped. No security configuration or eFuses
are changed, and the development application leaves NVS uninitialized.

## Verified images

| Image | Offset | Bytes | SHA-256 |
| --- | --- | --- | --- |
| bootloader/bootloader.bin | 0x0 | 22288 | c620e5fb3881d27ecab39ec8c626ca28283c717be31acaef2063b0d7bd386136 |
| partition_table/partition-table.bin | 0x8000 | 3072 | d698a34f5307e719b6460f3b6b5cb1b47ec1bd60d788e6c26066d846d49641f3 |
| wristfoundry.bin | 0x10000 | 1591120 | 92f8d48b7426eb7d855d44e2e3a695409441ea7563f07337a5aa9902c19ede9c |

The downloaded CI ZIP digest is
`3208e479ba14960774b13074ed686fde519029456fe58f0f787da1bd8ec799ad`.
Esptool validated the application and bootloader internal checksum/hash.
The partition table matches the verified recovery backup exactly. NVS occupies
0x9000..0xEFFF and PHY data 0xF000..0xFFFF; neither is in the write list. Esptool
erases only sectors covering the three supplied images, not the entire flash.

## Prepared command (not executed)

At preparation, the sole Espressif native USB candidate was **COM7**,
VID:PID 303a:1001. Its USB identity is pinned in a local, ignored preflight file.
Re-enumerate and match that identity immediately before flashing; do not guess
if the COM number changes or multiple candidates appear. VID/PID alone is not
cryptographic identity. Close the desktop companion and serial monitors first.
The current legacy firmware may need waking to expose its port for this flash.

From the repository root after checking the verified bundle:

```powershell
Push-Location -LiteralPath build-hardware/development/build
try {
  & 'C:\Espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe' -m esptool --chip esp32s3 --port COM7 --baud 460800 --before default_reset --after hard_reset write_flash '@flash_args'
} finally { Pop-Location }
```

The supplied `flash_args` selects DIO, 80 MHz and 32 MB, then lists exactly the
three offsets/images above. No erase-all, NVS image, encryption or eFuse command
is present. Flashing resets the watch and replaces the running application.

## Recovery prepared separately

The existing private backup is 16 contiguous 2 MiB chunks covering 32 MiB.
Every chunk matches RECOVERY-MANIFEST.txt. Its legacy provisioning marker is
present; companion v1 markers are absent. A full 16-offset restore argument
file and exact local command are prepared under the ignored `build-hardware`
directory. The originals remain untouched in their existing backup directory.

The recovery command uses esptool `write_flash --flash_mode keep --flash_freq
keep --flash_size keep` with all 16 original offsets. **It restores the entire
backup, including its saved NVS**, so it is only for deliberate recovery or an
explicit restore request. It has not been run. Follow [RECOVERY.md](RECOVERY.md)
and the board's BOOT/reset procedure if the application cannot expose USB.

## After explicit flash approval

1. Recheck port identity, image checksums and available serial access; flash only
   the development command above and verify the flash write.
2. Observe non-secret startup markers, normal-mode/profile and protocol-v1-ready
   status. Do not publish raw serial output, device IDs or credentials.
3. Test the real companion identity/capabilities/dashboard and PC UTC sync with
   RTC readback. Test Wi-Fi only with locally supplied credentials.
4. Observe USB through display-off, unplug/battery sleep, and reattachment while
   asleep. Test manual PWR-off/release too. Record actual results and failures;
   current draw and battery impact require measurement.

Until those tests occur, all new sleep/attach behavior and hardware acceptance
remain unverified. No backup restoration is planned as the final state.
