#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum SSID length (802.11 standard: 32 bytes + NUL) */
#define WIFI_SSID_MAX_LEN   32
/* Maximum WPA2 passphrase length (63 chars + NUL) */
#define WIFI_PASS_MAX_LEN   63

/* WiFi credential pair.  Stored in flash; exchanged between modules. */
typedef struct {
    char ssid[WIFI_SSID_MAX_LEN + 1];
    char password[WIFI_PASS_MAX_LEN + 1];
} wifi_credentials_t;

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

#ifdef __cplusplus
}
#endif

#endif /* WIFI_CONFIG_H */
