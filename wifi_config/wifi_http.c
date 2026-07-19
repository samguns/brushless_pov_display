#include "wifi_http.h"

#include <string.h>
#include <stdio.h>

#include "pov_log.h"

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"

#include "wifi_scan.h"
#include "wifi_web.h"
#include "wifi_flash.h"

/* ---- configuration -------------------------------------------------- */

#define HTTP_PORT           80
#define HTTP_BODY_BUF_SIZE  7168    /* 7 KB — config page with 20 networks */
#define HTTP_HDR_BUF_SIZE   256
#define HTTP_REQ_BUF_SIZE   1024    /* accumulation buffer — covers headers+body */

/* ---- module state --------------------------------------------------- */

static struct tcp_pcb *s_listen_pcb   = NULL;
static struct tcp_pcb *s_client_pcb   = NULL;
static wifi_err_t      s_current_err  = WIFI_ERR_NONE;

/* Set by recv callback; consumed by poll() */
static bool            s_get_pending  = false;
static bool            s_post_pending = false;

/* Set by poll() after sending the Connecting page */
static bool            s_connect_pending = false;

/* Submitted credentials (from POST /connect) */
static wifi_credentials_t s_pending_creds;

/* Body buffers */
static char s_page_buf[HTTP_BODY_BUF_SIZE];
static char s_hdr_buf[HTTP_HDR_BUF_SIZE];

/* Raw accumulated HTTP request — grows across multiple tcp_recv calls */
static char   s_req_buf[HTTP_REQ_BUF_SIZE];
static size_t s_req_len = 0;

/* Parsed POST body (URL-encoded) — extracted from s_req_buf */
static char s_post_body[HTTP_REQ_BUF_SIZE];

/* ---- URL-decode helper ---------------------------------------------- */

static char hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

static void url_decode(const char *src, char *dst, size_t max) {
    size_t i = 0;
    while (*src && i + 1 < max) {
        if (*src == '%' && src[1] && src[2]) {
            dst[i++] = (char)((hex_digit(src[1]) << 4) | hex_digit(src[2]));
            src += 3;
        } else if (*src == '+') {
            dst[i++] = ' ';
            src++;
        } else {
            dst[i++] = *src++;
        }
    }
    dst[i] = '\0';
}

/* Extract value for key from URL-encoded body.
 * Writes decoded value into `value` up to `vlen` bytes. */
static void extract_field(const char *body, const char *key,
                           char *value, size_t vlen) {
    value[0] = '\0';
    size_t klen = strlen(key);
    const char *p = body;
    while (*p) {
        if (strncmp(p, key, klen) == 0 && p[klen] == '=') {
            const char *v = p + klen + 1;
            const char *end = strchr(v, '&');
            char raw[256] = {0};
            size_t raw_len = end ? (size_t)(end - v) : strlen(v);
            if (raw_len >= sizeof(raw)) raw_len = sizeof(raw) - 1;
            memcpy(raw, v, raw_len);
            raw[raw_len] = '\0';
            url_decode(raw, value, vlen);
            return;
        }
        p = strchr(p, '&');
        if (!p) break;
        p++;
    }
}

/* ---- HTTP send helpers ---------------------------------------------- */

static void send_response(struct tcp_pcb *pcb,
                           int status_code, const char *status_text,
                           const char *body, int body_len) {
    int hdr_len = snprintf(s_hdr_buf, sizeof(s_hdr_buf),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n",
        status_code, status_text, body_len);

    tcp_write(pcb, s_hdr_buf, (u16_t)hdr_len, TCP_WRITE_FLAG_COPY);
    if (body && body_len > 0) {
        tcp_write(pcb, body, (u16_t)body_len, TCP_WRITE_FLAG_COPY);
    }
    tcp_output(pcb);
    tcp_close(pcb);
    if (s_client_pcb == pcb) s_client_pcb = NULL;
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
    tcp_close(pcb);
    if (s_client_pcb == pcb) s_client_pcb = NULL;
}

/* ---- TCP callbacks -------------------------------------------------- */

