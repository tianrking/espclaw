/*
 * ESPClaw - tool/tool_servo.c
 * Servo tool handlers for motor control.
 * Included directly by tool_registry.c.
 */
#include "hal/hal_servo.h"
#include "util/json_util.h"
#include "config.h"
#include <stdio.h>

static bool tool_servo_attach(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));
static bool tool_servo_write(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));
static bool tool_servo_write_us(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));
static bool tool_servo_detach(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));
static bool tool_servo_read(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));

static bool tool_servo_attach(const char *input_json, char *result_buf, size_t result_sz)
{
    int pin = -1, servo_id = -1;

    if (!json_get_int(input_json, "pin", &pin) || pin < 0) {
        snprintf(result_buf, result_sz, "Error: missing or invalid 'pin'");
        return false;
    }

    json_get_int(input_json, "id", &servo_id);
    if (servo_id < 0) servo_id = 0;
    if (servo_id >= SERVO_MAX_SERVOS) {
        snprintf(result_buf, result_sz, "Error: id must be 0-%d", SERVO_MAX_SERVOS - 1);
        return false;
    }

    esp_err_t err = hal_servo_attach(pin, servo_id);
    if (err == ESP_ERR_INVALID_ARG) {
        snprintf(result_buf, result_sz, "Error: pin %d not allowed (range: %d-%d)",
                 pin, GPIO_MIN_PIN, GPIO_MAX_PIN);
        return false;
    }
    if (err != ESP_OK) {
        snprintf(result_buf, result_sz, "Error: servo attach failed (%s)", esp_err_to_name(err));
        return false;
    }

    snprintf(result_buf, result_sz, "Servo %d attached to pin %d at center (0°)", servo_id, pin);
    return true;
}

static bool tool_servo_write(const char *input_json, char *result_buf, size_t result_sz)
{
    int servo_id = -1, angle = 0;
    bool has_angle = json_get_int(input_json, "angle", &angle);
    int pulse_us = 0;
    bool has_pulse = json_get_int(input_json, "pulse", &pulse_us);

    if (!json_get_int(input_json, "id", &servo_id) || servo_id < 0) {
        snprintf(result_buf, result_sz, "Error: missing or invalid 'id'");
        return false;
    }

    if (servo_id >= SERVO_MAX_SERVOS) {
        snprintf(result_buf, result_sz, "Error: id must be 0-%d", SERVO_MAX_SERVOS - 1);
        return false;
    }

    esp_err_t err;
    if (has_pulse && pulse_us > 0) {
        /* Use pulse width mode */
        if (pulse_us < SERVO_MIN_PULSE_US || pulse_us > SERVO_MAX_PULSE_US) {
            snprintf(result_buf, result_sz, "Error: pulse must be %d-%d us", SERVO_MIN_PULSE_US, SERVO_MAX_PULSE_US);
            return false;
        }
        err = hal_servo_write_us(servo_id, (uint32_t)pulse_us);
    } else if (has_angle) {
        /* Use angle mode */
        if (angle < SERVO_ANGLE_MIN || angle > SERVO_ANGLE_MAX) {
            snprintf(result_buf, result_sz, "Error: angle must be %d to %d degrees", SERVO_ANGLE_MIN, SERVO_ANGLE_MAX);
            return false;
        }
        err = hal_servo_write(servo_id, angle);
    } else {
        snprintf(result_buf, result_sz, "Error: specify 'angle' or 'pulse'");
        return false;
    }

    if (err == ESP_ERR_INVALID_STATE) {
        snprintf(result_buf, result_sz, "Error: servo %d not attached. Call servo_attach first.", servo_id);
        return false;
    }
    if (err != ESP_OK) {
        snprintf(result_buf, result_sz, "Error: servo write failed (%s)", esp_err_to_name(err));
        return false;
    }

    /* Read back current angle */
    int current_angle;
    hal_servo_read(servo_id, &current_angle);
    snprintf(result_buf, result_sz, "Servo %d: %d°", servo_id, current_angle);
    return true;
}

static bool tool_servo_write_us(const char *input_json, char *result_buf, size_t result_sz)
{
    int servo_id = -1, pulse_us = 0;

    if (!json_get_int(input_json, "id", &servo_id) || servo_id < 0) {
        snprintf(result_buf, result_sz, "Error: missing or invalid 'id'");
        return false;
    }

    if (!json_get_int(input_json, "pulse_us", &pulse_us) || pulse_us < SERVO_MIN_PULSE_US) {
        snprintf(result_buf, result_sz, "Error: missing or invalid 'pulse_us'");
        return false;
    }

    if (servo_id >= SERVO_MAX_SERVOS) {
        snprintf(result_buf, result_sz, "Error: id must be 0-%d", SERVO_MAX_SERVOS - 1);
        return false;
    }

    if (pulse_us > SERVO_MAX_PULSE_US) {
        snprintf(result_buf, result_sz, "Error: pulse_us must be %d-%d", SERVO_MIN_PULSE_US, SERVO_MAX_PULSE_US);
        return false;
    }

    esp_err_t err = hal_servo_write_us(servo_id, (uint32_t)pulse_us);
    if (err == ESP_ERR_INVALID_STATE) {
        snprintf(result_buf, result_sz, "Error: servo %d not attached. Call servo_attach first.", servo_id);
        return false;
    }
    if (err != ESP_OK) {
        snprintf(result_buf, result_sz, "Error: servo write failed (%s)", esp_err_to_name(err));
        return false;
    }

    /* Read back current angle */
    int current_angle;
    hal_servo_read(servo_id, &current_angle);
    snprintf(result_buf, result_sz, "Servo %d: pulse=%dus (%d°)", servo_id, pulse_us, current_angle);
    return true;
}

static bool tool_servo_detach(const char *input_json, char *result_buf, size_t result_sz)
{
    int servo_id = -1;

    if (!json_get_int(input_json, "id", &servo_id) || servo_id < 0) {
        snprintf(result_buf, result_sz, "Error: missing or invalid 'id'");
        return false;
    }

    if (servo_id >= SERVO_MAX_SERVOS) {
        snprintf(result_buf, result_sz, "Error: id must be 0-%d", SERVO_MAX_SERVOS - 1);
        return false;
    }

    esp_err_t err = hal_servo_detach(servo_id);
    if (err != ESP_OK) {
        snprintf(result_buf, result_sz, "Error: servo detach failed (%s)", esp_err_to_name(err));
        return false;
    }

    snprintf(result_buf, result_sz, "Servo %d detached", servo_id);
    return true;
}

static bool tool_servo_read(const char *input_json, char *result_buf, size_t result_sz)
{
    int servo_id = -1;

    if (!json_get_int(input_json, "id", &servo_id) || servo_id < 0) {
        snprintf(result_buf, result_sz, "Error: missing or invalid 'id'");
        return false;
    }

    if (servo_id >= SERVO_MAX_SERVOS) {
        snprintf(result_buf, result_sz, "Error: id must be 0-%d", SERVO_MAX_SERVOS - 1);
        return false;
    }

    int angle;
    esp_err_t err = hal_servo_read(servo_id, &angle);
    if (err == ESP_ERR_NOT_FOUND) {
        snprintf(result_buf, result_sz, "Servo %d: not attached", servo_id);
        return true;
    }
    if (err != ESP_OK) {
        snprintf(result_buf, result_sz, "Error: servo read failed (%s)", esp_err_to_name(err));
        return false;
    }

    int pin = hal_servo_get_pin(servo_id);
    snprintf(result_buf, result_sz, "Servo %d: pin=%d, angle=%d°", servo_id, pin, angle);
    return true;
}
