#include "wifi_sta_web.h"

#include <stdio.h>

/* Shared minimal styling kept inline so the pages are fully self-contained
 * (served from flash, no external assets — see contracts/http-api.md). */
#define STA_STYLE \
    "<style>" \
    "body{font-family:system-ui,sans-serif;max-width:30rem;margin:2rem auto;" \
    "padding:0 1rem;color:#1a1a1a;background:#fafafa}" \
    "h1{font-size:1.4rem}" \
    ".card{background:#fff;border:1px solid #e0e0e0;border-radius:8px;" \
    "padding:1rem 1.25rem;margin:1rem 0}" \
    ".k{color:#666;font-size:.85rem}.v{font-weight:600;font-size:1.1rem}" \
    "button{font-size:1rem;padding:.6rem 1.1rem;border-radius:6px;border:0;" \
    "cursor:pointer}" \
    ".primary{background:#c0392b;color:#fff}.cancel{display:inline-block;" \
    "margin-left:.75rem;color:#444}" \
    ".warn{background:#fff3cd;border:1px solid #ffe69c;border-radius:8px;" \
    "padding:1rem 1.25rem}" \
    "</style>"

int wifi_sta_web_build_status_page(char *buf, size_t buflen,
                                   const char *ssid,
                                   const char *ip,
                                   const char *connectivity_state,
                                   bool blink_active,
                                   uint32_t blink_hz) {
    int n = snprintf(buf, buflen,
        "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>pov-leds</title>" STA_STYLE "</head><body>"
        "<h1>pov-leds device</h1>"
        "<div class=\"card\">"
        "<div class=\"k\">Status</div><div class=\"v\">%s</div></div>"
        "<div class=\"card\">"
        "<div class=\"k\">Network (SSID)</div><div class=\"v\">%s</div></div>"
        "<div class=\"card\">"
        "<div class=\"k\">IP address</div><div class=\"v\">%s</div></div>"
        "<div class=\"card\">"
        "<div class=\"k\">Blink active</div><div class=\"v\">%s</div></div>"
        "<div class=\"card\">"
        "<div class=\"k\">Blink frequency (Hz)</div><div class=\"v\">%u</div></div>"
        "<form action=\"/update\" method=\"GET\">"
        "<button class=\"primary\" type=\"submit\">Update firmware</button>"
        "</form>"
        "</body></html>",
        connectivity_state ? connectivity_state : "unknown",
        ssid ? ssid : "",
        ip ? ip : "",
        blink_active ? "true" : "false",
        (unsigned)blink_hz);

    if (n < 0 || (size_t)n >= buflen) return -1;
    return n;
}

int wifi_sta_web_build_status_json(char *buf, size_t buflen,
                                   const char *ip,
                                   const char *connectivity_state,
                                   bool blink_active,
                                   uint32_t blink_hz) {
    int n = snprintf(buf, buflen,
        "{\"wifi\":{\"state\":\"%s\",\"ip\":\"%s\"},"
        "\"blink\":{\"active\":%s,\"frequency_hz\":%u}}",
        connectivity_state ? connectivity_state : "unknown",
        ip ? ip : "",
        blink_active ? "true" : "false",
        (unsigned)blink_hz);
    if (n < 0 || (size_t)n >= buflen) return -1;
    return n;
}

int wifi_sta_web_build_update_page(char *buf, size_t buflen) {
    int n = snprintf(buf, buflen,
        "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>Confirm update</title>" STA_STYLE "</head><body>"
        "<h1>Firmware update</h1>"
        "<div class=\"warn\">"
        "<p><strong>Warning:</strong> the device will reboot into USB drive "
        "mode. A removable drive will appear on your computer &mdash; drag a "
        "new <code>.uf2</code> firmware file onto it to update.</p>"
        "<p>Returning to normal mode in <span id=\"c\">60</span> s if no "
        "action is taken.</p>"
        "</div>"
        "<form action=\"/update\" method=\"POST\">"
        "<button class=\"primary\" type=\"submit\">Confirm update</button>"
        "<a class=\"cancel\" href=\"/\">Cancel</a>"
        "</form>"
        "<script>"
        "var n=60,e=document.getElementById('c');"
        "setInterval(function(){n--;if(e)e.textContent=n;"
        "if(n<=0)location.href='/';},1000);"
        "</script>"
        "</body></html>");

    if (n < 0 || (size_t)n >= buflen) return -1;
    return n;
}

int wifi_sta_web_build_rebooting_page(char *buf, size_t buflen) {
    int n = snprintf(buf, buflen,
        "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>Rebooting</title>" STA_STYLE "</head><body>"
        "<h1>Rebooting into update mode&hellip;</h1>"
        "<p>A USB drive should appear on your computer shortly. Drop a new "
        "<code>.uf2</code> file onto it to flash new firmware.</p>"
        "<p>Your saved WiFi network will be remembered after the update.</p>"
        "</body></html>");

    if (n < 0 || (size_t)n >= buflen) return -1;
    return n;
}
