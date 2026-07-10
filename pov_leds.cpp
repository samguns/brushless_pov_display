#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "hardware/pio.h"
#include "pico/stdlib.h"

#include "hall_sensor.h"
#include "pov_clock.h"
#include "pov_clock_renderer.h"
#include "time_sync.h"
#include "wifi_config.h"
#include "ws2812.pio.h"
#include "ws2812_driver.h"

#define LOG_DRIVER(fmt, ...) printf("[driver] " fmt "\n", ##__VA_ARGS__)
#define LOG_CLOCK(fmt, ...) printf("[clock] " fmt "\n", ##__VA_ARGS__)
#define LOG_HEALTH(fmt, ...) printf("[health] " fmt "\n", ##__VA_ARGS__)
#define LOG_HALL(fmt, ...) printf("[hall] " fmt "\n", ##__VA_ARGS__)
#define LOG_TIME(fmt, ...) printf("[time] " fmt "\n", ##__VA_ARGS__)

namespace {
constexpr uint8_t kRequestedLedCount = POV_LED_MAX_COUNT;
constexpr uint kDefaultDataPin = 2;
constexpr uint32_t kHallLogIntervalMs = 1000;
constexpr uint32_t kStatusFrameIntervalMs = 500;
constexpr const char *kTimeServer = "pool.ntp.org";
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

    uint8_t brightness_pct = wifi_config_get_brightness();
    ws2812_driver_set_brightness(&driver, (uint8_t)((brightness_pct * 255u) / 100u));

    hall_sensor_t hall;
    hall_sensor_config_t hall_cfg;
    hall_sensor_init_defaults(&hall_cfg);
    bool hall_ok = hall_sensor_init(&hall, &hall_cfg);
    if (hall_ok) {
        LOG_HALL("ready pin=%u magnets=%u stop_timeout_ms=%u nominal=%u rpm range=%u-%u rpm",
                 (unsigned)hall_cfg.pin,
                 (unsigned)hall_cfg.magnets_per_rev,
                 (unsigned)(hall_cfg.stop_timeout_us / 1000u),
                 (unsigned)POV_CLOCK_NOMINAL_RPM,
                 (unsigned)POV_CLOCK_MIN_RPM,
                 (unsigned)POV_CLOCK_MAX_RPM);
    } else {
        LOG_HALL("init failed pin=%u", (unsigned)hall_cfg.pin);
    }

    time_sync_t time_sync;
    time_sync_init_defaults(&time_sync);
    uint32_t boot_ms = (uint32_t)to_ms_since_boot(get_absolute_time());
    time_sync_start(&time_sync, kTimeServer, boot_ms);
    LOG_TIME("calibration start server=%s", kTimeServer);

    pov_clock_time_t clock_time;
    pov_clock_time_init(&clock_time);

    pov_clock_rotation_t rotation;
    pov_clock_rotation_init(&rotation);

    pov_clock_renderer_t renderer;
    pov_clock_renderer_init(&renderer);
    pov_clock_renderer_set_text(&renderer, clock_time.text);

    static uint32_t frame_words[POV_LED_MAX_COUNT] = {0};

    uint32_t hall_last_log_ms = 0;
    uint32_t status_last_ms = 0;
    bool status_frame_dirty = true;
    uint8_t last_brightness_pct = brightness_pct;
    time_sync_state_t last_sync_state = time_sync.state;
    time_sync_error_t last_sync_error = time_sync.last_error;
    pov_clock_rotation_status_t last_rotation_status = POV_CLOCK_ROTATION_UNAVAILABLE;
    pov_clock_health_t last_health = POV_CLOCK_HEALTH_TIME_UNAVAILABLE;
    bool time_loaded = false;
    bool dma_stall_warned = false;

