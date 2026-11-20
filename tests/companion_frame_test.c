#include <assert.h>
#include <stdio.h>
#include "../main/companion_frame.h"
#include "../main/timezone_rules.h"
#include "../main/companion_policy.h"
#include "../main/usb_sleep_policy.h"

int main(void)
{
    wf_frame_t frame = {0};
    const char *message = WF_PREFIX "{\"v\":1,\"id\":\"x\",\"cmd\":\"hello\",\"params\":{}}\n";
    int completed = 0;
    for (int round = 0; round < 2; ++round) {
        for (const char *p = message; *p; ++p) {
            int state = wf_frame_push(&frame, (unsigned char)*p);
            if (state == 1) { assert(!strncmp(frame.data, WF_PREFIX, 5)); ++completed; }
        }
    }
    assert(completed == 2); /* byte fragments + coalesced frames */
    for (int i = 0; i < WF_REQUEST_MAX; ++i) assert(wf_frame_push(&frame, 'x') == 0);
    assert(wf_frame_push(&frame, '\n') == 1); /* exact bound */
    for (int i = 0; i < WF_REQUEST_MAX + 50; ++i) assert(wf_frame_push(&frame, 'x') == 0);
    assert(wf_frame_push(&frame, '\n') == -1);
    assert(wf_frame_push(&frame, 'a') == 0);
    assert(wf_frame_push(&frame, '\r') == 0);
    assert(wf_frame_push(&frame, '\n') == 1);
    assert(!strcmp(frame.data, "a")); /* oversized tail cannot be dispatched */
    assert(wf_frame_push(&frame, 0) == 0);
    assert(wf_frame_push(&frame, '\n') == -1); /* embedded NUL cannot truncate JSON */
    assert(wf_timezone_supported("EST5EDT,M3.2.0,M11.1.0"));
    assert(!wf_timezone_supported("EST5")); /* no DST-erasing fallback */
    assert(!wf_timezone_supported(NULL));
    assert(wf_command_needs_awake("wifi.scan"));
    assert(wf_command_needs_awake("wifi.connect"));
    assert(wf_command_needs_awake("ntp.sync"));
    assert(wf_command_needs_awake("apps.open"));
    assert(!wf_command_needs_awake("device.get"));
    assert(!wf_command_needs_awake("wifi.configure"));
    assert(!wf_command_needs_awake("time.set"));
    assert(wf_usb_blocks_sleep(true, true)); /* VBUS before enumeration */
    assert(wf_usb_blocks_sleep(false, false)); /* PMU failure fails awake */
    assert(wf_usb_blocks_sleep(false, true));
    assert(!wf_usb_blocks_sleep(true, false)); /* unplug preserves sleep */
    puts("companion frame/rule tests passed");
    return 0;
}
