# Contributing

Keep changes small, testable, and truthful about hardware validation. A feature
is not marked supported until it has a build result, a hardware test record, a
sleep/off policy, and a measured or bounded power impact.

Do not commit credentials, device dumps, factory images, serial logs containing
identifiers, generated build directories, or dependency caches. New apps should
use the framework lifecycle instead of directly owning global navigation or
radio state.

Before submitting a change:

- run clean development and secure-profile builds
- run `git diff --check`
- confirm that the development profile does not enable encrypted NVS
- confirm that the secure profile enables HMAC-backed encrypted NVS and disables
  flash core dumps
- update the hardware matrix and power notes when changing board services

Hardware-dependent changes must identify what was tested on the watch and what
remains unverified. Never bypass the secure-flash acknowledgement script.
