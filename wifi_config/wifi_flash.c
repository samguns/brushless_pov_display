#include "wifi_flash.h"

#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#include "pov_log.h"
#include "pov_rotation_config.h"

#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"

/* Flash sector offset from the start of flash (last 4 KB sector). */
#define WIFI_FLASH_OFFSET   (PICO_FLASH_SIZE_BYTES - WIFI_FLASH_SECTOR_SIZE)

/* XIP base address of the credential sector */
#define WIFI_XIP_BASE       (XIP_BASE + WIFI_FLASH_OFFSET)

typedef struct __attribute__((packed)) {
    uint32_t magic;
    char     ssid[33];
    char     password[64];
    uint8_t  flags;
    uint32_t crc32;
} wifi_flash_record_v1_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t  version;
    char     ssid[33];
    char     password[64];
    char     admin_token[64];
    uint8_t  flags;
    uint32_t crc32;
} wifi_flash_record_v2_t;

/* V3 (legacy read format): V2 + a persisted display-brightness byte. */
typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t  version;
    char     ssid[33];
    char     password[64];
    char     admin_token[64];
    uint8_t  brightness;   /* display brightness percent, 0..100 */
    uint8_t  flags;
    uint32_t crc32;
} wifi_flash_record_v3_t;

/* V4 (current write format): V3 + persisted nominal angular speed. */
typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t  version;
    char     ssid[33];
    char     password[64];
    char     admin_token[64];
    uint8_t  brightness;
    uint16_t nominal_rad_s_x100;
    uint8_t  flags;
    uint32_t crc32;
} wifi_flash_record_v4_t;

_Static_assert(sizeof(wifi_flash_record_v1_t) == 106,
    "wifi_flash_record_v1_t size changed unexpectedly");
_Static_assert(sizeof(wifi_flash_record_v2_t) == 171,
    "wifi_flash_record_v2_t size changed unexpectedly");
_Static_assert(sizeof(wifi_flash_record_v3_t) == 172,
    "wifi_flash_record_v3_t size changed unexpectedly");
_Static_assert(sizeof(wifi_flash_record_v4_t) == 174,
    "wifi_flash_record_v4_t size changed unexpectedly");

/* --------------------------------------------------------------------
 * CRC-32 (ISO 3309 / IEEE 802.3 polynomial 0xEDB88320, bit-reverse)
 * -------------------------------------------------------------------- */
static uint32_t crc32_compute(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFFUL;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc >> 1) ^ (0xEDB88320UL & (uint32_t)(-(int32_t)(crc & 1u)));
        }
    }
    return crc ^ 0xFFFFFFFFUL;
}

/* ------------------------------------------------------------------ */