static void on_client_err(void *arg, err_t err) {
    (void)arg; (void)err;
    pov_logf(POV_LOG_SOURCE_WIFI_HTTP, "client error %d\n", (int)err);
    s_client_pcb  = NULL;
    s_get_pending  = false;
    s_post_pending = false;
}

static err_t on_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
    (void)arg;
    if (!p || err != ERR_OK) {
        if (p) pbuf_free(p);
        tcp_close(pcb);
        if (s_client_pcb == pcb) { s_client_pcb = NULL; s_req_len = 0; }
        return ERR_OK;
    }

    /* Accumulate into s_req_buf across multiple tcp_recv frames */
    u16_t space = (u16_t)(sizeof(s_req_buf) - 1 - s_req_len);
    u16_t copy_len = p->tot_len < space ? p->tot_len : space;
    pbuf_copy_partial(p, s_req_buf + s_req_len, copy_len, 0);
    s_req_len += copy_len;
    s_req_buf[s_req_len] = '\0';
    pbuf_free(p);
    tcp_recved(pcb, copy_len);

    if (strncmp(s_req_buf, "GET ", 4) == 0) {
        /* Captive-portal detection endpoints → redirect to / */
        if (strstr(s_req_buf, "generate_204") || strstr(s_req_buf, "hotspot-detect") ||
            strstr(s_req_buf, "connectivitycheck") || strstr(s_req_buf, "ncsi.txt")) {
            send_redirect(pcb);
        } else if (strncmp(s_req_buf + 4, "/scan ", 6) == 0 ||
                   strncmp(s_req_buf + 4, "/scan\r", 6) == 0) {
            /* GET /scan — return current scan results as JSON (T022) */
            s_client_pcb   = pcb;
            /* Run a fresh scan */
            wifi_scan_start();
            absolute_time_t dl = make_timeout_time_ms(10000);
            while (wifi_scan_is_active() && !time_reached(dl)) {
                cyw43_arch_poll();
                sleep_ms(50);
            }
            scan_result_t jr[WIFI_SCAN_MAX_RESULTS];
            int jn = wifi_scan_get_results(jr, WIFI_SCAN_MAX_RESULTS);

            static char json[2048];
            int jpos = 0;
            jpos += snprintf(json + jpos, sizeof(json) - jpos, "[");
            for (int i = 0; i < jn && jpos < (int)sizeof(json) - 64; i++) {
                jpos += snprintf(json + jpos, sizeof(json) - jpos,
                    "%s{\"ssid\":\"%s\",\"rssi\":%d,\"secured\":%s}",
                    i ? "," : "",
                    jr[i].ssid, jr[i].rssi,
                    jr[i].secured ? "true" : "false");
            }
            jpos += snprintf(json + jpos, sizeof(json) - jpos, "]");

            char jhdr[192];
            int jhlen = snprintf(jhdr, sizeof(jhdr),
                "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                "Content-Length: %d\r\nConnection: close\r\n\r\n", jpos);
            tcp_write(pcb, jhdr, (u16_t)jhlen, TCP_WRITE_FLAG_COPY);
            tcp_write(pcb, json, (u16_t)jpos, TCP_WRITE_FLAG_COPY);
            tcp_output(pcb);
            tcp_close(pcb);
            s_client_pcb = NULL;
            s_req_len = 0;
        } else {
            s_client_pcb  = pcb;
            s_get_pending = true;
            s_req_len = 0;
        }
    } else if (strncmp(s_req_buf, "POST /connect", 13) == 0) {
        /* Accumulate until we have the complete body.
         * Parse Content-Length header to know when done. */
        const char *hdr_end = strstr(s_req_buf, "\r\n\r\n");
        if (!hdr_end) return ERR_OK;  /* headers not fully received yet */

        const char *body = hdr_end + 4;
        int received_body = (int)(s_req_len - (size_t)(body - s_req_buf));

        /* Read Content-Length (may be absent for short bodies) */
        int content_len = 0;
        const char *cl = strstr(s_req_buf, "Content-Length: ");
        if (!cl) cl = strstr(s_req_buf, "content-length: ");
        if (cl) content_len = atoi(cl + 16);

        if (received_body < content_len) return ERR_OK; /* body incomplete */

        strncpy(s_post_body, body, sizeof(s_post_body) - 1);
        s_post_body[sizeof(s_post_body) - 1] = '\0';
        pov_logf(POV_LOG_SOURCE_WIFI_HTTP, "POST body received bytes=%d\n", received_body);
        s_client_pcb   = pcb;
        s_post_pending = true;
        s_req_len = 0;
    } else {
        send_redirect(pcb);
        s_req_len = 0;
    }

    return ERR_OK;
}

