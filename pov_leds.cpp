#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "hardware/pio.h"
#include "pico/stdlib.h"
#include "pico/rand.h"

#include "hall_sensor.h"
#include "pov_clock.h"
#include "pov_log.h"
#include "pov_clock_renderer.h"
#include "time_sync.h"
#include "wifi_config.h"
#include "wifi_firmware_update.h"
#include "ws2812.pio.h"
#include "ws2812_driver.h"

#define LOG_DRIVER(fmt, ...) pov_logf(POV_LOG_SOURCE_DRIVER, fmt, ##__VA_ARGS__)
#define LOG_CLOCK(fmt, ...) pov_logf(POV_LOG_SOURCE_CLOCK, fmt, ##__VA_ARGS__)
#define LOG_HEALTH(fmt, ...) pov_logf(POV_LOG_SOURCE_HEALTH, fmt, ##__VA_ARGS__)
#define LOG_HALL(fmt, ...) pov_logf(POV_LOG_SOURCE_HALL, fmt, ##__VA_ARGS__)
#define LOG_TIME(fmt, ...) pov_logf(POV_LOG_SOURCE_TIME, fmt, ##__VA_ARGS__)

namespace {
uint64_t log_clock_ms() { return time_us_64() / 1000u; }
uint32_t rad_s_x100_from_period(uint32_t period_us) {
    // 2*pi radians/revolution * 1,000,000 us/s * 100 centiradians/radian.
    constexpr uint64_t kTwoPiUsX100 = 628318531ULL;
    return period_us == 0u
               ? 0u
               : (uint32_t)((kTwoPiUsX100 + period_us / 2u) / period_us);
}
constexpr uint8_t kRequestedLedCount = POV_LED_MAX_COUNT;
constexpr uint kDefaultDataPin = 2;
constexpr uint32_t kHallLogIntervalMs = 1000;
constexpr uint32_t kStatusFrameIntervalMs = 500;
// Debug-only timing override. Production rendering always uses the measured Hall
// period (and retains the last measured period across temporary sample gaps).
constexpr bool kAssumeFixedRotation = false;
constexpr const char *kTimeServer = "ntp.tencent.com";

// Bench test mode: when true, ignore the POV clock/status logic and light every
// LED with a solid color so wiring + WS2812 protocol can be verified without
// rotation or NTP. Set to false to restore normal clock rendering.
constexpr bool kStaticTestPattern = false;
// Dim white in GRB word order (g<<16 | r<<8 | b) exercises all three channels.
constexpr uint32_t kStaticTestColor = (32u << 16) | (32u << 8) | 32u;
}