bool load_credentials(wifi_credentials_t *out) {
    const uint32_t magic = *(const uint32_t *)WIFI_XIP_BASE;
    memset(out, 0, sizeof(*out));

    if (magic == WIFI_FLASH_MAGIC_V4) {
        const wifi_flash_record_v4_t *rec =
            (const wifi_flash_record_v4_t *)WIFI_XIP_BASE;
        uint32_t expected = crc32_compute((const uint8_t *)rec,
                                          offsetof(wifi_flash_record_v4_t, crc32));
        if (rec->version != WIFI_FLASH_VERSION_V4 || rec->crc32 != expected) {
            pov_logf(POV_LOG_SOURCE_WIFI_FLASH,
                     "invalid V4 record (version=%u, crc=0x%08X/0x%08X)\n",
                     (unsigned)rec->version, rec->crc32, expected);
            return false;
        }
        if (rec->ssid[0] == '\0') {
            pov_logf(POV_LOG_SOURCE_WIFI_FLASH, "empty SSID in V4 record\n");
            return false;
        }
        strncpy(out->ssid, rec->ssid, WIFI_SSID_MAX_LEN);
        out->ssid[WIFI_SSID_MAX_LEN] = '\0';
        strncpy(out->password, rec->password, WIFI_PASS_MAX_LEN);
        out->password[WIFI_PASS_MAX_LEN] = '\0';
        strncpy(out->admin_token, rec->admin_token, WIFI_ADMIN_TOKEN_MAX_LEN);
        out->admin_token[WIFI_ADMIN_TOKEN_MAX_LEN] = '\0';
        pov_logf(POV_LOG_SOURCE_WIFI_FLASH,
                 "loaded V4 credentials for SSID: %s (brightness=%u rad_s_x100=%u)\n",
                 out->ssid, (unsigned)rec->brightness,
                 (unsigned)rec->nominal_rad_s_x100);
        return true;
    }

    if (magic == WIFI_FLASH_MAGIC_V3) {
        const wifi_flash_record_v3_t *rec = (const wifi_flash_record_v3_t *)WIFI_XIP_BASE;
        uint32_t expected = crc32_compute((const uint8_t *)rec,
                                          offsetof(wifi_flash_record_v3_t, crc32));
        if (rec->version != WIFI_FLASH_VERSION_V3 || rec->crc32 != expected) {
            pov_logf(POV_LOG_SOURCE_WIFI_FLASH, "invalid V3 record (version=%u, crc=0x%08X/0x%08X)\n",
                   (unsigned)rec->version, rec->crc32, expected);
            return false;
        }
        if (rec->ssid[0] == '\0') {
            pov_logf(POV_LOG_SOURCE_WIFI_FLASH, "empty SSID in V3 record\n");
            return false;
        }
        strncpy(out->ssid, rec->ssid, WIFI_SSID_MAX_LEN);
        out->ssid[WIFI_SSID_MAX_LEN] = '\0';
        strncpy(out->password, rec->password, WIFI_PASS_MAX_LEN);
        out->password[WIFI_PASS_MAX_LEN] = '\0';
        strncpy(out->admin_token, rec->admin_token, WIFI_ADMIN_TOKEN_MAX_LEN);
        out->admin_token[WIFI_ADMIN_TOKEN_MAX_LEN] = '\0';
        pov_logf(POV_LOG_SOURCE_WIFI_FLASH, "loaded V3 credentials for SSID: %s (brightness=%u)\n",
               out->ssid, (unsigned)rec->brightness);
        return true;
    }

    if (magic == WIFI_FLASH_MAGIC_V2) {
        const wifi_flash_record_v2_t *rec = (const wifi_flash_record_v2_t *)WIFI_XIP_BASE;
        uint32_t expected = crc32_compute((const uint8_t *)rec,
                                          offsetof(wifi_flash_record_v2_t, crc32));
        if (rec->version != WIFI_FLASH_VERSION_V2 || rec->crc32 != expected) {
            pov_logf(POV_LOG_SOURCE_WIFI_FLASH, "invalid V2 record (version=%u, crc=0x%08X/0x%08X)\n",
                   (unsigned)rec->version, rec->crc32, expected);
            return false;
        }
        if (rec->ssid[0] == '\0') {
            pov_logf(POV_LOG_SOURCE_WIFI_FLASH, "empty SSID in V2 record\n");
            return false;
        }

        strncpy(out->ssid, rec->ssid, WIFI_SSID_MAX_LEN);
        out->ssid[WIFI_SSID_MAX_LEN] = '\0';
        strncpy(out->password, rec->password, WIFI_PASS_MAX_LEN);
        out->password[WIFI_PASS_MAX_LEN] = '\0';
        strncpy(out->admin_token, rec->admin_token, WIFI_ADMIN_TOKEN_MAX_LEN);
        out->admin_token[WIFI_ADMIN_TOKEN_MAX_LEN] = '\0';
        pov_logf(POV_LOG_SOURCE_WIFI_FLASH, "loaded V2 credentials for SSID: %s\n", out->ssid);
        return true;
    }

    if (magic == WIFI_FLASH_MAGIC_V1) {
        const wifi_flash_record_v1_t *legacy = (const wifi_flash_record_v1_t *)WIFI_XIP_BASE;
        uint32_t expected = crc32_compute((const uint8_t *)legacy,
                                          offsetof(wifi_flash_record_v1_t, crc32));
        if (legacy->crc32 != expected || legacy->ssid[0] == '\0') {
            pov_logf(POV_LOG_SOURCE_WIFI_FLASH, "invalid V1 record\n");
            return false;
        }
        strncpy(out->ssid, legacy->ssid, WIFI_SSID_MAX_LEN);
        out->ssid[WIFI_SSID_MAX_LEN] = '\0';
        strncpy(out->password, legacy->password, WIFI_PASS_MAX_LEN);
        out->password[WIFI_PASS_MAX_LEN] = '\0';
        out->admin_token[0] = '\0';
        pov_logf(POV_LOG_SOURCE_WIFI_FLASH, "loaded legacy credentials for SSID: %s\n", out->ssid);
        return true;
    }

    pov_logf(POV_LOG_SOURCE_WIFI_FLASH, "no valid magic (0x%08X)\n", magic);
    return false;
}

