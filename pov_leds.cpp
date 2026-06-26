#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"

#include "blink.pio.h"
#include "pov_demo.h"
#include "ws2812_driver.h"
#include "wifi_config.h"

#define LOG_DRIVER(fmt, ...) printf("[driver] " fmt "\n", ##__VA_ARGS__)
#define LOG_DEMO(fmt, ...) printf("[demo] " fmt "\n", ##__VA_ARGS__)
#define LOG_TIMING(fmt, ...) printf("[timing] " fmt "\n", ##__VA_ARGS__)
#define LOG_HEALTH(fmt, ...) printf("[health] " fmt "\n", ##__VA_ARGS__)

namespace {
constexpr uint8_t kRequestedLedCount = POV_LED_MAX_COUNT;
constexpr uint kDefaultDataPin = 6;
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

    PIO pio = pio0;
    uint sm = 0;
    uint offset = pio_add_program(pio, &ws2812_program);

    ws2812_driver_t driver;
    ws2812_driver_init_defaults(&driver);

#ifdef PICO_DEFAULT_LED_PIN
    uint pin = PICO_DEFAULT_LED_PIN;
#else
    uint pin = kDefaultDataPin;
#endif

    bool driver_ok = ws2812_driver_init(&driver, pio, sm, offset, pin, kRequestedLedCount, false);
    if (!driver_ok) {
        LOG_HEALTH("WS2812 init failed err=%u", (unsigned)driver.health.last_error_code);
    } else {
        LOG_DRIVER("ready pin=%u active_leds=%u sys_clk=%uHz",
                   pin,
                   (unsigned)driver.strip.active_count,
                   (unsigned)ws2812_driver_get_sys_clock_hz(&driver));
    }

    pov_demo_t demo;
    pov_demo_init(&demo);

    static uint32_t frame_words[POV_LED_MAX_COUNT] = {0};

    bool first_h_logged = false;
    bool driver_unavailable_logged = false;
    uint32_t readiness_ms = 0;

    while (true) {
        wifi_config_runtime_step();

        uint32_t now_ms = (uint32_t)to_ms_since_boot(get_absolute_time());

        if (ws2812_driver_is_ready(&driver) && !demo.playback.started) {
            driver_unavailable_logged = false;
            readiness_ms = now_ms;
            pov_demo_start(&demo, now_ms);
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
            }

            pov_demo_render_frame(&demo,
                                  frame_words,
                                  POV_LED_MAX_COUNT,
                                  driver.strip.active_count);
            if (!ws2812_driver_submit_frame(&driver, frame_words, driver.strip.active_count) &&
                driver.health.last_error_code == WS2812_ERROR_DMA_BUSY) {
                LOG_HEALTH("bounded dma busy ts_ms=%u", (unsigned)now_ms);
            }

            if (!first_h_logged && pov_demo_current_char(&demo) == 'H') {
                first_h_logged = true;
                LOG_TIMING("startup_latency_ms=%u", (unsigned)(now_ms - readiness_ms));
            }
        } else if (!ws2812_driver_is_ready(&driver) && !driver_unavailable_logged) {
            driver_unavailable_logged = true;
            LOG_HEALTH("driver unavailable err=%u keep-loop-responsive", (unsigned)driver.health.last_error_code);
        }

        wifi_config_set_blink_status(ws2812_driver_is_ready(&driver), 1u);
        tight_loop_contents();
    }
}
