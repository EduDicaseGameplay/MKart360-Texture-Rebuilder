#ifndef MK64_X360_LOG_PRIVACY_H
#define MK64_X360_LOG_PRIVACY_H

/* MK64_R41_PRIVATE_LOGGING
 * Scrub dotted IPv4 addresses from human-readable diagnostic text before
 * it reaches disk or OutputDebugString. Networking still uses the real
 * numeric endpoint internally.
 */
#include <stddef.h>

static int mk64_log_digit(char c) {
    return c >= '0' && c <= '9';
}

static int mk64_log_ipv4_at(const char *s, size_t *used) {
    const char *p = s;
    int part;
    if (!s || !mk64_log_digit(*p)) return 0;
    for (part = 0; part < 4; ++part) {
        unsigned value = 0;
        int digits = 0;
        if (!mk64_log_digit(*p)) return 0;
        while (mk64_log_digit(*p)) {
            if (digits >= 3) return 0;
            value = value * 10u + (unsigned)(*p - '0');
            ++digits;
            ++p;
        }
        if (value > 255u) return 0;
        if (part != 3) {
            if (*p != '.') return 0;
            ++p;
        }
    }
    if (*p == '.' || mk64_log_digit(*p)) return 0;
    if (used) *used = (size_t)(p - s);
    return 1;
}

static void mk64_log_scrub_ipv4(const char *src, char *dst, size_t cap) {
    size_t i = 0, o = 0;
    static const char tag[] = "[IP]";
    if (!dst || cap == 0) return;
    if (!src) { dst[0] = 0; return; }
    while (src[i] && o + 1 < cap) {
        size_t used = 0;
        int boundary = (i == 0) ||
            (!mk64_log_digit(src[i - 1]) && src[i - 1] != '.');
        if (boundary && mk64_log_digit(src[i]) &&
            mk64_log_ipv4_at(src + i, &used)) {
            size_t k;
            for (k = 0; tag[k] && o + 1 < cap; ++k) dst[o++] = tag[k];
            i += used;
        } else {
            dst[o++] = src[i++];
        }
    }
    dst[o] = 0;
}

#endif
