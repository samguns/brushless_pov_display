#include "wifi_scan.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "pico/cyw43_arch.h"

/* ---- internal state ------------------------------------------------- */

static scan_result_t s_results[WIFI_SCAN_MAX_RESULTS];
static int           s_result_count = 0;
static bool          s_scan_active  = false;

/* ---- CYW43 scan callback -------------------------------------------- */

static int scan_callback(void *env, const cyw43_ev_scan_result_t *result) {
    (void)env;
    if (!result) {
        /* NULL result signals scan complete */
        s_scan_active = false;
        return 0;
    }

    if (result->ssid_len == 0) {
        return 0; /* skip hidden / empty SSID */
    }

    /* Build a local entry */
    scan_result_t entry;
    size_t copy_len = result->ssid_len < 32 ? result->ssid_len : 32;
    memcpy(entry.ssid, result->ssid, copy_len);
    entry.ssid[copy_len] = '\0';
    entry.rssi    = result->rssi;
    entry.secured = (result->auth_mode != 0) ? 1u : 0u;

    /* Deduplication: update existing entry if RSSI is stronger */
    for (int i = 0; i < s_result_count; i++) {
        if (strcmp(s_results[i].ssid, entry.ssid) == 0) {
            if (entry.rssi > s_results[i].rssi) {
                s_results[i].rssi    = entry.rssi;
                s_results[i].secured = entry.secured;
            }
            return 0;
        }
    }

    /* New entry — append if room */
    if (s_result_count < WIFI_SCAN_MAX_RESULTS) {
        s_results[s_result_count++] = entry;
    }

    return 0;
}

/* ---- sort helper (insertion sort — small N, simple) ----------------- */

static void sort_by_rssi(void) {
    for (int i = 1; i < s_result_count; i++) {
        scan_result_t key = s_results[i];
        int j = i - 1;
        while (j >= 0 && s_results[j].rssi < key.rssi) {
            s_results[j + 1] = s_results[j];
            j--;
        }
        s_results[j + 1] = key;
    }
}

/* ---- public API ----------------------------------------------------- */

void wifi_scan_start(void) {
    s_result_count = 0;
    s_scan_active  = true;

    cyw43_wifi_scan_options_t opts = {0};
    int err = cyw43_wifi_scan(&cyw43_state, &opts, NULL, scan_callback);
    if (err != 0) {
        printf("[wifi_scan] scan start failed: %d\n", err);
        s_scan_active = false;
        return;
    }
    printf("[wifi_scan] scan started\n");
}

bool wifi_scan_is_active(void) {
    /* Also pump the callback for completion if still active */
    if (s_scan_active && !cyw43_wifi_scan_active(&cyw43_state)) {
        s_scan_active = false;
        sort_by_rssi();
        printf("[wifi_scan] scan complete, %d networks found\n", s_result_count);
    }
    return s_scan_active;
}

int wifi_scan_get_results(scan_result_t *out, int max) {
    int n = s_result_count < max ? s_result_count : max;
    memcpy(out, s_results, n * sizeof(scan_result_t));
    return n;
}
