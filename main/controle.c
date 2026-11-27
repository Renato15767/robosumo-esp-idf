#include "controle.h"

static void enter(controller_t *c, state_t state, motion_t motion, int64_t now)
{
    c->state = state;
    c->motion = motion;
    c->state_since = now;
}

void controller_init(controller_t *c, int64_t now)
{
    *c = (controller_t){.state = WAITING, .motion = STANDBY, .state_since = now};
}

static void reacquire(controller_t *c, int64_t now)
{
    // Descarta as leituras anteriores à nova posição.
    c->hits[0] = c->hits[1] = 0;
    c->last_sample[0] = c->last_sample[1] = now;
    enter(c, REACQUIRE, BRAKE, now);
}

static bool handle_edge(controller_t *c, int64_t now, bool front, bool rear)
{
    const int64_t elapsed = now - c->state_since;
    const bool white = front || rear;

    if (front && rear) {
        enter(c, HALT, BRAKE, now);
        return true;
    }
    if (c->state == EDGE_BRAKE || c->state == ESCAPE) {
        // Verifica o sensor voltado para o caminho da fuga.
        const bool blocked = c->escape_motion == REVERSE ? rear : front;
        if (blocked) {
            enter(c, HALT, BRAKE, now);
        } else if (c->state == EDGE_BRAKE) {
            if (elapsed >= BRAKE_MS) enter(c, ESCAPE, c->escape_motion, now);
        } else if (elapsed >= ESCAPE_LIMIT_MS) {
            enter(c, HALT, BRAKE, now);
        } else if (white) {
            c->clear_since = -1;
        } else if (c->clear_since < 0) {
            c->clear_since = now;
        } else if (now - c->clear_since >= CLEAR_MARGIN_MS) {
            enter(c, TURN_BRAKE, BRAKE, now);
        }
        return true;
    }
    if (white) {
        c->escape_motion = front ? REVERSE : FORWARD;
        c->clear_since = -1;
        c->rear_latched = false;
        enter(c, EDGE_BRAKE, BRAKE, now);
        return true;
    }
    if (c->state == TURN_BRAKE) {
        if (elapsed >= BRAKE_MS) enter(c, EDGE_TURN, ROTATE, now);
        return true;
    }
    if (c->state == EDGE_TURN) {
        if (elapsed >= HALF_TURN_MS) reacquire(c, now);
        return true;
    }
    return false;
}

static void update_targets(controller_t *c, const sonar_readings_t *sonar, int64_t now)
{
    for (int i = 0; i < 2; ++i) {
        const int64_t measured = sonar->at_ms[i];
        if (measured > c->last_sample[i]) {
            c->last_sample[i] = measured;
            const float cm = sonar->cm[i];
            if (cm >= 2.0f && cm <= TARGET_CM) {
                if (c->hits[i] < 2) ++c->hits[i];
            } else {
                c->hits[i] = 0;
                if (i == 1) c->rear_latched = false;
            }
        }
        if (now - c->last_sample[i] > REACQUIRE_MS) c->hits[i] = 0;
    }
}

void controller_step(controller_t *c, int64_t now, bool front_white, bool rear_white,
                     const sonar_readings_t *sonar)
{
    if (c->state == HALT) return;
    if (c->state == WAITING) {
        if (now - c->state_since < START_MS) return;
        if (front_white || rear_white) {
            enter(c, HALT, BRAKE, now);
            return;
        }
        reacquire(c, now);
    }

    // A borda pode interromper qualquer giro ou avanço.
    if (handle_edge(c, now, front_white, rear_white)) return;
    const int64_t elapsed = now - c->state_since;
    if (c->state == REAR_BRAKE) {
        if (elapsed >= BRAKE_MS) enter(c, REAR_TURN, ROTATE, now);
        return;
    }
    if (c->state == REAR_TURN || c->state == SCAN_TURN) {
        const int64_t duration = c->state == REAR_TURN ? HALF_TURN_MS : SCAN_STEP_MS;
        if (elapsed >= duration) reacquire(c, now);
        return;
    }

    update_targets(c, sonar, now);
    const bool ahead = c->hits[0] >= 2;
    const bool behind = c->hits[1] >= 2 && !c->rear_latched;
    if (c->state == REACQUIRE && !ahead && !behind && elapsed < REACQUIRE_MS) return;
    if (ahead) {
        enter(c, SEARCH, FORWARD, now);
    } else if (behind) {
        c->rear_latched = true;
        enter(c, REAR_BRAKE, BRAKE, now);
    } else {
        enter(c, SCAN_TURN, ROTATE, now);
    }
}
