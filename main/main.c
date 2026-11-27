#include "config.h"
#include "hardware.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static void apply_motion(motion_t motion)
{
    switch (motion) {
        case STANDBY: motors_set(0, 0); break;
        case BRAKE: motors_brake(); break;
        case FORWARD: motors_set(DRIVE_DUTY, DRIVE_DUTY); break;
        case REVERSE: motors_set(-DRIVE_DUTY, -DRIVE_DUTY); break;
        case ROTATE: motors_set(-TURN_DUTY, TURN_DUTY); break;
    }
}

void app_main(void)
{
    hardware_init();
    controller_t controller;
    controller_init(&controller, now_ms());
    sonar_start();

    TickType_t wake = xTaskGetTickCount();
    int64_t next_log = 0;
    while (true) {
        const int64_t now = now_ms();
        const sonar_readings_t sonar = sonar_snapshot();
        const motion_t previous = controller.motion;
        controller_step(&controller, now, edge_front(), edge_rear(), &sonar);
        if (controller.motion != previous) apply_motion(controller.motion);

        if (!MOTORS_ENABLED && now >= next_log) {
            ESP_LOGI("sensores", "DO frente/tras=%d/%d | sonar frente/tras=%.1f/%.1f cm",
                     gpio_get_level(PIN_EDGE_FRONT), gpio_get_level(PIN_EDGE_REAR),
                     sonar.cm[0], sonar.cm[1]);
            next_log = now + 500;
        }
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(5));
    }
}
