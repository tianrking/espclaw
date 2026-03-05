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
static bool tool_servo_read(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));
static bool tool_servo_detach(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));

static bool tool_servo_attach(const char *input_json, char *result_buf, size_t result_sz)
{
    int pin = -1, servo_id = 0;

    if (!json_get_int(input_json, "pin", &pin) || pin < 0) {
        snprintf(result_buf, result_sz, "Error: missing or invalid 'pin'");
        return false;
    }

    json_get_int(input_json, "id", &servo_id);
    if (servo_id < 0 || servo_id >= SERVO_MAX_SERVOS) {
        snprintf(result_buf, result_sz, "Error: id must be 0-%d", SERVO_MAX_SERVOS - 1);
        return false;
    }

    if (!hal_servo_pin_valid(pin)) {
        snprintf(result_buf, result_sz, "Error: pin %d not allowed (range: %d-%d)",
                 pin, GPIO_MIN_PIN, GPIO_MAX_PIN);
        return false;
    }

    esp_err_t err = hal_servo_attach(pin, servo_id);
    if (err != ESP_OK) {
        snprintf(result_buf, result_sz, "Error: servo attach failed (%s)", esp_err_to_name(err));
        return false;
    }

    snprintf(result_buf, result_sz, "Servo %d attached to pin %d (center position, 0°)", servo_id, pin);
    return true;
}

static bool tool_servo_write(const char *input_json, char *result_buf, size_t result_sz)
{
    int servo_id = 0, angle = 0;

    json_get_int(input_json, "id", &servo_id);
    if (servo_id < 0 || servo_id >= SERVO_MAX_SERVOS) {
        snprintf(result_buf, result_sz, "Error: id must be 0-%d", SERVO_MAX_SERVOS - 1);
        return false;
    }

    if (!json_get_int(input_json, "angle", &angle)) {
        snprintf(result_buf, result_sz, "Error: missing 'angle'");
        return false;
    }

    /* Clamp angle to valid range */
    if (angle < SERVO_ANGLE_MIN) angle = SERVO_ANGLE_MIN;
    if (angle > SERVO_ANGLE_MAX) angle = SERVO_ANGLE_MAX;

    esp_err_t err = hal_servo_write(servo_id, angle);
    if (err == ESP_ERR_INVALID_STATE) {
        snprintf(result_buf, result_sz, "Error: servo %d not attached. Call servo_attach first.", servo_id);
        return false;
    }
    if (err != ESP_OK) {
        snprintf(result_buf, result_sz, "Error: servo write failed (%s)", esp_err_to_name(err));
        return false;
    }

    snprintf(result_buf, result_sz, "Servo %d: moved to %d°", servo_id, angle);
    return true;
}

static bool tool_servo_write_us(const char *input_json, char *result_buf, size_t result_sz)
{
    int servo_id = 0, pulse_us = 0;

    json_get_int(input_json, "id", &servo_id);
    if (servo_id < 0 || servo_id >= SERVO_MAX_SERVOS) {
        snprintf(result_buf, result_sz, "Error: id must be 0-%d", SERVO_MAX_SERVOS - 1);
        return false;
    }

    if (!json_get_int(input_json, "pulse_us", &pulse_us) || pulse_us < 0) {
        snprintf(result_buf, result_sz, "Error: missing or invalid 'pulse_us'");
        return false;
    }

    /* Clamp pulse width */
    if (pulse_us < SERVO_MIN_PULSE_US) pulse_us = SERVO_MIN_PULSE_US;
    if (pulse_us > SERVO_MAX_PULSE_US) pulse_us = SERVO_MAX_PULSE_US;

    esp_err_t err = hal_servo_write_us(servo_id, (uint32_t)pulse_us);
    if (err == ESP_ERR_INVALID_STATE) {
        snprintf(result_buf, result_sz, "Error: servo %d not attached", servo_id);
        return false;
    }
    if (err != ESP_OK) {
        snprintf(result_buf, result_sz, "Error: servo write failed (%s)", esp_err_to_name(err));
        return false;
    }

    snprintf(result_buf, result_sz, "Servo %d: pulse width %d µs", servo_id, pulse_us);
    return true;
}

static bool tool_servo_read(const char *input_json, char *result_buf, size_t result_sz)
{
    int servo_id = 0;

    json_get_int(input_json, "id", &servo_id);
    if (servo_id < 0 || servo_id >= SERVO_MAX_SERVOS) {
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

    snprintf(result_buf, result_sz, "Servo %d: current angle %d°", servo_id, angle);
    return true;
}

static bool tool_servo_detach(const char *input_json, char *result_buf, size_t result_sz)
{
    int servo_id = 0;

    json_get_int(input_json, "id", &servo_id);
    if (servo_id < 0 || servo_id >= SERVO_MAX_SERVOS) {
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
