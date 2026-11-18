# Extraction provenance

The public baseline was allowlist-extracted from a privately validated firmware
milestone and then de-branded and redesigned for public use. No private Git
history is imported.

Excluded material includes factory readbacks, backup images, all build output,
managed dependency caches, generated device configuration, private BLE
service/client, private product placeholders, and internal validation records.

The public tree must maintain an independent Git history. Never add the private
repository as a public remote or bulk-copy its root.
