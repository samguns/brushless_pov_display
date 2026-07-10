#ifndef POV_CLOCK_H
#define POV_CLOCK_H

#include <stdbool.h>
#include <stdint.h>

#include "hall_sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    POV_CLOCK_NOMINAL_RPM = 600,
    POV_CLOCK_MIN_RPM = 480,
    POV_CLOCK_MAX_RPM = 720,
    POV_CLOCK_COLUMNS = 48,
    POV_CLOCK_LED_ROWS = 57,
    POV_CLOCK_CST_OFFSET_SECONDS = 8 * 60 * 60,
    POV_CLOCK_TEXT_LEN = 8,
    POV_CLOCK_TEXT_BUF_LEN = 9,
};

typedef enum {
    POV_CLOCK_ROTATION_UNAVAILABLE = 0,
    POV_CLOCK_ROTATION_TOO_SLOW,
    POV_CLOCK_ROTATION_SUITABLE,
    POV_CLOCK_ROTATION_TOO_FAST,
    POV_CLOCK_ROTATION_UNSTABLE,
} pov_clock_rotation_status_t;

typedef enum {
    POV_CLOCK_HEALTH_TIME_UNAVAILABLE = 0,
    POV_CLOCK_HEALTH_ROTATION_UNAVAILABLE,
    POV_CLOCK_HEALTH_SPEED_TOO_SLOW,
    POV_CLOCK_HEALTH_SPEED_TOO_FAST,
    POV_CLOCK_HEALTH_SPEED_UNSTABLE,
    POV_CLOCK_HEALTH_NORMAL,
} pov_clock_health_t;

typedef struct {
    bool calibrated;
    uint32_t utc_base_seconds;
    uint64_t base_monotonic_us;
    uint32_t current_utc_seconds;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    char text[POV_CLOCK_TEXT_BUF_LEN];
} pov_clock_time_t;

typedef struct {
    float rpm;
    uint32_t period_us;
    bool fresh;
    bool within_range;
    bool stable;
    pov_clock_rotation_status_t status;
    uint32_t previous_period_us;
} pov_clock_rotation_t;

void pov_clock_time_init(pov_clock_time_t *clock);
void pov_clock_time_set_utc(pov_clock_time_t *clock, uint32_t utc_seconds, uint64_t now_us);
bool pov_clock_time_update(pov_clock_time_t *clock, uint64_t now_us);
void pov_clock_format_cst(uint32_t utc_seconds, char *out, uint32_t out_len,
                          uint8_t *hour, uint8_t *minute, uint8_t *second);

void pov_clock_rotation_init(pov_clock_rotation_t *rotation);
pov_clock_rotation_status_t pov_clock_rotation_update(
    pov_clock_rotation_t *rotation,
    const hall_rotation_measurement_t *measurement);

pov_clock_health_t pov_clock_derive_health(const pov_clock_time_t *clock,
                                           const pov_clock_rotation_t *rotation);
const char *pov_clock_rotation_status_text(pov_clock_rotation_status_t status);
const char *pov_clock_health_text(pov_clock_health_t health);

#ifdef __cplusplus
}
#endif

#endif
