#include "wifi_sta_http.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/bootrom.h"
#include "lwip/tcp.h"

#include "wifi_sta_web.h"
#include "wifi_config.h"
#include "wifi_scan.h"

/* ---- configuration -------------------------------------------------- */

#define HTTP_PORT           80
/* Sized to hold the redesigned Settings page (feature 007): shared dark-theme
 * CSS + three cards + a full 20-network scan list. Single fixed static buffer
 * (no heap); builders return -1 on overflow rather than truncating. */
#define STA_PAGE_BUF_SIZE   16384
#define STA_HDR_BUF_SIZE    256
#define STA_REQ_BUF_SIZE    1024    /* request accumulation buffer         */
#define STA_TCP_POLL_INTERVAL          2u     /* lwIP coarse ticks (~1 s) */
#define STA_CLIENT_IDLE_TIMEOUT_MS     5000u  /* silent/preconnect socket */
#define STA_CLIENT_PROGRESS_TIMEOUT_MS 10000u /* stalled request/response */

/* ---- module state --------------------------------------------------- */

static struct tcp_pcb *s_listen_pcb = NULL;
static struct tcp_pcb *s_client_pcb = NULL;

/* Set by the recv callback after a confirmed POST /update; consumed by poll()
 * which flushes the response and reboots into USB MSD mode (FR-010). */
static bool s_reboot_pending = false;

/* Connected network info shown on the status page (FR-007) */
static char s_ssid[33] = {0};
static char s_ip[16]   = {0};
static char s_connectivity_state[20] = "connected";
static bool s_blink_active = false;
static uint32_t s_blink_hz = 0;

/* Shared mutating-endpoint token. Empty token means mutating endpoints are disabled. */
static char s_admin_token[WIFI_ADMIN_TOKEN_MAX_LEN + 1] = {0};

/* Deferred Wi-Fi reconfiguration (feature 006): a validated POST /config stages
 * the change here; the runtime loop flushes the reply and then applies it. */
static bool s_change_pending = false;
static char s_pending_ssid[WIFI_SSID_MAX_LEN + 1] = {0};
static char s_pending_pass[WIFI_PASS_MAX_LEN + 1] = {0};
/* Last reconfiguration outcome, shown as a banner on the status/wifi pages. */
static char s_reconfig_notice[96] = {0};

typedef struct {
    uint32_t window_start_ms;
    uint16_t invalid_count;
    uint32_t blocked_until_ms;
} auth_throttle_state_t;

static auth_throttle_state_t s_auth_throttle;

/* Buffers */
static char   s_page_buf[STA_PAGE_BUF_SIZE];
static char   s_hdr_buf[STA_HDR_BUF_SIZE];
static char   s_req_buf[STA_REQ_BUF_SIZE];
static size_t s_req_len = 0;
static uint32_t s_client_last_progress_ms = 0;

/* Async response streaming: the redesigned pages can exceed TCP_SND_BUF, so the
 * body is sent in tcp_sndbuf()-sized chunks driven by the tcp_sent callback. The
 * body points into the persistent static s_page_buf and is COPY'd per chunk. */
static const char *s_tx_body   = NULL;
static size_t      s_tx_len    = 0;
static size_t      s_tx_sent   = 0;
static bool        s_tx_active = false;

/* ---- HTTP send helpers (mirror wifi_http.c) ------------------------- */

static uint32_t now_ms(void) {
    return to_ms_since_boot(get_absolute_time());
}

