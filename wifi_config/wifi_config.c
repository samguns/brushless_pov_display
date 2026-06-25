#include "wifi_config.h"

#include <string.h>
#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/bootrom.h"
#include "hardware/watchdog.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "dhcpserver.h"

#include "wifi_flash.h"
#include "wifi_http.h"
#include "wifi_dns.h"
#include "wifi_web.h"

/* ---- AP configuration (FR-003) -------------------------------------- */

#define AP_SSID     "pov-leds-setup"
#define AP_PASS     "12345678"
#define AP_CHANNEL  6

/* ---- STA connection timeouts (FR-004, SC-004) ----------------------- */

#define STA_CONNECT_TIMEOUT_MS  15000u  /* 15 s for user-submitted creds  */
#define STA_BOOT_TIMEOUT_MS     20000u  /* 20 s for stored boot creds     */

/* ---- AP gateway / netmask ------------------------------------------- */

#define GW_ADDR     { 192, 168, 4, 1 }
#define NETMASK     { 255, 255, 255, 0 }

/* ================================================================
 * Internal: attempt STA connection
 *
 * Disables AP mode first (CYW43439 is not simultaneous AP+STA).
 * Returns true if connected, false on timeout or auth failure.
 * On return, error_out is set to the appropriate wifi_err_t.
 * ================================================================ */
static bool try_sta(const wifi_credentials_t *creds,
                    uint32_t timeout_ms,
                    wifi_err_t *error_out) {
    printf("[wifi_config] attempting STA: %s\n", creds->ssid);

    int rc = cyw43_arch_wifi_connect_timeout_ms(
        creds->ssid,
        creds->password,
        CYW43_AUTH_WPA2_MIXED_PSK,
        timeout_ms);

    if (rc == 0) {
        printf("[wifi_config] STA connected\n");
        *error_out = WIFI_ERR_NONE;
        return true;
    }

    /* Distinguish auth failure from timeout / not-found by link status */
    int link = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
    printf("[wifi_config] STA failed rc=%d link=%d\n", rc, link);

    if (link == CYW43_LINK_BADAUTH) {
        *error_out = WIFI_ERR_BAD_AUTH;
    } else {
        *error_out = WIFI_ERR_TIMEOUT;
    }
    return false;
}

/* ================================================================
 * Internal: run the AP provisioning loop (T013).
 *
 * Starts WPA2 AP, DHCP server, HTTP server, DNS responder.
 * Loops until the user submits valid credentials via the web form.
 * Returns with creds_out filled in when wifi_http_connect_pending().
 * ================================================================ */
static void run_ap(wifi_err_t initial_error, wifi_credentials_t *creds_out) {
    printf("[wifi_config] starting AP: %s\n", AP_SSID);

    /* Bring up AP mode */
    cyw43_arch_enable_ap_mode(AP_SSID, AP_PASS, CYW43_AUTH_WPA2_AES_PSK);

    /* Configure AP netif IP / gateway / netmask.
     * Must use netif[CYW43_ITF_AP] — netif_default points to the STA
     * interface and will not have DHCP/DNS traffic routed through it. */
    struct netif *ap_netif = &cyw43_state.netif[CYW43_ITF_AP];
    IP4_ADDR(ip_2_ip4(&ap_netif->ip_addr), 192, 168, 4, 1);
    IP4_ADDR(ip_2_ip4(&ap_netif->gw),      192, 168, 4, 1);
    IP4_ADDR(ip_2_ip4(&ap_netif->netmask), 255, 255, 255, 0);

    /* Provide ip_addr_t versions for the DHCP server */
    ip_addr_t gw_ip, mask_ip;
    IP4_ADDR(ip_2_ip4(&gw_ip),   192, 168, 4,   1);
    IP4_ADDR(ip_2_ip4(&mask_ip), 255, 255, 255, 0);

    /* Start DHCP server */
    dhcp_server_t dhcp_server;
    dhcp_server_init(&dhcp_server, &gw_ip, &mask_ip, ap_netif);

    /* Start HTTP and DNS */
    wifi_http_start(initial_error);
    wifi_dns_start();

    printf("[wifi_config] AP ready — connect to %s (pw: %s)\n",
           AP_SSID, AP_PASS);

    /* Poll loop — exits once user submits a valid form (T013) */
    while (!wifi_http_connect_pending()) {
        cyw43_arch_poll();
        wifi_http_poll();
        sleep_ms(5);
    }

    /* Retrieve submitted credentials */
    wifi_http_get_pending_credentials(creds_out);

    /* Tear down AP services */
    wifi_dns_stop();
    wifi_http_stop();
    dhcp_server_deinit(&dhcp_server);
    cyw43_arch_disable_ap_mode();

    printf("[wifi_config] AP stopped, attempting STA for: %s\n",
           creds_out->ssid);
}

/* ================================================================
 * wifi_config_init() — public entry point (T015, T016).
 *
 * Phase 1: try stored credentials in STA mode (T016).
 * Phase 2: AP provisioning loop until a valid connection is made,
 *          then save credentials and reboot (T014).
 * ================================================================ */
void wifi_config_init(void) {
    /* Initialise CYW43 driver + lwIP */
    if (cyw43_arch_init()) {
        printf("[wifi_config] cyw43_arch_init failed — halting\n");
        while (1) tight_loop_contents();
    }

    wifi_credentials_t creds;
    wifi_err_t error = WIFI_ERR_NONE;

    /* --- T016: Attempt STA with stored credentials ------------------- */
    if (load_credentials(&creds)) {
        printf("[wifi_config] found stored credentials for: %s\n", creds.ssid);

        cyw43_arch_enable_sta_mode();

        if (try_sta(&creds, STA_BOOT_TIMEOUT_MS, &error)) {
            /* Connected — return to caller (app continues normally) */
            printf("[wifi_config] connected via stored credentials\n");
            return;
        }

        /* STA failed — fall into AP mode with recovery banner */
        printf("[wifi_config] stored credential STA failed (%d)\n",
               (int)error);
        error = WIFI_ERR_RECOVERY;
        cyw43_arch_disable_sta_mode();
    } else {
        printf("[wifi_config] no stored credentials\n");
    }

    /* --- T013–T015: AP provisioning loop ----------------------------- */
    while (true) {
        run_ap(error, &creds);

        /* Re-enable STA mode after AP teardown */
        cyw43_arch_enable_sta_mode();

        wifi_err_t connect_err = WIFI_ERR_NONE;
        if (try_sta(&creds, STA_CONNECT_TIMEOUT_MS, &connect_err)) {
            /* --- T014: Save credentials and reboot (FR-008) ----------- */
            printf("[wifi_config] STA connected, saving credentials\n");
            if (save_credentials(&creds)) {
                printf("[wifi_config] credentials saved, rebooting\n");
                sleep_ms(200); /* allow final log to flush */
                watchdog_reboot(0, 0, 0);
                while (1) tight_loop_contents();
            } else {
                /* Flash write failed (FR-014) — stay in AP, tell user */
                printf("[wifi_config] save_credentials FAILED\n");
                error = WIFI_ERR_SAVE_FAILED;
            }
        } else {
            /* Connection failed — restart AP with error banner */
            error = connect_err;
        }

        /* Disable STA before restarting AP */
        cyw43_arch_disable_sta_mode();

        /* Re-arm HTTP server for next attempt (T010, FR-010) */
        wifi_http_reset_pending(error);
    }
}
