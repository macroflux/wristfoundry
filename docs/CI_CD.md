# CI/CD policy

WristFoundry treats reproducible automation as part of the firmware, beginning
with the first public commit.

## Continuous integration

Every pull request and push to `main` runs three independent gates:

1. public-safety checks reject generated output, device backups, common secret
   file types, and identifiers from the private upstream;
2. the normal development profile builds for ESP32-S3 with ESP-IDF 5.5.5 and
   publishes a short-lived workflow artifact with SHA-256 checksums; and
3. the encrypted-NVS profile builds in an isolated directory, asserts the
   required security settings, including disabled secure-profile coredumps, and
   publishes a shorter-lived artifact.

The development build also produces a CycloneDX 1.6 SBOM with Espressif's SBOM
tool pinned to version 1.5.0.

CI builds never connect to hardware and cannot burn eFuses. A successful secure
build is not authorization to flash it.

## Continuous delivery

Pushing a semantic version tag such as `v0.1.0` is an explicit delivery action.
The release workflow rebuilds the normal development profile from that tag,
packages the three flash images with licensing material and checksums, and
creates a **draft** GitHub release. A human must inspect and publish the draft.

The secure profile is deliberately excluded from public release bundles because
its first boot can provision an irreversible HMAC eFuse key. Secure images stay
as short-lived CI artifacts for controlled device validation.

Configure the GitHub `release` environment with required reviewers before the
first version tag is pushed. Branch protection should require all CI jobs on
`main` and should disallow direct pushes once the repository is hosted.

The workflows use Espressif's official container-backed CI action, pin every
third-party action to an immutable commit SHA, and select ESP-IDF 5.5.5.
Dependabot checks GitHub Actions dependencies weekly.
