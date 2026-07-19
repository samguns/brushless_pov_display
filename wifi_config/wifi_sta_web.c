#include "wifi_sta_web.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ *
 * Shared dark/light design system (feature 007).
 *
 * Tokens mirror the Figma pov-mgmt frames (Overview 1:2, settings 9:4):
 *   bg #0d0f14, sidebar #0a0c11, card #13161e, inset #1a1d27,
 *   accent #2dd4bf, danger #ef4444, text #e2e4e9, muted #6b7280.
 * Dark is the default; a light override set is provided. Typography uses a
 * self-contained system font stack (no embedded webfonts). Emitted via "%s" so
 * literal '%' in the CSS needs no escaping.
 * ------------------------------------------------------------------ */
static const char STA_STYLE[] =
    "<style>"
    ":root{--bg:#0d0f14;--sidebar:#0a0c11;--card:#13161e;--inset:#1a1d27;"
    "--border:rgba(255,255,255,.07);--soft:rgba(255,255,255,.06);"
    "--accent:#2dd4bf;--accent-bg:rgba(45,212,191,.1);--danger:#ef4444;"
    "--text:#e2e4e9;--muted:#6b7280;--on-accent:#06201c;"
    "--sans:'Inter',system-ui,-apple-system,'Segoe UI',Roboto,sans-serif;"
    "--mono:ui-monospace,'Space Mono',SFMono-Regular,Menlo,Consolas,monospace}"
    ":root[data-theme=light]{--bg:#f4f5f7;--sidebar:#fff;--card:#fff;"
    "--inset:#eef0f3;--border:rgba(0,0,0,.1);--soft:rgba(0,0,0,.07);"
    "--accent:#0d9488;--accent-bg:rgba(13,148,136,.1);--text:#1a1d27;"
    "--muted:#6b7280;--on-accent:#fff}"
    "*{box-sizing:border-box}"
    "body{margin:0;background:var(--bg);color:var(--text);font-family:var(--sans);"
    "font-size:14px;-webkit-font-smoothing:antialiased}"
    "a{color:inherit;text-decoration:none}"
    ".app{display:flex;min-height:100vh}"
    ".sidebar{width:196px;flex-shrink:0;background:var(--sidebar);"
    "border-right:1px solid var(--soft);display:flex;flex-direction:column}"
    ".brand{display:flex;gap:9px;align-items:center;height:49px;padding:0 17px;"
    "border-bottom:1px solid var(--soft)}"
    ".brand .logo{width:21px;height:21px;border-radius:4px;background:var(--accent);"
    "display:flex;align-items:center;justify-content:center}"
    ".brand b{font-size:12.25px;font-weight:600}"
    ".nav{flex:1;padding:10px;display:flex;flex-direction:column;gap:2px}"
    ".nav .sec{font-family:var(--mono);font-size:10px;letter-spacing:1px;"
    "text-transform:uppercase;color:var(--muted);padding:7px}"
    ".nav a{display:flex;gap:10px;align-items:center;padding:7px 10px;"
    "border-radius:5px;color:var(--muted);font-weight:500;font-size:12.25px}"
    ".nav a.active{background:var(--accent-bg);color:var(--accent)}"
    ".ico{width:15px;height:15px;flex-shrink:0}"
    ".userbox{display:flex;gap:10px;align-items:center;padding:14px;"
    "border-top:1px solid var(--soft)}"
    ".avatar{width:24px;height:24px;border-radius:999px;background:var(--accent-bg);"
    "color:var(--accent);display:flex;align-items:center;justify-content:center;"
    "font-family:var(--mono);font-size:10px;flex-shrink:0}"
    ".userbox .nm{font-size:10.5px;font-weight:500}"
    ".userbox .em{font-size:10px;color:var(--muted)}"
    ".main{flex:1;min-width:0;display:flex;flex-direction:column}"
    ".topbar{height:49px;border-bottom:1px solid var(--border);display:flex;"
    "align-items:center;justify-content:flex-end;gap:12px;padding:0 21px}"
    ".content{padding:21px;display:flex;flex-direction:column;gap:21px}"
    "h1{font-size:15.75px;font-weight:600;margin:0}"
    ".sub{font-family:var(--mono);font-size:10.5px;color:var(--muted);margin:4px 0 0}"
    ".cards{display:flex;flex-wrap:wrap;gap:14px}"
    ".metric{flex:1 1 220px;min-width:0;background:var(--card);"
    "border:1px solid var(--border);border-radius:7px;padding:18px}"
    ".k{font-family:var(--mono);font-size:10.5px;letter-spacing:1px;"
    "text-transform:uppercase;color:var(--muted)}"
    ".metric .v{font-family:var(--mono);font-size:21px;line-height:28px;"
    "margin-top:10px;word-break:break-all}"
    ".grid{display:flex;flex-wrap:wrap;gap:21px}"
    ".col{flex:1 1 340px;max-width:500px;min-width:0;display:flex;"
    "flex-direction:column;gap:21px}"
    ".card{background:var(--card);border:1px solid var(--border);border-radius:7px;"
    "padding:17px;display:flex;flex-direction:column;gap:16px}"
    ".row{display:flex;gap:14px;align-items:center;justify-content:space-between}"
    ".row.col2{flex-direction:column;align-items:stretch}"
    ".lbl{font-weight:500;font-size:12.25px}"
    ".desc{font-size:10.5px;color:var(--muted);margin-top:3px}"
    ".val{font-family:var(--mono);font-size:10.5px;color:var(--accent)}"
    "input[type=text],input[type=password]{width:100%;background:var(--inset);"
    "border:1px solid var(--border);border-radius:5px;padding:7px 10px;"
    "color:var(--text);font-family:var(--mono);font-size:11px}"
    "input[readonly]{color:var(--muted)}"
    ".field{flex:1 1 auto;min-width:0}"
    ".btn{font-family:var(--mono);font-size:10.5px;letter-spacing:1px;"
    "text-transform:uppercase;border-radius:5px;padding:7px 11px;cursor:pointer;"
    "border:1px solid var(--border);background:var(--inset);color:var(--text)}"
    ".btn.scan{background:var(--card);border-color:var(--accent);color:var(--accent)}"
    ".btn.primary{background:var(--accent);color:var(--on-accent);border:0}"
    ".btn.danger{background:var(--danger);color:#fff;border:0;text-transform:none;"
    "letter-spacing:0}"
    ".notice{background:var(--accent-bg);border:1px solid var(--accent);"
    "border-radius:7px;padding:12px 14px;font-size:12px}"
    ".warn{background:var(--card);border:1px solid var(--danger);border-radius:7px;"
    "padding:14px 16px;font-size:13px;line-height:1.5}"
    ".sw{width:28px;height:14px;border-radius:999px;background:var(--accent);"
    "border:0;position:relative;cursor:pointer;padding:0;flex-shrink:0}"
    ".sw::after{content:'';position:absolute;top:2px;left:2px;width:10px;"
    "height:10px;border-radius:999px;background:var(--on-accent);transition:.15s}"
    ":root[data-theme=light] .sw.theme::after{left:16px}"
    ".sw.off{background:var(--inset);border:1px solid var(--border)}"
    ".sw.off::after{background:var(--muted)}"
    ".themebox{display:flex;gap:10px;align-items:center}"
    ".themebox span{font-size:10.5px;color:var(--muted)}"
    ".scanlist{display:flex;flex-direction:column;gap:6px;max-height:160px;"
    "overflow:auto}"
    ".scanlist button{text-align:left;display:flex;justify-content:space-between;"
    "gap:8px;width:100%;background:var(--inset);border:1px solid var(--border);"
    "border-radius:5px;padding:7px 10px;color:var(--text);cursor:pointer;"
    "font-family:var(--mono);font-size:10.5px}"
    ".indent{border-left:1px solid var(--soft);padding-left:14px;display:flex;"
    "flex-direction:column;gap:10px}"
    ".bright{display:flex;gap:10px;align-items:center}"
    "input[type=range]{accent-color:var(--accent);flex:1 1 auto;min-width:0}"
    "output{font-family:var(--mono);font-size:10.5px;min-width:34px;text-align:right}"
    ".center{max-width:34rem;margin:8vh auto;padding:0 16px}"
    "@media(max-width:640px){.app{flex-direction:column}"
    ".sidebar{width:100%;flex-direction:row;flex-wrap:wrap;align-items:center}"
    ".nav{flex-direction:row;flex-wrap:wrap;gap:6px}.nav .sec{display:none}"
    ".userbox{display:none}.content{padding:16px}}"
    "</style>";

