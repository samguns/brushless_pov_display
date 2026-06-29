#ifndef HALL_SENSOR_H
#define HALL_SENSOR_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Shared feature constants and compile-time bounds (T002). */
enum {
    HALL_DEFAULT_PIN = 15,                 /* GP15 */
    HALL_DEFAULT_MAGNETS_PER_REV = 1,      /* single reference magnet */
    HALL_DEFAULT_DEBOUNCE_US = 1000,       /* min inter-event lockout (~1 ms) */
    HALL_DEFAULT_STOP_TIMEOUT_US = 1500000 /* 1.5 s with no edge -> stale/zero */
};

/* Supported measurement window. 60 RPM = 1 Hz (1,000,000 us per rev);
 * 6000 RPM = 100 Hz (10,000 us per rev). Values outside this window are
 * reported as bounded rather than as impossible readings. */
enum {
    HALL_MIN_SUPPORTED_RPM = 60,
    HALL_MAX_SUPPORTED_RPM = 6000
};

/* Time-unit conversion constants (NOT system-clock literals). */
#define HALL_US_PER_MINUTE 60000000ULL
#define HALL_US_PER_SECOND 1000000ULL

/* Static configuration for the Hall input and measurement behavior. */
typedef struct {
    uint8_t pin;              /* input GPIO (default 15 / GP15) */
    bool active_low;          /* true if magnet-present asserts logic low */
    bool pull_up;             /* internal pull-up (false for push-pull output) */
    uint32_t debounce_us;     /* minimum inter-event lockout in microseconds */
    uint8_t magnets_per_rev;  /* magnet passes per revolution (>= 1) */
    uint32_t stop_timeout_us; /* no-event timeout before speed is zeroed/stale */
} hall_sensor_config_t;

/* Interrupt-shared raw capture state. Written by the edge handler; read under a
 * critical section by the derivation path. */
typedef struct {
    volatile uint64_t last_edge_us;     /* timestamp of most recent accepted edge */
    volatile uint64_t prev_edge_us;     /* timestamp of the edge before last */
    volatile uint32_t last_interval_us; /* most recent accepted inter-event interval */
    volatile uint32_t edge_count;       /* accepted edges since init (diagnostics) */
    volatile bool has_two_edges;        /* true once >= 2 edges captured */
} hall_capture_t;

/* Derived, consumer-facing measurement result. */
typedef struct {
    uint32_t period_us;     /* revolution period (interval * magnets_per_rev) */
    float rpm;              /* spinning speed, revolutions per minute */
    float hz;              /* spinning speed, revolutions per second */
    bool valid;            /* true when a fresh in-range measurement exists */
    bool stale;            /* true when no edge within stop_timeout (speed = 0) */
    uint64_t last_update_us;/* timestamp when this result was computed */
} hall_rotation_measurement_t;

/* Top-level driver instance. */
typedef struct {
    hall_sensor_config_t config;
    hall_capture_t capture;
    bool initialized;
} hall_sensor_t;

/* Populate a config with defaults (GP15, push-pull/no pull, active-low, 1 magnet). */
void hall_sensor_init_defaults(hall_sensor_config_t *config);

/* Configure the input pin, register a per-GPIO edge interrupt, reset capture.
 * Returns true on success; non-blocking. Pass NULL config to use defaults. */
bool hall_sensor_init(hall_sensor_t *sensor, const hall_sensor_config_t *config);

/* Disable the interrupt and release the handler. Safe to call once. */
void hall_sensor_deinit(hall_sensor_t *sensor);

/* Non-blocking read: snapshots capture state atomically and derives speed. */
hall_rotation_measurement_t hall_sensor_read(hall_sensor_t *sensor, uint64_t now_us);

/* Convenience getters (each performs a read). */
float hall_sensor_get_rpm(hall_sensor_t *sensor, uint64_t now_us);
float hall_sensor_get_hz(hall_sensor_t *sensor, uint64_t now_us);
uint32_t hall_sensor_get_period_us(hall_sensor_t *sensor, uint64_t now_us);
uint32_t hall_sensor_get_edge_count(const hall_sensor_t *sensor);

/* Pure speed/period derivation over a captured snapshot. Hardware-independent and
 * unit-testable with synthetic timestamps. */
hall_rotation_measurement_t hall_sensor_derive(uint32_t last_interval_us,
                                               uint64_t last_edge_us,
                                               bool has_two_edges,
                                               const hall_sensor_config_t *config,
                                               uint64_t now_us);

#ifdef __cplusplus
}
#endif

#endif
