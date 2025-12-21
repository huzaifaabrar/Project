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

// Task config
#define TASK_WEB_READER_NAME      "WebNotifyTask"
#define TASK_WEB_READER_STACK     4096
#define TASK_WEB_READER_PRIORITY  3

// Queue handle for fire alarm events
extern QueueHandle_t xFireAlarmEventQueue;

/**
 * @brief Start HTTP + WebSocket server
 *        Call this AFTER Wi-Fi is connected
 */
void vWebServerStart(void);

/**
 * @brief Send fire alarm notification to all clients
 */
void webserver_send_alarm(void);