int main() {
    stdio_init_all();
    pov_log_init(get_rand_64(), log_clock_ms);
    pov_logf(POV_LOG_SOURCE_SYSTEM, "boot log initialized");

#if POV_LOG_CONSOLE
    for (int i = 0; i < 30 && !stdio_usb_connected(); ++i) {
        sleep_ms(100);
    }
#endif

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
    pov_rotation_config_t active_rotation_config =
        wifi_config_get_rotation_config();

    hall_sensor_t hall;
    hall_sensor_config_t hall_cfg;
    hall_sensor_init_defaults(&hall_cfg);
    bool hall_ok = hall_sensor_init(&hall, &hall_cfg);
    if (hall_ok) {
        LOG_HALL("ready pin=%u magnets=%u stop_timeout_ms=%u nominal=%u.%02u rad/s target=%u rpm range=%u-%u rpm",
                 (unsigned)hall_cfg.pin,
                 (unsigned)hall_cfg.magnets_per_rev,
                 (unsigned)(hall_cfg.stop_timeout_us / 1000u),
                 (unsigned)(active_rotation_config.rad_s_x100 / 100u),
                 (unsigned)(active_rotation_config.rad_s_x100 % 100u),
                 (unsigned)active_rotation_config.nominal_rpm,
                 (unsigned)active_rotation_config.min_rpm,
                 (unsigned)active_rotation_config.max_rpm);
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
    rotation.speed_config = active_rotation_config;
    pov_clock_rotation_t hall_rotation;
    pov_clock_rotation_init(&hall_rotation);
    hall_rotation.speed_config = active_rotation_config;

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
    bool rotation_speed_available = false;
    uint32_t rotation_speed_rpm = 0;

    // Commit a candidate image only after normal WiFi/display initialization.
    wifi_fw_update_boot_status();

    while (true) {
        wifi_config_runtime_step();

        pov_rotation_config_t current_rotation_config =
            wifi_config_get_rotation_config();
        if (current_rotation_config.rad_s_x100 !=
            active_rotation_config.rad_s_x100) {
            active_rotation_config = current_rotation_config;
            rotation.speed_config = active_rotation_config;
            hall_rotation.speed_config = active_rotation_config;
            if (kAssumeFixedRotation) {
                rotation.fresh = false;
                renderer.phase_locked = false;
            }
            status_frame_dirty = true;
            LOG_CLOCK("rotation target rad_s=%u.%02u rpm=%u range=%u-%u period_us=%u",
                      (unsigned)(active_rotation_config.rad_s_x100 / 100u),
                      (unsigned)(active_rotation_config.rad_s_x100 % 100u),
                      (unsigned)active_rotation_config.nominal_rpm,
                      (unsigned)active_rotation_config.min_rpm,
                      (unsigned)active_rotation_config.max_rpm,
                      (unsigned)active_rotation_config.period_us);
        }

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

        if (kAssumeFixedRotation) {
            if (!rotation.fresh) {
                rotation.phase_reference_us = now_us;
            }
            rotation.rpm = (float)active_rotation_config.nominal_rpm;
            rotation.period_us = active_rotation_config.period_us;
            rotation.fresh = true;
            rotation.within_range = true;
            rotation.stable = true;
            rotation.status = POV_CLOCK_ROTATION_SUITABLE;
        }

        if (hall_ok) {
            hall_rotation_measurement_t measurement = hall_sensor_read(&hall, now_us);
            if (measurement.valid && !measurement.stale) {
                rotation_speed_available = true;
                rotation_speed_rpm = (uint32_t)(measurement.rpm + 0.5f);
            }
            pov_clock_rotation_status_t status =
                pov_clock_rotation_update(&hall_rotation, &measurement);
            uint32_t hall_rad_s_x100 =
                rad_s_x100_from_period(hall_rotation.period_us);
            if (status != last_rotation_status) {
                last_rotation_status = status;
                status_frame_dirty = true;
                LOG_HALL("suitability=%s rpm=%d rad_s=%u.%02u period_us=%u",
                         pov_clock_rotation_status_text(status),
                         (int)(hall_rotation.rpm + 0.5f),
                         (unsigned)(hall_rad_s_x100 / 100u),
                         (unsigned)(hall_rad_s_x100 % 100u),
                         (unsigned)hall_rotation.period_us);
            }

            if (hall_rotation.fresh &&
                (now_ms - hall_last_log_ms) >= kHallLogIntervalMs) {
                hall_last_log_ms = now_ms;
                LOG_HALL("speed rpm=%d rad_s=%u.%02u target_rpm=%u target_rad_s=%u.%02u range=%u-%u period_us=%u",
                         (int)(hall_rotation.rpm + 0.5f),
                         (unsigned)(hall_rad_s_x100 / 100u),
                         (unsigned)(hall_rad_s_x100 % 100u),
                         (unsigned)active_rotation_config.nominal_rpm,
                         (unsigned)(active_rotation_config.rad_s_x100 / 100u),
                         (unsigned)(active_rotation_config.rad_s_x100 % 100u),
                         (unsigned)active_rotation_config.min_rpm,
                         (unsigned)active_rotation_config.max_rpm,
                         (unsigned)hall_rotation.period_us);
            }
        }

        if (!kAssumeFixedRotation) {
            rotation = hall_rotation;
        }

        pov_clock_health_t health = pov_clock_derive_health(&clock_time, &rotation);
        /* Target-RPM suitability remains useful diagnostic information, but it
         * must not gate adaptive rendering. Once Hall timing exists, render at
         * that measured cadence even when it differs from the debug target. */
        bool adaptive_display_ready =
            clock_time.calibrated &&
            pov_clock_rotation_ready_for_display(&rotation);
        if (health != last_health) {
            last_health = health;
            status_frame_dirty = true;
            dma_stall_warned = false;
            LOG_HEALTH("display=%s", pov_clock_health_text(health));
        }

        if (ws2812_driver_is_ready(&driver)) {
            if (kStaticTestPattern) {
                if (status_frame_dirty ||
                    (now_ms - status_last_ms) >= kStatusFrameIntervalMs) {
                    status_last_ms = now_ms;
                    for (size_t i = 0;
                         i < driver.strip.active_count && i < POV_LED_MAX_COUNT;
                         ++i) {
                        frame_words[i] = kStaticTestColor;
                    }
                    if (!ws2812_driver_is_dma_busy(&driver)) {
                        ws2812_driver_submit_frame(&driver, frame_words,
                                                   driver.strip.active_count);
                        status_frame_dirty = false;
                    }
                }
            } else if (adaptive_display_ready) {
                uint32_t frame_duration_us =
                    ws2812_driver_get_frame_duration_us(
                        &driver, driver.strip.active_count);
                uint64_t presentation_us = now_us + frame_duration_us;
                if (pov_clock_renderer_step(&renderer,
                                            rotation.period_us,
                                            rotation.phase_reference_us,
                                            presentation_us)) {
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

        wifi_config_set_blink_status(ws2812_driver_is_ready(&driver));
        wifi_config_set_rotation_speed_status(rotation_speed_available,
                                              rotation_speed_rpm);
        wifi_config_set_clock_status(clock_time.calibrated, clock_time.text);
        tight_loop_contents();
    }
}