/* Theme bootstrap (applies saved theme before paint) + toggle, in <head>. */
static const char STA_THEME[] =
    "<script>"
    "(function(){try{if(localStorage.getItem('pov-theme')==='light')"
    "document.documentElement.setAttribute('data-theme','light');}catch(e){}})();"
    "function povTheme(){var d=document.documentElement,"
    "l=d.getAttribute('data-theme')==='light';"
    "d.setAttribute('data-theme',l?'dark':'light');"
    "try{localStorage.setItem('pov-theme',l?'dark':'light');}catch(e){}}"
    "</script>";

/* Inline SVG icons (inherit currentColor; no external assets). */
#define SVG_BOLT \
    "<svg viewBox=\"0 0 24 24\" width=\"13\" height=\"13\" fill=\"#06201c\">" \
    "<path d=\"M13 2 4 14h6l-1 8 9-12h-6z\"/></svg>"
#define SVG_GRID \
    "<svg class=\"ico\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" " \
    "stroke-width=\"2\"><rect x=\"3\" y=\"3\" width=\"7\" height=\"7\" rx=\"1\"/>" \
    "<rect x=\"14\" y=\"3\" width=\"7\" height=\"7\" rx=\"1\"/>" \
    "<rect x=\"3\" y=\"14\" width=\"7\" height=\"7\" rx=\"1\"/>" \
    "<rect x=\"14\" y=\"14\" width=\"7\" height=\"7\" rx=\"1\"/></svg>"
