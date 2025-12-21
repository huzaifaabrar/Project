#pragma once
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"


// Event types for web server notifications
typedef enum {
    EVENT_FIRE_ALARM
} web_event_type_t;

typedef struct {
    web_event_type_t type;
    int64_t timestamp_ms;
} web_event_t;

// Queue handle for fire alarm events
extern QueueHandle_t xFireAlarmEventQueue;

/**
 * @brief Start HTTP + WebSocket server
 *        Call this AFTER Wi-Fi is connected
 */
void webserver_start(void);

/**
 * @brief Send fire alarm notification to all clients
 */
void webserver_send_alarm(void);
