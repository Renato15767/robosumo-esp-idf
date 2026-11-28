#pragma once

#include "sdkconfig.h"

#define PIN_AIN1 18
#define PIN_AIN2 19
#define PIN_PWMA 25
#define PIN_BIN1 21
#define PIN_BIN2 22
#define PIN_PWMB 26
#define PIN_STBY 23
#define PIN_TRIG_FRONT 17
#define PIN_ECHO_FRONT 35
#define PIN_TRIG_REAR 16
#define PIN_ECHO_REAR 34
#define PIN_EDGE_FRONT 33
#define PIN_EDGE_REAR 32
#define EDGE_FRONT_WHITE_LEVEL 0
#define EDGE_REAR_WHITE_LEVEL 0

#define MOTOR_LEFT_SIGN (-1)
#define MOTOR_RIGHT_SIGN (-1)
#define PWM_HZ 1000
#define MAX_DUTY 255
#define ATTACK_DUTY MAX_DUTY
#define DRIVE_DUTY 128
#define TURN_DUTY 128

#ifdef CONFIG_ROBOSUMO_DIAGNOSTICO
#define MOTORS_ENABLED 0
#else
#define MOTORS_ENABLED 1
#endif
