# Recovery and flashing

Building is non-destructive. Flashing replaces firmware and may make vendor
recovery harder, so the included flash scripts require the explicit
`-RecoveryReady` switch.

Before flashing a device, obtain the vendor recovery image from an authorized
source or create and verify a private backup of your own device using Espressif
tools. Never commit that backup to this repository. Record the board revision,
flash size, MAC redaction policy, and restore command outside the public tree.

The secure NVS build adds an additional one-way risk: its first boot can burn an
HMAC key into the configured eFuse block. `-RecoveryReady` does not make eFuses
reversible. Read `SECURE_STORAGE.md` separately. Secure flashing is only exposed
through `Flash-Secure.ps1`, which also requires
`-AcknowledgePermanentHmacEfuseProvisioning` and prints a read-only eFuse summary
before writing flash.

Normal flash, after preparation:

```powershell
.\scripts\Flash.ps1 -Port COM5 -RecoveryReady
```

If the application does not boot, enter the board’s download mode according to
the manufacturer documentation and restore the known-good image. Do not publish
factory readbacks or device-specific dumps when filing an issue.
