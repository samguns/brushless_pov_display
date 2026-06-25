#ifndef WIFI_SCAN_H
#define WIFI_SCAN_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_SCAN_MAX_RESULTS   20

typedef struct {
    char     ssid[33];      /* null-terminated network name */
    int16_t  rssi;          /* signal strength in dBm (negative; closer to 0 = stronger) */
    uint8_t  secured;       /* 1 = WPA/WPA2/WPA3, 0 = open */
} scan_result_t;

/* Start an async WiFi scan.  Non-blocking; check wifi_scan_is_active(). */
void wifi_scan_start(void);

/* Returns true while a scan is still running. */
bool wifi_scan_is_active(void);

/*
 * Copy up to `max` deduplicated, RSSI-sorted scan results into `out`.
 * Returns the number of results written.
 * Safe to call only after wifi_scan_is_active() returns false.
 */
int wifi_scan_get_results(scan_result_t *out, int max);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_SCAN_H */
