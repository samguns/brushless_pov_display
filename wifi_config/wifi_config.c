#include "wifi_config.h"

#include <string.h>
#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "cyw43.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "dhcpserver.h"

#include "wifi_flash.h"
#include "wifi_http.h"
#include "wifi_dns.h"
#include "wifi_sta_http.h"

#define WIFI_LOG_CONN(fmt, ...) printf("[wifi_conn] " fmt "\n", ##__VA_ARGS__)
#define WIFI_LOG_BLINK(fmt, ...) printf("[wifi_blink] " fmt "\n", ##__VA_ARGS__)

#define AP_SSID     "pov-leds-setup"
#define AP_PASS     "12345678"

#define STA_CONNECT_TIMEOUT_MS  WIFI_STA_RECONNECT_TIMEOUT_MS
#define STA_BOOT_TIMEOUT_MS     20000u

typedef struct {
    wifi_credentials_t creds;
    bool creds_loaded;

    wifi_connectivity_state_t connectivity_state;
    bool portal_started;

    uint32_t reconnect_started_ms;
    uint32_t last_reconnect_attempt_ms;

    char active_ip[16];

    bool blink_active;
    uint32_t blink_frequency_hz;
} wifi_runtime_state_t;

static wifi_runtime_state_t s_runtime;

static uint32_t now_ms(void) {
    return to_ms_since_boot(get_absolute_time());
}

static const char *state_text(wifi_connectivity_state_t s) {
    switch (s) {
    case WIFI_CONN_CONNECTED: return "connected";
    case WIFI_CONN_DISCONNECTED: return "disconnected";
    case WIFI_CONN_RECONNECTING: return "reconnecting";
    case WIFI_CONN_AP_FALLBACK: return "ap_fallback";
    default: return "unknown";
    }
}

static bool try_sta_blocking(const wifi_credentials_t *creds,
                             uint32_t timeout_ms,
                             wifi_err_t *error_out) {
    int rc = cyw43_arch_wifi_connect_timeout_ms(
        creds->ssid,
        creds->password,
        CYW43_AUTH_WPA2_MIXED_PSK,
        timeout_ms);

    if (rc == 0) {
        *error_out = WIFI_ERR_NONE;
        return true;
    }

    int link = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
    *error_out = (link == CYW43_LINK_BADAUTH) ? WIFI_ERR_BAD_AUTH : WIFI_ERR_TIMEOUT;
    return false;
}

static void refresh_active_ip(void) {
    struct netif *sta_netif = &cyw43_state.netif[CYW43_ITF_STA];
    ip4addr_ntoa_r(netif_ip4_addr(sta_netif), s_runtime.active_ip, sizeof(s_runtime.active_ip));
}

static void run_ap(wifi_err_t initial_error, wifi_credentials_t *creds_out) {
    cyw43_arch_enable_ap_mode(AP_SSID, AP_PASS, CYW43_AUTH_WPA2_AES_PSK);

    struct netif *ap_netif = &cyw43_state.netif[CYW43_ITF_AP];
    IP4_ADDR(ip_2_ip4(&ap_netif->ip_addr), 192, 168, 4, 1);
    IP4_ADDR(ip_2_ip4(&ap_netif->gw),      192, 168, 4, 1);
    IP4_ADDR(ip_2_ip4(&ap_netif->netmask), 255, 255, 255, 0);

    ip_addr_t gw_ip, mask_ip;
    IP4_ADDR(ip_2_ip4(&gw_ip),   192, 168, 4, 1);
    IP4_ADDR(ip_2_ip4(&mask_ip), 255, 255, 255, 0);

    dhcp_server_t dhcp_server;
    dhcp_server_init(&dhcp_server, &gw_ip, &mask_ip, ap_netif);

    wifi_http_start(initial_error);
    wifi_dns_start();

    WIFI_LOG_CONN("AP provisioning active on %s", AP_SSID);
    while (!wifi_http_connect_pending()) {
        cyw43_arch_poll();
        wifi_http_poll();
        sleep_ms(5);
    }

    wifi_http_get_pending_credentials(creds_out);

    wifi_dns_stop();
    wifi_http_stop();
    dhcp_server_deinit(&dhcp_server);
    cyw43_arch_disable_ap_mode();
}

