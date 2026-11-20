#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#define WF_REQUEST_MAX 1024
#define WF_RESPONSE_MAX 8192
#define WF_PREFIX "\036WF1 "

typedef struct {
    char data[WF_REQUEST_MAX + 1];
    size_t used;
    bool overflow;
} wf_frame_t;

/* Return 1 for a complete frame, -1 for discarded oversize, 0 otherwise.
 * The caller consumes data synchronously and wipes it after a return of 1.
 * Dropping until newline prevents an oversized tail becoming a command. */
static inline int wf_frame_push(wf_frame_t *f, unsigned char byte)
{
    if (byte == '\n') {
        int result = f->overflow ? -1 : 1;
        f->data[f->used] = 0;
        f->used = 0;
        f->overflow = false;
        return result;
    }
    if (f->overflow) return 0;
    if (f->used == WF_REQUEST_MAX) { f->overflow = true; return 0; }
    if (byte != '\r') f->data[f->used++] = (char)byte;
    return 0;
}