static int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* URL-decode `in` into `out` (handles '+' as space and %XX escapes). */
static void url_decode(char *out, size_t out_len, const char *in) {
    size_t o = 0;
    for (size_t i = 0; in[i] != '\0' && o + 1 < out_len; i++) {
        char c = in[i];
        if (c == '+') {
            out[o++] = ' ';
        } else if (c == '%' && in[i + 1] && in[i + 2]) {
            int hi = hex_value(in[i + 1]);
            int lo = hex_value(in[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out[o++] = (char)((hi << 4) | lo);
                i += 2;
            } else {
                out[o++] = c;
            }
        } else {
            out[o++] = c;
        }
    }
    out[o] = '\0';
}

static void extract_form_field(const char *body, const char *key,
                               char *value, size_t vlen) {
    value[0] = '\0';
    size_t key_len = strlen(key);
    const char *p = body;
    while (p && *p) {
        if (strncmp(p, key, key_len) == 0 && p[key_len] == '=') {
            const char *v = p + key_len + 1;
            const char *end = strchr(v, '&');
            size_t n = end ? (size_t)(end - v) : strlen(v);
            if (n >= vlen) n = vlen - 1;
            memcpy(value, v, n);
            value[n] = '\0';
            return;
        }
        p = strchr(p, '&');
        if (p) p++;
    }
}

static bool extract_header_value(const char *req, const char *header,
                                 char *value, size_t vlen) {
    value[0] = '\0';
    const char *pos = strstr(req, header);
    if (!pos) return false;
    pos += strlen(header);
    while (*pos == ' ') pos++;
    const char *end = strstr(pos, "\r\n");
    size_t n = end ? (size_t)(end - pos) : strlen(pos);
    if (n >= vlen) n = vlen - 1;
    memcpy(value, pos, n);
    value[n] = '\0';
    return n > 0;
}

static void mark_client_progress(void) {
    s_client_last_progress_ms = now_ms();
}

static void clear_tx_state(void) {
    s_tx_active = false;
    s_tx_body   = NULL;
    s_tx_len    = 0;
    s_tx_sent   = 0;
}

static void clear_client_state(struct tcp_pcb *pcb) {
    if (!pcb || s_client_pcb == pcb) {
        s_client_pcb = NULL;
        s_req_len    = 0;
        s_req_buf[0] = '\0';
        clear_tx_state();
        s_client_last_progress_ms = 0;
    }
}

static bool client_timed_out(void) {
    if (!s_client_pcb || s_client_last_progress_ms == 0) return false;
    uint32_t elapsed = now_ms() - s_client_last_progress_ms;
    uint32_t limit = s_tx_active ? STA_CLIENT_PROGRESS_TIMEOUT_MS
                                 : STA_CLIENT_IDLE_TIMEOUT_MS;
    return elapsed > limit;
}

static void release_client(struct tcp_pcb *pcb, bool abort_client) {
    if (!pcb) return;

    tcp_arg(pcb, NULL);
    tcp_sent(pcb, NULL);
    tcp_recv(pcb, NULL);
    tcp_poll(pcb, NULL, 0);
    tcp_err(pcb, NULL);

    clear_client_state(pcb);

    if (abort_client) {
        tcp_abort(pcb);
        return;
    }

    if (tcp_close(pcb) != ERR_OK) {
        tcp_abort(pcb);
    }
}

static void tx_finish(struct tcp_pcb *pcb) {
    release_client(pcb, false);
}

/* Queue as much of the remaining body as the send buffer allows. Re-invoked by
 * the tcp_sent callback as the peer ACKs data, until the whole body is queued. */
static void tx_pump(struct tcp_pcb *pcb) {
    while (s_tx_sent < s_tx_len) {
        u16_t avail = tcp_sndbuf(pcb);
        if (avail == 0) break;  /* full: wait for ACKs (tcp_sent) */
        size_t remaining = s_tx_len - s_tx_sent;
        u16_t chunk = (remaining < (size_t)avail) ? (u16_t)remaining : avail;
        u8_t flags = TCP_WRITE_FLAG_COPY;
        if ((size_t)(s_tx_sent + chunk) < s_tx_len) flags |= TCP_WRITE_FLAG_MORE;
        err_t werr = tcp_write(pcb, s_tx_body + s_tx_sent, chunk, flags);
        if (werr != ERR_OK) break;  /* ERR_MEM: retry on next sent/poll */
        s_tx_sent += chunk;
        mark_client_progress();
    }
    tcp_output(pcb);
    if (s_tx_sent >= s_tx_len) tx_finish(pcb);
}

static err_t on_sent(void *arg, struct tcp_pcb *pcb, u16_t len) {
    (void)arg; (void)len;
    mark_client_progress();
    if (s_tx_active) tx_pump(pcb);
    return ERR_OK;
}

/* Send a full HTTP response, streaming the body in chunks (handles bodies larger
 * than TCP_SND_BUF). `body` must remain valid until the send completes (it points
 * into the persistent s_page_buf; a new page is not built until this client
 * closes). The header is small and always fits the send buffer. */
static void start_send(struct tcp_pcb *pcb, int status_code,
                       const char *status_text, const char *content_type,
                       const char *body, int body_len) {
    int hdr_len = snprintf(s_hdr_buf, sizeof(s_hdr_buf),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n",
        status_code, status_text, content_type, body_len);

    err_t herr = tcp_write(pcb, s_hdr_buf, (u16_t)hdr_len,
                           TCP_WRITE_FLAG_COPY | TCP_WRITE_FLAG_MORE);
    if (herr != ERR_OK) {
        printf("[wifi_sta_http] header write failed: %d\n", (int)herr);
        release_client(pcb, true);
        return;
    }
    mark_client_progress();

    s_tx_body   = body;
    s_tx_len    = (body_len > 0) ? (size_t)body_len : 0;
    s_tx_sent   = 0;
    s_tx_active = true;
    tcp_sent(pcb, on_sent);
    tx_pump(pcb);
}

static void send_json_response(struct tcp_pcb *pcb,
                               int status_code,
                               const char *status_text,
                               const char *body) {
    start_send(pcb, status_code, status_text, "application/json",
               body, (int)strlen(body));
}

static void send_response(struct tcp_pcb *pcb,
                          int status_code, const char *status_text,
                          const char *body, int body_len) {
    start_send(pcb, status_code, status_text, "text/html; charset=utf-8",
               body, body_len);
}

static void send_redirect(struct tcp_pcb *pcb) {
    const char *resp =
        "HTTP/1.1 302 Found\r\n"
        "Location: /\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n"
        "\r\n";
    tcp_write(pcb, resp, (u16_t)strlen(resp), TCP_WRITE_FLAG_COPY);
    tcp_output(pcb);
    release_client(pcb, false);
}

static bool auth_is_blocked(void) {
    return now_ms() < s_auth_throttle.blocked_until_ms;
}

static void auth_record_invalid_attempt(void) {
    uint32_t now = now_ms();
    if (now - s_auth_throttle.window_start_ms > WIFI_STA_AUTH_INVALID_WINDOW_MS) {
        s_auth_throttle.window_start_ms = now;
        s_auth_throttle.invalid_count = 0;
    }
    s_auth_throttle.invalid_count++;
    if (s_auth_throttle.invalid_count >= WIFI_STA_AUTH_INVALID_MAX_ATTEMPTS) {
        s_auth_throttle.blocked_until_ms = now + WIFI_STA_AUTH_BLOCK_MS;
    }
}

static void auth_record_valid_attempt(void) {
    s_auth_throttle.invalid_count = 0;
}

static bool request_is_authorized_mutation(const char *req,
                                           const char *body,
                                           char *provided_token,
                                           size_t provided_len) {
    provided_token[0] = '\0';
    if (!s_admin_token[0]) return false;

    if (!extract_header_value(req, "\r\nX-Admin-Token:", provided_token, provided_len) &&
        !extract_header_value(req, "\r\nx-admin-token:", provided_token, provided_len)) {
        extract_form_field(body, "admin_token", provided_token, provided_len);
    }

    return provided_token[0] && strcmp(provided_token, s_admin_token) == 0;
}

/* ---- TCP callbacks -------------------------------------------------- */

static void on_client_err(void *arg, err_t err) {
    (void)arg;
    /* ERR_ABRT(-13)/ERR_RST(-14)/ERR_CLSD(-15) are expected for a
     * "Connection: close" server: the browser closes after the page loads, or
     * opens extra parallel/preconnect sockets we don't keep. lwIP has already
     * freed this pcb; only flag genuinely unexpected errors. */
    if (err != ERR_ABRT && err != ERR_RST && err != ERR_CLSD) {
        printf("[wifi_sta_http] client error %d\n", (int)err);
    }
    clear_client_state(NULL);
}

static err_t on_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
    (void)arg;
    if (!p || err != ERR_OK) {
        if (p) pbuf_free(p);
        bool abort_client = (err != ERR_OK);
        release_client(pcb, abort_client);
        return abort_client ? ERR_ABRT : ERR_OK;
    }

    /* Accumulate request across multiple tcp_recv frames */
    u16_t space    = (u16_t)(sizeof(s_req_buf) - 1 - s_req_len);
    u16_t copy_len = p->tot_len < space ? p->tot_len : space;
    pbuf_copy_partial(p, s_req_buf + s_req_len, copy_len, 0);
    s_req_len += copy_len;
    s_req_buf[s_req_len] = '\0';
    pbuf_free(p);
    tcp_recved(pcb, copy_len);
    mark_client_progress();

    const char *hdr_end = strstr(s_req_buf, "\r\n\r\n");
    const char *body = hdr_end ? hdr_end + 4 : "";
    if (!hdr_end && strncmp(s_req_buf, "POST ", 5) == 0) {
        return ERR_OK;
    }

    if (strncmp(s_req_buf, "POST ", 5) == 0) {
        int content_len = 0;
        const char *cl = strstr(s_req_buf, "Content-Length: ");
        if (!cl) cl = strstr(s_req_buf, "content-length: ");
        if (cl) content_len = atoi(cl + 16);

        int received_body = hdr_end ? (int)(s_req_len - (size_t)(body - s_req_buf)) : 0;
        if (received_body < content_len) {
            return ERR_OK;
        }
    }

    if (strncmp(s_req_buf, "GET /status", 11) == 0) {
        int json_len = wifi_sta_web_build_status_json(
            s_page_buf, sizeof(s_page_buf), s_ip, s_connectivity_state,
            s_blink_active, s_blink_hz);
        if (json_len < 0) json_len = 0;
        s_page_buf[json_len] = '\0';
        send_json_response(pcb, 200, "OK", s_page_buf);
        s_req_len = 0;
    } else if (strncmp(s_req_buf, "POST /config", 12) == 0) {
        /* Credential change is unauthenticated (open), consistent with /update. */
        char ssid_raw[96] = {0};
        char pass_raw[128] = {0};
        char ssid[WIFI_SSID_MAX_LEN + 1] = {0};
        char password[WIFI_PASS_MAX_LEN + 1] = {0};

        extract_form_field(body, "ssid", ssid_raw, sizeof(ssid_raw));
        extract_form_field(body, "password", pass_raw, sizeof(pass_raw));
        url_decode(ssid, sizeof(ssid), ssid_raw);
        url_decode(password, sizeof(password), pass_raw);

        size_t slen = strlen(ssid);
        size_t plen = strlen(password);

        /* Validate before any radio action (no disconnect on bad input). */
        if (slen == 0 || slen > WIFI_SSID_MAX_LEN || plen < 8 || plen > WIFI_PASS_MAX_LEN) {
            int page_len = wifi_sta_web_build_settings_page(
                s_page_buf, sizeof(s_page_buf), s_ssid, s_ip, NULL, 0,
                (uint8_t)wifi_config_get_brightness(),
                "Invalid input: SSID must be 1-32 characters and the WPA2 "
                "password 8-63 characters.");
            send_response(pcb, 200, "OK", s_page_buf, page_len > 0 ? page_len : 0);
            s_req_len = 0;
            printf("[wifi_sta_http] reconfig rejected (ssid_len=%u pw_len=%u)\n",
                   (unsigned)slen, (unsigned)plen);
            return ERR_OK;
        }

        /* Stage the change; the runtime loop flushes this reply then switches. */
        strncpy(s_pending_ssid, ssid, sizeof(s_pending_ssid) - 1);
        s_pending_ssid[sizeof(s_pending_ssid) - 1] = '\0';
        strncpy(s_pending_pass, password, sizeof(s_pending_pass) - 1);
        s_pending_pass[sizeof(s_pending_pass) - 1] = '\0';
        s_reconfig_notice[0] = '\0';
        s_change_pending = true;

        int page_len = wifi_sta_web_build_applying_page(
            s_page_buf, sizeof(s_page_buf), s_pending_ssid);
        send_response(pcb, 200, "OK", s_page_buf, page_len > 0 ? page_len : 0);
        s_req_len = 0;
        printf("[wifi_sta_http] reconfig accepted target SSID=%s (pw_len=%u)\n",
               s_pending_ssid, (unsigned)plen);
    } else if (strncmp(s_req_buf, "POST /display", 13) == 0) {
        /* Display brightness change (feature 007). Open endpoint, consistent
         * with the other mutating portal endpoints. */
        char br_raw[8] = {0};
        extract_form_field(body, "brightness", br_raw, sizeof(br_raw));
        int br = atoi(br_raw);
        if (br < 0) br = 0;
        if (br > 100) br = 100;
        wifi_config_set_brightness((uint8_t)br);
        printf("[wifi_sta_http] brightness set to %d%%\n", br);

        int page_len = wifi_sta_web_build_settings_page(
            s_page_buf, sizeof(s_page_buf), s_ssid, s_ip, NULL, 0,
            (uint8_t)wifi_config_get_brightness(), "Brightness updated.");
        send_response(pcb, 200, "OK", s_page_buf, page_len > 0 ? page_len : 0);
        s_req_len = 0;
    } else if (strncmp(s_req_buf, "GET /wifi", 9) == 0 ||
               strncmp(s_req_buf, "GET /settings", 13) == 0) {
        bool do_scan = (strstr(s_req_buf, "scan=1") != NULL);
        const char *notice = s_reconfig_notice[0] ? s_reconfig_notice : NULL;
        scan_result_t results[WIFI_SCAN_MAX_RESULTS];
        int n_results = 0;

        if (do_scan) {
            wifi_scan_start();
            absolute_time_t deadline = make_timeout_time_ms(8000);
            while (wifi_scan_is_active() && !time_reached(deadline)) {
                cyw43_arch_poll();
                sleep_ms(20);
            }
            n_results = wifi_scan_get_results(results, WIFI_SCAN_MAX_RESULTS);
            if (n_results <= 0 && notice == NULL) {
                notice = "No networks found. Enter the SSID manually.";
            }
            printf("[wifi_sta_http] reconfig scan results=%d\n", n_results);
        }

        int page_len = wifi_sta_web_build_settings_page(
            s_page_buf, sizeof(s_page_buf), s_ssid, s_ip,
            do_scan ? results : NULL, do_scan ? n_results : 0,
            (uint8_t)wifi_config_get_brightness(), notice);
        send_response(pcb, 200, "OK", s_page_buf, page_len > 0 ? page_len : 0);
        s_req_len = 0;
    } else if (strncmp(s_req_buf, "POST /update", 12) == 0) {
        /* Firmware update confirmation is unauthenticated: any confirmed POST
         * /update reboots into USB BOOTSEL mode. */
        int page_len = wifi_sta_web_build_rebooting_page(s_page_buf, sizeof(s_page_buf));
        send_response(pcb, 200, "OK", s_page_buf, page_len > 0 ? page_len : 0);
        s_req_len = 0;
        s_reboot_pending = true;
        printf("[wifi_sta_http] firmware update confirmed\n");
    } else if (strncmp(s_req_buf, "GET /update", 11) == 0) {
        /* Confirmation page with 60 s countdown (FR-009) */
        int page_len = wifi_sta_web_build_update_page(
            s_page_buf, sizeof(s_page_buf));
        send_response(pcb, 200, "OK", s_page_buf, page_len > 0 ? page_len : 0);
        s_req_len = 0;
    } else if (strncmp(s_req_buf, "GET / ", 6) == 0 ||
               strncmp(s_req_buf, "GET /\r", 6) == 0 ||
               strncmp(s_req_buf, "GET /?", 6) == 0) {
        /* Status page (FR-006, FR-007) */
        int page_len = wifi_sta_web_build_status_page(
            s_page_buf, sizeof(s_page_buf), s_ssid, s_ip,
            s_connectivity_state, s_blink_active, s_blink_hz,
            s_reconfig_notice[0] ? s_reconfig_notice : NULL);
        send_response(pcb, 200, "OK", s_page_buf, page_len > 0 ? page_len : 0);
        s_req_len = 0;
    } else {
        /* Unknown path — redirect to / */
        send_redirect(pcb);
        s_req_len = 0;
    }

    return ERR_OK;
}

