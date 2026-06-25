#ifndef WIFI_HTTP_H
#define WIFI_HTTP_H

#include <stdbool.h>
#include "wifi_config.h"
#include "wifi_web.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * wifi_http_start() — open a TCP listener on port 80.
 * `initial_error` is shown as a banner on the first GET /.
 * Call once, after cyw43_arch_enable_ap_mode().
 */
void wifi_http_start(wifi_err_t initial_error);

/*
 * wifi_http_stop() — close the listening socket.
 */
void wifi_http_stop(void);

/*
 * wifi_http_poll() — process pending GET / requests (scan + render).
 * Call from the main AP loop on every iteration.
 */
void wifi_http_poll(void);

/*
 * wifi_http_connect_pending() — returns true once the user has submitted
 * a valid POST /connect form and the Connecting page has been sent.
 */
bool wifi_http_connect_pending(void);

/*
 * wifi_http_get_pending_credentials() — copy the submitted credentials.
 * Only valid when wifi_http_connect_pending() is true.
 */
void wifi_http_get_pending_credentials(wifi_credentials_t *out);

/*
 * wifi_http_reset_pending() — clear the pending-connect flag so the server
 * can accept a new request after a failed connection attempt.
 */
void wifi_http_reset_pending(wifi_err_t new_error);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_HTTP_H */
