#ifndef WIFI_DNS_H
#define WIFI_DNS_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * wifi_dns_start() — bind a raw UDP socket to port 53 and answer all
 * A-record queries with 192.168.4.1, triggering the OS captive-portal
 * mechanism on iOS, Android, and Windows.
 */
void wifi_dns_start(void);

/*
 * wifi_dns_stop() — release the UDP socket.
 */
void wifi_dns_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_DNS_H */