static err_t on_accept(void *arg, struct tcp_pcb *client_pcb, err_t err) {
    (void)arg;
    if (err != ERR_OK || !client_pcb) return ERR_VAL;

    /* Only handle one client at a time */
    if (s_client_pcb) {
        tcp_abort(client_pcb);
        return ERR_ABRT;
    }

    /* Reset request accumulator for the new connection */
    s_req_len = 0;
    s_req_buf[0] = '\0';

    tcp_arg(client_pcb, NULL);
    tcp_err(client_pcb, on_client_err);
    tcp_recv(client_pcb, on_recv);
    s_client_pcb = client_pcb;
    return ERR_OK;
}

/* ---- public API ----------------------------------------------------- */

void wifi_http_start(wifi_err_t initial_error) {
    s_current_err     = initial_error;
    s_connect_pending = false;
    s_get_pending     = false;
    s_post_pending    = false;
    s_client_pcb      = NULL;

    s_listen_pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
    if (!s_listen_pcb) {
        pov_logf(POV_LOG_SOURCE_WIFI_HTTP, "failed to create PCB\n");
        return;
    }
    tcp_bind(s_listen_pcb, IP_ANY_TYPE, HTTP_PORT);
    s_listen_pcb = tcp_listen_with_backlog(s_listen_pcb, 1);
    tcp_accept(s_listen_pcb, on_accept);
    pov_logf(POV_LOG_SOURCE_WIFI_HTTP, "listening on port %d\n", HTTP_PORT);
}

void wifi_http_stop(void) {
    if (s_client_pcb) { tcp_abort(s_client_pcb); s_client_pcb = NULL; }
    if (s_listen_pcb) { tcp_close(s_listen_pcb); s_listen_pcb = NULL; }
}

