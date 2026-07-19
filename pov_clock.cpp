#include "pov_clock.h"

#include <stdio.h>
#include <string.h>

namespace {
constexpr uint32_t kSecondsPerDay = 24u * 60u * 60u;
constexpr uint32_t kUsPerSecond = 1000000u;

uint32_t abs_diff_u32(uint32_t a, uint32_t b) {
    return (a > b) ? (a - b) : (b - a);
}
}  // namespace

void pov_clock_time_init(pov_clock_time_t *clock) {
    if (clock == nullptr) {
        return;
    }
    memset(clock, 0, sizeof(*clock));
    snprintf(clock->text, sizeof(clock->text), "--:--:--");
}

void pov_clock_format_cst(uint32_t utc_seconds, char *out, uint32_t out_len,
                          uint8_t *hour, uint8_t *minute, uint8_t *second) {
    uint32_t local = (utc_seconds + (uint32_t)POV_CLOCK_CST_OFFSET_SECONDS) % kSecondsPerDay;
    uint8_t h = (uint8_t)(local / 3600u);
    uint8_t m = (uint8_t)((local / 60u) % 60u);
    uint8_t s = (uint8_t)(local % 60u);

    if (hour != nullptr) {
        *hour = h;
    }
    if (minute != nullptr) {
        *minute = m;
    }
    if (second != nullptr) {
        *second = s;
    }
    if (out != nullptr && out_len > 0u) {
        snprintf(out, out_len, "%02u:%02u:%02u",
                 (unsigned)h, (unsigned)m, (unsigned)s);
    }
}

void pov_clock_time_set_utc(pov_clock_time_t *clock, uint32_t utc_seconds, uint64_t now_us) {
    if (clock == nullptr) {
        return;
    }
    clock->calibrated = true;
    clock->utc_base_seconds = utc_seconds;
    clock->base_monotonic_us = now_us;
    clock->current_utc_seconds = utc_seconds;
    pov_clock_format_cst(utc_seconds, clock->text, sizeof(clock->text),
                         &clock->hour, &clock->minute, &clock->second);
}

bool pov_clock_time_update(pov_clock_time_t *clock, uint64_t now_us) {
    if (clock == nullptr || !clock->calibrated) {
        return false;
    }

    uint32_t elapsed_seconds = (uint32_t)((now_us - clock->base_monotonic_us) / kUsPerSecond);
    uint32_t utc = clock->utc_base_seconds + elapsed_seconds;
    if (utc == clock->current_utc_seconds) {
        return false;
    }

    clock->current_utc_seconds = utc;
    pov_clock_format_cst(utc, clock->text, sizeof(clock->text),
                         &clock->hour, &clock->minute, &clock->second);
    return true;
}

void pov_clock_rotation_init(pov_clock_rotation_t *rotation) {
    if (rotation == nullptr) {
        return;
    }
    memset(rotation, 0, sizeof(*rotation));
    (void)pov_rotation_config_derive(POV_ROTATION_DEFAULT_RAD_S_X100,
                                     &rotation->speed_config);
    rotation->status = POV_CLOCK_ROTATION_UNAVAILABLE;
}

