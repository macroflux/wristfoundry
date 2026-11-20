#pragma once
#include <stdbool.h>
#include <string.h>

/* Version 1 deliberately supports these named rules, shared with the desktop
 * catalog. Unsupported named zones must not fall back to a current offset. */
static inline bool wf_timezone_supported(const char *zone)
{
    static const char *const rules[] = {
        "UTC0", "EST5EDT,M3.2.0,M11.1.0", "CST6CDT,M3.2.0,M11.1.0",
        "MST7MDT,M3.2.0,M11.1.0", "PST8PDT,M3.2.0,M11.1.0", "MST7",
        "GMT0BST,M3.5.0/1,M10.5.0", "CET-1CEST,M3.5.0,M10.5.0/3"
    };
    for (size_t i = 0; i < sizeof(rules)/sizeof(rules[0]); ++i)
        if (zone && !strcmp(zone, rules[i])) return true;
    return false;
}