void wifi_http_poll(void) {
    /* --- Handle GET / (scan + render) --- */
    if (s_get_pending && s_client_pcb) {
        s_get_pending = false;

        /* Run scan (wait up to 10 s, polling lwIP) */
        wifi_scan_start();
        absolute_time_t deadline = make_timeout_time_ms(10000);
        while (wifi_scan_is_active() && !time_reached(deadline)) {
            cyw43_arch_poll();
            sleep_ms(50);
        }

        scan_result_t results[WIFI_SCAN_MAX_RESULTS];
        int n = wifi_scan_get_results(results, WIFI_SCAN_MAX_RESULTS);

        int body_len = wifi_web_build_config_page(
            s_page_buf, sizeof(s_page_buf),
            s_current_err, results, n);

        if (body_len < 0) {
            pov_logf(POV_LOG_SOURCE_WIFI_HTTP, "config page too large for buffer\n");
            body_len = 0;
        }

        if (s_client_pcb) {
            send_response(s_client_pcb, 200, "OK", s_page_buf, body_len);
        }
    }

    /* --- Handle POST /connect --- */
    if (s_post_pending && s_client_pcb && !s_connect_pending) {
        s_post_pending = false;

        /* Parse form fields */
        char ssid_sel[64]    = {0};
        char ssid_manual[64] = {0};
        char password[128]   = {0};
        char admin_token[128] = {0};

        extract_field(s_post_body, "ssid",        ssid_sel,    sizeof(ssid_sel));
        extract_field(s_post_body, "ssid_manual", ssid_manual, sizeof(ssid_manual));
        extract_field(s_post_body, "password",    password,    sizeof(password));
        extract_field(s_post_body, "admin_token", admin_token, sizeof(admin_token));

        /* Resolve effective SSID: selected list entry, else manual entry */
        const char *effective_ssid = ssid_sel[0] ? ssid_sel : ssid_manual;
        pov_logf(POV_LOG_SOURCE_WIFI_HTTP, "ssid_sel=[%s] ssid_manual=[%s] password_len=%d\n",
               ssid_sel, ssid_manual, (int)strlen(password));
        pov_logf(POV_LOG_SOURCE_WIFI_HTTP, "effective SSID=[%s]\n", effective_ssid);

        /* Server-side validation (FR-006c, FR-007, T019) */
        if (effective_ssid[0] == '\0') {
            int body_len = wifi_web_build_error_page(
                s_page_buf, sizeof(s_page_buf),
                "Network name cannot be empty.");
            if (s_client_pcb)
                send_response(s_client_pcb, 400, "Bad Request",
                              s_page_buf, body_len > 0 ? body_len : 0);
            return;
        }
        if (strlen(effective_ssid) > WIFI_SSID_MAX_LEN) {
            int body_len = wifi_web_build_error_page(
                s_page_buf, sizeof(s_page_buf),
                "Network name is too long (max 32 characters).");
            if (s_client_pcb)
                send_response(s_client_pcb, 400, "Bad Request",
                              s_page_buf, body_len > 0 ? body_len : 0);
            return;
        }
        if (strlen(password) > WIFI_PASS_MAX_LEN) {
            int body_len = wifi_web_build_error_page(
                s_page_buf, sizeof(s_page_buf),
                "Password is too long (max 63 characters).");
            if (s_client_pcb)
                send_response(s_client_pcb, 400, "Bad Request",
                              s_page_buf, body_len > 0 ? body_len : 0);
            return;
        }
        if (strlen(admin_token) > WIFI_ADMIN_TOKEN_MAX_LEN) {
            int body_len = wifi_web_build_error_page(
                s_page_buf, sizeof(s_page_buf),
                "Admin token is too long (max 63 characters).");
            if (s_client_pcb)
                send_response(s_client_pcb, 400, "Bad Request",
                              s_page_buf, body_len > 0 ? body_len : 0);
            return;
        }

        /* Store credentials and send the Connecting page */
        strncpy(s_pending_creds.ssid, effective_ssid, WIFI_SSID_MAX_LEN);
        s_pending_creds.ssid[WIFI_SSID_MAX_LEN] = '\0';
        strncpy(s_pending_creds.password, password, WIFI_PASS_MAX_LEN);
        s_pending_creds.password[WIFI_PASS_MAX_LEN] = '\0';
        strncpy(s_pending_creds.admin_token, admin_token, WIFI_ADMIN_TOKEN_MAX_LEN);
        s_pending_creds.admin_token[WIFI_ADMIN_TOKEN_MAX_LEN] = '\0';

        int body_len = wifi_web_build_connecting_page(
            s_page_buf, sizeof(s_page_buf), effective_ssid);

        if (s_client_pcb) {
            send_response(s_client_pcb, 200, "OK",
                          s_page_buf, body_len > 0 ? body_len : 0);
        }

        /* Allow TCP to flush before the AP is dropped */
        absolute_time_t flush_deadline = make_timeout_time_ms(600);
        while (!time_reached(flush_deadline)) {
            cyw43_arch_poll();
            sleep_ms(10);
        }

        s_connect_pending = true;
        pov_logf(POV_LOG_SOURCE_WIFI_HTTP, "connect requested for SSID: %s\n",
               s_pending_creds.ssid);
    }
}

bool wifi_http_connect_pending(void) {
    return s_connect_pending;
}

void wifi_http_get_pending_credentials(wifi_credentials_t *out) {
    *out = s_pending_creds;
}

void wifi_http_reset_pending(wifi_err_t new_error) {
    s_connect_pending = false;
    s_post_pending    = false;
    s_get_pending     = false;
    s_current_err     = new_error;
}
