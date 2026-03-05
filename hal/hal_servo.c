/*
 * ESPClaw - hal/hal_servo.c
 * Servo HAL implementation using LEDC PWM.
 * Standard hobby servo control with 50Hz PWM.
 */
#include "hal_servo.h"
#include "config.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include <string.h>

static const char *TAG = "hal_servo";

/* Servo state tracking */
typedef struct {
    bool attached;
    int pin;
    int angle;          /* Current angle (-90 to +90) */
    uint32_t pulse_us;  /* Current pulse width */
} servo_state_t;

static servo_state_t s_servos[SERVO_MAX_SERVOS];
static bool s_initialized = false;

/* LEDC configuration for servos */
#define SERVO_LEDC_TIMER       LEDC_TIMER_1
#define SERVO_LEDC_SPEED_MODE  LEDC_LOW_SPEED_MODE
#define SERVO_LEDC_DUTY_RES    LEDC_TIMER_16_BIT  /* 16-bit for fine pulse control */

/* Calculate duty for a given pulse width in microseconds */
static uint32_t pulse_to_duty(uint32_t pulse_us)
{
    /* At 50Hz, period = 20000us = 20ms */
    /* 16-bit resolution: max_duty = 65535 */
    /* duty = (pulse_us / 20000us) * 65535 */
    return (pulse_us * 65535) / 20000;
}

/* Convert angle to pulse width */
static uint32_t angle_to_pulse(int angle)
{
    /* Clamp angle */
    if (angle < SERVO_ANGLE_MIN) angle = SERVO_ANGLE_MIN;
    if (angle > SERVO_ANGLE_MAX) angle = SERVO_ANGLE_MAX;

    /* Linear interpolation: -90 -> 500us, 0 -> 1500us, +90 -> 2500us */
    return SERVO_CENTER_PULSE_US + (angle * (SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US) / 180);
}

/* Convert pulse width to angle */
static int pulse_to_angle(uint32_t pulse_us)
{
    if (pulse_us < SERVO_MIN_PULSE_US) pulse_us = SERVO_MIN_PULSE_US;
    if (pulse_us > SERVO_MAX_PULSE_US) pulse_us = SERVO_MAX_PULSE_US;

    return ((int)pulse_us - SERVO_CENTER_PULSE_US) * 180 / (SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US);
}

esp_err_t hal_servo_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    memset(s_servos, 0, sizeof(s_servos));

    /* Configure LEDC timer for servo (50Hz) */
    ledc_timer_config_t timer_conf = {
        .speed_mode      = SERVO_LEDC_SPEED_MODE,
        .duty_resolution = SERVO_LEDC_DUTY_RES,
        .timer_num       = SERVO_LEDC_TIMER,
        .freq_hz         = SERVO_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    esp_err_t err = ledc_timer_config(&timer_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Servo LEDC timer config failed: %s", esp_err_to_name(err));
        return err;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Servo HAL ready: %d servos max", SERVO_MAX_SERVOS);
    return ESP_OK;
}

esp_err_t hal_servo_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    /* Detach all servos */
    for (int i = 0; i < SERVO_MAX_SERVOS; i++) {
        if (s_servos[i].attached) {
            hal_servo_detach(i);
        }
    }

    s_initialized = false;
    ESP_LOGI(TAG, "Servo HAL deinitialized");
    return ESP_OK;
}

bool hal_servo_pin_valid(int pin)
{
    if (pin < GPIO_MIN_PIN || pin > GPIO_MAX_PIN) {
        return false;
    }
    return true;
}

