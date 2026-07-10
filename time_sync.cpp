#include "time_sync.h"

#include <string.h>

#include "lwip/dns.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include "pico/time.h"

namespace {
constexpr uint16_t kNtpPort = 123;
constexpr uint16_t kNtpPacketLen = 48;
constexpr uint32_t kNtpUnixEpochDelta = 2208988800UL;

void set_failed(time_sync_t *sync, time_sync_error_t error) {
    if (sync == nullptr) {
        return;
    }
    sync->last_error = error;
    sync->state = TIME_SYNC_STATE_FAILED;
}

uint32_t read_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) |
           (uint32_t)p[3];
}

bool send_request(time_sync_t *sync, uint32_t now_ms) {
    if (sync == nullptr || sync->pcb == nullptr) {
        return false;
    }

    pbuf *packet = pbuf_alloc(PBUF_TRANSPORT, kNtpPacketLen, PBUF_RAM);
    if (packet == nullptr || packet->payload == nullptr) {
        if (packet != nullptr) {
            pbuf_free(packet);
        }
        return false;
    }

    memset(packet->payload, 0, kNtpPacketLen);
    uint8_t *payload = static_cast<uint8_t *>(packet->payload);
    payload[0] = 0x1bu;  // LI=0, VN=3, Mode=3 (client)

    err_t err = udp_sendto(sync->pcb, packet, &sync->server_addr, kNtpPort);
    pbuf_free(packet);

    if (err != ERR_OK) {
        return false;
    }

    sync->request_started_ms = now_ms;
    sync->last_attempt_ms = now_ms;
    sync->state = TIME_SYNC_STATE_WAITING_RESPONSE;
    sync->last_error = TIME_SYNC_ERROR_NONE;
    return true;
}

void dns_found_cb(const char *name, const ip_addr_t *ipaddr, void *arg) {
    (void)name;
    time_sync_t *sync = static_cast<time_sync_t *>(arg);
    if (sync == nullptr || sync->state != TIME_SYNC_STATE_RESOLVING) {
        return;
    }
    if (ipaddr == nullptr) {
        set_failed(sync, TIME_SYNC_ERROR_DNS);
        return;
    }

    sync->server_addr = *ipaddr;
    uint32_t now_ms = (uint32_t)to_ms_since_boot(get_absolute_time());
    if (!send_request(sync, now_ms)) {
        set_failed(sync, TIME_SYNC_ERROR_TRANSPORT);
    }
}

void recv_cb(void *arg, udp_pcb *pcb, pbuf *p, const ip_addr_t *addr, uint16_t port) {
    (void)pcb;
    (void)addr;
    (void)port;
    time_sync_t *sync = static_cast<time_sync_t *>(arg);
    if (sync == nullptr) {
        if (p != nullptr) {
            pbuf_free(p);
        }
        return;
    }

    if (p == nullptr || p->tot_len < kNtpPacketLen) {
        if (p != nullptr) {
            pbuf_free(p);
        }
        set_failed(sync, TIME_SYNC_ERROR_RESPONSE);
        return;
    }

    uint8_t buf[kNtpPacketLen];
    pbuf_copy_partial(p, buf, kNtpPacketLen, 0);
    pbuf_free(p);

    uint8_t mode = buf[0] & 0x7u;
    uint32_t ntp_seconds = read_be32(&buf[40]);
    if (mode != 4u || ntp_seconds <= kNtpUnixEpochDelta) {
        set_failed(sync, TIME_SYNC_ERROR_RESPONSE);
        return;
    }

    sync->calibrated_utc_seconds = ntp_seconds - kNtpUnixEpochDelta;
    sync->calibrated_at_us = time_us_64();
    sync->state = TIME_SYNC_STATE_CALIBRATED;
    sync->last_error = TIME_SYNC_ERROR_NONE;
}

