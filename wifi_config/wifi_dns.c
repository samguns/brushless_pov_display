#include "wifi_dns.h"

#include <string.h>
#include <stdio.h>

#include "pov_log.h"

#include "lwip/udp.h"
#include "lwip/ip_addr.h"

/* ---- DNS wire-format constants -------------------------------------- */

#define DNS_PORT        53
#define DNS_HDR_SIZE    12

/* DNS header flags for a standard query response (QR=1, AA=1, no error) */
#define DNS_FLAG_RESPONSE   0x8000
#define DNS_FLAG_AA         0x0400

static struct udp_pcb *s_dns_pcb = NULL;

/* ---- Minimal DNS A-record responder --------------------------------- */

/*
 * Build a DNS response that answers every A query with 192.168.4.1.
 * `in`  — raw DNS query packet
 * `in_len` — length of query
 * `out` — caller-supplied response buffer (at least 512 bytes)
 * Returns the length of the response to send.
 */
static int build_dns_response(const uint8_t *in, u16_t in_len,
                               uint8_t *out, size_t out_max) {
    if (in_len < DNS_HDR_SIZE || out_max < DNS_HDR_SIZE + 16u) {
        return -1;
    }

    /* Copy the question ID */
    memcpy(out, in, DNS_HDR_SIZE);

    /* Set response flags: QR=1, AA=1, OPCODE=0, RCODE=0 */
    uint16_t flags = DNS_FLAG_RESPONSE | DNS_FLAG_AA;
    out[2] = (uint8_t)(flags >> 8);
    out[3] = (uint8_t)(flags & 0xFF);

    /* QDCOUNT = 1, ANCOUNT = 1, NSCOUNT = 0, ARCOUNT = 0 */
    out[4] = 0; out[5] = 1;  /* QDCOUNT */
    out[6] = 0; out[7] = 1;  /* ANCOUNT */
    out[8] = 0; out[9] = 0;  /* NSCOUNT */
    out[10] = 0; out[11] = 0; /* ARCOUNT */

    /* Copy question section from query (everything after the header) */
    u16_t qlen = (u16_t)(in_len - DNS_HDR_SIZE);
    if ((size_t)(DNS_HDR_SIZE + qlen + 16) > out_max) {
        return -1;
    }
    memcpy(out + DNS_HDR_SIZE, in + DNS_HDR_SIZE, qlen);
    size_t pos = DNS_HDR_SIZE + qlen;

    /* Answer section:
     *   NAME     = 0xC00C (pointer to offset 12 = start of question QNAME)
     *   TYPE     = 0x0001 (A)
     *   CLASS    = 0x0001 (IN)
     *   TTL      = 60 seconds
     *   RDLENGTH = 4
     *   RDATA    = 192.168.4.1
     */
    out[pos++] = 0xC0; out[pos++] = 0x0C; /* name pointer */
    out[pos++] = 0x00; out[pos++] = 0x01; /* TYPE A */
    out[pos++] = 0x00; out[pos++] = 0x01; /* CLASS IN */
    out[pos++] = 0x00; out[pos++] = 0x00;
    out[pos++] = 0x00; out[pos++] = 60;   /* TTL = 60 */
    out[pos++] = 0x00; out[pos++] = 0x04; /* RDLENGTH = 4 */
    out[pos++] = 192;
    out[pos++] = 168;
    out[pos++] = 4;
    out[pos++] = 1;

    return (int)pos;
}

/* ---- UDP receive callback ------------------------------------------- */

static void dns_recv(void *arg, struct udp_pcb *pcb,
                     struct pbuf *p, const ip_addr_t *addr, u16_t port) {
    (void)arg;

    static uint8_t req_buf[512];
    static uint8_t rsp_buf[512];

    if (!p) return;

    u16_t copy_len = p->tot_len < (u16_t)sizeof(req_buf)
                     ? p->tot_len : (u16_t)sizeof(req_buf);
    pbuf_copy_partial(p, req_buf, copy_len, 0);
    pbuf_free(p);

    int rsp_len = build_dns_response(req_buf, copy_len,
                                      rsp_buf, sizeof(rsp_buf));
    if (rsp_len <= 0) return;

    struct pbuf *rsp = pbuf_alloc(PBUF_TRANSPORT, (u16_t)rsp_len, PBUF_RAM);
    if (!rsp) return;

    memcpy(rsp->payload, rsp_buf, rsp_len);
    udp_sendto(pcb, rsp, addr, port);
    pbuf_free(rsp);
}

/* ---- public API ----------------------------------------------------- */

void wifi_dns_start(void) {
    s_dns_pcb = udp_new();
    if (!s_dns_pcb) {
        pov_logf(POV_LOG_SOURCE_WIFI_DNS, "failed to allocate PCB\n");
        return;
    }
    udp_recv(s_dns_pcb, dns_recv, NULL);
    err_t err = udp_bind(s_dns_pcb, IP_ANY_TYPE, DNS_PORT);
    if (err != ERR_OK) {
        pov_logf(POV_LOG_SOURCE_WIFI_DNS, "bind to port 53 failed: %d\n", (int)err);
        udp_remove(s_dns_pcb);
        s_dns_pcb = NULL;
        return;
    }
    pov_logf(POV_LOG_SOURCE_WIFI_DNS, "captive portal DNS listening on port 53\n");
}

void wifi_dns_stop(void) {
    if (s_dns_pcb) {
        udp_remove(s_dns_pcb);
        s_dns_pcb = NULL;
    }
}
