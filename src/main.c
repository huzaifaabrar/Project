#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define LED_GPIO 48

static const char *TAG = "BLINK";

static void led_task(void *arg)
{
    // Configure GPIO for output
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

    int level = 0;
    while (1) {
        level = !level;
        gpio_set_level(LED_GPIO, level);
        ESP_LOGI(TAG, "LED GPIO %d -> %d", LED_GPIO, level);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void app_main(void)
{
    xTaskCreate(led_task, "led_task", 2048, NULL, 5, NULL);
}
