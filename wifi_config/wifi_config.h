#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum SSID length (802.11 standard: 32 bytes + NUL) */
#define WIFI_SSID_MAX_LEN   32
/* Maximum WPA2 passphrase length (63 chars + NUL) */
#define WIFI_PASS_MAX_LEN   63
/* Shared admin token length (63 chars + NUL) */
#define WIFI_ADMIN_TOKEN_MAX_LEN 63

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

/* Runtime status accessors for status/reporting endpoints. */
wifi_connectivity_state_t wifi_config_get_connectivity_state(void);
const char *wifi_config_get_connectivity_state_text(void);
bool wifi_config_is_portal_ready(void);
const char *wifi_config_get_active_ip(void);

/* Blink status published by the main loop for STA status reporting. */
void wifi_config_set_blink_status(bool active, uint32_t frequency_hz);
bool wifi_config_get_blink_active(void);
uint32_t wifi_config_get_blink_frequency_hz(void);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_CONFIG_H */