#define SVG_GEAR \
    "<svg class=\"ico\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" " \
    "stroke-width=\"2\"><circle cx=\"12\" cy=\"12\" r=\"3\"/>" \
    "<path d=\"M19.4 15a1.7 1.7 0 0 0 .3 1.9l.1.1a2 2 0 1 1-2.8 2.8l-.1-.1a1.7 " \
    "1.7 0 0 0-2.9 1.2V21a2 2 0 1 1-4 0v-.1A1.7 1.7 0 0 0 7 19.4a1.7 1.7 0 0 0-1.9.3l-.1.1" \
    "a2 2 0 1 1-2.8-2.8l.1-.1a1.7 1.7 0 0 0-1.2-2.9H1a2 2 0 1 1 0-4h.1A1.7 1.7 0 0 0 " \
    "2.6 7a1.7 1.7 0 0 0-.3-1.9l-.1-.1a2 2 0 1 1 2.8-2.8l.1.1A1.7 1.7 0 0 0 9 2.6 " \
    "1.7 1.7 0 0 0 10 1V1a2 2 0 1 1 4 0v.1a1.7 1.7 0 0 0 2.9 1.2 1.7 1.7 0 0 0 " \
    "1.9-.3l.1-.1a2 2 0 1 1 2.8 2.8l-.1.1a1.7 1.7 0 0 0 1.2 2.9H23a2 2 0 1 1 0 4h-.1" \
    "a1.7 1.7 0 0 0-1.5 1z\"/></svg>"
#define SVG_BELL \
    "<svg class=\"ico\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" " \
    "stroke-width=\"2\"><path d=\"M18 8a6 6 0 1 0-12 0c0 7-3 9-3 9h18s-3-2-3-9\"/>" \
    "<path d=\"M13.7 21a2 2 0 0 1-3.4 0\"/></svg>"

