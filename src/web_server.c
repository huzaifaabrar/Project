#include "web_server.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "web_server";
static httpd_handle_t server = NULL;

QueueHandle_t xFireAlarmEventQueue = NULL;
TaskHandle_t xWebNotifyTaskHandle = NULL;

/* Track one WebSocket client (enough for now) */
static int ws_client_fd = -1;

/* Embedded HTML page */
static const char index_html[] =
"<!DOCTYPE html>\n"
"<html>\n"
"<head>\n"
"  <meta charset='utf-8'>\n"
"  <title>Fire Alarm Monitor</title>\n"
"  <style>\n"
"    body { font-family: Arial; text-align: center; margin-top: 50px; }\n"
"    #log { font-family: monospace; text-align: left; margin: 20px auto; width: 80%; height: 300px;\n"
"           overflow-y: scroll; border: 1px solid #ccc; padding: 10px; background-color: #f9f9f9; }\n"
"  </style>\n"
"</head>\n"
"<body>\n"
"  <h1>🚨 ESP32 Fire Alarm Monitor 🚨</h1>\n"
"  <div id='log'>System Normal</div>\n"
"\n"
"  <script>\n"
"    const ws = new WebSocket('ws://' + location.host + '/ws');\n"
"    ws.onmessage = function(event) {\n"
"      const logDiv = document.getElementById('log');\n"
"      const msg = event.data;\n"
"\n"
"      // Append new log line\n"
"      logDiv.innerHTML += msg + '<br>';\n"
"\n"
"      // Scroll to bottom\n"
"      logDiv.scrollTop = logDiv.scrollHeight;\n"
"    };\n"
"  </script>\n"
"</body>\n"
"</html>";


/* HTTP handler for '/' */
static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, index_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* WebSocket handler */
static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET)
    {
        ws_client_fd = httpd_req_to_sockfd(req);
        ESP_LOGI(TAG, "WebSocket client connected (fd=%d)", ws_client_fd);
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt = {
        .final = true,
        .fragmented = false,
        .type = HTTPD_WS_TYPE_TEXT
    };

    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    return ret;
}

/* -----------------------------
 * Send arbitrary JSON message
 * ----------------------------- */
void webserver_send_message(const char *msg)
{
    if (server == NULL || ws_client_fd < 0)
        return;

    httpd_ws_frame_t ws_pkt = {
        .final = true,
        .fragmented = false,
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)msg,
        .len = strlen(msg)
    };

    httpd_ws_send_frame_async(server, ws_client_fd, &ws_pkt);
}


/* -----------------------------
 * Notification task
 * ----------------------------- */
void vWebNotifyTask(void *pvParameters)
{
    web_event_t event;

    for (;;)
    {
        if (xQueueReceive(xFireAlarmEventQueue, &event, portMAX_DELAY) == pdPASS)
        {
            switch(event.type)
            {
                case EVENT_FIRE_ALARM:
                {
                    // Prepare log message
                    char log_msg[128];
                    snprintf(log_msg, sizeof(log_msg), "[%lld ms] 🚨 Fire Alarm detected! at Frequency %d", event.timestamp_ms, event.i);

                    // Print to UART
                    printf("%s\n", log_msg);

                    // Send to WebSocket
                    if (server != NULL && ws_client_fd >= 0)
                    {
                        httpd_ws_frame_t ws_pkt = {
                            .final = true,
                            .fragmented = false,
                            .type = HTTPD_WS_TYPE_TEXT,
                            .payload = (uint8_t *)log_msg,
                            .len = strlen(log_msg)
                        };
                        httpd_ws_send_frame_async(server, ws_client_fd, &ws_pkt);
                    }
                }
                break;
            }
        }
    }
}



/* -----------------------------
 * Start server
 * ----------------------------- */
void vWebServerStart(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 8;

    ESP_LOGI(TAG, "Starting HTTP server...");
    ESP_ERROR_CHECK(httpd_start(&server, &config));

    httpd_uri_t root_uri = {
        .uri      = "/",
        .method   = HTTP_GET,
        .handler  = root_get_handler
    };
    httpd_register_uri_handler(server, &root_uri);

    httpd_uri_t ws_uri = {
        .uri        = "/ws",
        .method     = HTTP_GET,
        .handler    = ws_handler,
        .is_websocket = true
    };
    httpd_register_uri_handler(server, &ws_uri);

    // Create event queue
    xFireAlarmEventQueue = xQueueCreate(5, sizeof(web_event_t));

    // Start notification task pinned to Core 1
    xTaskCreatePinnedToCore(
        vWebNotifyTask,             // Task function
        TASK_WEB_READER_NAME,       // Name
        TASK_WEB_READER_STACK,      // Stack size
        NULL,                       // Parameters
        TASK_WEB_READER_PRIORITY,   // Priority
        &xWebNotifyTaskHandle,      // Task handle
        1                           // Core 1
    );

    ESP_LOGI(TAG, "Web server started");
}