    while (true) {
        wifi_config_runtime_step();

        uint8_t cur_brightness_pct = wifi_config_get_brightness();
        if (cur_brightness_pct != last_brightness_pct) {
            last_brightness_pct = cur_brightness_pct;
            ws2812_driver_set_brightness(
                &driver, (uint8_t)((cur_brightness_pct * 255u) / 100u));
            status_frame_dirty = true;
        }

        absolute_time_t now_abs = get_absolute_time();
        uint32_t now_ms = (uint32_t)to_ms_since_boot(now_abs);
        uint64_t now_us = to_us_since_boot(now_abs);

        time_sync_step(&time_sync, now_ms);
        if (time_sync.state != last_sync_state || time_sync.last_error != last_sync_error) {
            last_sync_state = time_sync.state;
            last_sync_error = time_sync.last_error;
            LOG_TIME("state=%s error=%s attempts=%u",
                     time_sync_state_text(time_sync.state),
                     time_sync_error_text(time_sync.last_error),
                     (unsigned)time_sync.attempt_count);
        }

        if (time_sync_has_time(&time_sync) && !time_loaded) {
            time_loaded = true;
            pov_clock_time_set_utc(&clock_time,
                                   time_sync_get_utc_seconds(&time_sync),
                                   time_sync_get_calibrated_at_us(&time_sync));
            pov_clock_renderer_set_text(&renderer, clock_time.text);
            status_frame_dirty = true;
            LOG_TIME("calibrated utc=%u cst=%s",
                     (unsigned)clock_time.current_utc_seconds,
                     clock_time.text);
        }

        bool second_changed = pov_clock_time_update(&clock_time, now_us);
        if (second_changed) {
            pov_clock_renderer_set_text(&renderer, clock_time.text);
            LOG_CLOCK("tick %s", clock_time.text);
        }

        if (hall_ok) {
            hall_rotation_measurement_t measurement = hall_sensor_read(&hall, now_us);
            pov_clock_rotation_status_t status = pov_clock_rotation_update(&rotation, &measurement);
            if (status != last_rotation_status) {
                last_rotation_status = status;
                status_frame_dirty = true;
                LOG_HALL("suitability=%s rpm=%d period_us=%u",
                         pov_clock_rotation_status_text(status),
                         (int)(rotation.rpm + 0.5f),
                         (unsigned)rotation.period_us);
            }

            if (rotation.fresh && (now_ms - hall_last_log_ms) >= kHallLogIntervalMs) {
                hall_last_log_ms = now_ms;
                LOG_HALL("speed rpm=%d target=%u range=%u-%u period_us=%u",
                         (int)(rotation.rpm + 0.5f),
                         (unsigned)POV_CLOCK_NOMINAL_RPM,
                         (unsigned)POV_CLOCK_MIN_RPM,
                         (unsigned)POV_CLOCK_MAX_RPM,
                         (unsigned)rotation.period_us);
            }
        }

        pov_clock_health_t health = pov_clock_derive_health(&clock_time, &rotation);
        if (health != last_health) {
            last_health = health;
            status_frame_dirty = true;
            dma_stall_warned = false;
            LOG_HEALTH("display=%s", pov_clock_health_text(health));
        }

        if (ws2812_driver_is_ready(&driver)) {
            if (health == POV_CLOCK_HEALTH_NORMAL) {
                if (pov_clock_renderer_step(&renderer, rotation.period_us, now_us)) {
                    pov_clock_renderer_render_current(&renderer,
                                                      frame_words,
                                                      POV_LED_MAX_COUNT,
                                                      driver.strip.active_count);
                    if (!ws2812_driver_is_dma_busy(&driver)) {
                        ws2812_driver_submit_frame(&driver, frame_words, driver.strip.active_count);
                        dma_stall_warned = false;
                    } else if (!dma_stall_warned) {
                        dma_stall_warned = true;
                        LOG_HEALTH("dma busy: clock column delayed ts_ms=%u", (unsigned)now_ms);
                    }
                }
            } else if (status_frame_dirty || (now_ms - status_last_ms) >= kStatusFrameIntervalMs) {
                status_last_ms = now_ms;
                pov_clock_renderer_render_status(health,
                                                 frame_words,
                                                 POV_LED_MAX_COUNT,
                                                 driver.strip.active_count);
                if (!ws2812_driver_is_dma_busy(&driver)) {
                    ws2812_driver_submit_frame(&driver, frame_words, driver.strip.active_count);
                    status_frame_dirty = false;
                    dma_stall_warned = false;
                } else if (!dma_stall_warned) {
                    dma_stall_warned = true;
                    LOG_HEALTH("dma busy: status frame delayed ts_ms=%u", (unsigned)now_ms);
                }
            }
        }

        wifi_config_set_blink_status(ws2812_driver_is_ready(&driver),
                                     health == POV_CLOCK_HEALTH_NORMAL ? 10u : 1u);
        tight_loop_contents();
    }
}