/* Append helper for incremental page building. Returns -1 on overflow. */
#define STA_APPEND(...)                                                       \
    do {                                                                      \
        int _w = snprintf(buf + off, buflen - off, __VA_ARGS__);             \
        if (_w < 0 || (size_t)_w >= buflen - off) return -1;                  \
        off += (size_t)_w;                                                    \
    } while (0)

/* ----- shared shell ------------------------------------------------- */

/* Opens <html>…<div class="content">. `active` is "overview" or "settings". */
static int shell_open(char *buf, size_t buflen, size_t off_in,
                      const char *title, const char *active) {
    size_t off = off_in;
    bool ov = (active && strcmp(active, "overview") == 0);
    bool se = (active && strcmp(active, "settings") == 0);

    STA_APPEND(
        "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>%s</title>", title);
    STA_APPEND("%s%s</head><body><div class=\"app\">", STA_THEME, STA_STYLE);

    /* Sidebar */
    STA_APPEND(
        "<div class=\"sidebar\">"
        "<div class=\"brand\"><span class=\"logo\">" SVG_BOLT "</span>"
        "<b>POV Display</b></div>"
        "<nav class=\"nav\">"
        "<div class=\"sec\">Main</div>"
        "<a class=\"%s\" href=\"/\">" SVG_GRID "Overview</a>"
        "<div class=\"sec\">System</div>"
        "<a class=\"%s\" href=\"/settings\">" SVG_GEAR "Settings</a>"
        "</nav>"
        "<div class=\"userbox\"><span class=\"avatar\">PD</span>"
        "<div style=\"flex:1;min-width:0\"><div class=\"nm\">POV Display</div>"
        "<div class=\"em\">device portal</div></div></div>"
        "</div>",
        ov ? "active" : "", se ? "active" : "");

    /* Main + topbar */
    STA_APPEND(
        "<div class=\"main\"><div class=\"topbar\">" SVG_BELL
        "<span class=\"avatar\">PD</span></div>"
        "<div class=\"content\">");
    return (int)off;
}

static int shell_close(char *buf, size_t buflen, size_t off_in) {
    size_t off = off_in;
    STA_APPEND("</div></div></div></body></html>");
    return (int)off;
}

/* ----- Overview (GET /) -------------------------------------------- */

int wifi_sta_web_build_status_page(char *buf, size_t buflen,
                                   const char *ssid,
                                   const char *ip,
                                   const char *connectivity_state,
                                   bool blink_active,
                                   bool clock_available,
                                   const char *clock_text,
                                   bool rotation_speed_available,
                                   uint32_t rotation_speed_rpm,
                                   const char *notice) {
    int r = shell_open(buf, buflen, 0, "POV Display", "overview");
    if (r < 0) return -1;
    size_t off = (size_t)r;

    STA_APPEND(
        "<div><h1>Overview</h1><p class=\"sub\">Device status</p></div>");

    if (notice && notice[0]) {
        STA_APPEND("<div class=\"notice\">%s</div>", notice);
    }

    STA_APPEND("<div class=\"cards\">");
    STA_APPEND(
        "<div class=\"metric\"><div class=\"k\">Status</div>"
        "<div class=\"v\">%s</div></div>",
        connectivity_state ? connectivity_state : "unknown");
    STA_APPEND(
        "<div class=\"metric\"><div class=\"k\">Network (SSID)</div>"
        "<div class=\"v\">%s</div></div>",
        (ssid && ssid[0]) ? ssid : "(none)");
    STA_APPEND(
        "<div class=\"metric\"><div class=\"k\">IP Address</div>"
        "<div class=\"v\">%s</div></div>",
        (ip && ip[0]) ? ip : "0.0.0.0");
    STA_APPEND(
        "<div class=\"metric\"><div class=\"k\">Current Clock</div>"
        "<div class=\"v\">%s CST</div></div>",
        (clock_available && clock_text && clock_text[0])
            ? clock_text : "--:--:--");
    STA_APPEND(
        "<div class=\"metric\"><div class=\"k\">Blink</div>"
        "<div class=\"v\">%s</div></div>",
        blink_active ? "Active" : "Idle");
    if (rotation_speed_available) {
        STA_APPEND(
            "<div class=\"metric\"><div class=\"k\">Rotation Speed</div>"
            "<div class=\"v\">%u RPM</div></div>",
            (unsigned)rotation_speed_rpm);
    } else {
        STA_APPEND(
            "<div class=\"metric\"><div class=\"k\">Rotation Speed</div>"
            "<div class=\"v\">-- RPM</div></div>");
    }
    STA_APPEND("</div>");

    r = shell_close(buf, buflen, off);
    if (r < 0) return -1;
    return r;
}

