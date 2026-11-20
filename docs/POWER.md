# Power policy and measurement

Power is an acceptance criterion for every feature, not a final cleanup step.
The original vendor-style always-on baseline drained the battery too quickly;
WristFoundry carries forward the working lessons from the private implementation.

Current policies:

- dim and turn off the AMOLED after inactivity;
- stop Wi-Fi, NTP and BLE when the display turns off;
- use explicit radio leases and idle timeouts;
- enter light sleep on battery and wake through the validated power-key path;
- defer sleep until radio resources are released;
- avoid large internal-RAM LVGL buffers that destabilize the display path;
- keep BLE scanning bounded and user initiated;
- tethered companion dispatch runs outside LVGL, publishes status every two
  seconds and releases its manual Wi-Fi lease on USB loss/session expiry;
- provide reproducible power-test build modes for active, Wi-Fi, display-off,
  light-sleep and deep-sleep comparisons.

Every hardware phase must record at least active current, idle/display-off
current, sleep current, wake latency, duty cycle, and battery-path runtime. USB
measurements alone are insufficient because charger/PMIC behavior changes the
power path. Tests must also cover failed Wi-Fi, absent SD card, audio amp idle,
sensor polling, and repeated wake cycles.

Companion USB open/reset behavior and incremental power impact are unmeasured.
No new permanent radio lease is taken. The VBUS guard and
display-off radio cancellation remain authoritative; see `COMPANION.md` for
the separate physical acceptance checklist.

The hardware-debugging follow-up blocks every manual light-sleep entry while
VBUS is present (including before enumeration), and fails awake on PMU read
errors. Normal battery sleep detaches the USB pad and wakes once a second to
check VBUS. On attachment it restores the pad for fresh enumeration and leaves
the display off. On battery-only timer ticks it returns directly to light sleep
without restoring display/touch/radios. GPIO10 still wakes the normal UI. This
periodic wake work has not been measured; preserving the previous numeric sleep
current or battery runtime is not claimed. Explicit power-test modes retain
their original uninterrupted sleep behavior. The optional automatic-sleep IDF
connection guard is requested too, but does not guard manual sleep calls.

Before release, define numeric budgets from repeatable measurements on the
actual battery. Until then, runtime claims are intentionally omitted.

Tethered usability follow-up: confirmed VBUS also suspends automatic display
dim/off and wakes the display on attachment. VBUS is sampled once a second;
unplugging starts a fresh normal battery idle interval. Explicit PWR-button
blanking still works, with radios stopped by the existing display-off owner.
PMU read failure does not force full display brightness on battery. This display
policy change is separate from the already prepared CPU/USB guard and needs its
own development-build flash and hardware check.
