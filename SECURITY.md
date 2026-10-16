# Security policy

Please report suspected vulnerabilities privately to the repository maintainer
once a public hosting location is established. Do not open a public issue with
credentials, device dumps, eFuse values, or exploitable details.

WristFoundry treats Wi-Fi passwords as secrets. Release builds must enable
HMAC-backed NVS encryption. SD-card logs and exports must never contain Wi-Fi
credentials, raw audio, stable BLE addresses, authentication tokens, or secret
material. Secure boot and flash encryption are separate production-hardening
decisions and are not automatically enabled by this starter.
