#include "ws2812_driver.h"

#include <string.h>

#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "pico/time.h"

#include "blink.pio.h"

namespace {
constexpr uint32_t kWs2812BitRateHz = 800000;
constexpr uint32_t kWs2812CyclesPerBit = ws2812_T1 + ws2812_T2 + ws2812_T3;

uint8_t clamp_led_count(int requested_led_count, bool *bounded) {
    if (bounded != nullptr) {
        *bounded = false;
    }

    if (requested_led_count < POV_LED_MIN_COUNT) {
        if (bounded != nullptr) {
            *bounded = true;
        }
        return POV_LED_MIN_COUNT;
    }

    if (requested_led_count > POV_LED_MAX_COUNT) {
        if (bounded != nullptr) {
            *bounded = true;
        }
        return POV_LED_MAX_COUNT;
    }

    return (uint8_t)requested_led_count;
}

void set_error_state(ws2812_driver_t *driver, ws2812_error_t error_code) {
    if (driver == nullptr) {
        return;
    }

    driver->health.last_error_code = error_code;
    driver->health.last_error_ms = (uint32_t)to_ms_since_boot(get_absolute_time());
}
}  // namespace

void ws2812_driver_init_defaults(ws2812_driver_t *driver) {
    if (driver == nullptr) {
        return;
    }

    memset(driver, 0, sizeof(*driver));
    driver->dma_chan = -1;
    driver->strip.max_count = POV_LED_MAX_COUNT;
    driver->ws2812_bit_hz = kWs2812BitRateHz;
    driver->brightness = 255;  /* full intensity by default (no scaling) */
}

void ws2812_driver_set_brightness(ws2812_driver_t *driver, uint8_t brightness) {
    if (driver == nullptr) {
        return;
    }
    driver->brightness = brightness;
}

bool ws2812_driver_set_led_count(ws2812_driver_t *driver, int requested_led_count) {
    if (driver == nullptr) {
        return false;
    }

    bool bounded = false;
    uint8_t active_count = clamp_led_count(requested_led_count, &bounded);
    driver->strip.configured_count = (requested_led_count > 0) ? (uint8_t)requested_led_count : 0;
    driver->strip.active_count = active_count;
    driver->strip.is_bounded = bounded;

    if (requested_led_count <= 0) {
        set_error_state(driver, WS2812_ERROR_INVALID_LED_COUNT);
        return false;
    }

    return true;
}

bool ws2812_driver_init(ws2812_driver_t *driver,
                        PIO pio,
                        uint sm,
                        uint offset,
                        uint pin,
                        int requested_led_count,
                        bool rgbw) {
    if (driver == nullptr) {
        return false;
    }

    ws2812_driver_init_defaults(driver);

    driver->pio = pio;
    driver->sm = sm;
    driver->offset = offset;
    driver->pin = pin;
    driver->rgbw = rgbw;
    driver->sys_clock_hz = (uint32_t)clock_get_hz(clk_sys);

    if (!ws2812_driver_set_led_count(driver, requested_led_count)) {
        driver->health.driver_ready = false;
        return false;
    }

    // Timing is derived from runtime clock source to avoid hardcoded clock assumptions.
    float div = (float)driver->sys_clock_hz / (kWs2812BitRateHz * (float)kWs2812CyclesPerBit);
    ws2812_program_init(pio, sm, offset, pin, div, rgbw);

    driver->dma_chan = dma_claim_unused_channel(false);
    if (driver->dma_chan < 0) {
        set_error_state(driver, WS2812_ERROR_DMA_CHANNEL_UNAVAILABLE);
        driver->health.driver_ready = false;
        return false;
    }

    dma_channel_config dma_cfg = dma_channel_get_default_config((uint)driver->dma_chan);
    channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&dma_cfg, true);
    channel_config_set_write_increment(&dma_cfg, false);
    channel_config_set_dreq(&dma_cfg, pio_get_dreq(pio, sm, true));

    dma_channel_configure((uint)driver->dma_chan,
                          &dma_cfg,
                          &pio->txf[sm],
                          nullptr,
                          0,
                          false);

    driver->initialized = true;
    driver->health.driver_ready = true;
    driver->health.last_error_code = WS2812_ERROR_NONE;
    return true;
}

void ws2812_driver_deinit(ws2812_driver_t *driver) {
    if (driver == nullptr) {
        return;
    }

    if (driver->dma_chan >= 0) {
        dma_channel_unclaim((uint)driver->dma_chan);
    }

    ws2812_driver_init_defaults(driver);
}

bool ws2812_driver_is_dma_busy(const ws2812_driver_t *driver) {
    if (driver == nullptr || driver->dma_chan < 0) {
        return false;
    }

    return dma_channel_is_busy((uint)driver->dma_chan);
}

bool ws2812_driver_submit_frame(ws2812_driver_t *driver,
                                const uint32_t *frame,
                                size_t frame_words) {
    if (driver == nullptr || !driver->initialized || !driver->health.driver_ready || frame == nullptr) {
        return false;
    }

    if (frame_words > driver->strip.active_count) {
        frame_words = driver->strip.active_count;
    }

    if (frame_words == 0) {
        return false;
    }

    if (ws2812_driver_is_dma_busy(driver)) {
        set_error_state(driver, WS2812_ERROR_DMA_BUSY);
        return false;
    }

    const uint32_t *src = frame;

    // Apply global brightness by scaling the 24-bit GRB pixel values into a
    // driver-owned buffer (the DMA reads asynchronously from this address, so it
    // must persist until the transfer completes — guaranteed by the caller
    // waiting for !dma_busy before the next submit). Pixel data only; the PIO
    // bit timing is untouched (constitution I & II).
    if (driver->brightness < 255) {
        static uint32_t scaled[POV_LED_MAX_COUNT];
        uint32_t b = driver->brightness;
        for (size_t i = 0; i < frame_words; ++i) {
            uint32_t w = frame[i];
            uint32_t g = (w >> 16) & 0xFFu;
            uint32_t r = (w >> 8) & 0xFFu;
            uint32_t bl = w & 0xFFu;
            g = (g * b) / 255u;
            r = (r * b) / 255u;
            bl = (bl * b) / 255u;
            scaled[i] = (g << 16) | (r << 8) | bl;
        }
        src = scaled;
    }

    dma_channel_set_read_addr((uint)driver->dma_chan, src, false);
    dma_channel_set_trans_count((uint)driver->dma_chan, frame_words, true);

    return true;
}

bool ws2812_driver_is_ready(const ws2812_driver_t *driver) {
    return driver != nullptr && driver->initialized && driver->health.driver_ready;
}

uint32_t ws2812_driver_get_sys_clock_hz(const ws2812_driver_t *driver) {
    if (driver == nullptr) {
        return 0;
    }

    return driver->sys_clock_hz;
}
