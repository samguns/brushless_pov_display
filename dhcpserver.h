#ifndef DHCPSERVER_H
#define DHCPSERVER_H

/*
 * Minimal DHCP server for AP mode (RFC 2131).
 * Based on the MicroPython / pico-examples reference implementation.
 * Assigns IPs gateway_ip.1 + 16 ... +23 to connecting clients.
 */

#include "lwip/ip_addr.h"
#include "lwip/netif.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DHCPS_BASE_IP   16   /* first client host octet offset from gateway */
#define DHCPS_MAX_IP     8   /* maximum simultaneous leases */

typedef struct {
    uint8_t  mac[6];
    uint16_t expiry;         /* ms >> 16 */
} dhcp_server_lease_t;

typedef struct {
    ip_addr_t            ip;    /* server (gateway) IP */
    ip_addr_t            nm;    /* subnet mask */
    dhcp_server_lease_t  lease[DHCPS_MAX_IP];
    struct udp_pcb      *udp;
} dhcp_server_t;

/*
 * dhcp_server_init() — start the DHCP server.
 * `ip`    : server/gateway address (e.g. 192.168.4.1)
 * `nm`    : subnet mask
 * `netif` : AP network interface to bind to (use &cyw43_state.netif[CYW43_ITF_AP])
 */
void dhcp_server_init(dhcp_server_t *d,
                      ip_addr_t *ip,
                      ip_addr_t *nm,
                      struct netif *netif);

void dhcp_server_deinit(dhcp_server_t *d);

#ifdef __cplusplus
}
#endif

#endif /* DHCPSERVER_H */
