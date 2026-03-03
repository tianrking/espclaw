/*
 * ESPClaw - net/wifi_manager.h
 * WiFi STA connection manager.
 */
#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>

/* Initialize WiFi STA and connect using NVS or Kconfig credentials */
esp_err_t wifi_mgr_init_and_connect(void);

/* Check if WiFi is connected */
bool wifi_mgr_is_connected(void);

#endif /* WIFI_MANAGER_H */