/* ----- Settings (GET /settings) ------------------------------------ */

int wifi_sta_web_build_settings_page(char *buf, size_t buflen,
                                     const char *current_ssid,
                                     const char *ip,
                                     const scan_result_t *results,
                                     int n_results,
                                     uint8_t brightness,
                                     bool reboot_available,
                                     const char *notice) {
    int r = shell_open(buf, buflen, 0, "Settings", "settings");
    if (r < 0) return -1;
    size_t off = (size_t)r;

    if (brightness > 100) brightness = 100;

    /* Derive a read-only gateway from the active IP (x.y.z.1) for fidelity. */
    char gw[16];
    {
        const char *src = (ip && ip[0]) ? ip : "0.0.0.0";
        strncpy(gw, src, sizeof(gw) - 1);
        gw[sizeof(gw) - 1] = '\0';
        char *dot = strrchr(gw, '.');
        if (dot && (size_t)(dot - gw) + 2 < sizeof(gw)) {
            dot[1] = '1';
            dot[2] = '\0';
        }
    }

    STA_APPEND(
        "<div><h1>Settings</h1>"
        "<p class=\"sub\">System &amp; Identity Management</p></div>");

    if (notice && notice[0]) {
        STA_APPEND("<div class=\"notice\">%s</div>", notice);
    }

    STA_APPEND("<div class=\"grid\">");

    /* ---- Column 1: Display + System ---- */
    STA_APPEND("<div class=\"col\">");

    /* Display card */
    STA_APPEND(
        "<div class=\"card\"><div class=\"k\">Display</div>"
        "<div class=\"row\"><div><div class=\"lbl\">Theme</div>"
        "<div class=\"desc\">Switch between dark and light monitoring "
        "interfaces</div></div>"
        "<div class=\"themebox\"><span>Dark</span>"
        "<button type=\"button\" class=\"sw theme\" onclick=\"povTheme()\" "
        "aria-label=\"Toggle theme\"></button><span>Light</span></div></div>"
        "<form class=\"row col2\" action=\"/display\" method=\"POST\">"
        "<div class=\"lbl\">Brightness</div>"
        "<div class=\"desc\">Adjust panel luminosity for late-night shifts</div>"
        "<div class=\"bright\">"
        "<input type=\"range\" name=\"brightness\" min=\"0\" max=\"100\" "
        "value=\"%u\" oninput=\"bv.value=this.value+'%%'\">"
        "<output id=\"bv\">%u%%</output>"
        "<button class=\"btn primary\" type=\"submit\">Set</button>"
        "</div></form>"
        "</div>",
        (unsigned)brightness, (unsigned)brightness);

    /* System card */
    STA_APPEND(
        "<div class=\"card\"><div class=\"k\">System</div>"
        "<div class=\"row\"><div class=\"lbl\">Firmware Version</div>"
        "<div class=\"val\">%s</div></div>"
        "<div class=\"row\"><a class=\"btn danger\" href=\"/update\">Update Firmware (USB)</a></div>"
        "<div class=\"row\"><a class=\"btn primary\" href=\"/ota\">Update Firmware (OTA)</a></div>",
        WIFI_STA_FW_VERSION);
    if (reboot_available) {
        STA_APPEND("<div class=\"row\"><a class=\"btn\" href=\"/reboot\">Reboot</a><div class=\"desc\">Restart normally; display and network pause briefly.</div></div>");
    } else {
        STA_APPEND("<div class=\"row\"><button class=\"btn\" type=\"button\" disabled>Reboot</button><div class=\"desc\">Firmware update or restart is in progress.</div></div>");
    }
    STA_APPEND("</div>"); /* end col 1 */

    /* ---- Column 2: Network ---- */
    STA_APPEND("<div class=\"col\">");
    STA_APPEND(
        "<div class=\"card\"><div class=\"k\">Network</div>"
        "<form action=\"/config\" method=\"POST\">"
        "<div class=\"row col2\"><div class=\"lbl\">SSID</div>"
        "<div style=\"display:flex;gap:10px;align-items:center\">"
        "<input class=\"field\" id=\"ssid\" name=\"ssid\" type=\"text\" "
        "maxlength=\"32\" autocomplete=\"off\" placeholder=\"%s\">"
        "<a class=\"btn scan\" href=\"/settings?scan=1\">Scan</a></div></div>",
        (current_ssid && current_ssid[0]) ? current_ssid : "Wi-Fi network");

    if (results && n_results > 0) {
        STA_APPEND("<div class=\"scanlist\">");
        for (int i = 0; i < n_results; i++) {
            const char *s = results[i].ssid;
            if (!s || !s[0]) continue;
            STA_APPEND(
                "<button type=\"button\" "
                "onclick=\"document.getElementById('ssid').value='%s'\">"
                "<span>%s</span><span>%s</span></button>",
                s, s, results[i].secured ? "&#128274;" : "open");
        }
        STA_APPEND("</div>");
    }

    STA_APPEND(
        "<div class=\"row col2\"><div class=\"lbl\">Password</div>"
        "<input id=\"pw\" name=\"password\" type=\"password\" maxlength=\"63\" "
        "autocomplete=\"off\" placeholder=\"WPA2 password (8-63 chars)\"></div>"
        "<div class=\"row\"><div><div class=\"lbl\">Static IP</div>"
        "<div class=\"desc\">Assign a persistent network address</div></div>"
        "<span class=\"sw off\" aria-disabled=\"true\"></span></div>"
        "<div class=\"indent\">"
        "<div class=\"row\"><div class=\"lbl\">IP Address</div>"
        "<input type=\"text\" class=\"field\" style=\"max-width:180px\" "
        "value=\"%s\" readonly></div>"
        "<div class=\"row\"><div class=\"lbl\">Subnet Mask</div>"
        "<input type=\"text\" class=\"field\" style=\"max-width:180px\" "
        "value=\"255.255.255.0\" readonly></div>"
        "<div class=\"row\"><div class=\"lbl\">Gateway</div>"
        "<input type=\"text\" class=\"field\" style=\"max-width:180px\" "
        "value=\"%s\" readonly></div>"
        "</div>"
        "<div class=\"row\" style=\"margin-top:4px\">"
        "<button class=\"btn primary\" type=\"submit\">Connect</button></div>"
        "</form>"
        "<p class=\"desc\">Open networks are not supported; a WPA2 password "
        "(8&ndash;63 characters) is required.</p>"
        "</div>",
        (ip && ip[0]) ? ip : "0.0.0.0", gw);

    STA_APPEND("</div>"); /* end col 2 */
    STA_APPEND("</div>"); /* end grid */

    r = shell_close(buf, buflen, off);
    if (r < 0) return -1;
    return r;
}

