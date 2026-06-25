#include "wifi_web.h"

#include <stdio.h>
#include <string.h>

/* ---- Helper: signal strength bar ------------------------------------ */

static const char *rssi_label(int16_t rssi) {
    if (rssi >= -55) return "▂▄▆█";
    if (rssi >= -67) return "▂▄▆_";
    if (rssi >= -75) return "▂▄__";
    return "▂___";
}

/* ---- Config page ---------------------------------------------------- */

int wifi_web_build_config_page(char *buf, size_t buflen,
                               wifi_err_t error,
                               const scan_result_t *results, int n_results) {
    /* Error banner text */
    const char *banner = "";
    const char *banner_class = "error";
    switch (error) {
    case WIFI_ERR_BAD_AUTH:
        banner = "Incorrect password. Please try again.";
        break;
    case WIFI_ERR_TIMEOUT:
        banner = "Network not found or out of range. Move closer to your router and try again.";
        break;
    case WIFI_ERR_SAVE_FAILED:
        banner = "Connected but failed to save settings. Please try again.";
        break;
    case WIFI_ERR_RECOVERY:
        banner = "Previous connection failed. Please reconfigure.";
        banner_class = "info";
        break;
    default:
        break;
    }

    /* Build <option> list */
    static char options[3072];
    int opt_pos = 0;
    for (int i = 0; i < n_results && opt_pos < (int)sizeof(options) - 128; i++) {
        const scan_result_t *r = &results[i];
        opt_pos += snprintf(options + opt_pos, sizeof(options) - opt_pos,
            "<option value=\"%s\">%s %s %s</option>\n",
            r->ssid, r->ssid,
            rssi_label(r->rssi),
            r->secured ? "" : "(open)");
    }
    options[opt_pos] = '\0';

    /* Build banner HTML */
    static char banner_html[320];
    if (banner[0]) {
        snprintf(banner_html, sizeof(banner_html),
            "<div class=\"%s\">%s</div>\n", banner_class, banner);
    } else {
        banner_html[0] = '\0';
    }

    /* Render full page */
    int len = snprintf(buf, buflen,
        "<!DOCTYPE html>\n"
        "<html lang=\"en\">\n"
        "<head>\n"
        "<meta charset=\"utf-8\">\n"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
        "<title>WiFi Setup</title>\n"
        "<style>\n"
        "body{font-family:sans-serif;max-width:420px;margin:40px auto;padding:0 16px}\n"
        "h1{color:#333}label{display:block;margin:10px 0 3px;font-weight:bold}\n"
        "select,input{width:100%%;padding:8px;box-sizing:border-box;margin-bottom:6px}\n"
        "button{background:#0078d4;color:#fff;padding:10px;border:none;cursor:pointer;width:100%%}\n"
        ".error{background:#fde8e8;border:1px solid #c00;padding:10px;margin-bottom:12px;border-radius:4px}\n"
        ".info{background:#e8f0fe;border:1px solid #00c;padding:10px;margin-bottom:12px;border-radius:4px}\n"
        "</style>\n"
        "</head>\n"
        "<body>\n"
        "<h1>WiFi Setup</h1>\n"
        "%s"
        "<form method=\"POST\" action=\"/connect\">\n"
        "<label>Select network (%d found):</label>\n"
        "<select name=\"ssid\">\n"
        "<option value=\"\">-- Choose a network --</option>\n"
        "%s"
        "</select>\n"
        "<label>Or enter network name manually (for hidden networks):</label>\n"
        "<input type=\"text\" name=\"ssid_manual\" maxlength=\"32\" "
        "placeholder=\"Hidden network SSID\">\n"
        "<label>Password:</label>\n"
        "<input type=\"password\" name=\"password\" maxlength=\"63\">\n"
        "<button type=\"submit\">Connect</button>\n"
        "</form>\n"
        "<p><a href=\"/\">Refresh networks</a></p>\n"
        "</body>\n"
        "</html>\n",
        banner_html, n_results, options);

    return (len > 0 && (size_t)len < buflen) ? len : -1;
}

/* ---- Connecting page ------------------------------------------------ */

int wifi_web_build_connecting_page(char *buf, size_t buflen,
                                   const char *ssid) {
    int len = snprintf(buf, buflen,
        "<!DOCTYPE html>\n"
        "<html lang=\"en\">\n"
        "<head>\n"
        "<meta charset=\"utf-8\">\n"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
        "<title>Connecting\xe2\x80\xa6</title>\n"
        "<style>\n"
        "body{font-family:sans-serif;max-width:420px;margin:40px auto;padding:0 16px}\n"
        ".info{background:#e8f0fe;border:1px solid #00c;padding:16px;border-radius:4px}\n"
        "</style>\n"
        "</head>\n"
        "<body>\n"
        "<h1>Connecting\xe2\x80\xa6</h1>\n"
        "<div class=\"info\">\n"
        "<p>The device is now attempting to connect to <strong>%s</strong>.</p>\n"
        "<p>The <em>pov-leds-setup</em> network will <strong>disappear</strong> "
        "if the connection succeeds. Reconnect your device to your home WiFi.</p>\n"
        "<p>If <em>pov-leds-setup</em> reappears within 30 seconds, the "
        "connection failed. Reconnect to it (password: <code>12345678</code>) "
        "and try again.</p>\n"
        "</div>\n"
        "</body>\n"
        "</html>\n",
        ssid);

    return (len > 0 && (size_t)len < buflen) ? len : -1;
}

/* ---- 400 error page ------------------------------------------------- */

int wifi_web_build_error_page(char *buf, size_t buflen,
                              const char *message) {
    int len = snprintf(buf, buflen,
        "<!DOCTYPE html>\n"
        "<html lang=\"en\">\n"
        "<head><meta charset=\"utf-8\"><title>Error</title>\n"
        "<style>body{font-family:sans-serif;max-width:420px;margin:40px auto;padding:0 16px}\n"
        ".error{background:#fde8e8;border:1px solid #c00;padding:12px;border-radius:4px}</style>\n"
        "</head>\n"
        "<body>\n"
        "<h1>Invalid Request</h1>\n"
        "<div class=\"error\"><p>%s</p></div>\n"
        "<p><a href=\"/\">Go back</a></p>\n"
        "</body>\n"
        "</html>\n",
        message);

    return (len > 0 && (size_t)len < buflen) ? len : -1;
}