pov_clock_rotation_status_t pov_clock_rotation_update(
    pov_clock_rotation_t *rotation,
    const hall_rotation_measurement_t *measurement) {
    if (rotation == nullptr) {
        return POV_CLOCK_ROTATION_UNAVAILABLE;
    }

    if (measurement == nullptr || !measurement->valid || measurement->stale) {
        rotation->rpm = 0.0f;
        rotation->period_us = 0u;
        rotation->phase_reference_us = 0u;
        rotation->fresh = false;
        rotation->within_range = false;
        rotation->stable = false;
        rotation->status = POV_CLOCK_ROTATION_UNAVAILABLE;
        /* Reset the smoothing window so a resumed spin starts clean. */
        rotation->hist_count = 0u;
        rotation->hist_head = 0u;
        rotation->period_sum = 0u;
        rotation->smoothed_period_us = 0u;
        return rotation->status;
    }

    rotation->fresh = true;
    /* Angular phase always tracks the real latest edge; only the period/speed is
     * smoothed (feature 019). */
    rotation->phase_reference_us = measurement->reference_edge_us;

    bool new_sample = measurement->sample_generation != rotation->sample_generation;
    if (new_sample) {
        rotation->sample_generation = measurement->sample_generation;
        uint32_t sample = measurement->period_us;

        /* Outlier rejection: once enough history exists, drop samples that deviate
         * implausibly from the current mean (missed magnet ~2x, bounce ~0.5x). */
        bool outlier = false;
        if (rotation->hist_count >= (uint8_t)POV_CLOCK_SPEED_MIN_SAMPLES &&
            rotation->smoothed_period_us > 0u) {
            uint32_t diff = abs_diff_u32(sample, rotation->smoothed_period_us);
            outlier = (diff * 100u) >
                      (rotation->smoothed_period_us *
                       (uint32_t)POV_CLOCK_SPEED_OUTLIER_PCT);
        }

        if (!outlier) {
            /* Bounded moving average: push into the ring, evicting the oldest. */
            if (rotation->hist_count < (uint8_t)POV_CLOCK_SPEED_WINDOW) {
                rotation->period_hist[rotation->hist_head] = sample;
                rotation->period_sum += sample;
                rotation->hist_count++;
            } else {
                rotation->period_sum -= rotation->period_hist[rotation->hist_head];
                rotation->period_hist[rotation->hist_head] = sample;
                rotation->period_sum += sample;
            }
            rotation->hist_head =
                (uint8_t)((rotation->hist_head + 1u) % (uint8_t)POV_CLOCK_SPEED_WINDOW);
            rotation->smoothed_period_us =
                (uint32_t)(rotation->period_sum / rotation->hist_count);
            rotation->previous_period_us = sample;

            /* Hysteresis on the stable decision (accepted samples only). */
            if (rotation->hist_count >= (uint8_t)POV_CLOCK_SPEED_MIN_SAMPLES) {
                uint32_t dev = abs_diff_u32(sample, rotation->smoothed_period_us);
                uint32_t dev_pct = (rotation->smoothed_period_us > 0u)
                    ? (dev * 100u) / rotation->smoothed_period_us : 100u;
                if (rotation->stable) {
                    if (dev_pct > (uint32_t)POV_CLOCK_SPEED_STABLE_EXIT_PCT) {
                        rotation->stable = false;
                    }
                } else if (dev_pct <= (uint32_t)POV_CLOCK_SPEED_STABLE_ENTER_PCT) {
                    rotation->stable = true;
                }
            } else {
                rotation->stable = false;  /* not yet confident */
            }
        }
        /* Outlier: leave ring, smoothed period, and stability unchanged. */
    }

    /* Rendering outputs come from the smoothed estimate (raw until the first
     * accepted sample exists). */
    uint32_t eff_period = (rotation->smoothed_period_us > 0u)
                              ? rotation->smoothed_period_us
                              : measurement->period_us;
    rotation->period_us = eff_period;
    rotation->rpm = (eff_period > 0u)
                        ? (float)HALL_US_PER_MINUTE / (float)eff_period
                        : 0.0f;
    rotation->within_range =
        rotation->rpm >= (float)rotation->speed_config.min_rpm &&
        rotation->rpm <= (float)rotation->speed_config.max_rpm;

    bool confident = rotation->hist_count >= (uint8_t)POV_CLOCK_SPEED_MIN_SAMPLES;
    if (rotation->rpm < (float)rotation->speed_config.min_rpm) {
        rotation->status = POV_CLOCK_ROTATION_TOO_SLOW;
    } else if (rotation->rpm > (float)rotation->speed_config.max_rpm) {
        rotation->status = POV_CLOCK_ROTATION_TOO_FAST;
    } else if (!confident || !rotation->stable) {
        rotation->status = POV_CLOCK_ROTATION_UNSTABLE;
    } else {
        rotation->status = POV_CLOCK_ROTATION_SUITABLE;
    }

    return rotation->status;
}

pov_clock_health_t pov_clock_derive_health(const pov_clock_time_t *clock,
                                           const pov_clock_rotation_t *rotation) {
    if (clock == nullptr || !clock->calibrated) {
        return POV_CLOCK_HEALTH_TIME_UNAVAILABLE;
    }
    if (rotation == nullptr || !rotation->fresh) {
        return POV_CLOCK_HEALTH_ROTATION_UNAVAILABLE;
    }

    switch (rotation->status) {
        case POV_CLOCK_ROTATION_SUITABLE:
            return POV_CLOCK_HEALTH_NORMAL;
        case POV_CLOCK_ROTATION_TOO_SLOW:
            return POV_CLOCK_HEALTH_SPEED_TOO_SLOW;
        case POV_CLOCK_ROTATION_TOO_FAST:
            return POV_CLOCK_HEALTH_SPEED_TOO_FAST;
        case POV_CLOCK_ROTATION_UNSTABLE:
            return POV_CLOCK_HEALTH_SPEED_UNSTABLE;
        case POV_CLOCK_ROTATION_UNAVAILABLE:
        default:
            return POV_CLOCK_HEALTH_ROTATION_UNAVAILABLE;
    }
}

const char *pov_clock_rotation_status_text(pov_clock_rotation_status_t status) {
    switch (status) {
        case POV_CLOCK_ROTATION_UNAVAILABLE: return "unavailable";
        case POV_CLOCK_ROTATION_TOO_SLOW: return "too_slow";
        case POV_CLOCK_ROTATION_SUITABLE: return "suitable";
        case POV_CLOCK_ROTATION_TOO_FAST: return "too_fast";
        case POV_CLOCK_ROTATION_UNSTABLE: return "unstable";
        default: return "unknown";
    }
}

const char *pov_clock_health_text(pov_clock_health_t health) {
    switch (health) {
        case POV_CLOCK_HEALTH_TIME_UNAVAILABLE: return "time_unavailable";
        case POV_CLOCK_HEALTH_ROTATION_UNAVAILABLE: return "rotation_unavailable";
        case POV_CLOCK_HEALTH_SPEED_TOO_SLOW: return "speed_too_slow";
        case POV_CLOCK_HEALTH_SPEED_TOO_FAST: return "speed_too_fast";
        case POV_CLOCK_HEALTH_SPEED_UNSTABLE: return "speed_unstable";
        case POV_CLOCK_HEALTH_NORMAL: return "normal_clock";
        default: return "unknown";
    }
}