/* ----- transitional pages ------------------------------------------ */

int wifi_sta_web_build_applying_page(char *buf, size_t buflen,
                                     const char *target_ssid) {
    int r = shell_open(buf, buflen, 0, "Applying", "settings");
    if (r < 0) return -1;
    size_t off = (size_t)r;

    STA_APPEND(
        "<div><h1>Switching Wi-Fi network&hellip;</h1></div>"
        "<div class=\"warn\">"
        "<p>The device is attempting to join <strong>%s</strong>. Its current "
        "connection will drop during the switch.</p>"
        "<p><strong>Reconnect your device to that network</strong> and open the "
        "device page to confirm.</p>"
        "<p>If the new network cannot be joined, the device returns to its "
        "previous network &mdash; reconnect there to retry.</p>"
        "</div>",
        (target_ssid && target_ssid[0]) ? target_ssid : "(unknown)");

    r = shell_close(buf, buflen, off);
    if (r < 0) return -1;
    return r;
}

int wifi_sta_web_build_update_page(char *buf, size_t buflen) {
    int r = shell_open(buf, buflen, 0, "Confirm update", "settings");
    if (r < 0) return -1;
    size_t off = (size_t)r;

    STA_APPEND(
        "<div><h1>Firmware update</h1></div>"
        "<div class=\"warn\">"
        "<p><strong>Warning:</strong> the device will reboot into USB drive "
        "mode. A removable drive will appear on your computer &mdash; drag a "
        "new <code>.uf2</code> firmware file onto it to update.</p>"
        "<p>Returning to normal mode in <span id=\"c\">60</span> s if no "
        "action is taken.</p></div>"
        "<form action=\"/update\" method=\"POST\" "
        "style=\"display:flex;gap:12px;align-items:center\">"
        "<button class=\"btn danger\" type=\"submit\">Confirm update</button>"
        "<a class=\"btn\" href=\"/\">Cancel</a></form>"
        "<script>var n=60,e=document.getElementById('c');"
        "setInterval(function(){n--;if(e)e.textContent=n;"
        "if(n<=0)location.href='/';},1000);</script>");

    r = shell_close(buf, buflen, off);
    if (r < 0) return -1;
    return r;
}

