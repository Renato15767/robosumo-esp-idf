#include "hardware.h"
#include "config.h"
#include <stdlib.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "esp_log.h"

static const char *TAG = "hardware";
static portMUX_TYPE sonar_lock = portMUX_INITIALIZER_UNLOCKED;
static sonar_readings_t readings = {.cm = {-1, -1}, .at_ms = {0, 0}};
static QueueHandle_t echo_queue;
static int active_echo = -1;
static int64_t rise_us;

int64_t now_ms(void) { return esp_timer_get_time() / 1000; }
bool edge_front(void) { return gpio_get_level(PIN_EDGE_FRONT) == EDGE_FRONT_WHITE_LEVEL; }
bool edge_rear(void) { return gpio_get_level(PIN_EDGE_REAR) == EDGE_REAR_WHITE_LEVEL; }
static void pwm(int channel, int value) {
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, value));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, channel));
}

void motors_brake(void) {
    if (!MOTORS_ENABLED) { motors_set(0, 0); return; }
    gpio_set_level(PIN_STBY, 0);
    pwm(0, 0); pwm(1, 0);
    gpio_set_level(PIN_AIN1, 1); gpio_set_level(PIN_AIN2, 1);
    gpio_set_level(PIN_BIN1, 1); gpio_set_level(PIN_BIN2, 1);
    // Ambos IN altos: saidas baixas e motor eletricamente freado (datasheet).
    gpio_set_level(PIN_STBY, 1);
}

static int limited(int v) {
    if (v > MAX_DUTY) return MAX_DUTY;
    if (v < -MAX_DUTY) return -MAX_DUTY;
    return v;
}

void motors_set(int left, int right) {
    static int old_l = 0, old_r = 0;
    if (!MOTORS_ENABLED) left = right = 0;
    left = limited(left) * MOTOR_LEFT_SIGN;
    right = limited(right) * MOTOR_RIGHT_SIGN;
    if (!left && !right) {
        gpio_set_level(PIN_STBY, 0);
        pwm(0, 0); pwm(1, 0);
        old_l = old_r = 0;
        return;
    }
    // Interrompe a excitacao antes de inverter. Nao e um limitador de corrente.
    if ((left * old_l < 0) || (right * old_r < 0)) {
        gpio_set_level(PIN_STBY, 0);
        pwm(0, 0); pwm(1, 0);
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    gpio_set_level(PIN_AIN1, left > 0);
    gpio_set_level(PIN_AIN2, left < 0);
    gpio_set_level(PIN_BIN1, right > 0);
    gpio_set_level(PIN_BIN2, right < 0);
    pwm(0, abs(left)); pwm(1, abs(right));
    gpio_set_level(PIN_STBY, 1);
    old_l = left; old_r = right;
}

static void echo_isr(void *arg) {
    const int pin = (int)(intptr_t)arg;
    int64_t duration = 0;
    portENTER_CRITICAL_ISR(&sonar_lock);
    if (pin == active_echo) {
        if (gpio_get_level(pin)) rise_us = esp_timer_get_time();
        else if (rise_us > 0) {
            duration = esp_timer_get_time() - rise_us;
            active_echo = -1;
            rise_us = 0;
        }
    }
    portEXIT_CRITICAL_ISR(&sonar_lock);
    if (duration > 0) {
        BaseType_t woken = pdFALSE;
        xQueueOverwriteFromISR(echo_queue, &duration, &woken);
        if (woken) portYIELD_FROM_ISR();
    }
}

static void sonar_task(void *arg) {
    (void)arg;
    const int trig[2] = {PIN_TRIG_FRONT, PIN_TRIG_REAR};
    const int echo[2] = {PIN_ECHO_FRONT, PIN_ECHO_REAR};
    TickType_t previous = xTaskGetTickCount();
    for (int side = 0;; side ^= 1) {
        float cm = -1;
        int64_t duration;
        const int64_t sampled_at = now_ms();
        xQueueReset(echo_queue);
        if (!gpio_get_level(echo[side])) {
            portENTER_CRITICAL(&sonar_lock);
            active_echo = echo[side]; rise_us = 0;
            portEXIT_CRITICAL(&sonar_lock);
            gpio_set_level(trig[side], 1);
            esp_rom_delay_us(10);
            gpio_set_level(trig[side], 0);
            // A tarefa de controle continua verificando a borda enquanto o sonar espera.
            if (xQueueReceive(echo_queue, &duration, pdMS_TO_TICKS(ECHO_TIMEOUT_MS)) == pdTRUE &&
                duration >= 116 && duration <= 23324) cm = duration * 0.01715f;
        }
        portENTER_CRITICAL(&sonar_lock);
        active_echo = -1; rise_us = 0;
        readings.cm[side] = cm;
        readings.at_ms[side] = sampled_at;
        portEXIT_CRITICAL(&sonar_lock);
        vTaskDelayUntil(&previous, pdMS_TO_TICKS(SONAR_GAP_MS));
    }
}

sonar_readings_t sonar_snapshot(void) {
    portENTER_CRITICAL(&sonar_lock);
    sonar_readings_t copy = readings;
    portEXIT_CRITICAL(&sonar_lock);
    return copy;
}

void sonar_start(void) {
    echo_queue = xQueueCreate(1, sizeof(int64_t));
    configASSERT(echo_queue);
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_set_intr_type(PIN_ECHO_FRONT, GPIO_INTR_ANYEDGE));
    ESP_ERROR_CHECK(gpio_set_intr_type(PIN_ECHO_REAR, GPIO_INTR_ANYEDGE));
    ESP_ERROR_CHECK(gpio_isr_handler_add(PIN_ECHO_FRONT, echo_isr, (void *)(intptr_t)PIN_ECHO_FRONT));
    ESP_ERROR_CHECK(gpio_isr_handler_add(PIN_ECHO_REAR, echo_isr, (void *)(intptr_t)PIN_ECHO_REAR));
    configASSERT(xTaskCreate(sonar_task, "sonar", 3072, NULL, 4, NULL) == pdPASS);
}

