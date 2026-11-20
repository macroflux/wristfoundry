# WristFoundry USB protocol v1

Protocol identity is self-reported compatibility, **not cryptographic device
authentication**. A user explicitly selects and opens a local USB serial device.
No arbitrary COM port probing, wireless provisioning listener, firmware writes,
security provisioning, or package execution is part of this service.

## Framing and ownership

Every request/reply/event is `RS + "WF1 " + JSON + LF` (RS byte `0x1e`). JSON is
UTF-8, compact, single-line, and strings escape control characters. Request
limit: 1024 bytes including prefix, excluding LF. Reply limit: 8192 bytes.
Firmware consumes input incrementally, discards oversized lines through LF,
and wipes command buffers and parsed strings after processing. Desktop rejects
malformed frames and resynchronizes at RS. It never renders or exports raw
console output. Timeouts remain failures, not fabricated success.

`companion_service` replaces the legacy provisioning task and is the only USB
input owner. Firmware logs use a bounded serialized TX path, replace control
characters, and start with `# `; they cannot inject protocol records. Protocol
TX and redirected ESP logging share a mutex. Panic/ROM output may still truncate
a frame; desktop discards it and requires a new handshake after reconnect.

The desktop permits at most four outstanding requests, uses monotonically
increasing string IDs per connection, and times out after five seconds. It does
not retry writes automatically. Firmware handles one command at a time; its
2048-byte driver RX ring is bounded. Host flooding may lose bytes; recovery is
disconnect/re-identify. Events carry a monotonically increasing `sequence` per
boot. A new session resets the desktop sequence gate. Responses for unknown or
expired IDs are ignored. Pending operations fail on unplug/cancel.

Begin with `hello`; require `v=1`, `product=WristFoundry`, a stable device ID,
and supported capabilities. Fetch `device.get` before opening a dashboard.
Native USB VID/PID `303a:1001` is only a candidate hint. It is not proof of watch
identity. Reconnect requires matching selected USB identity **and** handshake
device ID; never silently select another watch. Heartbeats refresh device state
every two seconds. After 15 seconds without a successful request, firmware ends
the session and releases the companion's manual Wi-Fi lease. Pending mutations
are never replayed on reconnect.

## Envelope

```json
{"v":1,"id":"42","cmd":"time.set","params":{"epoch_utc":1795185000}}
```

Replies contain exactly one of `result` or `error` plus matching `id` and `v`.
Asynchronous `device.status` events contain `data` instead of an ID. See
`v1/*.schema.json` and `v1/fixtures.json`. Limits on strings in firmware are
UTF-8 **bytes**, which can be stricter than schema Unicode character limits.

| Command / capability | Parameters | Meaning of acknowledgement |
| --- | --- | --- |
| `hello` | `{}` | Identity, model, firmware, capability list, package support |
| `device.get` | `{}` | Fresh health, battery/charging, RTC, Wi-Fi, sync and scan snapshot |
| `wifi.scan` | `{}` | Scan accepted; await changed scan generation/results in snapshots |
| `wifi.configure` | `ssid`, `password` | Shared service accepted RAM/encrypted-NVS config; not connected yet |
| `wifi.connect` | `{}` | Manual network lease requested; await ONLINE/error state |
| `wifi.disconnect` | `{}` | Releases manual lease; other service leases may remain |
| `time.set` | integer `epoch_utc` | UTC set after successful local RTC write; 2024–2099 |
| `timezone.set` | named catalog POSIX `timezone` | Updates timezone independently of credentials |
| `ntp.sync` | `{}` | Queued to existing sync owner; await SUCCESS/FAILED and detail |
| `ntp.cancel` | `{}` | Cancel requested through existing sync service |
| `settings.get` | `{}` | Existing compile-time dim/off policy; setter flags false |
| `apps.list` | `{}` | Snapshot of statically registered `wf_app_t` descriptors |
| `apps.open` | `app_id` | UI owner opened registered app, or returned timeout/error |
| `apps.home` | `{}` | UI owner returned watch home |
| `diagnostics.get` | `{}` | Same allowlisted health snapshot; no raw secret-bearing logs |

Radio/sync commands acknowledge **acceptance**, never final success. Always
display refreshed device state. Power policy may cancel radio work. Unknown
battery/charge values are JSON null. `rtc_local` is empty when unavailable.
Desktop treats unsupported capabilities as unavailable, not placeholder success.

## Errors

`malformed`, `frame_too_large`, `invalid_argument`, `unsupported_version`,
`handshake_required`, `unsupported_command`, `busy`, `not_configured`,
`storage_unavailable`, `rtc_write_failed`, `rtc_or_storage_failed`, and
`ui_unavailable`, and `display_asleep` are firmware codes. Parser errors with no valid ID use an empty
ID and do not complete any pending host operation. The host additionally reports
`timeout`, `write_failed`, `disconnected`, `port_in_use`, `access_denied`,
`wrong_device`, `identity_changed`, `invalid_snapshot`, and `incompatible_firmware`.
Only fixed bounded codes, never echoed commands or passwords, enter diagnostics.

## Compatibility and future extensions

Breaking envelope/semantics changes increment `v`; peers reject unsupported
versions. Additive commands must be advertised as capabilities. Unknown commands
return `unsupported_command`. Discovery is not authorization for writes.

`app_packages=false` is explicit. Reserve future `apps.packages.list`,
`apps.packages.install`, `apps.packages.remove` capability names, but do not
advertise them until a reviewed package format, manifest/version negotiation,
signatures, bounded transfer, storage quota, execution isolation, rollback and
lifecycle ownership exist. Native compiled apps are firmware, not packages.
Firmware update support similarly needs partition/security-aware review and is
not implemented here. No automatic flash path or eFuse API is present.
