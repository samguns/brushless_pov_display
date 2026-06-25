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
 *   Offset  Size  Field
 *   0       4     magic  (0xC0FFEE01 = valid record)
 *   4       33    ssid   (null-terminated, ≤32 chars)
 *   37      64    password (null-terminated, ≤63 chars)
 *   101     1     flags  (reserved, 0x00)
 *   102     4     crc32  (CRC-32 of bytes 0–101)
 *   106     …     padding (0xFF)
 *
 * XIP base address: 0x101FF000 (PICO_FLASH_SIZE_BYTES = 2 MB, sector = 4 KB)
 */
#define WIFI_FLASH_MAGIC        0xC0FFEE01UL
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
