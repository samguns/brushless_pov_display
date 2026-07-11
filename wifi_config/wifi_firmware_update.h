#ifndef WIFI_FIRMWARE_UPDATE_H
#define WIFI_FIRMWARE_UPDATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WIFI_FW_IDLE, WIFI_FW_RECEIVING, WIFI_FW_VALIDATING, WIFI_FW_READY,
    WIFI_FW_FAILED, WIFI_FW_ROLLED_BACK
} wifi_fw_state_t;

bool wifi_fw_update_begin(uint32_t total_bytes);
bool wifi_fw_update_write(const uint8_t *data, size_t len);
bool wifi_fw_update_finish(void);
void wifi_fw_update_abort(const char *reason);
bool wifi_fw_update_ready(void);
void wifi_fw_update_perform(void);
wifi_fw_state_t wifi_fw_update_state(void);
const char *wifi_fw_update_message(void);
uint32_t wifi_fw_update_received(void);
uint32_t wifi_fw_update_expected(void);
const char *wifi_fw_update_build_id(void);
void wifi_fw_update_boot_status(void);

#ifdef __cplusplus
}
#endif

#endif