void hardware_init(void) {
    gpio_config_t out = {
        .pin_bit_mask = (1ULL << PIN_AIN1) | (1ULL << PIN_AIN2) |
            (1ULL << PIN_BIN1) | (1ULL << PIN_BIN2) | (1ULL << PIN_STBY) |
            (1ULL << PIN_TRIG_FRONT) | (1ULL << PIN_TRIG_REAR),
        .mode = GPIO_MODE_OUTPUT
    };
    ESP_ERROR_CHECK(gpio_config(&out));
    gpio_set_level(PIN_STBY, 0);
    gpio_set_level(PIN_TRIG_FRONT, 0); gpio_set_level(PIN_TRIG_REAR, 0);
    gpio_config_t in = {
        .pin_bit_mask = (1ULL << PIN_EDGE_FRONT) | (1ULL << PIN_EDGE_REAR),
        .mode = GPIO_MODE_INPUT
    };
    ESP_ERROR_CHECK(gpio_config(&in));
    in.pin_bit_mask = (1ULL << PIN_ECHO_FRONT) | (1ULL << PIN_ECHO_REAR);
    ESP_ERROR_CHECK(gpio_config(&in));
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE, .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0, .freq_hz = PWM_HZ, .clk_cfg = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));
    const int pins[2] = {PIN_PWMA, PIN_PWMB};
    for (int i = 0; i < 2; ++i) {
        ledc_channel_config_t ch = {.gpio_num = pins[i], .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = i, .timer_sel = LEDC_TIMER_0, .duty = 0, .hpoint = 0};
        ESP_ERROR_CHECK(ledc_channel_config(&ch));
    }
    motors_set(0, 0);
    ESP_LOGI(TAG, "Motores %s. GPIO33/32 (frente/tras): branco = %d/%d.",
        MOTORS_ENABLED ? "habilitados" : "bloqueados",
        EDGE_FRONT_WHITE_LEVEL, EDGE_REAR_WHITE_LEVEL);
}
