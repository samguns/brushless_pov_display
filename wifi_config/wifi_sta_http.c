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

/* ---- configuration -------------------------------------------------- */

#define HTTP_PORT           80
#define STA_PAGE_BUF_SIZE   4096    /* generous for the two small pages    */
#define STA_HDR_BUF_SIZE    256
#define STA_REQ_BUF_SIZE    1024    /* request accumulation buffer         */

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

/* ---- HTTP send helpers (mirror wifi_http.c) ------------------------- */

static uint32_t now_ms(void) {
    return to_ms_since_boot(get_absolute_time());
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

static void send_json_response(struct tcp_pcb *pcb,
                               int status_code,
                               const char *status_text,
                               const char *body) {
    int body_len = (int)strlen(body);
    int hdr_len = snprintf(s_hdr_buf, sizeof(s_hdr_buf),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n",
        status_code, status_text, body_len);

    tcp_write(pcb, s_hdr_buf, (u16_t)hdr_len, TCP_WRITE_FLAG_COPY);
    tcp_write(pcb, body, (u16_t)body_len, TCP_WRITE_FLAG_COPY);
    tcp_output(pcb);
    tcp_close(pcb);
    if (s_client_pcb == pcb) s_client_pcb = NULL;
}

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
    (void)arg; (void)err;
    printf("[wifi_sta_http] client error %d\n", (int)err);
    s_client_pcb = NULL;
    s_req_len    = 0;
}

static err_t on_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
    (void)arg;
    if (!p || err != ERR_OK) {
        if (p) pbuf_free(p);
        tcp_close(pcb);
        if (s_client_pcb == pcb) { s_client_pcb = NULL; s_req_len = 0; }
        return ERR_OK;
    }

    /* Accumulate request across multiple tcp_recv frames */
    u16_t space    = (u16_t)(sizeof(s_req_buf) - 1 - s_req_len);
    u16_t copy_len = p->tot_len < space ? p->tot_len : space;
    pbuf_copy_partial(p, s_req_buf + s_req_len, copy_len, 0);
    s_req_len += copy_len;
    s_req_buf[s_req_len] = '\0';
    pbuf_free(p);
    tcp_recved(pcb, copy_len);

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
        char provided_token[WIFI_ADMIN_TOKEN_MAX_LEN + 1];
        if (auth_is_blocked()) {
            send_json_response(pcb, 429, "Too Many Requests",
                               "{\"error\":\"too_many_invalid_attempts\"}");
            s_req_len = 0;
            return ERR_OK;
        }
        if (!request_is_authorized_mutation(s_req_buf, body,
                                            provided_token, sizeof(provided_token))) {
            auth_record_invalid_attempt();
            if (auth_is_blocked()) {
                send_json_response(pcb, 429, "Too Many Requests",
                                   "{\"error\":\"too_many_invalid_attempts\"}");
            } else {
                send_json_response(pcb, 401, "Unauthorized",
                                   "{\"error\":\"unauthorized\"}");
            }
            s_req_len = 0;
            return ERR_OK;
        }

        auth_record_valid_attempt();
        send_json_response(pcb, 200, "OK", "{\"result\":\"ok\"}");
        s_req_len = 0;
    } else if (strncmp(s_req_buf, "POST /update", 12) == 0) {
        char provided_token[WIFI_ADMIN_TOKEN_MAX_LEN + 1];
        if (auth_is_blocked()) {
            send_json_response(pcb, 429, "Too Many Requests",
                               "{\"error\":\"too_many_invalid_attempts\"}");
            s_req_len = 0;
            return ERR_OK;
        }
        if (!request_is_authorized_mutation(s_req_buf, body,
                                            provided_token, sizeof(provided_token))) {
            auth_record_invalid_attempt();
            if (auth_is_blocked()) {
                send_json_response(pcb, 429, "Too Many Requests",
                                   "{\"error\":\"too_many_invalid_attempts\"}");
            } else {
                send_json_response(pcb, 401, "Unauthorized",
                                   "{\"error\":\"unauthorized\"}");
            }
            s_req_len = 0;
            return ERR_OK;
        }

        auth_record_valid_attempt();
        int page_len = wifi_sta_web_build_rebooting_page(s_page_buf, sizeof(s_page_buf));
        send_response(pcb, 200, "OK", s_page_buf, page_len > 0 ? page_len : 0);
        s_req_len = 0;
        s_reboot_pending = true;
        printf("[wifi_sta_http] authorized firmware update confirmed\n");
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
            s_connectivity_state, s_blink_active, s_blink_hz);
        send_response(pcb, 200, "OK", s_page_buf, page_len > 0 ? page_len : 0);
        s_req_len = 0;
    } else {
        /* Unknown path — redirect to / */
        send_redirect(pcb);
        s_req_len = 0;
    }

    return ERR_OK;
}

static err_t on_accept(void *arg, struct tcp_pcb *client_pcb, err_t err) {
    (void)arg;
    if (err != ERR_OK || !client_pcb) return ERR_VAL;

    /* Single client at a time; a second concurrent request is aborted. */
    if (s_client_pcb) {
        tcp_abort(client_pcb);
        return ERR_ABRT;
    }

    s_req_len    = 0;
    s_req_buf[0] = '\0';

    tcp_arg(client_pcb, NULL);
    tcp_err(client_pcb, on_client_err);
    tcp_recv(client_pcb, on_recv);
    s_client_pcb = client_pcb;
    return ERR_OK;
}

/* ---- public API ----------------------------------------------------- */

void wifi_sta_http_start(const char *ssid, const char *ip) {
    s_reboot_pending = false;
    s_client_pcb     = NULL;
    s_req_len        = 0;
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
    if (s_client_pcb) { tcp_abort(s_client_pcb); s_client_pcb = NULL; }
    if (s_listen_pcb) { tcp_close(s_listen_pcb); s_listen_pcb = NULL; }
}

void wifi_sta_http_poll(void) {
    if (!s_reboot_pending) return;

    /* Allow lwIP to flush the response and close the connection before we
     * hand control to the ROM bootloader (the browser must get its reply). */
    absolute_time_t deadline = make_timeout_time_ms(600);
    while (!time_reached(deadline)) {
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
