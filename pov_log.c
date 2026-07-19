#include "pov_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifndef POV_LOG_CONSOLE
#define POV_LOG_CONSOLE 0
#endif

typedef struct {
    pov_log_entry_t entries[POV_LOG_CAPACITY];
    uint64_t boot_id;
    pov_log_clock_fn_t clock_fn;
    uint32_t next_sequence;
    uint16_t head;
    uint16_t count;
    bool initialized;
} pov_log_state_t;

static pov_log_state_t s_log;

#ifdef __cplusplus
static_assert(sizeof(pov_log_state_t) <= POV_LOG_STATE_MAX_BYTES,
              "pov_log static RAM budget exceeded");
#else
_Static_assert(sizeof(pov_log_state_t) <= POV_LOG_STATE_MAX_BYTES,
               "pov_log static RAM budget exceeded");
#endif

static uint64_t now_ms(void) {
    return s_log.clock_fn ? s_log.clock_fn() : 0u;
}

static char lower_ascii(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c + ('a' - 'A')) : c;
}

static bool contains_case_insensitive(const char *text, const char *needle) {
    if (!text || !needle || !needle[0]) return false;
    for (size_t i = 0; text[i]; ++i) {
        size_t j = 0;
        while (needle[j] && text[i + j] &&
               lower_ascii(text[i + j]) == lower_ascii(needle[j])) {
            ++j;
        }
        if (!needle[j]) return true;
    }
    return false;
}

static bool contains_sensitive_value(const char *text) {
    static const char *const patterns[] = {
        "password=", "password:", "passphrase=", "passphrase:",
        "admin_token=", "admin_token:", "authorization:",
        "x-admin-token", "bearer ",
    };
    for (size_t i = 0; i < sizeof(patterns) / sizeof(patterns[0]); ++i) {
        if (contains_case_insensitive(text, patterns[i])) return true;
    }
    return false;
}

static size_t valid_utf8_length(const unsigned char *p, size_t remaining) {
    if (remaining == 0u) return 0u;
    if (p[0] < 0x80u) return 1u;
    if (p[0] >= 0xC2u && p[0] <= 0xDFu && remaining >= 2u &&
        (p[1] & 0xC0u) == 0x80u) return 2u;
    if (p[0] == 0xE0u && remaining >= 3u && p[1] >= 0xA0u && p[1] <= 0xBFu &&
        (p[2] & 0xC0u) == 0x80u) return 3u;
    if (((p[0] >= 0xE1u && p[0] <= 0xECu) ||
         (p[0] >= 0xEEu && p[0] <= 0xEFu)) && remaining >= 3u &&
        (p[1] & 0xC0u) == 0x80u && (p[2] & 0xC0u) == 0x80u) return 3u;
    if (p[0] == 0xEDu && remaining >= 3u && p[1] >= 0x80u && p[1] <= 0x9Fu &&
        (p[2] & 0xC0u) == 0x80u) return 3u;
    if (p[0] == 0xF0u && remaining >= 4u && p[1] >= 0x90u && p[1] <= 0xBFu &&
        (p[2] & 0xC0u) == 0x80u && (p[3] & 0xC0u) == 0x80u) return 4u;
    if (p[0] >= 0xF1u && p[0] <= 0xF3u && remaining >= 4u &&
        (p[1] & 0xC0u) == 0x80u && (p[2] & 0xC0u) == 0x80u &&
        (p[3] & 0xC0u) == 0x80u) return 4u;
    if (p[0] == 0xF4u && remaining >= 4u && p[1] >= 0x80u && p[1] <= 0x8Fu &&
        (p[2] & 0xC0u) == 0x80u && (p[3] & 0xC0u) == 0x80u) return 4u;
    return 0u;
}

static size_t copy_safe_text(char *out, size_t out_capacity,
                             const char *input, size_t input_len,
                             bool truncated) {
    size_t limit = out_capacity - 1u;
    if (truncated && limit >= 3u) limit -= 3u;
    size_t in = 0u;
    size_t written = 0u;

    while (in < input_len && input[in] && written < limit) {
        const unsigned char *p = (const unsigned char *)input + in;
        size_t n = valid_utf8_length(p, input_len - in);
        if (n == 0u) {
            out[written++] = '?';
            ++in;
            continue;
        }
        if (n == 1u && (p[0] < 0x20u || p[0] == 0x7Fu)) {
            out[written++] = '?';
            ++in;
            continue;
        }
        if (written + n > limit) break;
        memcpy(out + written, input + in, n);
        written += n;
        in += n;
    }

    if (truncated) {
        memcpy(out + written, "...", 3u);
        written += 3u;
    }
    out[written] = '\0';
    return written;
}

