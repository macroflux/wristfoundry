# Extraction provenance

The public baseline was allowlist-extracted from private milestone commit
`5d83e3782f0f7d7cacc1beaafb31e1fe2b7510c8` and then de-branded and redesigned
for public use. That identifier records technical provenance only; no private
history is imported.

Excluded material includes factory readbacks, backup images, all build output,
managed dependency caches, generated device configuration, the BLE phone-time
service/client, private product placeholders, and internal validation records.

The public tree must maintain an independent Git history. Never add the private
repository as a public remote or bulk-copy its root.
