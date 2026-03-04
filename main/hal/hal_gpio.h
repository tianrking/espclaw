/*
 * ESPClaw - hal/hal_gpio.h
 * GPIO HAL vtable + safety guardrails.
 * Step 6: tools call through this layer; pin validation happens here.
 */
#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include "esp_err.h"
#include <stdbool.h>

/* Initialise GPIO HAL (call once at startup) */
esp_err_t hal_gpio_init(void);

/*
 * Check if a pin is within the Kconfig-configured safety range.
 * Returns true if the tool is allowed to use this pin.
 */
bool hal_gpio_is_allowed(int pin);

/* Write pin HIGH(1) or LOW(0). Returns ESP_ERR_INVALID_ARG if pin not allowed. */
esp_err_t hal_gpio_write(int pin, int level);

/* Read pin state into *level (0 or 1). Returns ESP_ERR_INVALID_ARG if not allowed. */
esp_err_t hal_gpio_read(int pin, int *level);

/* Build a CSV string of all allowed pins into buf[buf_sz] */
void hal_gpio_allowed_pins_str(char *buf, size_t buf_sz);

#endif /* HAL_GPIO_H */
