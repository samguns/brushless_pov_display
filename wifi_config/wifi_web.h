#ifndef WIFI_WEB_H
#define WIFI_WEB_H

#include <stddef.h>
#include "wifi_scan.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Error banner types (FR-009, FR-014, US3).
 * Pass WIFI_ERR_NONE for a clean page.
 */
typedef enum {
    WIFI_ERR_NONE = 0,
    WIFI_ERR_BAD_AUTH,      /* wrong password */
    WIFI_ERR_TIMEOUT,       /* network not found / out of range */
    WIFI_ERR_SAVE_FAILED,   /* connected but flash write failed */
    WIFI_ERR_RECOVERY,      /* boot-time credentials failed */
} wifi_err_t;

/*
 * wifi_web_build_config_page() — render the full WiFi configuration HTML
 * page into `buf` (size `buflen`).  Embeds scan results and an optional
 * error banner (pass WIFI_ERR_NONE for none).
 *
 * Returns the number of bytes written (excluding NUL), or -1 if the buffer
 * is too small.
 */
int wifi_web_build_config_page(char *buf, size_t buflen,
                               wifi_err_t error,
                               const scan_result_t *results, int n_results);

/*
 * wifi_web_build_connecting_page() — render the "Connecting…" response page.
 * `ssid` is the network the device is about to attempt.
 *
 * Returns bytes written or -1.
 */
int wifi_web_build_connecting_page(char *buf, size_t buflen, const char *ssid);

/*
 * wifi_web_build_error_page() — render a minimal 400 Bad Request page.
 * `message` is the human-readable reason.
 *
 * Returns bytes written or -1.
 */
int wifi_web_build_error_page(char *buf, size_t buflen, const char *message);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_WEB_H */
