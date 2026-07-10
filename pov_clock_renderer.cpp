#include "pov_clock_renderer.h"

#include <string.h>

namespace {
constexpr uint8_t kFontRows = 5;
constexpr uint8_t kGlyphWidth = 3;
constexpr uint8_t kColonWidth = 1;
constexpr uint8_t kGlyphScaleY = 6;
constexpr uint8_t kTextTop = (POV_CLOCK_LED_ROWS - (kFontRows * kGlyphScaleY)) / 2;
constexpr uint8_t kLayoutStartColumn = 10;

uint32_t grb(uint8_t g, uint8_t r, uint8_t b) {
    return ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;
}

constexpr uint32_t kHourColor = (0x20u << 8);       // red in GRB word order
constexpr uint32_t kMinuteColor = (0x20u << 16);    // green in GRB word order
constexpr uint32_t kSecondColor = 0x20u;            // blue in GRB word order
constexpr uint32_t kSeparatorColor = (0x18u << 16) | (0x18u << 8) | 0x18u;

uint32_t color_for_text_index(uint8_t index, char c) {
    if (c == ':') {
        return kSeparatorColor;
    }
    if (index <= 1u) {
        return kHourColor;
    }
    if (index >= 3u && index <= 4u) {
        return kMinuteColor;
    }
    if (index >= 6u && index <= 7u) {
        return kSecondColor;
    }
    return kSeparatorColor;
}

const uint8_t *glyph_for(char c, uint8_t *width) {
    static const uint8_t digits[10][kFontRows] = {
        {0b111, 0b101, 0b101, 0b101, 0b111},
        {0b010, 0b110, 0b010, 0b010, 0b111},
        {0b111, 0b001, 0b111, 0b100, 0b111},
        {0b111, 0b001, 0b111, 0b001, 0b111},
        {0b101, 0b101, 0b111, 0b001, 0b001},
        {0b111, 0b100, 0b111, 0b001, 0b111},
        {0b111, 0b100, 0b111, 0b101, 0b111},
        {0b111, 0b001, 0b001, 0b001, 0b001},
        {0b111, 0b101, 0b111, 0b101, 0b111},
        {0b111, 0b101, 0b111, 0b001, 0b111},
    };
    static const uint8_t colon[kFontRows] = {0b0, 0b1, 0b0, 0b1, 0b0};
    static const uint8_t dash[kFontRows] = {0b000, 0b000, 0b111, 0b000, 0b000};

    if (c >= '0' && c <= '9') {
        *width = kGlyphWidth;
        return digits[c - '0'];
    }
    if (c == ':') {
        *width = kColonWidth;
        return colon;
    }
    *width = kGlyphWidth;
    return dash;
}

void build_columns(pov_clock_renderer_t *renderer) {
    memset(renderer->column_masks, 0, sizeof(renderer->column_masks));
    memset(renderer->column_colors, 0, sizeof(renderer->column_colors));

    uint8_t cursor = kLayoutStartColumn;  // center the 28-column HH:MM:SS string in 48 columns
    for (uint8_t i = 0; i < POV_CLOCK_TEXT_LEN && renderer->text[i] != '\0'; ++i) {
        uint8_t width = 0;
        const uint8_t *glyph = glyph_for(renderer->text[i], &width);
        uint32_t color = color_for_text_index(i, renderer->text[i]);
        for (uint8_t x = 0; x < width && cursor < POV_CLOCK_COLUMNS; ++x, ++cursor) {
            uint8_t mask = 0;
            for (uint8_t y = 0; y < kFontRows; ++y) {
                uint8_t bit = (width == 1) ? 0 : (uint8_t)(width - 1u - x);
                if ((glyph[y] & (1u << bit)) != 0u) {
                    mask |= (uint8_t)(1u << y);
                }
            }
            renderer->column_masks[cursor] = mask;
            renderer->column_colors[cursor] = color;
        }
        if (cursor < POV_CLOCK_COLUMNS) {
            cursor++;
        }
    }
}
}  // namespace