static bool ap_provisioning_until_connected(wifi_err_t initial_error) {
    wifi_err_t error = initial_error;
    wifi_credentials_t submitted;

    while (true) {
        run_ap(error, &submitted);

        cyw43_arch_enable_sta_mode();
        wifi_err_t connect_err = WIFI_ERR_NONE;
        if (try_sta_blocking(&submitted, STA_CONNECT_TIMEOUT_MS, &connect_err)) {
            if (!save_credentials(&submitted)) {
                WIFI_LOG_CONN("save_credentials failed after provisioning");
                error = WIFI_ERR_SAVE_FAILED;
                cyw43_arch_disable_sta_mode();
                wifi_http_reset_pending(error);
                continue;
            }

            s_runtime.creds = submitted;
            s_runtime.creds_loaded = true;
            refresh_active_ip();
            WIFI_LOG_CONN("provisioning complete: STA connected SSID=%s IP=%s",
                          s_runtime.creds.ssid, s_runtime.active_ip);
            return true;
        }

        error = connect_err;
        cyw43_arch_disable_sta_mode();
        wifi_http_reset_pending(error);
    }
}

void wifi_config_init(void) {
    memset(&s_runtime, 0, sizeof(s_runtime));
    s_runtime.connectivity_state = WIFI_CONN_DISCONNECTED;

    if (cyw43_arch_init()) {
        WIFI_LOG_CONN("cyw43_arch_init failed");
        while (1) tight_loop_contents();
    }

    wifi_credentials_t creds;
    wifi_err_t error = WIFI_ERR_NONE;

    if (load_credentials(&creds)) {
        cyw43_arch_enable_sta_mode();
        if (try_sta_blocking(&creds, STA_BOOT_TIMEOUT_MS, &error)) {
            s_runtime.creds = creds;
            s_runtime.creds_loaded = true;
            refresh_active_ip();
            WIFI_LOG_CONN("boot STA connected SSID=%s IP=%s",
                          s_runtime.creds.ssid, s_runtime.active_ip);
            return;
        }

        WIFI_LOG_CONN("stored credential connect failed; falling back to AP");
        cyw43_arch_disable_sta_mode();
        error = WIFI_ERR_RECOVERY;
    }

    ap_provisioning_until_connected(error);
}

bool wifi_config_sta_runtime_init(void) {
    if (!s_runtime.creds_loaded) {
        if (!load_credentials(&s_runtime.creds)) {
            WIFI_LOG_CONN("runtime init failed: credentials not available");
            return false;
        }
        s_runtime.creds_loaded = true;
    }

    refresh_active_ip();
    wifi_sta_http_set_admin_token(s_runtime.creds.admin_token);
    wifi_sta_http_start(s_runtime.creds.ssid, s_runtime.active_ip);

    s_runtime.portal_started = true;
    s_runtime.connectivity_state = WIFI_CONN_CONNECTED;
    s_runtime.reconnect_started_ms = 0;
    s_runtime.last_reconnect_attempt_ms = 0;

    WIFI_LOG_CONN("STA portal ready SSID=%s IP=%s token_len=%u",
                  s_runtime.creds.ssid,
                  s_runtime.active_ip,
                  (unsigned)strlen(s_runtime.creds.admin_token));
    return true;
}

