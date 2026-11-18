# Secure credential storage — irreversible-operation warning

> **Warning:** flashing and booting this profile can permanently program an
> ESP32-S3 eFuse key block. Erasing or restoring flash cannot reverse that
> operation. The automatic first-boot behavior is intentional in WristFoundry
> 0.1.0. Do not use this profile until you have inspected the target device and
> accepted the permanent change.

The normal development configuration does not enable NVS encryption. It neither
loads nor saves persistent Wi-Fi credentials. Settings can use credentials in
RAM until reboot, preventing an accidental setup that silently uses plaintext
passwords left by other firmware.

The supplied `sdkconfig.secure.defaults` enables ESP-IDF HMAC-backed NVS
encryption and selects eFuse key block 0. On first boot, ESP-IDF can permanently
burn a generated `HMAC_UP` key into an empty selected block. eFuses cannot be
undone.

## Benefit and boundary

The generated HMAC key is unique to the device, cannot be read back normally,
and derives the keys that encrypt NVS. A raw flash copy therefore does not by
itself reveal the saved password. This does not enable secure boot, encrypt the
rest of flash, or prevent replacement firmware from asking the HMAC peripheral
to derive the same NVS keys.

The key is not a general application secret automatically shared with other
services. Future services may use hardware-derived keys only with independent
domain labels and a reviewed protocol.

## Before using it

1. Read the target device’s eFuse summary with Espressif tooling.
2. Choose an unused compatible key block; edit
   `CONFIG_NVS_SEC_HMAC_EFUSE_KEY_ID` if block 0 is not appropriate.
3. Back up any data you are authorized to preserve.
4. Build with both defaults files:

   ```powershell
   idf.py -B build-secure `
     -D SDKCONFIG=build-secure/sdkconfig `
     -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.secure.defaults" build
   ```

5. Confirm the generated configuration names the block you inspected.
6. Use `scripts/Flash-Secure.ps1`, which requires explicit recovery and eFuse
   acknowledgement switches. Flashing and first boot are the point that may
   cause the irreversible change.

If the selected block already contains a compatible `HMAC_UP` key, ESP-IDF
reuses it. If it contains an incompatible purpose, initialization fails rather
than overwriting it. Changing the configured key ID can consume another block.
Do not retry after a power interruption until the eFuse state has been reread.

Encrypted NVS does not transparently convert an existing plaintext NVS
partition. Before the first secure-profile boot, preserve any authorized data
and use an explicitly reviewed NVS erase or migration procedure. This is a
destructive operation and is deliberately not part of the flash script. A
device already using the same HMAC-backed profile should retain its encrypted
NVS rather than erasing it.

This profile encrypts NVS credentials and disables persistent flash coredumps
because task stacks can contain decrypted credentials. Secure boot and full
flash encryption are separate production decisions and are not enabled by this
starter.

CI builds this profile and verifies its configuration without access to a
device. CI therefore cannot burn eFuses. Secure build artifacts are retained
only briefly and are excluded from automated public release bundles.
