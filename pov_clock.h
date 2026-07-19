#ifndef POV_CLOCK_H
#define POV_CLOCK_H

#include <stdbool.h>
#include <stdint.h>

#include "hall_sensor.h"
#include "pov_rotation_config.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    /* Default mechanical target is 40 rad/s = 381.97 RPM. Runtime settings may
     * override it and derive the same 0.8x-to-1.333x eligibility envelope. */
    POV_CLOCK_NOMINAL_RAD_PER_SEC = 40,
    POV_CLOCK_NOMINAL_RPM = 382,
    POV_CLOCK_NOMINAL_PERIOD_US = 157080,
    POV_CLOCK_MIN_RPM = 306,
    POV_CLOCK_MAX_RPM = 509,
    POV_CLOCK_COLUMNS = 40,
    POV_CLOCK_LED_ROWS = 57,
    POV_CLOCK_CST_OFFSET_SECONDS = 8 * 60 * 60,
    POV_CLOCK_TEXT_LEN = 8,
    POV_CLOCK_TEXT_BUF_LEN = 9,
};

/* Rotation-speed smoothing (feature 019): bounded moving average of recent
 * revolution periods with outlier rejection and hysteresis, so the rendering
 * cadence stays steady without lagging real speed changes. */
enum {
    POV_CLOCK_SPEED_WINDOW = 8,           /* moving-average window (revolutions) */
    POV_CLOCK_SPEED_MIN_SAMPLES = 3,      /* samples before a confident speed */
    POV_CLOCK_SPEED_OUTLIER_PCT = 40,     /* reject band vs current mean */
    POV_CLOCK_SPEED_STABLE_ENTER_PCT = 10,/* enter-stable band */
    POV_CLOCK_SPEED_STABLE_EXIT_PCT = 25, /* exit-stable band (hysteresis) */
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
    pov_rotation_config_t speed_config;
    float rpm;
    uint32_t period_us;
    uint64_t phase_reference_us;
    uint32_t sample_generation;
    bool fresh;
    bool within_range;
    bool stable;
    pov_clock_rotation_status_t status;
    uint32_t previous_period_us;
    /* Bounded moving-average speed filter state (feature 019). */
    uint32_t period_hist[POV_CLOCK_SPEED_WINDOW];
    uint64_t period_sum;
    uint8_t hist_count;
    uint8_t hist_head;
    uint32_t smoothed_period_us;
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
