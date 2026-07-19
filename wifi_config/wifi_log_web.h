#ifndef WIFI_LOG_WEB_H
#define WIFI_LOG_WEB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    WIFI_LOG_BATCH_MAX = 16,
    WIFI_LOG_JSON_INVALID_CURSOR = -2,
};

/* Self-contained management page for GET /logs. */
int wifi_log_web_build_page(char *buf, size_t buflen);

/* JSON body for GET /logs/updates. A missing session starts at retained data. */
int wifi_log_web_build_updates(char *buf, size_t buflen,
                               bool has_session, uint64_t session,
                               uint32_t after, uint8_t limit);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_LOG_WEB_H */
