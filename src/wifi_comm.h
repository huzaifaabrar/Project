#pragma once

#include <stdbool.h>

/**
 * @brief Initialize Wi-Fi in STA mode and connect to AP
 */
void vWifiInitSta(void);

/**
 * @brief Check whether ESP32 is connected and has IP
 *
 * @return true if connected
 */
bool wifi_is_connected(void);
