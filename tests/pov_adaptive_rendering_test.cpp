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

uint32_t abs_diff(uint32_t a, uint32_t b) { return a > b ? a - b : b - a; }

pov_clock_rotation_status_t status_for(float rpm, uint32_t period_us) {
    // Feed enough steady samples to reach the confidence threshold (feature 019
    // no longer trusts a single revolution).
    pov_clock_rotation_t rotation{};
    pov_clock_rotation_init(&rotation);
    pov_clock_rotation_status_t status = POV_CLOCK_ROTATION_UNAVAILABLE;
    for (uint32_t g = 1u; g <= (uint32_t)POV_CLOCK_SPEED_MIN_SAMPLES; ++g) {
        hall_rotation_measurement_t m =
            sample(rpm, period_us, 1000000u + (uint64_t)g * period_us, g + 1u);
        status = pov_clock_rotation_update(&rotation, &m);
    }
    return status;
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

void test_speed_calibration() {
    pov_clock_rotation_t rotation{};
    pov_clock_rotation_init(&rotation);

    uint32_t gen = 1u;
    uint64_t edge = 1000000u;
    auto feed = [&](uint32_t period) -> pov_clock_rotation_status_t {
        edge += period;
        gen += 1u;
        hall_rotation_measurement_t m =
            sample(60000000.0f / (float)period, period, edge, gen);
        return pov_clock_rotation_update(&rotation, &m);
    };

    /* Confidence gating (C6): a single/second sample is not yet SUITABLE; the
     * third steady sample is. Phase tracks the real edge (C2). */
    assert(feed(100000u) != POV_CLOCK_ROTATION_SUITABLE);
    assert(feed(100000u) != POV_CLOCK_ROTATION_SUITABLE);
    assert(feed(100000u) == POV_CLOCK_ROTATION_SUITABLE);
    assert(rotation.phase_reference_us == edge);
    assert(rotation.smoothed_period_us == 100000u);

    /* Generation dedup (C9): re-reading the same generation changes nothing. */
    hall_rotation_measurement_t same = sample(600.0f, 100000u, edge, gen);
    uint32_t before = rotation.smoothed_period_us;
    uint8_t before_count = rotation.hist_count;
    assert(pov_clock_rotation_update(&rotation, &same) == POV_CLOCK_ROTATION_SUITABLE);
    assert(rotation.smoothed_period_us == before);
    assert(rotation.hist_count == before_count);

    /* Variance reduction (C1): alternating +/-2% noise averages to a far tighter
     * band than the raw +/-2000us swing. */
    for (int i = 0; i < POV_CLOCK_SPEED_WINDOW; ++i) {
        feed((i % 2 == 0) ? 102000u : 98000u);
    }
    assert(rotation.smoothed_period_us >= 99500u &&
           rotation.smoothed_period_us <= 100500u);

    /* Outlier rejection (C3): a missed-magnet (2x) and a bounce (0.5x) barely
     * move the estimate and do not drop stability. */
    uint32_t base = rotation.smoothed_period_us;
    assert(feed(200000u) == POV_CLOCK_ROTATION_SUITABLE);  /* rejected */
    assert(feed(50000u) == POV_CLOCK_ROTATION_SUITABLE);   /* rejected */
    assert(abs_diff(rotation.smoothed_period_us, base) * 100u <= base * 2u);

    /* Hysteresis (C5): an ~18% deviation (between enter 10% and exit 25%) does
     * not destabilize once locked. */
    assert(feed(118000u) == POV_CLOCK_ROTATION_SUITABLE);

    /* Convergence (C4): a sustained step to 75000us reaches the new period within
     * the window and re-stabilizes. */
    for (int i = 0; i < POV_CLOCK_SPEED_WINDOW; ++i) feed(75000u);
    assert(rotation.smoothed_period_us >= 73500u &&
           rotation.smoothed_period_us <= 76500u);
    assert(rotation.status == POV_CLOCK_ROTATION_SUITABLE);

    /* Stop resets the window (C8) and a resumed spin is not immediately SUITABLE. */
    hall_rotation_measurement_t stale = sample(600.0f, 75000u, edge + 1u, gen + 100u);
    stale.valid = false;
    stale.stale = true;
    assert(pov_clock_rotation_update(&rotation, &stale) ==
           POV_CLOCK_ROTATION_UNAVAILABLE);
    assert(rotation.hist_count == 0u);
    assert(feed(75000u) != POV_CLOCK_ROTATION_SUITABLE);
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

bool is_palette_color(uint32_t w) {
    return w == 0x00002000u ||  // hours red   (0x20 << 8)
           w == 0x00200000u ||  // minutes green (0x20 << 16)
           w == 0x00000020u ||  // seconds blue
           w == 0x00181818u;    // separator gray
}

void test_round_rendering() {
    constexpr size_t kLeds = POV_CLOCK_LED_ROWS;  // 57
    constexpr uint32_t kSentinel = 0xABCD1234u;

    pov_clock_renderer_t renderer{};
    pov_clock_renderer_init(&renderer);

    // Buffer with two guard words past frame_len to catch any overflow write.
    uint32_t frame[kLeds + 2];

    /* Blank when no text set (C4): every column produces an all-dark frame. */
    for (uint8_t c = 0; c < POV_CLOCK_COLUMNS; ++c) {
        for (size_t i = 0; i < kLeds + 2; ++i) frame[i] = 0xDEADBEEFu;
        renderer.active_column = c;
        pov_clock_renderer_render_current(&renderer, frame, kLeds, (uint8_t)kLeds);
        for (size_t i = 0; i < kLeds; ++i) assert(frame[i] == 0u);
    }

    pov_clock_renderer_set_text(&renderer, "12:34:56");

    /* Bounds safety across all columns (C3): guard words past frame_len are never
     * written; every lit LED is a known palette color (C5). */
    size_t total_lit = 0;
    for (uint8_t c = 0; c < POV_CLOCK_COLUMNS; ++c) {
        for (size_t i = 0; i < kLeds + 2; ++i) frame[i] = kSentinel;
        renderer.active_column = c;
        pov_clock_renderer_render_current(&renderer, frame, kLeds, (uint8_t)kLeds);
        assert(frame[kLeds] == kSentinel);      // no write past frame_len
        assert(frame[kLeds + 1] == kSentinel);
        for (size_t i = 0; i < kLeds; ++i) {
            if (frame[i] != 0u) {
                assert(is_palette_color(frame[i]));
                ++total_lit;
            }
        }
    }
    /* The text actually lights pixels somewhere across the revolution (US1). */
    assert(total_lit > 0);

    /* Column 0 (arm horizontal) crosses the text's middle band, so it lights
     * several LEDs (upright text is present, not empty). */
    for (size_t i = 0; i < kLeds + 2; ++i) frame[i] = 0u;
    renderer.active_column = 0;
    pov_clock_renderer_render_current(&renderer, frame, kLeds, (uint8_t)kLeds);
    size_t col0_lit = 0;
    for (size_t i = 0; i < kLeds; ++i) if (frame[i] != 0u) ++col0_lit;
    assert(col0_lit > 0);

    /* Determinism (C6): same inputs yield identical output. */
    uint32_t frame_a[kLeds];
    uint32_t frame_b[kLeds];
    renderer.active_column = 7;
    pov_clock_renderer_render_current(&renderer, frame_a, kLeds, (uint8_t)kLeds);
    pov_clock_renderer_render_current(&renderer, frame_b, kLeds, (uint8_t)kLeds);
    for (size_t i = 0; i < kLeds; ++i) assert(frame_a[i] == frame_b[i]);

    /* Reduced active count: indices in [active, frame_len) stay dark and guard
     * words past frame_len are untouched. */
    for (size_t i = 0; i < kLeds + 2; ++i) frame[i] = kSentinel;
    renderer.active_column = 3;
    pov_clock_renderer_render_current(&renderer, frame, kLeds, 20u);
    assert(frame[kLeds] == kSentinel);          // no write past frame_len
    assert(frame[kLeds + 1] == kSentinel);
    for (size_t i = 20u; i < kLeds; ++i) assert(frame[i] == 0u);
}

void test_status_full_span() {
    constexpr size_t kLeds = POV_CLOCK_LED_ROWS;  // 57
    uint32_t frame[kLeds];
    for (size_t i = 0; i < kLeds; ++i) frame[i] = 0u;

    pov_clock_renderer_render_status(POV_CLOCK_HEALTH_ROTATION_UNAVAILABLE, frame,
                                     kLeds, (uint8_t)kLeds);

    bool lit_lower = false;
    bool lit_upper = false;
    for (size_t i = 0; i < kLeds; ++i) {
        if (frame[i] != 0u && i < kLeds / 3u) lit_lower = true;
        if (frame[i] != 0u && i >= (2u * kLeds) / 3u) lit_upper = true;
    }
    assert(lit_lower && lit_upper);
}

void test_transport_budget() {
    assert(ws2812_dma_word(0x00123456u, false) == 0x12345600u);
    assert(ws2812_dma_word(0x12345678u, true) == 0x12345678u);
    assert(ws2812_frame_duration_us(57u, false) == 1810u);
    assert(ws2812_frame_duration_us(57u, true) == 2380u);
    assert((60000000u / 800u) / POV_CLOCK_COLUMNS == 1875u);
    assert(ws2812_frame_duration_us(57u, false) < 1875u);
}

}  // namespace

int main() {
    test_supported_envelope();
    test_speed_calibration();
    test_phase_mapping_and_reanchor();
    test_round_rendering();
    test_status_full_span();
    test_transport_budget();
    std::puts("adaptive Hall rendering tests passed");
    return 0;
}
