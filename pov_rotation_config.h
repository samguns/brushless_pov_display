#ifndef POV_ROTATION_CONFIG_H
#define POV_ROTATION_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    POV_ROTATION_DEFAULT_RAD_S_X100 = 4000,
    POV_ROTATION_MIN_RAD_S_X100 = 50,
    POV_ROTATION_MAX_RAD_S_X100 = 10000,
};

typedef struct {
    uint16_t rad_s_x100;
    uint16_t nominal_rpm;
    uint16_t min_rpm;
    uint16_t max_rpm;
    uint32_t period_us;
} pov_rotation_config_t;

/* Derive all rotation parameters from hundredths of a radian per second.
 * The accepted speed envelope preserves the established 0.8x-to-4/3x
 * relationship around nominal speed. */
static inline bool pov_rotation_config_derive(uint16_t rad_s_x100,
                                              pov_rotation_config_t *out) {
    if (out == 0 || rad_s_x100 < POV_ROTATION_MIN_RAD_S_X100 ||
        rad_s_x100 > POV_ROTATION_MAX_RAD_S_X100) {
        return false;
    }

    /* 2*pi * 1,000,000 us/s * 100 fixed-point rad/s units. */
    const uint64_t two_pi_us_x100 = 628318531ULL;
    uint32_t period_us =
        (uint32_t)((two_pi_us_x100 + rad_s_x100 / 2u) / rad_s_x100);
    uint32_t nominal_rpm = (60000000u + period_us / 2u) / period_us;

    out->rad_s_x100 = rad_s_x100;
    out->nominal_rpm = (uint16_t)nominal_rpm;
    out->min_rpm = (uint16_t)((nominal_rpm * 4u + 2u) / 5u);
    out->max_rpm = (uint16_t)((nominal_rpm * 4u + 1u) / 3u);
    out->period_us = period_us;
    return true;
}

#ifdef __cplusplus
}
#endif

#endif
