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
- provide reproducible power-test build modes for active, Wi-Fi, display-off,
  light-sleep and deep-sleep comparisons.

Every hardware phase must record at least active current, idle/display-off
current, sleep current, wake latency, duty cycle, and battery-path runtime. USB
measurements alone are insufficient because charger/PMIC behavior changes the
power path. Tests must also cover failed Wi-Fi, absent SD card, audio amp idle,
sensor polling, and repeated wake cycles.

Before release, define numeric budgets from repeatable measurements on the
actual battery. Until then, runtime claims are intentionally omitted.
