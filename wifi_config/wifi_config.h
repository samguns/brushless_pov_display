#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

#include "pov_rotation_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum SSID length (802.11 standard: 32 bytes + NUL) */
#define WIFI_SSID_MAX_LEN   32
/* Maximum WPA2 passphrase length (63 chars + NUL) */
#define WIFI_PASS_MAX_LEN   63
/* Shared admin token length (63 chars + NUL) */
#define WIFI_ADMIN_TOKEN_MAX_LEN 63
#define WIFI_RUNTIME_CLOCK_TEXT_BUF_LEN 9

/* WiFi credential pair.  Stored in flash; exchanged between modules. */
typedef struct {
    char ssid[WIFI_SSID_MAX_LEN + 1];
    char password[WIFI_PASS_MAX_LEN + 1];
    char admin_token[WIFI_ADMIN_TOKEN_MAX_LEN + 1];
} wifi_credentials_t;

typedef enum {
    WIFI_CONN_CONNECTED = 0,
    WIFI_CONN_DISCONNECTED,
    WIFI_CONN_RECONNECTING,
    WIFI_CONN_AP_FALLBACK,
} wifi_connectivity_state_t;

/*
 * wifi_config_init() — call once from main() before the application loop.
 *
 * Behaviour:
 *   1. Reads credentials from flash.
 *   2. If valid credentials found, attempts STA connection (20 s timeout).
 *      On success: returns — WiFi is now connected.
 *      On failure: falls through to AP mode with an error banner.
 *   3. If no credentials (or STA failed), starts WPA2 AP "pov-leds-setup" /
 *      "12345678", serves the configuration web page, and loops until the user
 *      submits valid credentials that successfully connect.
 *   4. On successful connection after AP provisioning: saves credentials to
 *      flash and reboots.  Does not return.
 *
 * Returns only when the device is connected in STA mode (case 2 success).
 */
void wifi_config_init(void);

/*
 * wifi_config_sta_runtime_init() — initialize STA portal runtime state.
 * Call once after wifi_config_init() returns in connected STA mode.
 */
bool wifi_config_sta_runtime_init(void);

/*
 * wifi_config_runtime_step() — non-blocking runtime step for WiFi/STA portal.
 * Call on every main-loop iteration.
 */
void wifi_config_runtime_step(void);

/*
 * wifi_config_apply_credentials() — switch to a new Wi-Fi network at runtime.
 *
 * Backs up the current working credentials, then disconnects and test-connects
 * to (ssid, password) within a bounded timeout. The existing admin_token is
 * preserved.
 *   On success: persists the new credentials, refreshes the active IP, restarts
 *               the STA portal on the new network, and returns true.
 *   On failure: reverts to the previously working credentials (reconnecting and
 *               restarting the portal) and returns false.
 *
 * Blocks for the bounded connection attempt; intended to be called from the
 * runtime step after the HTTP response has been flushed.
 */
bool wifi_config_apply_credentials(const char *ssid, const char *password);

/* Runtime status accessors for status/reporting endpoints. */
wifi_connectivity_state_t wifi_config_get_connectivity_state(void);
const char *wifi_config_get_connectivity_state_text(void);
bool wifi_config_is_portal_ready(void);
const char *wifi_config_get_active_ip(void);

/* Display-driver readiness published by the main loop for STA status reporting. */
void wifi_config_set_blink_status(bool active);
bool wifi_config_get_blink_active(void);
/* Latest operator-facing Hall rotation status for the STA Overview page.
 * available becomes true after the first valid measurement; rpm is rounded
 * to a whole number and is zero when previously measured rotation has stopped. */
void wifi_config_set_rotation_speed_status(bool available, uint32_t rpm);

/* Latest device-local clock status for the STA Overview page. The bounded text
 * is canonical HH:MM:SS from pov_clock; unavailable state is kept separately
 * so midnight cannot be confused with an uncalibrated clock. */
void wifi_config_set_clock_status(bool available, const char *clock_text);

/*
 * Display brightness (feature 007). Value is a percentage 0..100. Loaded from
 * flash at runtime init and applied to the LED panel by the main loop.
 *   wifi_config_get_brightness() — current brightness percent (0..100).
 *   wifi_config_set_brightness() — clamp to 0..100, update runtime, and persist
 *     to flash only when the value changed (write-on-change to limit wear).
 */
uint8_t wifi_config_get_brightness(void);
void wifi_config_set_brightness(uint8_t brightness_pct);

/* Persisted POV target speed and all parameters derived from it. The setter
 * accepts hundredths of rad/s and rejects values outside the supported range. */
pov_rotation_config_t wifi_config_get_rotation_config(void);
bool wifi_config_set_nominal_rad_s_x100(uint16_t rad_s_x100);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_CONFIG_H */