static err_t on_poll(void *arg, struct tcp_pcb *pcb) {
    (void)arg;
    if (pcb != s_client_pcb) return ERR_OK;

    if (client_timed_out()) {
        printf("[wifi_sta_http] client timeout (req_len=%u tx=%u sent=%u/%u)\n",
               (unsigned)s_req_len,
               (unsigned)s_tx_active,
               (unsigned)s_tx_sent,
               (unsigned)s_tx_len);
        release_client(pcb, true);
        return ERR_ABRT;
    }

    if (s_tx_active) tx_pump(pcb);
    return ERR_OK;
}

static err_t on_accept(void *arg, struct tcp_pcb *client_pcb, err_t err) {
    (void)arg;
    if (err != ERR_OK || !client_pcb) return ERR_VAL;

    /* Single client at a time; a second concurrent request is aborted. */
    if (s_client_pcb) {
        if (client_timed_out()) {
            printf("[wifi_sta_http] replacing stale client\n");
            release_client(s_client_pcb, true);
        } else {
            tcp_abort(client_pcb);
            return ERR_ABRT;
        }
    }

    s_req_len    = 0;
    s_req_buf[0] = '\0';
    clear_tx_state();
    mark_client_progress();

    tcp_arg(client_pcb, NULL);
    tcp_err(client_pcb, on_client_err);
    tcp_recv(client_pcb, on_recv);
    tcp_poll(client_pcb, on_poll, STA_TCP_POLL_INTERVAL);
    s_client_pcb = client_pcb;
    return ERR_OK;
}

