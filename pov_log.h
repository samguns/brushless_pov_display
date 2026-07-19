#ifndef POV_LOG_H
#define POV_LOG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    POV_LOG_CAPACITY = 128,
    POV_LOG_TEXT_CAPACITY = 101,
    POV_LOG_TEXT_MAX = POV_LOG_TEXT_CAPACITY - 1,
    POV_LOG_ENTRY_TARGET_BYTES = 120,
    POV_LOG_STATE_MAX_BYTES = 15392,
};

typedef enum {
    POV_LOG_SOURCE_SYSTEM = 0,
    POV_LOG_SOURCE_DRIVER,
    POV_LOG_SOURCE_CLOCK,
    POV_LOG_SOURCE_HEALTH,
    POV_LOG_SOURCE_HALL,
    POV_LOG_SOURCE_TIME,
    POV_LOG_SOURCE_WIFI_CONN,
    POV_LOG_SOURCE_WIFI_HTTP,
    POV_LOG_SOURCE_WIFI_STA_HTTP,
    POV_LOG_SOURCE_WIFI_DNS,
    POV_LOG_SOURCE_WIFI_SCAN,
    POV_LOG_SOURCE_WIFI_FLASH,
    POV_LOG_SOURCE_DHCP,
    POV_LOG_SOURCE_UPDATE,
    POV_LOG_SOURCE_COUNT,
} pov_log_source_t;

enum {
    POV_LOG_FLAG_TRUNCATED = 1u << 0,
    POV_LOG_FLAG_SANITIZED = 1u << 1,
};

typedef struct {
    uint64_t uptime_ms;
    uint32_t sequence;
    uint8_t source;
    uint8_t flags;
    uint8_t text_len;
    char text[POV_LOG_TEXT_CAPACITY];
} pov_log_entry_t;

typedef struct {
    uint64_t boot_id;
    uint64_t uptime_ms;
    uint32_t oldest_sequence;
    uint32_t newest_sequence;
    uint16_t count;
} pov_log_snapshot_t;

typedef uint64_t (*pov_log_clock_fn_t)(void);

/* Reset current-boot history. A zero boot_id is normalized to one. */
void pov_log_init(uint64_t boot_id, pov_log_clock_fn_t clock_fn);

/* Main/cooperative-context only. This API is deliberately not ISR-safe. */
void pov_logf(pov_log_source_t source, const char *fmt, ...);

/* Snapshot metadata and copy one retained sequence at a time. */
pov_log_snapshot_t pov_log_snapshot(void);
bool pov_log_read(uint32_t sequence, pov_log_entry_t *out);

const char *pov_log_source_text(pov_log_source_t source);
size_t pov_log_static_bytes(void);

#ifdef __cplusplus
}
static_assert(sizeof(pov_log_entry_t) == POV_LOG_ENTRY_TARGET_BYTES,
              "pov_log_entry_t layout changed");
#else
_Static_assert(sizeof(pov_log_entry_t) == POV_LOG_ENTRY_TARGET_BYTES,
               "pov_log_entry_t layout changed");
#endif

#endif /* POV_LOG_H */
