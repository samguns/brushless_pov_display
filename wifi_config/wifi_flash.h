#ifndef WIFI_FLASH_H
#define WIFI_FLASH_H

#include <stdbool.h>
#include "wifi_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Flash layout (last 4 KB sector of the board flash):
 *
 * V1 record (legacy, still readable):
 *   magic + ssid + password + flags + crc32
 *
 * V2 record (current):
 *   magic + version + ssid + password + admin_token + flags + crc32
 *
 * XIP base address: XIP_BASE + PICO_FLASH_SIZE_BYTES - 4 KB
 */
#define WIFI_FLASH_MAGIC_V1     0xC0FFEE01UL
#define WIFI_FLASH_MAGIC_V2     0xC0FFEE02UL
#define WIFI_FLASH_MAGIC_V3     0xC0FFEE03UL
#define WIFI_FLASH_VERSION_V2   2u
#define WIFI_FLASH_VERSION_V3   3u
#define WIFI_FLASH_SECTOR_SIZE  4096u

/* Default display brightness (percent) when no V3 record is present. */
#define WIFI_FLASH_DEFAULT_BRIGHTNESS  100u

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

/*
 * load_brightness() — read the persisted display brightness (percent, 0..100)
 * from the V3 record. Returns WIFI_FLASH_DEFAULT_BRIGHTNESS if no V3 record is
 * present (e.g. a legacy V1/V2 record or empty sector).
 */
uint8_t load_brightness(void);

/*
 * save_brightness() — persist a new display brightness (percent, 0..100),
 * preserving the currently stored credentials/token (read-modify-write into a
 * V3 record via the same atomic erase+write+verify path as save_credentials).
 * Returns true on success, false if no credentials are stored or the write
 * could not be verified.
 */
bool save_brightness(uint8_t brightness_pct);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_FLASH_H */