esp_err_t hal_servo_attach(int pin, int servo_id)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (servo_id < 0 || servo_id >= SERVO_MAX_SERVOS) {
        ESP_LOGW(TAG, "Invalid servo ID %d (valid: 0-%d)", servo_id, SERVO_MAX_SERVOS - 1);
        return ESP_ERR_INVALID_ARG;
    }

    if (!hal_servo_pin_valid(pin)) {
        ESP_LOGW(TAG, "Pin %d not valid for servo", pin);
        return ESP_ERR_INVALID_ARG;
    }

    /* Check if already attached to a different pin */
    if (s_servos[servo_id].attached && s_servos[servo_id].pin != pin) {
        ledc_stop(SERVO_LEDC_SPEED_MODE, servo_id, 0);
    }

    /* Configure LEDC channel for this servo */
    ledc_channel_config_t chan_conf = {
        .gpio_num   = pin,
        .speed_mode = SERVO_LEDC_SPEED_MODE,
        .channel    = servo_id,  /* Use servo_id as LEDC channel */
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = SERVO_LEDC_TIMER,
        .duty       = pulse_to_duty(SERVO_CENTER_PULSE_US),  /* Start at center */
        .hpoint     = 0,
    };
    esp_err_t err = ledc_channel_config(&chan_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Servo channel config failed: %s", esp_err_to_name(err));
        return err;
    }

    s_servos[servo_id].attached = true;
    s_servos[servo_id].pin = pin;
    s_servos[servo_id].angle = 0;
    s_servos[servo_id].pulse_us = SERVO_CENTER_PULSE_US;

    ESP_LOGI(TAG, "Servo %d attached to pin %d (center position)", servo_id, pin);
    return ESP_OK;
}

esp_err_t hal_servo_detach(int servo_id)
{
    if (servo_id < 0 || servo_id >= SERVO_MAX_SERVOS) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_servos[servo_id].attached) {
        return ESP_OK;
    }

    ledc_stop(SERVO_LEDC_SPEED_MODE, servo_id, 0);
    s_servos[servo_id].attached = false;
    s_servos[servo_id].pin = -1;

    ESP_LOGI(TAG, "Servo %d detached", servo_id);
    return ESP_OK;
}

esp_err_t hal_servo_write(int servo_id, int angle)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (servo_id < 0 || servo_id >= SERVO_MAX_SERVOS) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_servos[servo_id].attached) {
        ESP_LOGW(TAG, "Servo %d not attached", servo_id);
        return ESP_ERR_INVALID_STATE;
    }

    return hal_servo_write_us(servo_id, angle_to_pulse(angle));
}

esp_err_t hal_servo_write_us(int servo_id, uint32_t pulse_us)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (servo_id < 0 || servo_id >= SERVO_MAX_SERVOS) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_servos[servo_id].attached) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Clamp pulse width */
    if (pulse_us < SERVO_MIN_PULSE_US) pulse_us = SERVO_MIN_PULSE_US;
    if (pulse_us > SERVO_MAX_PULSE_US) pulse_us = SERVO_MAX_PULSE_US;

    uint32_t duty = pulse_to_duty(pulse_us);

    esp_err_t err = ledc_set_duty(SERVO_LEDC_SPEED_MODE, servo_id, duty);
    if (err != ESP_OK) {
        return err;
    }

    err = ledc_update_duty(SERVO_LEDC_SPEED_MODE, servo_id);
    if (err != ESP_OK) {
        return err;
    }

    s_servos[servo_id].pulse_us = pulse_us;
    s_servos[servo_id].angle = pulse_to_angle(pulse_us);

    return ESP_OK;
}

esp_err_t hal_servo_read(int servo_id, int *angle)
{
    if (servo_id < 0 || servo_id >= SERVO_MAX_SERVOS) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_servos[servo_id].attached) {
        return ESP_ERR_NOT_FOUND;
    }

    *angle = s_servos[servo_id].angle;
    return ESP_OK;
}

bool hal_servo_attached(int servo_id)
{
    if (servo_id < 0 || servo_id >= SERVO_MAX_SERVOS) {
        return false;
    }
    return s_servos[servo_id].attached;
}

int hal_servo_get_pin(int servo_id)
{
    if (servo_id < 0 || servo_id >= SERVO_MAX_SERVOS) {
        return -1;
    }
    if (!s_servos[servo_id].attached) {
        return -1;
    }
    return s_servos[servo_id].pin;
}