/* ---- public API ----------------------------------------------------- */

void wifi_sta_http_start(const char *ssid, const char *ip) {
    s_reboot_pending = false;
    s_client_pcb     = NULL;
    s_req_len        = 0;
    clear_tx_state();
    s_client_last_progress_ms = 0;
    memset(&s_auth_throttle, 0, sizeof(s_auth_throttle));

    strncpy(s_ssid, ssid ? ssid : "", sizeof(s_ssid) - 1);
    s_ssid[sizeof(s_ssid) - 1] = '\0';
    strncpy(s_ip, ip ? ip : "", sizeof(s_ip) - 1);
    s_ip[sizeof(s_ip) - 1] = '\0';

    s_listen_pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
    if (!s_listen_pcb) {
        printf("[wifi_sta_http] failed to create PCB\n");
        return;
    }
    tcp_bind(s_listen_pcb, IP_ANY_TYPE, HTTP_PORT);
    s_listen_pcb = tcp_listen_with_backlog(s_listen_pcb, 1);
    tcp_accept(s_listen_pcb, on_accept);
    printf("[wifi_sta_http] management portal listening on port %d\n", HTTP_PORT);
}

void wifi_sta_http_stop(void) {
    if (s_client_pcb) release_client(s_client_pcb, true);
    if (s_listen_pcb) { tcp_close(s_listen_pcb); s_listen_pcb = NULL; }
}

