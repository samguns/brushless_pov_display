#ifndef WIFI_STA_HTTP_H
#define WIFI_STA_HTTP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_STA_RECONNECT_TIMEOUT_MS            15000u
#define WIFI_STA_RECONNECT_RETRY_INTERVAL_MS     2000u
#define WIFI_STA_AUTH_INVALID_WINDOW_MS          10000u
#define WIFI_STA_AUTH_INVALID_MAX_ATTEMPTS       5u
#define WIFI_STA_AUTH_BLOCK_MS                   30000u

/*
 * STA-mode management HTTP server (feature 002).
 *
 * Serves the device management portal on port 80 while connected in STA mode.
 * Mirrors the raw-lwIP single-client pattern used by the AP-mode wifi_http.c.
 * The AP-mode server must be stopped before this one starts (the CYW43439 is
 * not simultaneous AP+STA), so there is no port conflict.
 *
 * Endpoints (see specs/002-sta-portal-fw-update/contracts/http-api.md):
 *   GET  /        -> status page (SSID + IP + "Update firmware")
 *   GET  /update  -> firmware-update confirmation page (60 s countdown)
 *   POST /update  -> confirm: reboot into USB mass-storage (BOOTSEL) mode
 */

/*
 * wifi_sta_http_start() — open the TCP listener on port 80.
 * `ssid` and `ip` are displayed on the status page (copied internally).
 */
void wifi_sta_http_start(const char *ssid, const char *ip);

/*
 * wifi_sta_http_stop() — close the listening socket and any open client.
 */
void wifi_sta_http_stop(void);

/*
 * wifi_sta_http_poll() — drive deferred work (flush + reboot into USB MSD after
 * a confirmed POST /update).  Call every iteration of the STA serving loop.
 */
void wifi_sta_http_poll(void);

/* Runtime status updates for GET / and GET /status payloads. */
void wifi_sta_http_set_runtime_status(const char *connectivity_state,
                                      const char *ip,
                                      bool blink_active,
                                      uint32_t blink_hz,
                                      bool clock_available,
                                      const char *clock_text,
                                      bool rotation_speed_available,
                                      uint32_t rotation_speed_rpm);

/* Configure/update the shared admin token used for mutating endpoints. */
void wifi_sta_http_set_admin_token(const char *token);

/*
 * Deferred Wi-Fi reconfiguration (feature 006).
 *
 * A valid POST /config stages a credential change and replies with an "applying"
 * page; the runtime loop flushes the response, then performs the switch so the
 * client receives feedback before the link drops.
 */

/* True when a validated credential change is staged and awaiting application. */
bool wifi_sta_http_change_pending(void);

/* Copy the staged SSID/password into the provided buffers. */
void wifi_sta_http_get_pending_change(char *ssid, size_t ssid_len,
                                      char *password, size_t password_len);

/* Clear the staged change after it has been applied. */
void wifi_sta_http_clear_change_pending(void);

/* Record the outcome of the last applied change for display as a banner. */
void wifi_sta_http_set_reconfig_result(bool success, const char *message);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_STA_HTTP_H */
