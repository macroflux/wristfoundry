# Persistent settings and shared app icons (0.2.0)

See SETTINGS_VALIDATION.md for CI results and the owner's local hardware report.
Release downloads are available through the repository's Releases page after
publication; a source version string alone does not establish publication.

## Settings contract

The watch Settings landing page and Companion edit the same service. It stores
one `power_v1` blob in the `preferences` NVS namespace. Values are loaded at
boot, validated, and copied under a mutex. Missing, corrupt or unknown versions
use defaults without erasing storage. Future schemas must explicitly migrate
known older records; this first schema has no older record to migrate.

| Field | Values | Default |
| --- | --- | --- |
| `dim_seconds` | Integer 5–300 | 10 |
| `off_seconds` | Integer 10–600, greater than dim | 25 |
| `usb_display` | `stay_on`, `timeout` | `stay_on` |
| `radio_policy` | `stop`, `finish_bounded` | `stop` |

Timeouts are elapsed inactivity, not successive intervals. Applying a new
revision starts a fresh interval. USB attachment wakes the display. `timeout`
allows it to dim/blank thereafter, while USB processor/recovery guards remain
active. PWR can blank either mode. Unplug returns to battery inactivity policy.

`finish_bounded` lets already-running scan/connect/NTP/BLE work continue after
display-off for at most 30 seconds. Idle connections are stopped; new radio or
navigation commands require an awake display. No permanent radio lease is added.
An operation can be cancelled before completion at the deadline.

Save commits a single settings record before updating active RAM values. A
failed commit is not acknowledged as saved. Unchanged saved values skip flash
writes. Expected revisions reject conflicting watch/desktop edits; reload and
review before retrying. Companion preserves dirty drafts during refresh; leaving
the settings page discards its unsaved draft. Restore defaults saves only this
preferences record, not network credentials or other namespaces.

Development firmware now initializes ordinary NVS for these nonsecret values;
Wi-Fi credentials still remain RAM-only. Secure-profile preference writes reuse
the already configured encrypted NVS; no additional key block is requested.
Existing first-boot secure-profile provisioning behavior is unchanged. A failed
NVS init leaves defaults available and editing unavailable; never auto-erase to
recover it. The diagnostic credential `persistence` field still refers to Wi-Fi,
not these independently persistent preferences.

## Protocol additions

`settings.get` returns `schema_version`, `revision`, `storage_ready`, `saved`,
the four fields above, and `radio_grace_seconds`. `settings.set` takes all four
fields plus `expected_revision`. `settings.reset` takes `expected_revision`.
Both return acknowledged fresh settings. Failure codes include
`settings_conflict`, `invalid_argument`, and `storage_unavailable`.
`device.status` / `device.get` include `settings_revision` so the companion can
refresh changed settings. Protocol version stays 1 and capability negotiation
keeps older firmware usable. Cross-field validation is enforced by firmware,
in addition to request schema bounds.

`wf_app_t.icon_name` is optional portable metadata. Existing `icon` remains the
fallback initials/header field. `apps.list` adds:

```json
{"id":"SETTINGS","name":"SETTINGS","icon":{"type":"builtin","name":"settings"},"initials":"SE"}
```

Builtin names: `settings`, `system`, `radar`. Each renderer maps these to its
native icon. No LVGL codepoint is transmitted. Unknown types/names use initials.
This is an app-manifest foundation, not a package installer or custom-asset
loader. Existing lifecycle and square tile touch boundaries remain intact.

## Build and local acceptance

Use ESP-IDF 5.5.5 and Flutter 3.38.7 as documented in COMPANION.md. Run normal
development and secure compile checks (secure builds are not flashed), host C
tests, protocol fixtures, Flutter analysis/tests and Windows release packaging.
New host service tests mock NVS failures/reboot; they do not establish physical
flash behavior, real power loss, or mutex concurrency.

Before release, test on the physical watch:

1. Read defaults; save changed values from each UI, read from the other, reboot,
   and verify persistence. Check invalid ranges and a conflicting edit.
2. Verify Restore defaults preserves unrelated settings/network state.
3. Test USB stay-on, USB timeout, PWR blanking, reattach and unplug. A connected
   companion must keep working with the display off.
4. Test stop-immediately and bounded completion during scans/NTP/BLE/connect;
   verify shutdown by the deadline and normal battery sleep afterwards.
5. Test square launcher hit areas, names/icons, unknown icons and legacy apps.
6. Check both missing/unavailable NVS and unchanged-save behavior. Never
   provision secure hardware or erase storage as part of routine acceptance.

## Update and rollback

Preserve and verify the existing recovery backup first. Build a development
image, record its SHA-256 and exact source revision, and compare its partition
table to the installed image. Only when compatible, update the application at
the verified application offset (current repository layout: `0x10000`) using
the existing documented flash workflow. Do not erase NVS, replace the partition
table, or assume COM7 remains the selected device. Hardware flashing needs a
separate explicit go-ahead after artifacts are reviewed.

Extract the complete matching Windows runtime directory, close the old
companion and run the new EXE beside its DLLs/data. There is no automatic
firmware or desktop updater in this feature.

Rollback uses the verified prior compatible development app image and prior
Windows runtime. Old firmware ignores the preferences namespace. Do not use
an unverified app-only rollback across partition/security changes; use the
existing recovery guide instead.

## Release process

After local/physical acceptance, finalize the version and dated changelog entry,
tag the tested commit, and review the draft release. Release notes come from
that changelog section and describe compatibility, update steps and limitations.
Provide checksummed development firmware and full Windows runtime assets.
People watching Releases can follow publication without a separate blog post.
