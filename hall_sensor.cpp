#include "hall_sensor.h"

#include <limits.h>
#include <string.h>

#ifndef HALL_SENSOR_DERIVE_ONLY
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "pico/time.h"

namespace {
/* Single-instance assumption: the per-GPIO raw IRQ handler takes no argument, so
 * it resolves the active sensor through this file-static pointer. */
hall_sensor_t *s_irq_sensor = nullptr;

void hall_irq_handler(void) {
    hall_sensor_t *s = s_irq_sensor;
    if (s == nullptr) {
        return;
    }

    uint gpio = (uint)s->config.pin;
    uint32_t events = gpio_get_irq_event_mask(gpio);
    if (events == 0u) {
        return;
    }
    gpio_acknowledge_irq(gpio, events);

    uint64_t now = time_us_64();
    hall_capture_t *c = &s->capture;

    if (c->edge_count >= 1u) {
        uint64_t since = now - c->last_edge_us;
        if (since < (uint64_t)s->config.debounce_us) {
            return; /* debounce: reject bounce/noise within the lockout window */
        }
        c->prev_edge_us = c->last_edge_us;
        c->last_edge_us = now;
        c->last_interval_us = (uint32_t)since;
        c->edge_count++;
        if (c->edge_count >= 2u) {
            c->has_two_edges = true;
        }
    } else {
        c->last_edge_us = now;
        c->edge_count = 1u;
    }
}
}  // namespace
#endif

void hall_sensor_init_defaults(hall_sensor_config_t *config) {
    if (config == nullptr) {
        return;
    }
    config->pin = HALL_DEFAULT_PIN;
    config->active_low = true;   /* magnet-present asserts logic low */
    config->pull_up = false;     /* push-pull output drives the line directly */
    config->debounce_us = HALL_DEFAULT_DEBOUNCE_US;
    config->magnets_per_rev = HALL_DEFAULT_MAGNETS_PER_REV;
    config->stop_timeout_us = HALL_DEFAULT_STOP_TIMEOUT_US;
}

#ifndef HALL_SENSOR_DERIVE_ONLY
bool hall_sensor_init(hall_sensor_t *sensor, const hall_sensor_config_t *config) {
    if (sensor == nullptr) {
        return false;
    }

    memset(sensor, 0, sizeof(*sensor));
    if (config != nullptr) {
        sensor->config = *config;
    } else {
        hall_sensor_init_defaults(&sensor->config);
    }
    if (sensor->config.magnets_per_rev < 1u) {
        sensor->config.magnets_per_rev = 1u; /* normalize (FR-007) */
    }

    uint gpio = (uint)sensor->config.pin;
    gpio_init(gpio);
    gpio_set_dir(gpio, GPIO_IN);
    if (sensor->config.pull_up) {
        gpio_pull_up(gpio);
    } else {
        gpio_disable_pulls(gpio); /* push-pull output needs no pull */
    }

    s_irq_sensor = sensor;

    /* Per-GPIO raw handler so we coexist with the CYW43/RM2 GPIO IRQ usage
     * rather than clobbering the single shared callback. */
    uint32_t edge_mask = sensor->config.active_low ? GPIO_IRQ_EDGE_FALL
                                                   : GPIO_IRQ_EDGE_RISE;
    gpio_add_raw_irq_handler(gpio, hall_irq_handler);
    gpio_set_irq_enabled(gpio, edge_mask, true);
    irq_set_enabled(IO_IRQ_BANK0, true);

    sensor->initialized = true;
    return true;
}

void hall_sensor_deinit(hall_sensor_t *sensor) {
    if (sensor == nullptr || !sensor->initialized) {
        return;
    }
    uint gpio = (uint)sensor->config.pin;
    gpio_set_irq_enabled(gpio, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, false);
    gpio_remove_raw_irq_handler(gpio, hall_irq_handler);
    if (s_irq_sensor == sensor) {
        s_irq_sensor = nullptr;
    }
    sensor->initialized = false;
}
#endif

