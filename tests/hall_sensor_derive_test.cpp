#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "hall_sensor.h"

namespace {

void require_near(float actual, float expected) {
    assert(fabsf(actual - expected) < 0.01f);
}

hall_rotation_measurement_t derive(uint32_t interval_us,
                                   const hall_sensor_config_t *config) {
    constexpr uint64_t edge_us = 10000000u;
    return hall_sensor_derive(interval_us, edge_us, true, 2u, config,
                              edge_us + 100u);
}

void test_manual_speeds_are_not_clamped_to_60_rpm() {
    hall_sensor_config_t config;
    hall_sensor_init_defaults(&config);

    hall_rotation_measurement_t slow = derive(2000000u, &config);
    assert(slow.valid && !slow.stale);
    assert(slow.period_us == 2000000u);
    require_near(slow.rpm, 30.0f);

    constexpr uint64_t edge_us = 10000000u;
    hall_rotation_measurement_t slow_still_fresh = hall_sensor_derive(
        2000000u, edge_us, true, 3u, &config,
        edge_us + config.stop_timeout_us + 1u);
    assert(slow_still_fresh.valid && !slow_still_fresh.stale);
    require_near(slow_still_fresh.rpm, 30.0f);

    hall_rotation_measurement_t slow_stale = hall_sensor_derive(
        2000000u, edge_us, true, 3u, &config, edge_us + 4000001u);
    assert(!slow_stale.valid && slow_stale.stale);

    hall_rotation_measurement_t medium = derive(500000u, &config);
    assert(medium.period_us == 500000u);
    require_near(medium.rpm, 120.0f);

    hall_rotation_measurement_t operating = derive(100000u, &config);
    assert(operating.period_us == 100000u);
    require_near(operating.rpm, 600.0f);
}

void test_magnets_fast_edges_and_stale_state() {
    hall_sensor_config_t config;
    hall_sensor_init_defaults(&config);
    config.magnets_per_rev = 2u;

    hall_rotation_measurement_t two_magnets = derive(250000u, &config);
    assert(two_magnets.period_us == 500000u);
    require_near(two_magnets.rpm, 120.0f);

    config.magnets_per_rev = 1u;
    hall_rotation_measurement_t too_fast = derive(5000u, &config);
    assert(too_fast.period_us == 10000u);
    require_near(too_fast.rpm, 6000.0f);

    constexpr uint64_t edge_us = 10000000u;
    hall_rotation_measurement_t stale = hall_sensor_derive(
        500000u, edge_us, true, 3u, &config,
        edge_us + config.stop_timeout_us + 1u);
    assert(!stale.valid && stale.stale);
    require_near(stale.rpm, 0.0f);
}

}  // namespace

int main(void) {
    test_manual_speeds_are_not_clamped_to_60_rpm();
    test_magnets_fast_edges_and_stale_state();
    puts("hall sensor derivation tests passed");
    return 0;
}
