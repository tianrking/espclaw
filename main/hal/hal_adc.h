/*
 * ESPClaw - hal/hal_adc.h
 * ADC HAL interface for analog sensors (light, temperature, battery, etc.)
 * Uses ESP-IDF ADC driver with calibration support.
 */
#ifndef HAL_ADC_H
#define HAL_ADC_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

/* ADC configuration limits */
#define ADC_MAX_CHANNELS        5       /* ADC1 channels available for tools */
#define ADC_DEFAULT_ATTEN       3       /* ADC_ATTEN_DB_12 (0-3.3V range) */
#define ADC_DEFAULT_VREF_MV     3300    /* Reference voltage in mV */

/**
 * Initialize ADC HAL with calibration.
 * Call once at startup.
 */
esp_err_t hal_adc_init(void);

/**
 * Deinitialize ADC HAL and release resources.
 */
esp_err_t hal_adc_deinit(void);

/**
 * Check if a GPIO pin supports ADC input.
 * Returns true if the pin has ADC capability.
 */
bool hal_adc_pin_valid(int pin);

/**
 * Get ADC channel number for a GPIO pin.
 * @param pin   GPIO pin number
 * @return ADC channel (0-4), or -1 if invalid
 */
int hal_adc_pin_to_channel(int pin);

/**
 * Read raw ADC value from a channel.
 * @param pin       GPIO pin number
 * @param raw_value Output: raw ADC reading (0-4095 for 12-bit)
 * @return ESP_OK on success
 */
esp_err_t hal_adc_read_raw(int pin, int *raw_value);

/**
 * Read voltage in millivolts from a channel.
 * Uses calibration data for accurate conversion.
 * @param pin       GPIO pin number
 * @param voltage_mv Output: voltage in millivolts
 * @return ESP_OK on success
 */
esp_err_t hal_adc_read_voltage(int pin, int *voltage_mv);

/**
 * Read multiple samples and return averaged value.
 * Useful for noisy signals.
 * @param pin       GPIO pin number
 * @param samples   Number of samples to average (1-64)
 * @param voltage_mv Output: averaged voltage in millivolts
 * @return ESP_OK on success
 */
esp_err_t hal_adc_read_averaged(int pin, int samples, int *voltage_mv);

/**
 * Set ADC attenuation for voltage range.
 * @param pin   GPIO pin number
 * @param atten Attenuation: 0=0-1.1V, 1=0-1.5V, 2=0-2.2V, 3=0-3.3V
 * @return ESP_OK on success
 */
esp_err_t hal_adc_set_atten(int pin, int atten);

#endif /* HAL_ADC_H */
