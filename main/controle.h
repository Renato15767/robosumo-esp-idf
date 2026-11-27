#pragma once

#include <stdbool.h>
#include <stdint.h>

#define START_MS 5000
#define HALF_TURN_MS 480
#define BRAKE_MS 75
#define CLEAR_MARGIN_MS 110
#define ESCAPE_LIMIT_MS 550
#define SCAN_STEP_MS 50
#define SONAR_GAP_MS 60
#define ECHO_TIMEOUT_MS 5
// Tempo para cada sonar devolver duas amostras novas após a parada.
#define REACQUIRE_MS (4 * SONAR_GAP_MS + ECHO_TIMEOUT_MS + 20)
#define TARGET_CM 60.0f

typedef enum { STANDBY, BRAKE, FORWARD, REVERSE, ROTATE } motion_t;
typedef enum {
    WAITING, SEARCH, SCAN_TURN, REACQUIRE, REAR_BRAKE, REAR_TURN,
    EDGE_BRAKE, ESCAPE, TURN_BRAKE, EDGE_TURN, HALT,
} state_t;

typedef struct {
    float cm[2];       // Índice 0: frente. Índice 1: traseira.
    int64_t at_ms[2];  // Identifica uma nova medição, mesmo se ela não tiver eco.
} sonar_readings_t;

typedef struct {
    state_t state;
    motion_t motion;
    motion_t escape_motion;
    int64_t state_since;
    int64_t clear_since;
    int64_t last_sample[2];
    unsigned hits[2];
    bool rear_latched;
} controller_t;

void controller_init(controller_t *c, int64_t now);
void controller_step(controller_t *c, int64_t now, bool front_white, bool rear_white,
                     const sonar_readings_t *sonar);
