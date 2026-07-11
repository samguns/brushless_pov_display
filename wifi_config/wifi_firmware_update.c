#include "wifi_firmware_update.h"

#include <stdio.h>
#include <stddef.h>
#include <string.h>

#include "pico_fota_bootloader/core.h"

#ifndef POV_FIRMWARE_BOARD_ID
#define POV_FIRMWARE_BOARD_ID "pico_w_rp2040"
#endif

#define POVOTA_HEADER_SIZE 256u
#define POVOTA_MAX_PAYLOAD (1024u * 1024u)

typedef struct __attribute__((packed)) {
    char magic[8];
    uint8_t version;
    char board[16];
    uint32_t payload_len;
    char build_id[32];
    uint32_t crc32;
} povota_header_t;

static struct {
    wifi_fw_state_t state;
    uint32_t expected, received, payload_expected, payload_received;
    char message[96], build_id[33];
    uint8_t header[POVOTA_HEADER_SIZE];
    size_t header_len;
    uint8_t aligned[PFB_ALIGN_SIZE] __attribute__((aligned(PFB_ALIGN_SIZE)));
    size_t aligned_len;
} s_update;

static uint32_t crc32(const uint8_t *p, size_t n) {
    uint32_t c = 0xffffffffu;
    while (n--) { c ^= *p++; for (unsigned i = 0; i < 8; ++i) c = (c >> 1) ^ (0xedb88320u & -(int32_t)(c & 1u)); }
    return c ^ 0xffffffffu;
}

static void fail(const char *message) {
    pfb_mark_download_slot_as_invalid();
    s_update.state = WIFI_FW_FAILED;
    strncpy(s_update.message, message, sizeof(s_update.message) - 1);
    s_update.message[sizeof(s_update.message) - 1] = '\0';
}

bool wifi_fw_update_begin(uint32_t total_bytes) {
    if (s_update.state == WIFI_FW_RECEIVING || s_update.state == WIFI_FW_VALIDATING) return false;
    if (total_bytes <= POVOTA_HEADER_SIZE || total_bytes > POVOTA_MAX_PAYLOAD + POVOTA_HEADER_SIZE) {
        fail("Package is empty or too large for this device."); return false;
    }
    memset(&s_update, 0, sizeof(s_update));
    s_update.state = WIFI_FW_RECEIVING;
    s_update.expected = total_bytes;
    strcpy(s_update.message, "Receiving firmware package.");
    if (pfb_initialize_download_slot() != 0) { fail("Unable to prepare the update slot."); return false; }
    return true;
}

static bool validate_header(void) {
    const povota_header_t *h = (const povota_header_t *)s_update.header;
    if (memcmp(h->magic, "POVOTA01", 8) || h->version != 1 ||
        crc32(s_update.header, offsetof(povota_header_t, crc32)) != h->crc32 ||
        h->payload_len == 0 || h->payload_len > POVOTA_MAX_PAYLOAD ||
        h->payload_len + POVOTA_HEADER_SIZE != s_update.expected ||
        strncmp(h->board, POV_FIRMWARE_BOARD_ID, sizeof(h->board))) {
        fail("Package is invalid or targets a different board."); return false;
    }
    s_update.payload_expected = h->payload_len;
    memcpy(s_update.build_id, h->build_id, sizeof(h->build_id));
    s_update.build_id[sizeof(h->build_id)] = '\0';
    return true;
}

static bool write_payload_byte(uint8_t b) {
    s_update.aligned[s_update.aligned_len++] = b;
    s_update.payload_received++;
    if (s_update.aligned_len == PFB_ALIGN_SIZE) {
        if (pfb_write_to_flash_aligned_256_bytes(s_update.aligned,
                s_update.payload_received - PFB_ALIGN_SIZE, PFB_ALIGN_SIZE) != 0) {
            fail("Flash write failed."); return false;
        }
        s_update.aligned_len = 0;
    }
    return true;
}

bool wifi_fw_update_write(const uint8_t *data, size_t len) {
    if (s_update.state != WIFI_FW_RECEIVING || !data || s_update.received + len > s_update.expected) return false;
    while (len--) {
        uint8_t b = *data++; s_update.received++;
        if (s_update.header_len < POVOTA_HEADER_SIZE) {
            s_update.header[s_update.header_len++] = b;
            if (s_update.header_len == POVOTA_HEADER_SIZE && !validate_header()) return false;
        } else if (!write_payload_byte(b)) return false;
    }
    return true;
}

bool wifi_fw_update_finish(void) {
    if (s_update.state != WIFI_FW_RECEIVING || s_update.received != s_update.expected ||
        s_update.payload_received != s_update.payload_expected || s_update.aligned_len != 0) {
        fail("Upload was incomplete or has an invalid length."); return false;
    }
    s_update.state = WIFI_FW_VALIDATING;
    if (pfb_firmware_sha256_check(s_update.payload_received) != 0) { fail("Firmware integrity check failed."); return false; }
    s_update.state = WIFI_FW_READY;
    strcpy(s_update.message, "Firmware validated. Restarting shortly.");
    return true;
}

void wifi_fw_update_abort(const char *reason) { if (s_update.state == WIFI_FW_RECEIVING || s_update.state == WIFI_FW_VALIDATING) fail(reason ? reason : "Upload interrupted."); }
bool wifi_fw_update_ready(void) { return s_update.state == WIFI_FW_READY; }
void wifi_fw_update_perform(void) { if (wifi_fw_update_ready()) { pfb_mark_download_slot_as_valid(); pfb_perform_update(); } }
wifi_fw_state_t wifi_fw_update_state(void) { return s_update.state; }
const char *wifi_fw_update_message(void) { return s_update.message[0] ? s_update.message : "No update in progress."; }
uint32_t wifi_fw_update_received(void) { return s_update.received; }
uint32_t wifi_fw_update_expected(void) { return s_update.expected; }
const char *wifi_fw_update_build_id(void) { return s_update.build_id; }
void wifi_fw_update_boot_status(void) { if (pfb_is_after_rollback()) { s_update.state = WIFI_FW_ROLLED_BACK; strcpy(s_update.message, "Update rolled back; USB recovery remains available."); } else if (pfb_is_after_firmware_update()) { pfb_firmware_commit(); s_update.state = WIFI_FW_IDLE; strcpy(s_update.message, "Firmware update installed successfully."); } }
