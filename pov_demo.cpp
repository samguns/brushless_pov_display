#include "pov_demo.h"

#include <string.h>

namespace {
constexpr uint8_t kBrightness = 0x20;

uint32_t grb(uint8_t g, uint8_t r, uint8_t b) {
    return ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;
}

uint32_t color_for_char(char c) {
    switch (c) {
        case 'H':
            return grb(0, kBrightness, 0);
        case 'e':
            return grb(kBrightness, 0, 0);
        case 'l':
            return grb(0, 0, kBrightness);
        case 'o':
            return grb(kBrightness / 2, kBrightness / 2, kBrightness / 2);
        default:
            return grb(0, 0, 0);
    }
}
}  // namespace

void pov_demo_init(pov_demo_t *demo) {
    if (demo == nullptr) {
        return;
    }

    memset(demo, 0, sizeof(*demo));
    demo->sequence.chars[0] = 'H';
    demo->sequence.chars[1] = 'e';
    demo->sequence.chars[2] = 'l';
    demo->sequence.chars[3] = 'l';
    demo->sequence.chars[4] = 'o';
    demo->sequence.length = POV_HELLO_SEQUENCE_LENGTH;
    demo->sequence.duration_ms = POV_CHARACTER_DURATION_MS;
    demo->sequence.loop_enabled = true;
}

void pov_demo_start(pov_demo_t *demo, uint32_t now_ms) {
    if (demo == nullptr) {
        return;
    }

    demo->playback.started = true;
    demo->playback.current_index = 0;
    demo->playback.last_transition_ms = now_ms;
    demo->playback.cycles_completed = 0;
}

bool pov_demo_step(pov_demo_t *demo, uint32_t now_ms, bool *transitioned, uint32_t *elapsed_ms) {
    if (transitioned != nullptr) {
        *transitioned = false;
    }

    if (elapsed_ms != nullptr) {
        *elapsed_ms = 0;
    }

    if (demo == nullptr || !demo->playback.started || demo->sequence.length == 0) {
        return false;
    }

    uint32_t delta_ms = now_ms - demo->playback.last_transition_ms;
    if (elapsed_ms != nullptr) {
        *elapsed_ms = delta_ms;
    }

    if (delta_ms < demo->sequence.duration_ms) {
        return false;
    }

    demo->playback.last_transition_ms = now_ms;
    demo->playback.current_index++;
    if (demo->playback.current_index >= demo->sequence.length) {
        demo->playback.current_index = 0;
        demo->playback.cycles_completed++;
    }

    if (transitioned != nullptr) {
        *transitioned = true;
    }

    return true;
}

char pov_demo_current_char(const pov_demo_t *demo) {
    if (demo == nullptr || demo->sequence.length == 0) {
        return '?';
    }

    return demo->sequence.chars[demo->playback.current_index];
}

void pov_demo_render_frame(const pov_demo_t *demo,
                           uint32_t *frame_words,
                           size_t frame_len,
                           uint8_t active_led_count) {
    if (demo == nullptr || frame_words == nullptr || frame_len == 0) {
        return;
    }

    uint8_t max_count = (active_led_count > frame_len) ? (uint8_t)frame_len : active_led_count;
    uint32_t color = color_for_char(pov_demo_current_char(demo));

    for (size_t i = 0; i < frame_len; ++i) {
        frame_words[i] = (i < max_count) ? color : 0;
    }
}