/* Read the persisted brightness from V3/V4, or return the default. */
static uint8_t read_stored_brightness(void) {
    const uint32_t magic = *(const uint32_t *)WIFI_XIP_BASE;
    if (magic == WIFI_FLASH_MAGIC_V4) {
        const wifi_flash_record_v4_t *rec =
            (const wifi_flash_record_v4_t *)WIFI_XIP_BASE;
        uint32_t expected = crc32_compute((const uint8_t *)rec,
                                          offsetof(wifi_flash_record_v4_t, crc32));
        if (rec->version == WIFI_FLASH_VERSION_V4 && rec->crc32 == expected) {
            return (rec->brightness <= 100u) ? rec->brightness
                                             : WIFI_FLASH_DEFAULT_BRIGHTNESS;
        }
    }
    if (magic == WIFI_FLASH_MAGIC_V3) {
        const wifi_flash_record_v3_t *rec =
            (const wifi_flash_record_v3_t *)WIFI_XIP_BASE;
        uint32_t expected = crc32_compute((const uint8_t *)rec,
                                          offsetof(wifi_flash_record_v3_t, crc32));
        if (rec->version == WIFI_FLASH_VERSION_V3 && rec->crc32 == expected) {
            return (rec->brightness <= 100u) ? rec->brightness
                                             : WIFI_FLASH_DEFAULT_BRIGHTNESS;
        }
    }
    return WIFI_FLASH_DEFAULT_BRIGHTNESS;
}

static uint16_t read_stored_nominal_rad_s_x100(void) {
    const uint32_t magic = *(const uint32_t *)WIFI_XIP_BASE;
    if (magic == WIFI_FLASH_MAGIC_V4) {
        const wifi_flash_record_v4_t *rec =
            (const wifi_flash_record_v4_t *)WIFI_XIP_BASE;
        uint32_t expected = crc32_compute((const uint8_t *)rec,
                                          offsetof(wifi_flash_record_v4_t, crc32));
        if (rec->version == WIFI_FLASH_VERSION_V4 && rec->crc32 == expected &&
            rec->nominal_rad_s_x100 >= POV_ROTATION_MIN_RAD_S_X100 &&
            rec->nominal_rad_s_x100 <= POV_ROTATION_MAX_RAD_S_X100) {
            return rec->nominal_rad_s_x100;
        }
    }
    return POV_ROTATION_DEFAULT_RAD_S_X100;
}

/* Write a complete V4 record (credentials + display settings) with erase+write+
 * verify; interrupts disabled for the flash operation (constitution IV). */
