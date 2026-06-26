#ifndef POV_DEMO_H
#define POV_DEMO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    POV_HELLO_SEQUENCE_LENGTH = 5,
    POV_CHARACTER_DURATION_MS = 1000,
};

typedef struct {
    char chars[POV_HELLO_SEQUENCE_LENGTH];
    uint8_t length;
    uint16_t duration_ms;
    bool loop_enabled;
} pov_demo_sequence_t;

typedef struct {
    uint8_t current_index;
    bool started;
    uint32_t last_transition_ms;
    uint32_t cycles_completed;
} pov_playback_state_t;

typedef struct {
    pov_demo_sequence_t sequence;
    pov_playback_state_t playback;
} pov_demo_t;

void pov_demo_init(pov_demo_t *demo);

void pov_demo_start(pov_demo_t *demo, uint32_t now_ms);

bool pov_demo_step(pov_demo_t *demo, uint32_t now_ms, bool *transitioned, uint32_t *elapsed_ms);

char pov_demo_current_char(const pov_demo_t *demo);

void pov_demo_render_frame(const pov_demo_t *demo,
                           uint32_t *frame_words,
                           size_t frame_len,
                           uint8_t active_led_count);

#ifdef __cplusplus
}
#endif

#endif
