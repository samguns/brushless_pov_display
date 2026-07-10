#ifndef TIME_SYNC_H
#define TIME_SYNC_H

#include <stdbool.h>
#include <stdint.h>

#include "lwip/ip_addr.h"

#ifdef __cplusplus
extern "C" {
#endif

struct udp_pcb;

enum {
    TIME_SYNC_DEFAULT_TIMEOUT_MS = 3000,
    TIME_SYNC_DEFAULT_RETRY_MS = 500,
    TIME_SYNC_DEFAULT_MAX_ATTEMPTS = 3,
};

typedef enum {
    TIME_SYNC_STATE_IDLE = 0,
    TIME_SYNC_STATE_RESOLVING,
    TIME_SYNC_STATE_WAITING_RESPONSE,
    TIME_SYNC_STATE_CALIBRATED,
    TIME_SYNC_STATE_FAILED,
} time_sync_state_t;

typedef enum {
    TIME_SYNC_ERROR_NONE = 0,
    TIME_SYNC_ERROR_DNS,
    TIME_SYNC_ERROR_TRANSPORT,
    TIME_SYNC_ERROR_TIMEOUT,
    TIME_SYNC_ERROR_RESPONSE,
} time_sync_error_t;

typedef struct {
    const char *server_name;
    ip_addr_t server_addr;
    struct udp_pcb *pcb;
    time_sync_state_t state;
    time_sync_error_t last_error;
    uint32_t request_started_ms;
    uint32_t last_attempt_ms;
    uint8_t attempt_count;
    uint8_t max_attempts;
    uint32_t timeout_ms;
    uint32_t retry_delay_ms;
    uint32_t calibrated_utc_seconds;
    uint64_t calibrated_at_us;
    bool initialized;
} time_sync_t;

void time_sync_init_defaults(time_sync_t *sync);
void time_sync_start(time_sync_t *sync, const char *server_name, uint32_t now_ms);
void time_sync_step(time_sync_t *sync, uint32_t now_ms);
bool time_sync_has_time(const time_sync_t *sync);
uint32_t time_sync_get_utc_seconds(const time_sync_t *sync);
uint64_t time_sync_get_calibrated_at_us(const time_sync_t *sync);
const char *time_sync_state_text(time_sync_state_t state);
const char *time_sync_error_text(time_sync_error_t error);

#ifdef __cplusplus
}
#endif

#endif
