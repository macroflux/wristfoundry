# Contributing

Keep changes small, testable, and truthful about hardware validation. A feature
is not marked supported until it has a build result, a hardware test record, a
sleep/off policy, and a measured or bounded power impact.

Do not commit credentials, device dumps, factory images, serial logs containing
identifiers, generated build directories, or dependency caches. New apps should
use the framework lifecycle instead of directly owning global navigation or
radio state.

Run a clean build, review `git diff --check`, and update the hardware matrix and
power notes when changing board services.