int wifi_sta_web_build_rebooting_page(char *buf, size_t buflen) {
    size_t off = 0;
    STA_APPEND(
        "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>Rebooting</title>");
    STA_APPEND("%s%s</head><body><div class=\"center\">", STA_THEME, STA_STYLE);
    STA_APPEND(
        "<h1>Rebooting into update mode&hellip;</h1>"
        "<div class=\"warn\"><p>A USB drive should appear on your computer "
        "shortly. Drop a new <code>.uf2</code> file onto it to flash new "
        "firmware.</p>"
        "<p>Your saved Wi-Fi network will be remembered after the update.</p>"
        "</div></div></body></html>");
    return (int)off;
}

int wifi_sta_web_build_reboot_page(char *buf, size_t buflen) {
    int r = shell_open(buf, buflen, 0, "Confirm reboot", "settings");
    if (r < 0) return -1;
    size_t off = (size_t)r;
    STA_APPEND("<div><h1>Restart board</h1></div><div class=\"warn\"><p><strong>Warning:</strong> display and network access will pause briefly.</p><p>The board restarts in normal operating mode. Saved Wi-Fi and display settings are preserved.</p></div><form action=\"/reboot\" method=\"POST\" style=\"display:flex;gap:12px\"><button class=\"btn danger\" type=\"submit\">Confirm reboot</button><a class=\"btn\" href=\"/settings\">Cancel</a></form>");
    return shell_close(buf, buflen, off);
}

int wifi_sta_web_build_restart_accepted_page(char *buf, size_t buflen) {
    int r = shell_open(buf, buflen, 0, "Restart accepted", "settings");
    if (r < 0) return -1;
    size_t off = (size_t)r;
    STA_APPEND("<div><h1>Restart accepted</h1></div><div class=\"warn\"><p>The board is restarting normally. This connection will close shortly.</p><p>Reopen the management address after startup; saved settings are preserved.</p></div>");
    return shell_close(buf, buflen, off);
}

int wifi_sta_web_build_reboot_unavailable_page(char *buf, size_t buflen) {
    int r = shell_open(buf, buflen, 0, "Reboot unavailable", "settings");
    if (r < 0) return -1;
    size_t off = (size_t)r;
    STA_APPEND("<div><h1>Reboot unavailable</h1></div><div class=\"warn\"><p>A firmware update or another restart is in progress. Wait for it to finish before rebooting.</p><a class=\"btn\" href=\"/settings\">Back to Settings</a></div>");
    return shell_close(buf, buflen, off);
}

