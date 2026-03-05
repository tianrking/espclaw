/*
 * ESPClaw - hal/hal_servo.h
 * Servo HAL interface for motor control (robotic arms, curtains, camera gimbals).
 * Uses PWM with 50Hz frequency for standard hobby servos.
 */
#ifndef HAL_SERVO_H
#define HAL_SERVO_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

/* Servo PWM configuration */
#define SERVO_FREQ_HZ           50      /* Standard 50 Hz for servos */
#define SERVO_MIN_PULSE_US      500     /* 0.5 ms = -90 degrees */
#define SERVO_MAX_PULSE_US      2500    /* 2.5 ms = +90 degrees */
#define SERVO_CENTER_PULSE_US   1500    /* 1.5 ms = 0 degrees (center) */
#define SERVO_MAX_SERVOS        4       /* Maximum simultaneous servos */
#define SERVO_ANGLE_MIN         -90     /* Minimum angle */
#define SERVO_ANGLE_MAX         90      /* Maximum angle */

/**
 * Initialize Servo HAL.
 * Call once at startup.
 */
esp_err_t hal_servo_init(void);

/**
 * Deinitialize Servo HAL and release all resources.
 */
esp_err_t hal_servo_deinit(void);

/**
 * Check if a GPIO pin supports servo output (PWM capable).
 */
bool hal_servo_pin_valid(int pin);

/**
 * Attach a servo to a GPIO pin.
 * @param pin       GPIO pin number
 * @param servo_id  Servo ID (0-3) for tracking
 * @return ESP_OK on success
 */
esp_err_t hal_servo_attach(int pin, int servo_id);

/**
 * Detach a servo and release its resources.
 * @param servo_id  Servo ID (0-3)
 * @return ESP_OK on success
 */
esp_err_t hal_servo_detach(int servo_id);

/**
 * Set servo angle in degrees.
 * @param servo_id  Servo ID (0-3)
 * @param angle     Angle in degrees (-90 to +90, 0 = center)
 * @return ESP_OK on success
 */
esp_err_t hal_servo_write(int servo_id, int angle);

/**
 * Set servo pulse width directly (for non-standard servos).
 * @param servo_id      Servo ID (0-3)
 * @param pulse_us      Pulse width in microseconds (500-2500)
 * @return ESP_OK on success
 */
esp_err_t hal_servo_write_us(int servo_id, uint32_t pulse_us);

/**
 * Get current servo angle.
 * @param servo_id  Servo ID (0-3)
 * @param angle     Output: current angle in degrees
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if servo not attached
 */
esp_err_t hal_servo_read(int servo_id, int *angle);

/**
 * Check if a servo is currently attached.
 */
bool hal_servo_attached(int servo_id);

/**
 * Get the pin number for an attached servo.
 * @param servo_id  Servo ID (0-3)
 * @return Pin number, or -1 if not attached
 */
int hal_servo_get_pin(int servo_id);

#endif /* HAL_SERVO_H */
