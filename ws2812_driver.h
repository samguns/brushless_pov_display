#ifndef WS2812_DRIVER_H
#define WS2812_DRIVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hardware/pio.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    POV_LED_MAX_COUNT = 57,
    POV_LED_MIN_COUNT = 1,
};

typedef enum {
    WS2812_ERROR_NONE = 0,
    WS2812_ERROR_INVALID_LED_COUNT,
    WS2812_ERROR_INIT_FAILED,
    WS2812_ERROR_DMA_CHANNEL_UNAVAILABLE,
    WS2812_ERROR_DMA_BUSY,
} ws2812_error_t;

typedef struct {
    uint8_t configured_count;
    uint8_t active_count;
    bool is_bounded;
    uint8_t max_count;
} led_strip_config_t;

typedef struct {
    bool driver_ready;
    ws2812_error_t last_error_code;
    uint32_t last_error_ms;
    uint32_t transition_log_count;
} output_health_state_t;

typedef struct {
    bool initialized;
    PIO pio;
    uint sm;
    uint offset;
    uint pin;
    int dma_chan;
    bool rgbw;
    uint32_t sys_clock_hz;
    uint32_t ws2812_bit_hz;

    led_strip_config_t strip;
    output_health_state_t health;
} ws2812_driver_t;

void ws2812_driver_init_defaults(ws2812_driver_t *driver);

bool ws2812_driver_init(ws2812_driver_t *driver,
                        PIO pio,
                        uint sm,
                        uint offset,
                        uint pin,
                        int requested_led_count,
                        bool rgbw);

void ws2812_driver_deinit(ws2812_driver_t *driver);

bool ws2812_driver_set_led_count(ws2812_driver_t *driver, int requested_led_count);

bool ws2812_driver_submit_frame(ws2812_driver_t *driver,
                                const uint32_t *frame,
                                size_t frame_words);

bool ws2812_driver_is_dma_busy(const ws2812_driver_t *driver);

bool ws2812_driver_is_ready(const ws2812_driver_t *driver);

uint32_t ws2812_driver_get_sys_clock_hz(const ws2812_driver_t *driver);

#ifdef __cplusplus
}
#endif

#endif
