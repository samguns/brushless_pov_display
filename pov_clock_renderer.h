#ifndef POV_CLOCK_RENDERER_H
#define POV_CLOCK_RENDERER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "pov_clock.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t active_column;
    uint32_t column_interval_us;
    uint64_t last_column_us;
    char text[POV_CLOCK_TEXT_BUF_LEN];
    uint8_t column_masks[POV_CLOCK_COLUMNS];
    uint32_t column_colors[POV_CLOCK_COLUMNS];
    bool text_ready;
} pov_clock_renderer_t;

void pov_clock_renderer_init(pov_clock_renderer_t *renderer);
void pov_clock_renderer_set_text(pov_clock_renderer_t *renderer, const char *text);
bool pov_clock_renderer_step(pov_clock_renderer_t *renderer, uint32_t rotation_period_us,
                             uint64_t now_us);
void pov_clock_renderer_render_current(const pov_clock_renderer_t *renderer,
                                       uint32_t *frame_words,
                                       size_t frame_len,
                                       uint8_t active_led_count);
void pov_clock_renderer_render_status(pov_clock_health_t health,
                                      uint32_t *frame_words,
                                      size_t frame_len,
                                      uint8_t active_led_count);
void pov_clock_renderer_clear(uint32_t *frame_words, size_t frame_len);

#ifdef __cplusplus
}
#endif

#endif