int wifi_sta_web_build_ota_page(char *buf, size_t buflen) {
    int r = shell_open(buf, buflen, 0, "WiFi OTA", "settings");
    if (r < 0) return -1;
    size_t off = (size_t)r;
    STA_APPEND("<div><h1>Update Firmware (WIFI)</h1><p class=\"sub\">Secure local update · compatible .povota package</p></div>"
        "<div class=\"warn\"><p><strong>Restart required.</strong> A validated update briefly pauses display and network availability.</p>"
        "<p>Need recovery or first-time migration? Use <a href=\"/update\">Update Firmware (USB)</a>.</p></div>"
        "<div class=\"card\" style=\"max-width:620px;padding:24px\"><div class=\"k\">Firmware package</div>"
        "<input id=\"f\" class=\"field\" type=\"file\" accept=\".povota\" style=\"margin:12px 0\">"
        "<div style=\"display:flex;justify-content:space-between;align-items:center;margin:16px 0 7px\"><span id=\"stage\" class=\"lbl\">Ready to upload</span><b id=\"pct\" class=\"val\">0%</b></div>"
        "<div style=\"height:10px;background:var(--inset);border-radius:99px;overflow:hidden;border:1px solid var(--border)\"><div id=\"bar\" style=\"height:100%;width:0%;background:var(--accent);transition:width .25s ease\"></div></div>"
        "<p id=\"s\" class=\"desc\" style=\"min-height:20px;margin:10px 0 18px\">Choose a package to begin.</p>"
        "<button id=\"u\" class=\"btn danger\" type=\"button\" onclick=\"ota()\">Start WiFi update</button></div>"
        "<script>function show(x){var e=+x.expected_bytes||0,r=+x.received_bytes||0,p=e?Math.min(100,Math.round(r*100/e)):0;bar.style.width=p+'%';pct.textContent=p+'%';stage.textContent=x.state==='ready'?'Restarting':x.state==='validating'?'Validating package':p?'Uploading firmware':'Ready to upload';s.textContent=x.message+(e?' · '+r+' / '+e+' bytes':'');if(x.state==='ready'||x.state==='failed')u.disabled=x.state==='ready';}"
        "function st(){fetch('/ota/status').then(r=>r.json()).then(show).catch(()=>{});}"
        "function ota(){var x=f.files[0];if(!x){s.textContent='Choose a .povota package.';return;}u.disabled=true;stage.textContent='Uploading firmware';s.textContent='Preparing upload…';var q=new XMLHttpRequest();q.open('POST','/ota');q.setRequestHeader('Content-Type','application/octet-stream');q.upload.onprogress=function(e){if(e.lengthComputable){var p=Math.round(e.loaded*100/e.total);bar.style.width=p+'%';pct.textContent=p+'%';s.textContent='Uploading firmware · '+e.loaded+' / '+e.total+' bytes';}};q.onload=function(){try{show(JSON.parse(q.responseText));}catch(e){s.textContent='Upload failed. USB recovery remains available.';u.disabled=false;}st();};q.onerror=function(){s.textContent='Upload interrupted. USB recovery remains available.';u.disabled=false;};q.send(x);}setInterval(st,2000);st();</script>");
    r = shell_close(buf, buflen, off);
    return r < 0 ? -1 : r;
}

/* ----- status JSON (unchanged) ------------------------------------- */

int wifi_sta_web_build_status_json(char *buf, size_t buflen,
                                   const char *ip,
                                   const char *connectivity_state,
                                   bool blink_active) {
    int n = snprintf(buf, buflen,
        "{\"wifi\":{\"state\":\"%s\",\"ip\":\"%s\"},"
        "\"blink\":{\"active\":%s}}",
        connectivity_state ? connectivity_state : "unknown",
        ip ? ip : "", blink_active ? "true" : "false");
    if (n < 0 || (size_t)n >= buflen) return -1;
    return n;
}
#undef STA_APPEND
