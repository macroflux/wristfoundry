#include <assert.h>
#include "../main/phone_policy.h"
int main(void)
{
    assert(!wf_phone_expired(44999, 0, false));
    assert(wf_phone_expired(45000, 0, false));
    assert(!wf_phone_expired(309999, 10000, true));
    assert(wf_phone_expired(310000, 10000, true));
    assert(WF_PHONE_DISCOVERY_MS == 60000);
    const char *polls[] = {"hello", "device.get", "settings.get", "phone.get", "apps.list", "diagnostics.get"};
    int64_t last = 10000;
    for (int64_t now = 10000; now <= 310000; now += 2000)
        for (unsigned i = 0; i < sizeof(polls) / sizeof(polls[0]); ++i)
            if (wf_phone_command_activity(polls[i])) last = now;
    assert(last == 10000);
    assert(wf_phone_expired(310000, last, true));
    assert(wf_phone_command_activity("session.activity"));
    assert(wf_phone_command_activity("settings.set"));
    assert(wf_phone_command_activity("apps.open"));
    return 0;
}
