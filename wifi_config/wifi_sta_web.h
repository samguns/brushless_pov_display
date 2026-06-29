#ifndef WIFI_STA_WEB_H
#define WIFI_STA_WEB_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "wifi_scan.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * STA-mode management portal page builders.
 *
 * Feature 007 re-skins these pages to the "POV Display" dark dashboard design
 * (Figma pov-mgmt: Overview frame 1:2, settings-screen frame 9:4). Each function
 * renders a complete, self-contained HTML page (shared dark/light CSS, inline
 * SVG icons, minimal inline JS, no external assets) into `buf` (size `buflen`)
 * and returns the number of bytes written (excluding the NUL terminator), or -1
 * if the buffer is too small.
 */

/* Firmware version string shown read-only in the Settings > System card. */
#ifndef WIFI_STA_FW_VERSION
#define WIFI_STA_FW_VERSION "v0.1"
#endif

/*
 * wifi_sta_web_build_status_page() — render the Overview screen (GET /).
 * Sidebar + metric cards for Status, Network (SSID), IP Address, and blink
 * state/frequency, plus an optional notice banner. Values are as of page load.
 */
int wifi_sta_web_build_status_page(char *buf, size_t buflen,
                                   const char *ssid,
                                   const char *ip,
                                   const char *connectivity_state,
                                   bool blink_active,
                                   uint32_t blink_hz,
                                   const char *notice);

/*
 * wifi_sta_web_build_settings_page() — render the Settings screen
 * (GET /settings, alias GET /wifi). Display card (theme toggle + brightness),
 * System card (firmware version + Update Firmware), and Network card (current
 * SSID + scan + password + Connect, with read-only Static IP/IP/Subnet/Gateway).
 * When `results`/`n_results` are provided, renders the selectable scan list.
 * `brightness` is the current display brightness (0..100) for the slider.
 * `notice`, if non-NULL/non-empty, renders a banner.
 */
int wifi_sta_web_build_settings_page(char *buf, size_t buflen,
                                     const char *current_ssid,
                                     const char *ip,
                                     const scan_result_t *results,
                                     int n_results,
                                     uint8_t brightness,
                                     const char *notice);

/*
 * wifi_sta_web_build_applying_page() — immediate reply to a valid POST /config:
 * the device is switching to `target_ssid`; the client should reconnect to that
 * network (or the previous one on failure) to confirm.
 */
int wifi_sta_web_build_applying_page(char *buf, size_t buflen,
                                     const char *target_ssid);

int wifi_sta_web_build_status_json(char *buf, size_t buflen,
                                   const char *ip,
                                   const char *connectivity_state,
                                   bool blink_active,
                                   uint32_t blink_hz);

/*
 * wifi_sta_web_build_update_page() — firmware-update confirmation page
 * (GET /update): Confirm (POST /update), Cancel (to /), and a 60 s countdown
 * that redirects back to / when it reaches zero.
 */
int wifi_sta_web_build_update_page(char *buf, size_t buflen);

/*
 * wifi_sta_web_build_rebooting_page() — brief reply to a confirmed POST /update,
 * shown just before the device reboots into USB mass-storage mode.
 */
int wifi_sta_web_build_rebooting_page(char *buf, size_t buflen);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_STA_WEB_H */