static bool write_record_v4(const wifi_credentials_t *creds, uint8_t brightness,
                            uint16_t nominal_rad_s_x100) {
    /* Static to avoid 4 KB on the stack; flash writes are infrequent. */
    static uint8_t sector_buf[WIFI_FLASH_SECTOR_SIZE];
    memset(sector_buf, 0xFF, sizeof(sector_buf));

    if (brightness > 100u) brightness = 100u;
    if (nominal_rad_s_x100 < POV_ROTATION_MIN_RAD_S_X100 ||
        nominal_rad_s_x100 > POV_ROTATION_MAX_RAD_S_X100) {
        nominal_rad_s_x100 = POV_ROTATION_DEFAULT_RAD_S_X100;
    }

    wifi_flash_record_v4_t *rec = (wifi_flash_record_v4_t *)sector_buf;
    rec->magic = WIFI_FLASH_MAGIC_V4;
    rec->version = WIFI_FLASH_VERSION_V4;
    strncpy(rec->ssid, creds->ssid, 32);
    rec->ssid[32] = '\0';
    strncpy(rec->password, creds->password, 63);
    rec->password[63] = '\0';
    strncpy(rec->admin_token, creds->admin_token, 63);
    rec->admin_token[63] = '\0';
    rec->brightness = brightness;
    rec->nominal_rad_s_x100 = nominal_rad_s_x100;
    rec->flags = 0x00;
    rec->crc32 = crc32_compute(sector_buf,
                               offsetof(wifi_flash_record_v4_t, crc32));

    pov_logf(POV_LOG_SOURCE_WIFI_FLASH,
             "writing V4 record SSID: %s (token_len=%u brightness=%u rad_s_x100=%u)\n",
             creds->ssid, (unsigned)strlen(creds->admin_token),
             (unsigned)brightness, (unsigned)nominal_rad_s_x100);

    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(WIFI_FLASH_OFFSET, WIFI_FLASH_SECTOR_SIZE);
    flash_range_program(WIFI_FLASH_OFFSET, sector_buf, WIFI_FLASH_SECTOR_SIZE);
    restore_interrupts(ints);

    const wifi_flash_record_v4_t *stored =
        (const wifi_flash_record_v4_t *)WIFI_XIP_BASE;
    bool ok = (stored->magic == WIFI_FLASH_MAGIC_V4 &&
               stored->version == WIFI_FLASH_VERSION_V4 &&
               stored->brightness == brightness &&
               stored->nominal_rad_s_x100 == nominal_rad_s_x100 &&
               strncmp(stored->ssid, creds->ssid, 32) == 0 &&
               strncmp(stored->admin_token, creds->admin_token, 63) == 0);

    if (!ok) {
        pov_logf(POV_LOG_SOURCE_WIFI_FLASH, "verify FAILED after write\n");
    } else {
        pov_logf(POV_LOG_SOURCE_WIFI_FLASH, "write verified OK\n");
    }
    return ok;
}

bool save_credentials(const wifi_credentials_t *creds) {
    /* Preserve display settings across a credential change. */
    return write_record_v4(creds, read_stored_brightness(),
                           read_stored_nominal_rad_s_x100());
}

uint8_t load_brightness(void) {
    return read_stored_brightness();
}

bool save_brightness(uint8_t brightness_pct) {
    wifi_credentials_t creds;
    if (!load_credentials(&creds)) {
        pov_logf(POV_LOG_SOURCE_WIFI_FLASH, "save_brightness: no stored credentials\n");
        return false;
    }
    return write_record_v4(&creds, brightness_pct,
                           read_stored_nominal_rad_s_x100());
}

uint16_t load_nominal_rad_s_x100(void) {
    return read_stored_nominal_rad_s_x100();
}

bool save_nominal_rad_s_x100(uint16_t rad_s_x100) {
    if (rad_s_x100 < POV_ROTATION_MIN_RAD_S_X100 ||
        rad_s_x100 > POV_ROTATION_MAX_RAD_S_X100) {
        return false;
    }
    wifi_credentials_t creds;
    if (!load_credentials(&creds)) {
        pov_logf(POV_LOG_SOURCE_WIFI_FLASH,
                 "save nominal rad/s: no stored credentials\n");
        return false;
    }
    return write_record_v4(&creds, read_stored_brightness(), rad_s_x100);
}

void clear_credentials(void) {
    pov_logf(POV_LOG_SOURCE_WIFI_FLASH, "erasing credential sector\n");
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(WIFI_FLASH_OFFSET, WIFI_FLASH_SECTOR_SIZE);
    restore_interrupts(ints);
}
