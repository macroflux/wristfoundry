# 0.1.0 release checklist

## Host and repository gates

- [ ] Fresh development build succeeds from a clean checkout.
- [ ] Fresh secure-profile build succeeds from a clean checkout.
- [ ] CI safety scan passes.
- [ ] Both firmware artifacts have recorded SHA-256 checksums.
- [ ] CycloneDX SBOM is generated and reviewed.
- [ ] Dependency lock and third-party notices agree.
- [ ] No backup, build output, credential, key, dump, or device identifier is tracked.
- [ ] GitHub Actions are pinned to reviewed immutable SHAs.
- [ ] Repository name and public description receive final clearance.
- [ ] GitHub private vulnerability reporting is enabled.
- [ ] Branch protection requires all CI jobs.
- [ ] The `release` environment requires human approval.

## Final hardware gate

- [ ] Read-only eFuse summary is captured privately before testing.
- [ ] Development image boots without changing eFuse state.
- [ ] Display, touch, launcher, back/home, dim/off, and wake pass.
- [ ] Slow keyboard entry keeps the display awake.
- [ ] Secure-profile warning and KEY0 expectation are reviewed before flashing.
- [ ] Existing NVS format is identified; any required erase or migration is separately approved.
- [ ] Secure profile reuses the existing compatible KEY0.
- [ ] Credential save, reboot restore, connection, and clear pass.
- [ ] Real access-point NTP sync writes the PCF85063 RTC.
- [ ] Offline RTC continuity passes after Wi-Fi shutdown and reboot.
- [ ] Flash coredumps are absent in the secure profile.
- [ ] Serial logs contain no password, SSID, or stable device identifier.
- [ ] Display-off, light-sleep, and wake behavior regressions are checked.

## Publication gate

- [ ] Hardware results and known limitations are recorded truthfully.
- [ ] The development bundle is flashed using the documented offsets on a test device.
- [ ] Release archive contents and checksums are reviewed.
- [ ] Secure-profile binaries are absent from the public release bundle.
- [ ] `v0.1.0` tag and draft release are approved explicitly.
- [ ] Draft release is inspected before publication.
