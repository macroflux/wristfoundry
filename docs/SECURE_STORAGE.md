# Secure credential storage

The normal development configuration does not enable NVS encryption and the
Settings app therefore refuses to save Wi-Fi credentials. This prevents an
accidental “working” setup that silently writes passwords in plaintext.

The supplied `sdkconfig.secure.defaults` enables ESP-IDF HMAC-backed NVS
encryption and selects eFuse key block 0. On first boot, ESP-IDF can permanently
burn a generated `HMAC_UP` key into an empty selected block. eFuses cannot be
undone.

Before using it:

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

5. Review the generated configuration before flashing. Flashing/first boot is
   the point that may cause the irreversible eFuse change.

This profile encrypts NVS credentials. Secure boot and full flash encryption are
separate production decisions. Do not enable or burn either automatically on a
development watch.

CI builds this profile and verifies its configuration without access to a
device. CI therefore cannot burn eFuses. Secure build artifacts are retained
only briefly and are excluded from automated public release bundles.