void wifi_sta_http_poll(void) {
    /* Drive any in-progress chunked response from the main super-loop too, so
     * streaming makes forward progress regardless of tcp_sent callback timing.
     * (Pages can exceed TCP_SND_BUF; see start_send/tx_pump.) */
    if (s_tx_active && s_client_pcb) {
        tx_pump(s_client_pcb);
    }

    if (!s_reboot_pending) return;

    /* Allow lwIP to flush the (chunked) response and close the connection before
     * we hand control to the ROM bootloader (the browser must get its reply).
     * Wait until the streamed body is fully queued/sent, then a short grace. */
    absolute_time_t deadline = make_timeout_time_ms(3000);
    while (s_tx_active && !time_reached(deadline)) {
        cyw43_arch_poll();
        sleep_ms(5);
    }
    absolute_time_t grace = make_timeout_time_ms(400);
    while (!time_reached(grace)) {
        cyw43_arch_poll();
        sleep_ms(10);
    }

    printf("[wifi_sta_http] reset_usb_boot() — rebooting to USB MSD\n");
    sleep_ms(50);                 /* let the final log line flush over USB */
    reset_usb_boot(0, 0);         /* enter USB mass-storage (BOOTSEL) mode */
    /* does not return */
}

void wifi_sta_http_set_runtime_status(const char *connectivity_state,
                                      const char *ip,
                                      bool blink_active,
                                      uint32_t blink_hz) {
    strncpy(s_connectivity_state, connectivity_state ? connectivity_state : "unknown",
            sizeof(s_connectivity_state) - 1);
    s_connectivity_state[sizeof(s_connectivity_state) - 1] = '\0';

    strncpy(s_ip, ip ? ip : "", sizeof(s_ip) - 1);
    s_ip[sizeof(s_ip) - 1] = '\0';

    s_blink_active = blink_active;
    s_blink_hz = blink_hz;
}

