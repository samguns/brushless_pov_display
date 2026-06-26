#include "wifi_flash.h"

#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"

/* Flash sector offset from the start of flash (last 4 KB sector of 2 MB) */
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

_Static_assert(sizeof(wifi_flash_record_v1_t) == 106,
    "wifi_flash_record_v1_t size changed unexpectedly");
_Static_assert(sizeof(wifi_flash_record_v2_t) == 171,
    "wifi_flash_record_v2_t size changed unexpectedly");

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

    if (magic == WIFI_FLASH_MAGIC_V2) {
        const wifi_flash_record_v2_t *rec = (const wifi_flash_record_v2_t *)WIFI_XIP_BASE;
        uint32_t expected = crc32_compute((const uint8_t *)rec,
                                          offsetof(wifi_flash_record_v2_t, crc32));
        if (rec->version != WIFI_FLASH_VERSION_V2 || rec->crc32 != expected) {
            printf("[wifi_flash] invalid V2 record (version=%u, crc=0x%08X/0x%08X)\n",
                   (unsigned)rec->version, rec->crc32, expected);
            return false;
        }
        if (rec->ssid[0] == '\0') {
            printf("[wifi_flash] empty SSID in V2 record\n");
            return false;
        }

        strncpy(out->ssid, rec->ssid, WIFI_SSID_MAX_LEN);
        out->ssid[WIFI_SSID_MAX_LEN] = '\0';
        strncpy(out->password, rec->password, WIFI_PASS_MAX_LEN);
        out->password[WIFI_PASS_MAX_LEN] = '\0';
        strncpy(out->admin_token, rec->admin_token, WIFI_ADMIN_TOKEN_MAX_LEN);
        out->admin_token[WIFI_ADMIN_TOKEN_MAX_LEN] = '\0';
        printf("[wifi_flash] loaded V2 credentials for SSID: %s\n", out->ssid);
        return true;
    }

    if (magic == WIFI_FLASH_MAGIC_V1) {
        const wifi_flash_record_v1_t *legacy = (const wifi_flash_record_v1_t *)WIFI_XIP_BASE;
        uint32_t expected = crc32_compute((const uint8_t *)legacy,
                                          offsetof(wifi_flash_record_v1_t, crc32));
        if (legacy->crc32 != expected || legacy->ssid[0] == '\0') {
            printf("[wifi_flash] invalid V1 record\n");
            return false;
        }
        strncpy(out->ssid, legacy->ssid, WIFI_SSID_MAX_LEN);
        out->ssid[WIFI_SSID_MAX_LEN] = '\0';
        strncpy(out->password, legacy->password, WIFI_PASS_MAX_LEN);
        out->password[WIFI_PASS_MAX_LEN] = '\0';
        out->admin_token[0] = '\0';
        printf("[wifi_flash] loaded legacy credentials for SSID: %s\n", out->ssid);
        return true;
    }

    printf("[wifi_flash] no valid magic (0x%08X)\n", magic);
    return false;
}

bool save_credentials(const wifi_credentials_t *creds) {
    /* Build the full 4 KB sector image in a static buffer.
     * Static to avoid 4 KB on the stack; only called during provisioning. */
    static uint8_t sector_buf[WIFI_FLASH_SECTOR_SIZE];
    memset(sector_buf, 0xFF, sizeof(sector_buf));

        wifi_flash_record_v2_t *rec = (wifi_flash_record_v2_t *)sector_buf;
        rec->magic = WIFI_FLASH_MAGIC_V2;
        rec->version = WIFI_FLASH_VERSION_V2;
    strncpy(rec->ssid, creds->ssid, 32);
    rec->ssid[32] = '\0';
    strncpy(rec->password, creds->password, 63);
    rec->password[63] = '\0';
        strncpy(rec->admin_token, creds->admin_token, 63);
        rec->admin_token[63] = '\0';
    rec->flags = 0x00;
    rec->crc32 = crc32_compute(sector_buf,
                       offsetof(wifi_flash_record_v2_t, crc32));

        printf("[wifi_flash] writing credentials for SSID: %s (token_len=%u)\n",
            creds->ssid, (unsigned)strlen(creds->admin_token));

    /* Erase + program with interrupts disabled (FR-011, constitution IV) */
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(WIFI_FLASH_OFFSET, WIFI_FLASH_SECTOR_SIZE);
    flash_range_program(WIFI_FLASH_OFFSET, sector_buf, WIFI_FLASH_SECTOR_SIZE);
    restore_interrupts(ints);

    /* Verify write succeeded by re-reading (FR-014) */
    const wifi_flash_record_v2_t *stored =
        (const wifi_flash_record_v2_t *)WIFI_XIP_BASE;
    bool ok = (stored->magic == WIFI_FLASH_MAGIC_V2 &&
               stored->version == WIFI_FLASH_VERSION_V2 &&
               strncmp(stored->ssid, creds->ssid, 32) == 0 &&
               strncmp(stored->admin_token, creds->admin_token, 63) == 0);

    if (!ok) {
        printf("[wifi_flash] verify FAILED after write\n");
    } else {
        printf("[wifi_flash] write verified OK\n");
    }
    return ok;
}

void clear_credentials(void) {
    printf("[wifi_flash] erasing credential sector\n");
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(WIFI_FLASH_OFFSET, WIFI_FLASH_SECTOR_SIZE);
    restore_interrupts(ints);
}