void begin_resolve_or_send(time_sync_t *sync, uint32_t now_ms) {
    if (sync == nullptr || sync->server_name == nullptr || sync->server_name[0] == '\0') {
        set_failed(sync, TIME_SYNC_ERROR_DNS);
        return;
    }

    sync->attempt_count++;
    err_t err = dns_gethostbyname(sync->server_name, &sync->server_addr, dns_found_cb, sync);
    if (err == ERR_OK) {
        if (!send_request(sync, now_ms)) {
            set_failed(sync, TIME_SYNC_ERROR_TRANSPORT);
        }
    } else if (err == ERR_INPROGRESS) {
        sync->state = TIME_SYNC_STATE_RESOLVING;
        sync->request_started_ms = now_ms;
        sync->last_attempt_ms = now_ms;
    } else {
        set_failed(sync, TIME_SYNC_ERROR_DNS);
    }
}
}  // namespace

void time_sync_init_defaults(time_sync_t *sync) {
    if (sync == nullptr) {
        return;
    }
    memset(sync, 0, sizeof(*sync));
    sync->state = TIME_SYNC_STATE_IDLE;
    sync->last_error = TIME_SYNC_ERROR_NONE;
    sync->max_attempts = TIME_SYNC_DEFAULT_MAX_ATTEMPTS;
    sync->timeout_ms = TIME_SYNC_DEFAULT_TIMEOUT_MS;
    sync->retry_delay_ms = TIME_SYNC_DEFAULT_RETRY_MS;
    sync->initialized = true;
}

void time_sync_start(time_sync_t *sync, const char *server_name, uint32_t now_ms) {
    if (sync == nullptr) {
        return;
    }
    if (!sync->initialized) {
        time_sync_init_defaults(sync);
    }
    if (sync->pcb == nullptr) {
        sync->pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
        if (sync->pcb == nullptr) {
            set_failed(sync, TIME_SYNC_ERROR_TRANSPORT);
            return;
        }
        udp_recv(sync->pcb, recv_cb, sync);
    }

    sync->server_name = server_name;
    sync->attempt_count = 0;
    sync->last_error = TIME_SYNC_ERROR_NONE;
    begin_resolve_or_send(sync, now_ms);
}

void time_sync_step(time_sync_t *sync, uint32_t now_ms) {
    if (sync == nullptr || !sync->initialized || sync->state == TIME_SYNC_STATE_CALIBRATED) {
        return;
    }

    if (sync->state == TIME_SYNC_STATE_IDLE) {
        return;
    }

    if (sync->state == TIME_SYNC_STATE_RESOLVING ||
        sync->state == TIME_SYNC_STATE_WAITING_RESPONSE) {
        if ((now_ms - sync->request_started_ms) <= sync->timeout_ms) {
            return;
        }
        sync->last_error = TIME_SYNC_ERROR_TIMEOUT;
        sync->state = TIME_SYNC_STATE_FAILED;
    }

    if (sync->state == TIME_SYNC_STATE_FAILED) {
        if (sync->attempt_count >= sync->max_attempts) {
            return;
        }
        if ((now_ms - sync->last_attempt_ms) < sync->retry_delay_ms) {
            return;
        }
        begin_resolve_or_send(sync, now_ms);
    }
}

bool time_sync_has_time(const time_sync_t *sync) {
    return sync != nullptr && sync->state == TIME_SYNC_STATE_CALIBRATED;
}

uint32_t time_sync_get_utc_seconds(const time_sync_t *sync) {
    return time_sync_has_time(sync) ? sync->calibrated_utc_seconds : 0u;
}

uint64_t time_sync_get_calibrated_at_us(const time_sync_t *sync) {
    return time_sync_has_time(sync) ? sync->calibrated_at_us : 0u;
}

const char *time_sync_state_text(time_sync_state_t state) {
    switch (state) {
        case TIME_SYNC_STATE_IDLE: return "idle";
        case TIME_SYNC_STATE_RESOLVING: return "resolving";
        case TIME_SYNC_STATE_WAITING_RESPONSE: return "waiting";
        case TIME_SYNC_STATE_CALIBRATED: return "calibrated";
        case TIME_SYNC_STATE_FAILED: return "failed";
        default: return "unknown";
    }
}

const char *time_sync_error_text(time_sync_error_t error) {
    switch (error) {
        case TIME_SYNC_ERROR_NONE: return "none";
        case TIME_SYNC_ERROR_DNS: return "dns";
        case TIME_SYNC_ERROR_TRANSPORT: return "transport";
        case TIME_SYNC_ERROR_TIMEOUT: return "timeout";
        case TIME_SYNC_ERROR_RESPONSE: return "response";
        default: return "unknown";
    }
}
