#include <cassert>
#include <cstdint>
#include <cstdio>

#include "pov_clock.h"
#include "pov_clock_renderer.h"
#include "ws2812_timing.h"

namespace {

hall_rotation_measurement_t sample(float rpm, uint32_t period_us,
                                   uint64_t edge_us, uint32_t generation) {
    hall_rotation_measurement_t m{};
    m.period_us = period_us;
    m.rpm = rpm;
    m.hz = rpm / 60.0f;
    m.valid = true;
    m.stale = false;
    m.last_update_us = edge_us;
    m.reference_edge_us = edge_us;
    m.sample_generation = generation;
    return m;
}

pov_clock_rotation_status_t status_for(float rpm, uint32_t period_us) {
    pov_clock_rotation_t rotation{};
    pov_clock_rotation_init(&rotation);
    hall_rotation_measurement_t m = sample(rpm, period_us, 1000000u, 2u);
    return pov_clock_rotation_update(&rotation, &m);
}

void test_supported_envelope() {
    assert(POV_CLOCK_COLUMNS == 40);
    assert(status_for(480.0f, 125000u) == POV_CLOCK_ROTATION_SUITABLE);
    assert(status_for(600.0f, 100000u) == POV_CLOCK_ROTATION_SUITABLE);
    assert(status_for(764.0f, 78534u) == POV_CLOCK_ROTATION_SUITABLE);
    assert(status_for(800.0f, 75000u) == POV_CLOCK_ROTATION_SUITABLE);
    assert(status_for(479.0f, 125261u) == POV_CLOCK_ROTATION_TOO_SLOW);
    assert(status_for(801.0f, 74906u) == POV_CLOCK_ROTATION_TOO_FAST);
}

void test_sample_aware_stability() {
    pov_clock_rotation_t rotation{};
    pov_clock_rotation_init(&rotation);

    hall_rotation_measurement_t steady = sample(600.0f, 100000u, 1000000u, 10u);
    assert(pov_clock_rotation_update(&rotation, &steady) ==
           POV_CLOCK_ROTATION_SUITABLE);
    assert(rotation.phase_reference_us == steady.reference_edge_us);
    assert(rotation.sample_generation == 10u);

    /* Rereading one physical sample must not advance stability history. */
    assert(pov_clock_rotation_update(&rotation, &steady) ==
           POV_CLOCK_ROTATION_SUITABLE);

    hall_rotation_measurement_t changed = sample(800.0f, 75000u, 1100000u, 11u);
    assert(pov_clock_rotation_update(&rotation, &changed) ==
           POV_CLOCK_ROTATION_UNSTABLE);
    assert(pov_clock_rotation_update(&rotation, &changed) ==
           POV_CLOCK_ROTATION_UNSTABLE);

    hall_rotation_measurement_t confirmed = sample(800.0f, 75000u, 1175000u, 12u);
    assert(pov_clock_rotation_update(&rotation, &confirmed) ==
           POV_CLOCK_ROTATION_SUITABLE);

    hall_rotation_measurement_t stale = confirmed;
    stale.valid = false;
    stale.stale = true;
    assert(pov_clock_rotation_update(&rotation, &stale) ==
           POV_CLOCK_ROTATION_UNAVAILABLE);

    hall_rotation_measurement_t resumed = sample(800.0f, 75000u, 1250000u, 13u);
    assert(pov_clock_rotation_update(&rotation, &resumed) ==
           POV_CLOCK_ROTATION_SUITABLE);
}

void test_phase_mapping_and_reanchor() {
    constexpr uint32_t period = 78534u;
    constexpr uint64_t edge = 1000000u;
    pov_clock_renderer_t renderer{};
    pov_clock_renderer_init(&renderer);

    assert(pov_clock_renderer_step(&renderer, period, edge, edge));
    assert(renderer.active_column == 0u);

    assert(pov_clock_renderer_step(&renderer, period, edge, edge + period / 4u));
    assert(renderer.active_column == 9u || renderer.active_column == 10u);

    assert(pov_clock_renderer_step(&renderer, period, edge, edge + period / 2u));
    assert(renderer.active_column == 20u);

    /* A long delay selects the current phase directly and emits once. */
    uint64_t column_30_phase =
        ((uint64_t)period * 30u + POV_CLOCK_COLUMNS - 1u) /
        POV_CLOCK_COLUMNS;
    uint64_t delayed = edge + 3u * period + column_30_phase;
    assert(pov_clock_renderer_step(&renderer, period, edge, delayed));
    assert(renderer.active_column == 30u);
    assert(!pov_clock_renderer_step(&renderer, period, edge, delayed));

    /* No fractional interval drift after one hundred revolutions. */
    assert(pov_clock_renderer_step(&renderer, period, edge,
                                   edge + 100u * (uint64_t)period));
    assert(renderer.active_column == 0u);

    uint64_t new_edge = edge + 101u * (uint64_t)period + 123u;
    assert(pov_clock_renderer_step(&renderer, 75000u, new_edge, new_edge));
    assert(renderer.active_column == 0u);
    assert(renderer.phase_reference_us == new_edge);

    assert(!pov_clock_renderer_step(&renderer, 0u, new_edge, new_edge));
    assert(!renderer.phase_locked);
    assert(!pov_clock_renderer_step(&renderer, 75000u, new_edge + 1u,
                                    new_edge));
}

void test_transport_budget() {
    assert(ws2812_frame_duration_us(57u, false) == 1760u);
    assert(ws2812_frame_duration_us(57u, true) == 2330u);
    assert((60000000u / 800u) / POV_CLOCK_COLUMNS == 1875u);
    assert(ws2812_frame_duration_us(57u, false) < 1875u);
}

}  // namespace

int main() {
    test_supported_envelope();
    test_sample_aware_stability();
    test_phase_mapping_and_reanchor();
    test_transport_budget();
    std::puts("adaptive Hall rendering tests passed");
    return 0;
}
