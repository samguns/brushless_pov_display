/*
 * Minimal DHCP server for Pico W AP mode (RFC 2131 / RFC 2132).
 *
 * Based on the MicroPython / pico-examples reference implementation by
 * Damien P. George (MIT licence).
 *
 * Key design choices:
 *  - Always responds to broadcast (0xffffffff) — simplest and most compatible.
 *  - Binds the UDP socket to the AP netif via udp_bind_netif() so DHCP traffic
 *    is correctly routed through the AP interface, not the STA interface.
 *  - Assigns client IPs gateway.{BASE_IP+0} ... gateway.{BASE_IP+MAX_IP-1}.
 */

#include "dhcpserver.h"

#include <string.h>
#include <stdio.h>

#include "pov_log.h"

#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include "pico/time.h"

/* ---- Constants ------------------------------------------------------ */

#define PORT_DHCP_SERVER    67
#define PORT_DHCP_CLIENT    68
#define DHCP_MIN_SIZE       (240 + 3)
#define DEFAULT_LEASE_S     86400u  /* 24 hours */

#define DHCPDISCOVER    1
#define DHCPOFFER       2
#define DHCPREQUEST     3
#define DHCPACK         5

#define OPT_SUBNET_MASK     1
#define OPT_ROUTER          3
#define OPT_DNS             6
#define OPT_LEASE_TIME      51
#define OPT_MSG_TYPE        53
#define OPT_SERVER_ID       54
#define OPT_REQUESTED_IP    50
#define OPT_END             255

/* ---- DHCP packet ----------------------------------------------------- */

typedef struct {
    uint8_t  op, htype, hlen, hops;
    uint32_t xid;
    uint16_t secs, flags;
    uint8_t  ciaddr[4], yiaddr[4], siaddr[4], giaddr[4];
    uint8_t  chaddr[16];
    uint8_t  sname[64];
    uint8_t  file[128];
    uint8_t  options[312]; /* starts with 4-byte magic cookie */
} dhcp_msg_t;

/* ---- Option write helpers ------------------------------------------- */

static void opt_u8(uint8_t **p, uint8_t code, uint8_t val) {
    *(*p)++ = code; *(*p)++ = 1; *(*p)++ = val;
}

static void opt_u32(uint8_t **p, uint8_t code, uint32_t val) {
    *(*p)++ = code; *(*p)++ = 4;
    *(*p)++ = val >> 24; *(*p)++ = val >> 16;
    *(*p)++ = val >> 8;  *(*p)++ = val;
}

static void opt_ip(uint8_t **p, uint8_t code, const ip_addr_t *ip) {
    *(*p)++ = code; *(*p)++ = 4;
    memcpy(*p, ip_2_ip4(ip), 4); *p += 4;
}

/* Find an option by code in the options buffer (past the magic cookie). */
static uint8_t *opt_find(uint8_t *opt, uint8_t code) {
    for (int i = 0; i < 308 && opt[i] != OPT_END; ) {
        if (opt[i] == code) return &opt[i];
        i += 2 + opt[i + 1];
    }
    return NULL;
}

/* ---- Send helper ----------------------------------------------------- */

static void send_to_broadcast(struct udp_pcb *pcb,
                               const void *data, size_t len) {
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, (u16_t)len, PBUF_RAM);
    if (!p) return;
    memcpy(p->payload, data, len);

    /* Always broadcast — client has no IP yet during DISCOVER/REQUEST */
    ip_addr_t bcast;
    IP4_ADDR(ip_2_ip4(&bcast), 255, 255, 255, 255);
    udp_sendto(pcb, p, &bcast, PORT_DHCP_CLIENT);
    pbuf_free(p);
}

/* ---- Lease helpers -------------------------------------------------- */

static inline uint32_t ticks_ms(void) {
    return (uint32_t)to_ms_since_boot(get_absolute_time());
}

/* Return 0-based lease index for this MAC (existing or new), or -1 if full. */
static int lease_for_mac(dhcp_server_t *d, const uint8_t *mac) {
    int free_slot = -1;
    const uint8_t zero[6] = {0};
    for (int i = 0; i < DHCPS_MAX_IP; i++) {
        if (memcmp(d->lease[i].mac, mac, 6) == 0) return i;   /* existing */
        if (free_slot < 0) {
            if (memcmp(d->lease[i].mac, zero, 6) == 0) {
                free_slot = i;
            } else {
                uint32_t expiry = (uint32_t)d->lease[i].expiry << 16 | 0xffff;
                if ((int32_t)(expiry - ticks_ms()) < 0) {
                    memset(d->lease[i].mac, 0, 6);
                    free_slot = i;
                }
            }
        }
    }
    if (free_slot >= 0) {
        memcpy(d->lease[free_slot].mac, mac, 6);
        return free_slot;
    }
    return -1;
}

/* ---- UDP receive callback ------------------------------------------- */