static uint16_t oldest_index(void) {
    return (uint16_t)((s_log.head + POV_LOG_CAPACITY - s_log.count) %
                      POV_LOG_CAPACITY);
}

void pov_log_init(uint64_t boot_id, pov_log_clock_fn_t clock_fn) {
    memset(&s_log, 0, sizeof(s_log));
    s_log.boot_id = boot_id ? boot_id : 1u;
    s_log.clock_fn = clock_fn;
    s_log.next_sequence = 1u;
    s_log.initialized = true;
}

void pov_logf(pov_log_source_t source, const char *fmt, ...) {
    if (!s_log.initialized || !fmt) return;
    if ((unsigned)source >= (unsigned)POV_LOG_SOURCE_COUNT) {
        source = POV_LOG_SOURCE_SYSTEM;
    }

    if (s_log.next_sequence == 0u) {
        uint64_t next_boot = s_log.boot_id + 1u;
        pov_log_clock_fn_t clock_fn = s_log.clock_fn;
        pov_log_init(next_boot ? next_boot : 1u, clock_fn);
    }

    char raw[256];
    va_list args;
    va_start(args, fmt);
    int formatted = vsnprintf(raw, sizeof(raw), fmt, args);
    va_end(args);
    if (formatted < 0) return;
    raw[sizeof(raw) - 1u] = '\0';

    size_t available = strlen(raw);
    while (available && (raw[available - 1u] == '\n' ||
                         raw[available - 1u] == '\r')) {
        raw[--available] = '\0';
    }

    pov_log_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.uptime_ms = now_ms();
    entry.sequence = s_log.next_sequence++;
    entry.source = (uint8_t)source;

    if (contains_sensitive_value(raw)) {
        static const char redacted[] = "[redacted sensitive log message]";
        memcpy(entry.text, redacted, sizeof(redacted));
        entry.text_len = (uint8_t)(sizeof(redacted) - 1u);
        entry.flags |= POV_LOG_FLAG_SANITIZED;
    } else {
        bool truncated = available > POV_LOG_TEXT_MAX;
        if ((size_t)formatted >= sizeof(raw)) truncated = true;
        entry.text_len = (uint8_t)copy_safe_text(
            entry.text, sizeof(entry.text), raw, available, truncated);
        if (truncated) entry.flags |= POV_LOG_FLAG_TRUNCATED;
    }

    s_log.entries[s_log.head] = entry;
    s_log.head = (uint16_t)((s_log.head + 1u) % POV_LOG_CAPACITY);
    if (s_log.count < POV_LOG_CAPACITY) ++s_log.count;

#if POV_LOG_CONSOLE
    printf("[%s] %s\n", pov_log_source_text(source), entry.text);
#endif
}

pov_log_snapshot_t pov_log_snapshot(void) {
    pov_log_snapshot_t result;
    memset(&result, 0, sizeof(result));
    if (!s_log.initialized) return result;

    result.boot_id = s_log.boot_id;
    result.uptime_ms = now_ms();
    result.count = s_log.count;
    if (s_log.count) {
        const pov_log_entry_t *oldest = &s_log.entries[oldest_index()];
        uint16_t newest = (uint16_t)((s_log.head + POV_LOG_CAPACITY - 1u) %
                                     POV_LOG_CAPACITY);
        result.oldest_sequence = oldest->sequence;
        result.newest_sequence = s_log.entries[newest].sequence;
    }
    return result;
}

bool pov_log_read(uint32_t sequence, pov_log_entry_t *out) {
    if (!out || !s_log.initialized || !s_log.count) return false;
    pov_log_snapshot_t snapshot = pov_log_snapshot();
    if (sequence < snapshot.oldest_sequence || sequence > snapshot.newest_sequence) {
        return false;
    }
    uint32_t offset = sequence - snapshot.oldest_sequence;
    uint16_t index = (uint16_t)((oldest_index() + offset) % POV_LOG_CAPACITY);
    *out = s_log.entries[index];
    return out->sequence == sequence;
}

const char *pov_log_source_text(pov_log_source_t source) {
    static const char *const labels[POV_LOG_SOURCE_COUNT] = {
        "system", "driver", "clock", "health", "hall", "time",
        "wifi_conn", "wifi_http", "wifi_sta_http", "wifi_dns",
        "wifi_scan", "wifi_flash", "dhcp", "update",
    };
    return (unsigned)source < (unsigned)POV_LOG_SOURCE_COUNT
               ? labels[(unsigned)source]
               : labels[POV_LOG_SOURCE_SYSTEM];
}

size_t pov_log_static_bytes(void) {
    return sizeof(s_log);
}
