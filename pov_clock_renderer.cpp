#include "pov_clock_renderer.h"

#include <math.h>
#include <string.h>

namespace {
constexpr uint8_t kFontRows = 5;
constexpr uint8_t kGlyphWidth = 3;
constexpr uint8_t kColonWidth = 1;

// Round-display geometry (feature 018): the 57 LEDs span the disc diameter, so
// the display is an upright 57x57 Cartesian grid inscribed in the disc. LED index
// kCenter is at the middle; each LED i has signed radius (i - kCenter).
constexpr int kGrid = POV_CLOCK_LED_ROWS;               // 57
constexpr int kCenter = (POV_CLOCK_LED_ROWS - 1) / 2;   // 28
constexpr int kTrigShift = 8;                           // Q8 fixed-point
constexpr int kTrigOne = 1 << kTrigShift;               // 256

// Upright text raster scale inside the 57x57 grid.
constexpr int kScaleX = 2;
constexpr int kScaleY = 3;

uint32_t grb(uint8_t g, uint8_t r, uint8_t b) {
    return ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;
}

constexpr uint32_t kHourColor = (0x20u << 8);       // red in GRB word order
constexpr uint32_t kMinuteColor = (0x20u << 16);    // green in GRB word order
constexpr uint32_t kSecondColor = 0x20u;            // blue in GRB word order
constexpr uint32_t kSeparatorColor = (0x18u << 16) | (0x18u << 8) | 0x18u;
constexpr uint32_t kAlphaColor = (0x20u << 16);     // green in GRB word order

// Palette indices stored per pixel in the Cartesian framebuffer.
enum : uint8_t {
    kPalOff = 0,
    kPalHour = 1,
    kPalMinute = 2,
    kPalSecond = 3,
    kPalSeparator = 4,
    kPalAlpha = 5,
};

const uint32_t kPalette[6] = {
    0u, kHourColor, kMinuteColor, kSecondColor, kSeparatorColor, kAlphaColor,
};

// Upright Cartesian image (palette indices), rebuilt on set_text. File-static to
// keep the ~3.2 KB buffer off the main stack and satisfy static-allocation rules
// (one renderer instance in this application).
uint8_t g_framebuffer[kGrid * kGrid];

// Per-column fixed-point cos/sin (Q8), computed once at first use.
int16_t g_cos256[POV_CLOCK_COLUMNS];
int16_t g_sin256[POV_CLOCK_COLUMNS];
bool g_trig_ready = false;

uint8_t palette_for_text_index(uint8_t index, char c) {
    if (c == ':') {
        return kPalSeparator;
    }
    if (c >= 'A' && c <= 'Z') {
        return kPalAlpha;
    }
    if (index <= 1u) {
        return kPalHour;
    }
    if (index >= 3u && index <= 4u) {
        return kPalMinute;
    }
    if (index >= 6u && index <= 7u) {
        return kPalSecond;
    }
    return kPalSeparator;
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
    static const uint8_t letter_h[kFontRows] = {0b101, 0b101, 0b111, 0b101, 0b101};
    static const uint8_t letter_e[kFontRows] = {0b111, 0b100, 0b111, 0b100, 0b111};
    static const uint8_t letter_l[kFontRows] = {0b100, 0b100, 0b100, 0b100, 0b111};
    static const uint8_t letter_o[kFontRows] = {0b111, 0b101, 0b101, 0b101, 0b111};

    if (c >= '0' && c <= '9') {
        *width = kGlyphWidth;
        return digits[c - '0'];
    }
    if (c == ':') {
        *width = kColonWidth;
        return colon;
    }
    switch (c) {
        case 'H': *width = kGlyphWidth; return letter_h;
        case 'E': *width = kGlyphWidth; return letter_e;
        case 'L': *width = kGlyphWidth; return letter_l;
        case 'O': *width = kGlyphWidth; return letter_o;
        default: break;
    }
    *width = kGlyphWidth;
    return dash;
}

int text_native_width(const char *text) {
    int total = 0;
    int count = 0;
    for (uint8_t i = 0; i < POV_CLOCK_TEXT_LEN && text[i] != '\0'; ++i) {
        uint8_t width = 0;
        (void)glyph_for(text[i], &width);
        total += width;
        ++count;
    }
    if (count > 1) {
        total += (count - 1);  // 1px spacing between glyphs
    }
    return total;
}

// Rasterize the text upright and centered into the Cartesian framebuffer.
void build_framebuffer(const pov_clock_renderer_t *renderer) {
    memset(g_framebuffer, kPalOff, sizeof(g_framebuffer));

    int scaled_w = text_native_width(renderer->text) * kScaleX;
    int scaled_h = kFontRows * kScaleY;
    int x0 = (kGrid - scaled_w) / 2;
    int y0 = (kGrid - scaled_h) / 2;

    int native_x = 0;
    for (uint8_t i = 0; i < POV_CLOCK_TEXT_LEN && renderer->text[i] != '\0'; ++i) {
        uint8_t width = 0;
        const uint8_t *glyph = glyph_for(renderer->text[i], &width);
        uint8_t pal = palette_for_text_index(i, renderer->text[i]);
        for (uint8_t x = 0; x < width; ++x) {
            uint8_t bit = (width == 1) ? 0 : (uint8_t)(width - 1u - x);
            for (uint8_t y = 0; y < kFontRows; ++y) {
                if ((glyph[y] & (1u << bit)) == 0u) {
                    continue;
                }
                for (int ddx = 0; ddx < kScaleX; ++ddx) {
                    for (int ddy = 0; ddy < kScaleY; ++ddy) {
                        int fx = x0 + (native_x + (int)x) * kScaleX + ddx;
                        int fy = y0 + (int)y * kScaleY + ddy;
                        if (fx >= 0 && fx < kGrid && fy >= 0 && fy < kGrid) {
                            g_framebuffer[fy * kGrid + fx] = pal;
                        }
                    }
                }
            }
        }
        native_x += (int)width + 1;  // glyph plus one spacing column
    }
}

void ensure_trig() {
    if (g_trig_ready) {
        return;
    }
    for (int c = 0; c < POV_CLOCK_COLUMNS; ++c) {
        double angle = (2.0 * 3.14159265358979323846 * (double)c) / POV_CLOCK_COLUMNS;
        g_cos256[c] = (int16_t)lround(cos(angle) * kTrigOne);
        g_sin256[c] = (int16_t)lround(sin(angle) * kTrigOne);
    }
    g_trig_ready = true;
}

// Project a signed LED radius onto a Cartesian axis using Q8 trig, rounded to the
// nearest pixel and symmetric about the center.
int project(int dx, int trig) {
    int prod = dx * trig;
    if (prod >= 0) {
        return kCenter + ((prod + (kTrigOne / 2)) >> kTrigShift);
    }
    return kCenter - (((-prod) + (kTrigOne / 2)) >> kTrigShift);
}
}  // namespace