static void dhcp_recv(void *arg, struct udp_pcb *pcb,
                      struct pbuf *p, const ip_addr_t *addr, u16_t port) {
    (void)addr; (void)port;
    dhcp_server_t *d = (dhcp_server_t *)arg;

    static dhcp_msg_t msg; /* static: ~548 bytes, keep off stack */

    if (p->tot_len < DHCP_MIN_SIZE) goto done;
    if (pbuf_copy_partial(p, &msg, sizeof(msg), 0) < DHCP_MIN_SIZE) goto done;

    /* Set up the reply in-place */
    msg.op = DHCPOFFER;
    /* yiaddr: copy server IP bytes then overwrite last octet per lease */
    memcpy(msg.yiaddr, ip_2_ip4(&d->ip), 4);

    uint8_t *opt = msg.options + 4; /* skip 4-byte magic cookie */

    /* opt[0]=code, opt[1]=len, opt[2]=value for the first option */
    switch (opt[2]) {

    case DHCPDISCOVER: {
        int idx = lease_for_mac(d, msg.chaddr);
        if (idx < 0) goto done;
        msg.yiaddr[3] = (uint8_t)(DHCPS_BASE_IP + idx);
        opt_u8(&opt, OPT_MSG_TYPE, DHCPOFFER);
        pov_logf(POV_LOG_SOURCE_DHCP, "OFFER .%d to %02x:%02x:%02x:%02x:%02x:%02x\n",
               msg.yiaddr[3],
               msg.chaddr[0], msg.chaddr[1], msg.chaddr[2],
               msg.chaddr[3], msg.chaddr[4], msg.chaddr[5]);
        break;
    }

    case DHCPREQUEST: {
        uint8_t *req_ip = opt_find(msg.options + 4, OPT_REQUESTED_IP);
        if (!req_ip) goto done;
        /* First 3 octets must match our subnet (e.g. 192.168.4) */
        if (memcmp(req_ip + 2, ip_2_ip4(&d->ip), 3) != 0) goto done;
        uint8_t host = req_ip[5];
        int idx = (int)(host - DHCPS_BASE_IP);
        if (idx < 0 || idx >= DHCPS_MAX_IP) goto done;

        const uint8_t zero[6] = {0};
        if (memcmp(d->lease[idx].mac, msg.chaddr, 6) == 0 ||
            memcmp(d->lease[idx].mac, zero, 6) == 0) {
            memcpy(d->lease[idx].mac, msg.chaddr, 6);
            d->lease[idx].expiry =
                (uint16_t)((ticks_ms() + DEFAULT_LEASE_S * 1000u) >> 16);
        } else {
            goto done; /* IP in use by a different client */
        }
        msg.yiaddr[3] = host;
        opt_u8(&opt, OPT_MSG_TYPE, DHCPACK);
        pov_logf(POV_LOG_SOURCE_DHCP, "ACK %d.%d.%d.%d\n",
               msg.yiaddr[0], msg.yiaddr[1], msg.yiaddr[2], msg.yiaddr[3]);
        break;
    }

    default:
        goto done;
    }

    opt_ip(&opt,  OPT_SERVER_ID,   &d->ip);
    opt_ip(&opt,  OPT_SUBNET_MASK, &d->nm);
    opt_ip(&opt,  OPT_ROUTER,      &d->ip);
    opt_ip(&opt,  OPT_DNS,         &d->ip);
    opt_u32(&opt, OPT_LEASE_TIME,  DEFAULT_LEASE_S);
    *opt++ = OPT_END;

    send_to_broadcast(pcb, &msg, (size_t)(opt - (uint8_t *)&msg));

done:
    pbuf_free(p);
}

/* ---- Public API ----------------------------------------------------- */

void dhcp_server_init(dhcp_server_t *d,
                      ip_addr_t *ip,
                      ip_addr_t *nm,
                      struct netif *netif) {
    ip_addr_copy(d->ip, *ip);
    ip_addr_copy(d->nm, *nm);
    memset(d->lease, 0, sizeof(d->lease));

    d->udp = udp_new();
    if (!d->udp) { pov_logf(POV_LOG_SOURCE_DHCP, "udp_new failed\n"); return; }

    udp_recv(d->udp, dhcp_recv, d);

    err_t err = udp_bind(d->udp, IP_ANY_TYPE, PORT_DHCP_SERVER);
    if (err != ERR_OK) {
        pov_logf(POV_LOG_SOURCE_DHCP, "bind failed: %d\n", (int)err);
        udp_remove(d->udp); d->udp = NULL; return;
    }

    /* Bind to the specific AP netif so DHCP broadcasts only arrive from
     * the AP side and responses egress through the right interface. */
    if (netif) {
        udp_bind_netif(d->udp, netif);
    }

    pov_logf(POV_LOG_SOURCE_DHCP, "started on %s, pool .%d-.%d\n",
           ip4addr_ntoa(ip_2_ip4(ip)),
           DHCPS_BASE_IP, DHCPS_BASE_IP + DHCPS_MAX_IP - 1);
}

void dhcp_server_deinit(dhcp_server_t *d) {
    if (d->udp) { udp_remove(d->udp); d->udp = NULL; }
}
