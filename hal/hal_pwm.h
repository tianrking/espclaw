/*
 * ESPClaw - hal/hal_pwm.h
 * PWM HAL interface for LED dimming, motor speed control, etc.
 * Uses ESP-IDF LEDC driver for hardware PWM generation.
 */
#ifndef HAL_PWM_H
#define HAL_PWM_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

/* Default PWM configuration */
#define PWM_DEFAULT_FREQ_HZ     5000    /* 5 kHz - good for LEDs */
#define PWM_MIN_FREQ_HZ         100     /* 100 Hz minimum */
#define PWM_MAX_FREQ_HZ         40000   /* 40 kHz maximum */
#define PWM_MAX_CHANNELS        6       /* LEDC channels available for tools */
#define PWM_MAX_DUTY            100     /* Duty cycle 0-100% */

/**
 * Initialize PWM HAL.
 * Call once at startup.
 */
esp_err_t hal_pwm_init(void);

/**
 * Deinitialize PWM HAL and release all resources.
 */
esp_err_t hal_pwm_deinit(void);

/**
 * Check if a GPIO pin supports PWM output.
 * Returns true if the pin can be used for PWM.
 */
bool hal_pwm_pin_valid(int pin);

/**
 * Setup a PWM channel on the specified pin.
 * @param pin       GPIO pin number
 * @param channel   LEDC channel (0-5)
 * @param freq_hz   Frequency in Hz (100-40000)
 * @return ESP_OK on success
 */
esp_err_t hal_pwm_setup(int pin, int channel, uint32_t freq_hz);

/**
 * Set PWM duty cycle.
 * @param channel   LEDC channel (0-5)
 * @param duty_pct  Duty cycle percentage (0-100)
 * @return ESP_OK on success
 */
esp_err_t hal_pwm_set_duty(int channel, uint32_t duty_pct);

/**
 * Set PWM frequency.
 * @param channel   LEDC channel (0-5)
 * @param freq_hz   New frequency in Hz
 * @return ESP_OK on success
 */
esp_err_t hal_pwm_set_freq(int channel, uint32_t freq_hz);

/**
 * Stop PWM output on a channel.
 * @param channel   LEDC channel (0-5)
 * @return ESP_OK on success
 */
esp_err_t hal_pwm_stop(int channel);

/**
 * Get current PWM status.
 * @param channel   LEDC channel (0-5)
 * @param freq_hz   Output: current frequency
 * @param duty_pct  Output: current duty cycle
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if channel not configured
 */
esp_err_t hal_pwm_get_status(int channel, uint32_t *freq_hz, uint32_t *duty_pct);

/**
 * Check if a channel is currently active.
 */
bool hal_pwm_channel_active(int channel);

#endif /* HAL_PWM_H */
