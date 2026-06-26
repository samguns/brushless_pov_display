#ifndef WIFI_FLASH_H
#define WIFI_FLASH_H

#include <stdbool.h>
#include "wifi_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Flash layout (last 4 KB sector of 2 MB flash):
 *
 * V1 record (legacy, still readable):
 *   magic + ssid + password + flags + crc32
 *
 * V2 record (current):
 *   magic + version + ssid + password + admin_token + flags + crc32
 *
 * XIP base address: 0x101FF000 (PICO_FLASH_SIZE_BYTES = 2 MB, sector = 4 KB)
 */
#define WIFI_FLASH_MAGIC_V1     0xC0FFEE01UL
#define WIFI_FLASH_MAGIC_V2     0xC0FFEE02UL
#define WIFI_FLASH_VERSION_V2   2u
#define WIFI_FLASH_SECTOR_SIZE  4096u

/*
 * load_credentials() — read and validate a credential record from flash.
 * Returns true and fills *out if a valid record is present; false otherwise.
 */
bool load_credentials(wifi_credentials_t *out);

/*
 * save_credentials() — erase the flash sector and write a new record.
 * Verifies the write by re-reading.
 * Returns true on success, false if the write or verify fails (FR-014).
 * Must be called with no other flash-executing code running; disables
 * interrupts internally for the duration of the erase+write.
 */
bool save_credentials(const wifi_credentials_t *creds);

/*
 * clear_credentials() — erase the flash sector (invalidates stored record).
 */
void clear_credentials(void);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_FLASH_H */
