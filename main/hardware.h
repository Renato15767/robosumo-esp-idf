#pragma once
#include "controle.h"

void hardware_init(void);
void sonar_start(void);
sonar_readings_t sonar_snapshot(void);
int64_t now_ms(void);
bool edge_front(void);
bool edge_rear(void);

// Chamar somente pela tarefa de controle. Zero/zero desativa STBY (roda livre).
void motors_set(int left, int right);
// TB6612: IN1=IN2=1 e STBY=1 -> short brake, sem girar ao contrario.
void motors_brake(void);
