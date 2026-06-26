#ifndef WIFI_STA_WEB_H
#define WIFI_STA_WEB_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * STA-mode management portal page builders (feature 002).
 *
 * Each function renders a complete HTML page into `buf` (size `buflen`) and
 * returns the number of bytes written (excluding the NUL terminator), or -1 if
 * the buffer is too small.
 */

/*
 * wifi_sta_web_build_status_page() — render the management/status page
 * (contracts/http-api.md GET /).  Shows the connected SSID and IP address and
 * an "Update firmware" link to /update.  (FR-006, FR-007, FR-008)
 */
int wifi_sta_web_build_status_page(char *buf, size_t buflen,
                                   const char *ssid,
                                   const char *ip,
                                   const char *connectivity_state,
                                   bool blink_active,
                                   uint32_t blink_hz);

int wifi_sta_web_build_status_json(char *buf, size_t buflen,
                                   const char *ip,
                                   const char *connectivity_state,
                                   bool blink_active,
                                   uint32_t blink_hz);

/*
 * wifi_sta_web_build_update_page() — render the firmware-update confirmation
 * page (contracts/http-api.md GET /update).  Contains a Confirm form posting to
 * /update, a Cancel link to /, and a client-side 60 s countdown that redirects
 * back to / when it reaches zero.  (FR-009, FR-011, FR-013)
 */
int wifi_sta_web_build_update_page(char *buf, size_t buflen);

/*
 * wifi_sta_web_build_rebooting_page() — render the brief response sent in reply
 * to a confirmed POST /update, shown just before the device reboots into USB
 * mass-storage mode.  (FR-010)
 */
int wifi_sta_web_build_rebooting_page(char *buf, size_t buflen);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_STA_WEB_H */