void pov_clock_renderer_init(pov_clock_renderer_t *renderer) {
    if (renderer == nullptr) {
        return;
    }
    memset(renderer, 0, sizeof(*renderer));
    memset(g_framebuffer, kPalOff, sizeof(g_framebuffer));
    ensure_trig();
}

void pov_clock_renderer_set_text(pov_clock_renderer_t *renderer, const char *text) {
    if (renderer == nullptr || text == nullptr) {
        return;
    }
    strncpy(renderer->text, text, sizeof(renderer->text) - 1u);
    renderer->text[sizeof(renderer->text) - 1u] = '\0';
    build_framebuffer(renderer);
    renderer->text_ready = true;
}

bool pov_clock_renderer_step(pov_clock_renderer_t *renderer, uint32_t rotation_period_us,
                             uint64_t phase_reference_us,
                             uint64_t presentation_us) {
    if (renderer == nullptr || rotation_period_us == 0u ||
        phase_reference_us == 0u || presentation_us < phase_reference_us) {
        if (renderer != nullptr) {
            renderer->phase_locked = false;
        }
        return false;
    }

    uint32_t interval = rotation_period_us / (uint32_t)POV_CLOCK_COLUMNS;
    if (interval == 0u) {
        interval = 1u;
    }
    renderer->column_interval_us = interval;

    uint64_t elapsed_us = presentation_us - phase_reference_us;
    uint32_t phase_us = (uint32_t)(elapsed_us % rotation_period_us);
    uint8_t target_column = (uint8_t)(
        ((uint64_t)phase_us * POV_CLOCK_COLUMNS) / rotation_period_us);

    bool schedule_changed = !renderer->phase_locked ||
                            renderer->phase_reference_us != phase_reference_us ||
                            renderer->rotation_period_us != rotation_period_us;
    bool column_changed = target_column != renderer->active_column;

    renderer->phase_reference_us = phase_reference_us;
    renderer->rotation_period_us = rotation_period_us;
    renderer->active_column = target_column;
    renderer->phase_locked = true;
    return schedule_changed || column_changed;
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
    if (!renderer->text_ready) {
        return;
    }
    ensure_trig();
    size_t max_count = active_led_count < frame_len ? active_led_count : frame_len;

    // For the active angular column, project each LED's signed radius (i - center)
    // onto the upright Cartesian framebuffer and emit that pixel's color. This is
    // the inverse of the physical polar sweep, so the displayed image equals the
    // upright framebuffer (feature 018: round-display rendering). Points always
    // lie within radius (i - center) of the center, so they stay inside the disc.
    uint8_t c = (uint8_t)(renderer->active_column % POV_CLOCK_COLUMNS);
    int cosv = (int)g_cos256[c];
    int sinv = (int)g_sin256[c];
    for (size_t i = 0; i < max_count; ++i) {
        int dx = (int)i - kCenter;
        int x = project(dx, cosv);
        int y = project(dx, sinv);
        if (x < 0 || x >= kGrid || y < 0 || y >= kGrid) {
            continue;
        }
        uint8_t pal = g_framebuffer[y * kGrid + x];
        if (pal != kPalOff) {
            frame_words[i] = kPalette[pal];
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
            color = grb(0, 0, 24);   // blue
            break;
        case POV_CLOCK_HEALTH_ROTATION_UNAVAILABLE:
            color = grb(0, 24, 0);   // red
            break;
        case POV_CLOCK_HEALTH_SPEED_TOO_SLOW:
            color = grb(16, 16, 0);  // yellow
            break;
        case POV_CLOCK_HEALTH_SPEED_TOO_FAST:
            color = grb(24, 0, 24);  // cyan
            break;
        case POV_CLOCK_HEALTH_SPEED_UNSTABLE:
            color = grb(0, 24, 24);  // magenta
            break;
        case POV_CLOCK_HEALTH_NORMAL:
        default:
            color = grb(24, 0, 0);   // green
            break;
    }

    for (size_t i = 0; i < max_count; ++i) {
        if ((i % 6u) < 3u) {
            frame_words[i] = color;
        }
    }
}
