#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "wifi_sta_web.h"

static void require_contains(const char *text, const char *needle) {
    assert(strstr(text, needle) != NULL);
}

static void require_absent(const char *text, const char *needle) {
    assert(strstr(text, needle) == NULL);
}

int main(void) {
    char page[16384];
    char json[256];
    pov_rotation_config_t rotation{};
    assert(pov_rotation_config_derive(4000u, &rotation));

    int n = wifi_sta_web_build_settings_page(page, sizeof(page), "lab", "192.168.1.10",
                                             NULL, 0, 55, rotation, true, NULL);
    assert(n > 0);
    require_contains(page, "href=\"/reboot\"");
    require_contains(page, "Restart normally");
    require_contains(page, "href=\"/logs\"");
    require_contains(page, "action=\"/rotation\" method=\"POST\"");
    require_contains(page, "name=\"rad_s\"");
    require_contains(page, "value=\"40.00\"");
    require_contains(page, "382 RPM, 157080 us/rev, range 306-509 RPM");

    scan_result_t results[WIFI_SCAN_MAX_RESULTS]{};
    for (int i = 0; i < WIFI_SCAN_MAX_RESULTS; ++i) {
        snprintf(results[i].ssid, sizeof(results[i].ssid),
                 "rotation-config-network-%02d", i);
        results[i].rssi = (int16_t)(-30 - i);
        results[i].secured = 1u;
    }
    n = wifi_sta_web_build_settings_page(
        page, sizeof(page), "lab", "192.168.1.10", results,
        WIFI_SCAN_MAX_RESULTS, 55, rotation, true, NULL);
    assert(n > 0 && n < (int)sizeof(page));
    require_contains(page, "rotation-config-network-19");

    n = wifi_sta_web_build_settings_page(page, sizeof(page), "lab", "192.168.1.10",
                                         NULL, 0, 55, rotation, false, NULL);
    assert(n > 0);
    require_contains(page, "Reboot</button>");
    require_contains(page, "Firmware update or restart is in progress.");

    assert(wifi_sta_web_build_reboot_page(page, sizeof(page)) > 0);
    require_contains(page, "action=\"/reboot\" method=\"POST\"");
    require_contains(page, "normal operating mode");
    require_contains(page, "Cancel");

    assert(wifi_sta_web_build_restart_accepted_page(page, sizeof(page)) > 0);
    require_contains(page, "Restart accepted");
    require_contains(page, "restarting normally");

    assert(wifi_sta_web_build_reboot_unavailable_page(page, sizeof(page)) > 0);
    require_contains(page, "Reboot unavailable");

    n = wifi_sta_web_build_status_page(page, sizeof(page), "lab", "192.168.1.10",
                                       "connected", true, true, "12:34:56", true, 600, NULL);
    assert(n > 0);
    require_contains(page, "Blink");
    require_contains(page, "Active");
    require_contains(page, "Current Clock");
    require_contains(page, "Rotation Speed");
    require_contains(page, "href=\"/logs\"");
    require_absent(page, "Blink Frequency");
    require_absent(page, " Hz");

    n = wifi_sta_web_build_status_json(json, sizeof(json), "192.168.1.10", "connected", true);
    assert(n > 0);
    require_contains(json, "\"active\":true");
    require_absent(json, "frequency_hz");

    n = wifi_sta_web_build_ota_page(page, sizeof(page));
    assert(n > 0);
    require_contains(page, "function startOta()");
    require_contains(page, "u.addEventListener('click',startOta)");
    require_contains(page, "Preparing upload...");
    require_contains(page, "p+'%'");
    require_contains(page, "height:100%;width:0%");
    require_absent(page, "%%");
    require_absent(page, "onclick=\"ota()\"");

    assert(wifi_sta_web_build_reboot_page(page, 16) < 0);
    puts("wifi reboot controls tests passed");
    return 0;
}
