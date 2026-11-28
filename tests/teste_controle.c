#include "controle.h"
#include <assert.h>
#include <stdio.h>

static void step(controller_t *c, int64_t now, bool front, bool rear)
{
    const sonar_readings_t no_sample = {0};
    controller_step(c, now, front, rear, &no_sample);
}

static void sample_distance(controller_t *c, int index, int64_t now, float cm)
{
    sonar_readings_t sonar = {0};
    sonar.at_ms[index] = now;
    sonar.cm[index] = cm;
    controller_step(c, now, false, false, &sonar);
}

static void sample(controller_t *c, int index, int64_t now)
{
    sample_distance(c, index, now, 25.0f);
}

static void test_start_and_front(void)
{
    controller_t c;
    controller_init(&c, 0);
    step(&c, START_MS - 1, false, false);
    assert(c.state == WAITING && c.motion == STANDBY);
    step(&c, START_MS, false, false);
    assert(c.state == REACQUIRE && c.motion == BRAKE);
    sample(&c, 0, START_MS + 5);
    sample(&c, 0, START_MS + 5);  // A mesma leitura não conta duas vezes.
    assert(c.state == REACQUIRE);
    sample(&c, 0, START_MS + 125);
    assert(c.state == SEARCH && c.motion == ATTACK);
    step(&c, START_MS + 125 + REACQUIRE_MS + 1, false, false);
    assert(c.state == SCAN_TURN && c.motion == ROTATE);
}

static void test_rear_turn(void)
{
    controller_t c;
    controller_init(&c, 0);
    step(&c, 5000, false, false);
    sample(&c, 1, 5005);
    sample(&c, 1, 5125);
    assert(c.state == REAR_BRAKE && c.motion == BRAKE);
    const int64_t turn = 5125 + BRAKE_MS;
    step(&c, turn, false, false);
    assert(c.state == REAR_TURN && c.motion == ROTATE);
    step(&c, turn + HALF_TURN_MS - 1, false, false);
    assert(c.motion == ROTATE);
    step(&c, turn + HALF_TURN_MS, false, false);
    assert(c.state == REACQUIRE && c.motion == BRAKE);
    sample(&c, 0, turn + HALF_TURN_MS + 5);
    sample(&c, 0, turn + HALF_TURN_MS + 125);
    assert(c.motion == ATTACK);
}

static void test_intermittent_front_echo(void)
{
    controller_t c;
    controller_init(&c, 0);
    step(&c, START_MS, false, false);
    sample(&c, 0, 5055);
    sample(&c, 1, 5115);
    sample_distance(&c, 0, 5175, -1.0f);
    sample(&c, 1, 5235);
    assert(c.state == REACQUIRE && c.motion == BRAKE);
    sample(&c, 0, 5295);
    assert(c.state == SEARCH && c.motion == ATTACK);

    sample_distance(&c, 0, 5415, -1.0f);
    sample_distance(&c, 0, 5535, -1.0f);
    assert(c.motion == ATTACK);
    step(&c, 5295 + FRONT_HOLD_MS, false, false);
    assert(c.motion != ATTACK);
}

static void test_edge(bool front)
{
    controller_t c;
    controller_init(&c, 0);
    step(&c, 5000, false, false);
    sample(&c, 0, 5005);
    sample(&c, 0, 5125);
    assert(c.motion == ATTACK);
    step(&c, 5130, front, !front);
    assert(c.state == EDGE_BRAKE && c.motion == BRAKE);
    const int64_t escape = 5130 + BRAKE_MS;
    step(&c, escape, front, !front);
    assert(c.state == ESCAPE && c.motion == (front ? REVERSE : FORWARD));
    step(&c, escape + 5, false, false);
    const int64_t turn_brake = escape + 5 + CLEAR_MARGIN_MS;
    step(&c, turn_brake, false, false);
    assert(c.state == TURN_BRAKE && c.motion == BRAKE);
    step(&c, turn_brake + BRAKE_MS, false, false);
    assert(c.state == EDGE_TURN && c.motion == ROTATE);
    step(&c, turn_brake + BRAKE_MS + HALF_TURN_MS, false, false);
    assert(c.state == REACQUIRE && c.motion == BRAKE);
}

static void test_halt(void)
{
    controller_t c;
    controller_init(&c, 0);
    step(&c, 5000, true, false);
    assert(c.state == HALT && c.motion == BRAKE);

    controller_init(&c, 0);
    step(&c, 5000, false, false);
    step(&c, 5005, true, true);
    assert(c.state == HALT && c.motion == BRAKE);

    controller_init(&c, 0);
    step(&c, 5000, false, false);
    step(&c, 5005, true, false);
    step(&c, 5005 + BRAKE_MS, true, false);
    step(&c, 5005 + BRAKE_MS + ESCAPE_LIMIT_MS, true, false);
    assert(c.state == HALT && c.motion == BRAKE);
}

int main(void)
{
    test_start_and_front();
    test_rear_turn();
    test_intermittent_front_echo();
    test_edge(true);
    test_edge(false);
    test_halt();
    puts("RoboSumo: testes da logica aprovados.");
    return 0;
}
