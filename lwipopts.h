#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

/* -----------------------------------------------------------------------
 * lwipopts.h — lwIP configuration for pov_leds / wifi_config feature
 * Based on Pico W SDK examples with additions for raw TCP HTTP server
 * and raw UDP DNS captive-portal responder.
 * ----------------------------------------------------------------------- */

/* No OS (polling mode via pico_cyw43_arch_lwip_poll) */
#define NO_SYS                      1
#define LWIP_SOCKET                 0
#define LWIP_NETCONN                0

/* Memory */
#define MEM_LIBC_MALLOC             1
#define MEM_ALIGNMENT               4
#define MEM_SIZE                    (16 * 1024)
#define MEMP_NUM_TCP_SEG            32
#define MEMP_NUM_ARP_QUEUE          10
#define PBUF_POOL_SIZE              24

/* Protocols */
#define LWIP_ARP                    1
#define LWIP_ETHERNET               1
#define LWIP_ICMP                   1
#define LWIP_RAW                    1
#define LWIP_UDP                    1
#define LWIP_TCP                    1
#define LWIP_IPV4                   1
#define LWIP_IPV6                   0

/* TCP tuning (single-connection embedded HTTP server) */
#define TCP_MSS                     536
#define TCP_WND                     (8 * TCP_MSS)
#define TCP_SND_BUF                 (8 * TCP_MSS)
#define TCP_SND_QUEUELEN            ((4 * (TCP_SND_BUF) + (TCP_MSS - 1)) / (TCP_MSS))

/* Maximum TCP connections (1 client at a time for config page) */
#define MEMP_NUM_TCP_PCB            4
#define MEMP_NUM_TCP_PCB_LISTEN     2

/* DHCP client (used in STA mode) */
#define LWIP_DHCP                   1
#define DHCP_DOES_ARP_CHECK         0
#define LWIP_DHCP_DOES_ACD_CHECK    0

/* DNS client (needed for SDK compat; we don't use it as a client) */
#define LWIP_DNS                    1

/* Netif callbacks */
#define LWIP_NETIF_STATUS_CALLBACK  1
#define LWIP_NETIF_LINK_CALLBACK    1
#define LWIP_NETIF_TX_SINGLE_PBUF   1

/* Checksum */
#define LWIP_CHKSUM_ALGORITHM       3

/* TCP keepalive */
#define LWIP_TCP_KEEPALIVE          1

/* Stats — disable in release, enable for debugging */
#ifndef NDEBUG
#define LWIP_STATS                  1
#define LWIP_STATS_DISPLAY          1
#else
#define LWIP_STATS                  0
#endif

/* Debug — all off by default */
#define LWIP_DEBUG                  0
#define ETHARP_DEBUG                LWIP_DBG_OFF
#define NETIF_DEBUG                 LWIP_DBG_OFF
#define PBUF_DEBUG                  LWIP_DBG_OFF
#define IP_DEBUG                    LWIP_DBG_OFF
#define TCP_DEBUG                   LWIP_DBG_OFF
#define TCP_INPUT_DEBUG             LWIP_DBG_OFF
#define TCP_OUTPUT_DEBUG            LWIP_DBG_OFF
#define UDP_DEBUG                   LWIP_DBG_OFF
#define DHCP_DEBUG                  LWIP_DBG_OFF

#endif /* _LWIPOPTS_H */
