# Security policy

## Supported versions

Until 1.0, security fixes are provided on the latest published 0.x release and
the `main` branch only.

## Reporting

Use GitHub private vulnerability reporting after the repository is published.
Until then, contact the maintainer through the private project channel. Do not
open a public issue containing credentials, device dumps, full eFuse summaries,
stable device identifiers, or unredacted exploit details.

WristFoundry treats Wi-Fi passwords as secrets. Public release bundles contain
the development profile, which does not persist credentials. The optional
secure profile uses HMAC-backed NVS encryption and may permanently provision an
eFuse key on first boot. SD-card logs and exports must never contain Wi-Fi
credentials, raw audio, stable BLE addresses, authentication tokens, or secret
material. Secure boot and flash encryption are separate production-hardening
decisions and are not automatically enabled by this starter.

HMAC-backed NVS protects credentials against straightforward offline flash
inspection. Without secure boot, it does not prevent replacement firmware from
using the device's HMAC peripheral to access decrypted credentials.