void pov_clock_renderer_init(pov_clock_renderer_t *renderer) {
    if (renderer == nullptr) {
        return;
    }
    memset(renderer, 0, sizeof(*renderer));
}

void pov_clock_renderer_set_text(pov_clock_renderer_t *renderer, const char *text) {
    if (renderer == nullptr || text == nullptr) {
        return;
    }
    strncpy(renderer->text, text, sizeof(renderer->text) - 1u);
    renderer->text[sizeof(renderer->text) - 1u] = '\0';
    build_columns(renderer);
    renderer->text_ready = true;
}

bool pov_clock_renderer_step(pov_clock_renderer_t *renderer, uint32_t rotation_period_us,
                             uint64_t now_us) {
    if (renderer == nullptr || rotation_period_us == 0u) {
        return false;
    }

    uint32_t interval = rotation_period_us / (uint32_t)POV_CLOCK_COLUMNS;
    if (interval == 0u) {
        interval = 1u;
    }
    renderer->column_interval_us = interval;

    if (renderer->last_column_us == 0u || (now_us - renderer->last_column_us) >= interval) {
        renderer->last_column_us = now_us;
        renderer->active_column = (uint8_t)((renderer->active_column + 1u) % POV_CLOCK_COLUMNS);
        return true;
    }

    return false;
}

void pov_clock_renderer_clear(uint32_t *frame_words, size_t frame_len) {
    if (frame_words == nullptr) {
        return;
    }
    for (size_t i = 0; i < frame_len; ++i) {
        frame_words[i] = 0;
    }
}

void pov_clock_renderer_render_current(const pov_clock_renderer_t *renderer,
                                       uint32_t *frame_words,
                                       size_t frame_len,
                                       uint8_t active_led_count) {
    if (renderer == nullptr || frame_words == nullptr) {
        return;
    }

    pov_clock_renderer_clear(frame_words, frame_len);
    size_t max_count = active_led_count < frame_len ? active_led_count : frame_len;
    uint8_t mask = renderer->column_masks[renderer->active_column % POV_CLOCK_COLUMNS];
    uint32_t color = renderer->column_colors[renderer->active_column % POV_CLOCK_COLUMNS];
    for (uint8_t y = 0; y < kFontRows; ++y) {
        if ((mask & (uint8_t)(1u << y)) == 0u) {
            continue;
        }
        uint8_t start = (uint8_t)(kTextTop + y * kGlyphScaleY);
        for (uint8_t r = 0; r < kGlyphScaleY; ++r) {
            uint8_t led = (uint8_t)(start + r);
            if (led < max_count) {
                frame_words[led] = color;
            }
        }
    }
}

void pov_clock_renderer_render_status(pov_clock_health_t health,
                                      uint32_t *frame_words,
                                      size_t frame_len,
                                      uint8_t active_led_count) {
    if (frame_words == nullptr) {
        return;
    }
    pov_clock_renderer_clear(frame_words, frame_len);
    size_t max_count = active_led_count < frame_len ? active_led_count : frame_len;

    uint32_t color = 0;
    switch (health) {
        case POV_CLOCK_HEALTH_TIME_UNAVAILABLE:
            color = grb(0, 0, 24);
            break;
        case POV_CLOCK_HEALTH_ROTATION_UNAVAILABLE:
            color = grb(24, 0, 0);
            break;
        case POV_CLOCK_HEALTH_SPEED_TOO_SLOW:
            color = grb(16, 16, 0);
            break;
        case POV_CLOCK_HEALTH_SPEED_TOO_FAST:
            color = grb(0, 24, 24);
            break;
        case POV_CLOCK_HEALTH_SPEED_UNSTABLE:
            color = grb(24, 0, 24);
            break;
        case POV_CLOCK_HEALTH_NORMAL:
        default:
            color = grb(0, 24, 0);
            break;
    }

    for (size_t i = 0; i < max_count; ++i) {
        if ((i % 6u) < 3u) {
            frame_words[i] = color;
        }
    }
}