void wifi_config_runtime_step(void) {
    cyw43_arch_poll();

    if (s_runtime.portal_started) {
        wifi_sta_http_set_runtime_status(state_text(s_runtime.connectivity_state),
                                         s_runtime.active_ip,
                                         s_runtime.blink_active,
                                         s_runtime.blink_frequency_hz);
        wifi_sta_http_poll();
    }

    int link = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
    if (link == CYW43_LINK_UP) {
        if (s_runtime.connectivity_state != WIFI_CONN_CONNECTED) {
            refresh_active_ip();
            if (!s_runtime.portal_started) {
                wifi_sta_http_set_admin_token(s_runtime.creds.admin_token);
                wifi_sta_http_start(s_runtime.creds.ssid, s_runtime.active_ip);
                s_runtime.portal_started = true;
            }
            s_runtime.connectivity_state = WIFI_CONN_CONNECTED;
            s_runtime.reconnect_started_ms = 0;
            WIFI_LOG_CONN("reconnected IP=%s", s_runtime.active_ip);
        }
        return;
    }

    if (s_runtime.connectivity_state == WIFI_CONN_CONNECTED) {
        s_runtime.connectivity_state = WIFI_CONN_DISCONNECTED;
        if (s_runtime.portal_started) {
            wifi_sta_http_stop();
            s_runtime.portal_started = false;
        }
        s_runtime.reconnect_started_ms = now_ms();
        s_runtime.last_reconnect_attempt_ms = 0;
        WIFI_LOG_CONN("link down (status=%d)", link);
    }

    if (s_runtime.connectivity_state == WIFI_CONN_DISCONNECTED ||
        s_runtime.connectivity_state == WIFI_CONN_RECONNECTING) {
        uint32_t now = now_ms();
        if (s_runtime.reconnect_started_ms == 0) {
            s_runtime.reconnect_started_ms = now;
        }

        if (now - s_runtime.reconnect_started_ms > WIFI_STA_RECONNECT_TIMEOUT_MS) {
            WIFI_LOG_CONN("reconnect timeout, entering AP fallback");
            s_runtime.connectivity_state = WIFI_CONN_AP_FALLBACK;
            cyw43_arch_disable_sta_mode();

            if (ap_provisioning_until_connected(WIFI_ERR_RECOVERY)) {
                wifi_sta_http_set_admin_token(s_runtime.creds.admin_token);
                wifi_sta_http_start(s_runtime.creds.ssid, s_runtime.active_ip);
                s_runtime.portal_started = true;
                s_runtime.connectivity_state = WIFI_CONN_CONNECTED;
                s_runtime.reconnect_started_ms = 0;
                s_runtime.last_reconnect_attempt_ms = 0;
            }
            return;
        }

        if (now - s_runtime.last_reconnect_attempt_ms >= WIFI_STA_RECONNECT_RETRY_INTERVAL_MS) {
            int rc = cyw43_wifi_join(&cyw43_state,
                                     strlen(s_runtime.creds.ssid),
                                     (const uint8_t *)s_runtime.creds.ssid,
                                     strlen(s_runtime.creds.password),
                                     (const uint8_t *)s_runtime.creds.password,
                                     CYW43_AUTH_WPA2_MIXED_PSK,
                                     NULL,
                                     0);
            s_runtime.last_reconnect_attempt_ms = now;
            s_runtime.connectivity_state = WIFI_CONN_RECONNECTING;
            WIFI_LOG_CONN("reconnect attempt rc=%d", rc);
        }
    }
}

wifi_connectivity_state_t wifi_config_get_connectivity_state(void) {
    return s_runtime.connectivity_state;
}

const char *wifi_config_get_connectivity_state_text(void) {
    return state_text(s_runtime.connectivity_state);
}

bool wifi_config_is_portal_ready(void) {
    return s_runtime.portal_started;
}

const char *wifi_config_get_active_ip(void) {
    return s_runtime.active_ip;
}

void wifi_config_set_blink_status(bool active, uint32_t frequency_hz) {
    if (s_runtime.blink_active != active) {
        WIFI_LOG_BLINK("state -> %s", active ? "active" : "inactive");
    }
    s_runtime.blink_active = active;
    s_runtime.blink_frequency_hz = frequency_hz;
}

bool wifi_config_get_blink_active(void) {
    return s_runtime.blink_active;
}

uint32_t wifi_config_get_blink_frequency_hz(void) {
    return s_runtime.blink_frequency_hz;
}