void wifi_sta_http_set_admin_token(const char *token) {
    strncpy(s_admin_token, token ? token : "", sizeof(s_admin_token) - 1);
    s_admin_token[sizeof(s_admin_token) - 1] = '\0';
    printf("[wifi_sta_http] admin token configured (len=%u)\n",
           (unsigned)strlen(s_admin_token));
}

bool wifi_sta_http_change_pending(void) {
    return s_change_pending;
}

void wifi_sta_http_get_pending_change(char *ssid, size_t ssid_len,
                                      char *password, size_t password_len) {
    if (ssid && ssid_len) {
        strncpy(ssid, s_pending_ssid, ssid_len - 1);
        ssid[ssid_len - 1] = '\0';
    }
    if (password && password_len) {
        strncpy(password, s_pending_pass, password_len - 1);
        password[password_len - 1] = '\0';
    }
}

void wifi_sta_http_clear_change_pending(void) {
    s_change_pending = false;
    /* Wipe the staged password so it does not linger in RAM. */
    memset(s_pending_pass, 0, sizeof(s_pending_pass));
}

void wifi_sta_http_set_reconfig_result(bool success, const char *message) {
    (void)success;
    strncpy(s_reconfig_notice, message ? message : "", sizeof(s_reconfig_notice) - 1);
    s_reconfig_notice[sizeof(s_reconfig_notice) - 1] = '\0';
}
