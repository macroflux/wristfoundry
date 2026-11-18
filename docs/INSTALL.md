# Installation and flashing

## Requirements

- Waveshare ESP32-S3-Touch-AMOLED-2.06
- ESP-IDF 5.5.5
- PowerShell 7 or Windows PowerShell 5.1 for the included scripts
- A verified recovery image or private backup before replacing existing firmware

## Development profile

The development profile never programs security eFuses. Wi-Fi credentials stay
in RAM and are cleared at reboot.

```powershell
.\scripts\Build.ps1 -Clean
.\scripts\Flash.ps1 -Port COM5 -RecoveryReady
.\scripts\Monitor.ps1 -Port COM5
```

Replace `COM5` with the device's port. `Flash.ps1` refuses to run without the
recovery acknowledgement.

## Published development bundle

Verify `SHA256SUMS` after extracting the release archive. With ESP-IDF's Python
environment active, flash the included images using the supplied arguments:

```powershell
python -m esptool --chip esp32s3 --port COM5 write_flash '@flash_args'
```

The release bundle contains only the development profile. Its directory layout
matches the relative paths in `flash_args`.

## Secure profile

The secure profile can automatically and permanently provision an `HMAC_UP`
key on first boot. Read `SECURE_STORAGE.md` before proceeding.

```powershell
.\scripts\Inspect-Security.ps1 -Port COM5
.\scripts\Build-Secure.ps1 -Clean
.\scripts\Flash-Secure.ps1 -Port COM5 -RecoveryReady `
  -AcknowledgePermanentHmacEfuseProvisioning
```

Do not use ordinary `idf.py flash` as a shortcut around the secure-profile
inspection and acknowledgement.

## Recovery

See `RECOVERY.md`. A flash backup can restore flash contents but cannot undo an
eFuse operation. Never publish device backups, generated configuration, serial
logs containing identifiers, or credential-bearing NVS data.