hall_rotation_measurement_t hall_sensor_derive(uint32_t last_interval_us,
                                               uint64_t last_edge_us,
                                               bool has_two_edges,
                                               uint32_t edge_count,
                                               const hall_sensor_config_t *config,
                                               uint64_t now_us) {
    hall_rotation_measurement_t m;
    m.period_us = 0u;
    m.rpm = 0.0f;
    m.hz = 0.0f;
    m.valid = false;
    m.stale = false;
    m.last_update_us = now_us;
    m.reference_edge_us = last_edge_us;
    m.sample_generation = edge_count;

    uint8_t magnets = (config != nullptr && config->magnets_per_rev >= 1u)
                          ? config->magnets_per_rev
                          : 1u;
    uint32_t stop_timeout = (config != nullptr) ? config->stop_timeout_us
                                                : (uint32_t)HALL_DEFAULT_STOP_TIMEOUT_US;

    /* First revolution is unknown until two edges have been observed. */
    if (!has_two_edges) {
        m.stale = (last_edge_us == 0u) || ((now_us - last_edge_us) > stop_timeout);
        return m;
    }

    /* Do not declare slow motion stopped before its next expected pulse. The
     * configured timeout is the minimum; two observed event intervals allow
     * one missed/late pulse while preserving the normal-speed 1.5 s behavior. */
    uint64_t stale_after_us = (uint64_t)stop_timeout;
    uint64_t interval_timeout_us = (uint64_t)last_interval_us * 2u;
    if (interval_timeout_us > stale_after_us) {
        stale_after_us = interval_timeout_us;
    }
    if ((now_us - last_edge_us) > stale_after_us) {
        m.stale = true;
        return m;
    }

    uint64_t period = (uint64_t)last_interval_us * (uint64_t)magnets;
    if (period == 0u) {
        m.stale = true;
        return m;
    }

    /* Clamp only implausibly fast edges. Preserve slow periods so operator
     * telemetry reports the measured speed instead of flattening it to 60 RPM.
     * Saturate the public 32-bit period if an extreme configuration overflows. */
    const uint64_t period_min_us = HALL_US_PER_MINUTE / (uint64_t)HALL_MAX_SUPPORTED_RPM;
    if (period < period_min_us) {
        period = period_min_us;
    } else if (period > UINT32_MAX) {
        period = UINT32_MAX;
    }

    m.period_us = (uint32_t)period;
    m.hz = (float)HALL_US_PER_SECOND / (float)period;
    m.rpm = (float)HALL_US_PER_MINUTE / (float)period;
    m.valid = true;
    m.stale = false;
    return m;
}

#ifndef HALL_SENSOR_DERIVE_ONLY
hall_rotation_measurement_t hall_sensor_read(hall_sensor_t *sensor, uint64_t now_us) {
    hall_rotation_measurement_t empty;
    empty.period_us = 0u;
    empty.rpm = 0.0f;
    empty.hz = 0.0f;
    empty.valid = false;
    empty.stale = true;
    empty.last_update_us = now_us;
    empty.reference_edge_us = 0u;
    empty.sample_generation = 0u;

    if (sensor == nullptr || !sensor->initialized) {
        return empty;
    }

    /* Atomic snapshot of the interrupt-shared 64-bit state (T009). */
    uint32_t save = save_and_disable_interrupts();
    uint32_t interval = sensor->capture.last_interval_us;
    uint64_t last_edge = sensor->capture.last_edge_us;
    bool two = sensor->capture.has_two_edges;
    uint32_t edge_count = sensor->capture.edge_count;
    restore_interrupts(save);

    return hall_sensor_derive(interval, last_edge, two, edge_count,
                              &sensor->config, now_us);
}

float hall_sensor_get_rpm(hall_sensor_t *sensor, uint64_t now_us) {
    return hall_sensor_read(sensor, now_us).rpm;
}

float hall_sensor_get_hz(hall_sensor_t *sensor, uint64_t now_us) {
    return hall_sensor_read(sensor, now_us).hz;
}

uint32_t hall_sensor_get_period_us(hall_sensor_t *sensor, uint64_t now_us) {
    return hall_sensor_read(sensor, now_us).period_us;
}

uint32_t hall_sensor_get_edge_count(const hall_sensor_t *sensor) {
    if (sensor == nullptr) {
        return 0u;
    }
    uint32_t save = save_and_disable_interrupts();
    uint32_t count = sensor->capture.edge_count;
    restore_interrupts(save);
    return count;
}
#endif
