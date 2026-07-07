#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"

#include "ws2812.pio.h"
#include "pov_demo.h"
#include "ws2812_driver.h"
#include "hall_sensor.h"
#include "wifi_config.h"

#define LOG_DRIVER(fmt, ...) printf("[driver] " fmt "\n", ##__VA_ARGS__)
#define LOG_DEMO(fmt, ...) printf("[demo] " fmt "\n", ##__VA_ARGS__)
#define LOG_TIMING(fmt, ...) printf("[timing] " fmt "\n", ##__VA_ARGS__)
#define LOG_HEALTH(fmt, ...) printf("[health] " fmt "\n", ##__VA_ARGS__)
#define LOG_HALL(fmt, ...) printf("[hall] " fmt "\n", ##__VA_ARGS__)

namespace {
constexpr uint8_t kRequestedLedCount = POV_LED_MAX_COUNT;
constexpr uint kDefaultDataPin = 2;
constexpr uint32_t kHallLogIntervalMs = 1000;
}

int main() {
    stdio_init_all();

    for (int i = 0; i < 30 && !stdio_usb_connected(); ++i) {
        sleep_ms(100);
    }

    wifi_config_init();
    if (!wifi_config_sta_runtime_init()) {
        LOG_HEALTH("wifi runtime init failed");
        while (true) {
            tight_loop_contents();
        }
    }

#ifdef PICO_DEFAULT_LED_PIN
    uint pin = PICO_DEFAULT_LED_PIN;
#else
    uint pin = kDefaultDataPin;
#endif

    // Claim a PIO + state machine capable of driving `pin`. The range-aware
    // claim avoids the CYW43/RM2 wireless PIO, whose high-numbered data/clock
    // pins force a non-zero GPIO base that cannot address the WS2812 pin.
    PIO pio = nullptr;
    uint sm = 0;
    uint offset = 0;
    bool pio_ok = pio_claim_free_sm_and_add_program_for_gpio_range(
        &ws2812_program, &pio, &sm, &offset, pin, 1, true);

    ws2812_driver_t driver;
    ws2812_driver_init_defaults(&driver);

    bool driver_ok = pio_ok &&
                     ws2812_driver_init(&driver, pio, sm, offset, pin, kRequestedLedCount, false);
    if (!driver_ok) {
        LOG_HEALTH("WS2812 init failed pio_ok=%d err=%u",
                   (int)pio_ok, (unsigned)driver.health.last_error_code);
    } else {
        LOG_DRIVER("ready pin=%u active_leds=%u sys_clk=%uHz",
                   pin,
                   (unsigned)driver.strip.active_count,
                   (unsigned)ws2812_driver_get_sys_clock_hz(&driver));
    }

    // Apply the persisted display brightness (percent 0..100 -> 0..255).
    uint8_t brightness_pct = wifi_config_get_brightness();
    ws2812_driver_set_brightness(&driver, (uint8_t)((brightness_pct * 255u) / 100u));

    pov_demo_t demo;
    pov_demo_init(&demo);

    hall_sensor_t hall;
    hall_sensor_config_t hall_cfg;
    hall_sensor_init_defaults(&hall_cfg);
    bool hall_ok = hall_sensor_init(&hall, &hall_cfg);
    if (hall_ok) {
        LOG_HALL("ready pin=%u magnets=%u stop_timeout_ms=%u",
                 (unsigned)hall_cfg.pin,
                 (unsigned)hall_cfg.magnets_per_rev,
                 (unsigned)(hall_cfg.stop_timeout_us / 1000u));
    } else {
        LOG_HALL("init failed pin=%u", (unsigned)hall_cfg.pin);
    }

    static uint32_t frame_words[POV_LED_MAX_COUNT] = {0};

    bool first_h_logged = false;
    bool driver_unavailable_logged = false;
    uint32_t readiness_ms = 0;
    bool frame_dirty = false;
    uint32_t frame_dirty_since_ms = 0;
    bool dma_stall_warned = false;
    uint32_t hall_last_log_ms = 0;
    bool hall_was_spinning = false;
    uint8_t last_brightness_pct = brightness_pct;

    while (true) {
        wifi_config_runtime_step();

        // Reflect portal brightness changes onto the live LED output.
        uint8_t cur_brightness_pct = wifi_config_get_brightness();
        if (cur_brightness_pct != last_brightness_pct) {
            last_brightness_pct = cur_brightness_pct;
            ws2812_driver_set_brightness(
                &driver, (uint8_t)((cur_brightness_pct * 255u) / 100u));
            frame_dirty = true;  // re-submit so new brightness takes effect now
        }

        uint32_t now_ms = (uint32_t)to_ms_since_boot(get_absolute_time());

        if (ws2812_driver_is_ready(&driver) && !demo.playback.started) {
            driver_unavailable_logged = false;
            readiness_ms = now_ms;
            pov_demo_start(&demo, now_ms);
            frame_dirty = true;
            frame_dirty_since_ms = now_ms;
            dma_stall_warned = false;
            LOG_DEMO("start sequence=%c%c%c%c%c duration_ms=%u",
                     demo.sequence.chars[0],
                     demo.sequence.chars[1],
                     demo.sequence.chars[2],
                     demo.sequence.chars[3],
                     demo.sequence.chars[4],
                     (unsigned)demo.sequence.duration_ms);
        }

        bool transitioned = false;
        uint32_t elapsed_ms = 0;
        if (demo.playback.started) {
            char from_char = pov_demo_current_char(&demo);
            if (pov_demo_step(&demo, now_ms, &transitioned, &elapsed_ms) && transitioned) {
                char to_char = pov_demo_current_char(&demo);
                LOG_DEMO("transition %c->%c idx=%u ts_ms=%u",
                         from_char,
                         to_char,
                         (unsigned)demo.playback.current_index,
                         (unsigned)now_ms);
                LOG_TIMING("cadence_ms=%u target_ms=%u",
                           (unsigned)elapsed_ms,
                           (unsigned)demo.sequence.duration_ms);
                driver.health.transition_log_count++;
                frame_dirty = true;
                frame_dirty_since_ms = now_ms;
                dma_stall_warned = false;
            }

            // WS2812 latches and holds the last frame, so only push on change.
            // Submitting every loop iteration would race the in-flight DMA and
            // flood the health log with benign "dma busy" events.
            if (frame_dirty) {
                if (!ws2812_driver_is_dma_busy(&driver)) {
                    pov_demo_render_frame(&demo,
                                          frame_words,
                                          POV_LED_MAX_COUNT,
                                          driver.strip.active_count);
                    if (ws2812_driver_submit_frame(&driver, frame_words, driver.strip.active_count)) {
                        frame_dirty = false;
                    }
                } else if (!dma_stall_warned && (now_ms - frame_dirty_since_ms) > 50u) {
                    dma_stall_warned = true;
                    LOG_HEALTH("dma busy: frame submit delayed ts_ms=%u", (unsigned)now_ms);
                }
            }

            if (!first_h_logged && pov_demo_current_char(&demo) == 'H') {
                first_h_logged = true;
                LOG_TIMING("startup_latency_ms=%u", (unsigned)(now_ms - readiness_ms));
            }
        } else if (!ws2812_driver_is_ready(&driver) && !driver_unavailable_logged) {
            driver_unavailable_logged = true;
            LOG_HEALTH("driver unavailable err=%u keep-loop-responsive", (unsigned)driver.health.last_error_code);
        }

        // Non-blocking rotation-speed read every loop iteration; O(1), no waits.
        if (hall_ok) {
            uint64_t now_us = to_us_since_boot(get_absolute_time());
            hall_rotation_measurement_t rot = hall_sensor_read(&hall, now_us);

            bool spinning = rot.valid && !rot.stale;
            if (spinning != hall_was_spinning) {
                hall_was_spinning = spinning;
                if (spinning) {
                    LOG_HALL("rotation started rpm=%d hz=%d.%02d edges=%u",
                             (int)(rot.rpm + 0.5f),
                             (int)rot.hz,
                             (int)((rot.hz - (int)rot.hz) * 100.0f),
                             (unsigned)hall_sensor_get_edge_count(&hall));
                } else {
                    LOG_HALL("rotation stopped/stale edges=%u",
                             (unsigned)hall_sensor_get_edge_count(&hall));
                }
            }

            if (spinning && (now_ms - hall_last_log_ms) >= kHallLogIntervalMs) {
                hall_last_log_ms = now_ms;
                LOG_HALL("speed rpm=%d hz=%d.%02d period_us=%u",
                         (int)(rot.rpm + 0.5f),
                         (int)rot.hz,
                         (int)((rot.hz - (int)rot.hz) * 100.0f),
                         (unsigned)rot.period_us);
            }
        }

        wifi_config_set_blink_status(ws2812_driver_is_ready(&driver), 1u);
        tight_loop_contents();
    }
}
