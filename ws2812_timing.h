#ifndef WS2812_TIMING_H
#define WS2812_TIMING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    WS2812_BIT_RATE_HZ = 800000u,
    WS2812_RESET_US = 50u,
};

/* Pure fixed-time budget used by both the hardware driver and host tests. */
static inline uint32_t ws2812_frame_duration_us(size_t frame_words, bool rgbw) {
    uint64_t bits_per_pixel = rgbw ? 32u : 24u;
    uint64_t total_bits = (uint64_t)frame_words * bits_per_pixel;
    uint64_t wire_us = (total_bits * 1000000u + WS2812_BIT_RATE_HZ - 1u) /
                       WS2812_BIT_RATE_HZ;
    return (uint32_t)(wire_us + WS2812_RESET_US);
}

#ifdef __cplusplus
}
#endif

#endif

