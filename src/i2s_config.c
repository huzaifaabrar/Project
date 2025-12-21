#include "i2s_config.h"
#include "esp_log.h"

// ------------------------------------
// GLOBALS
// ------------------------------------
i2s_chan_handle_t xI2S_RXChanHandle = NULL;
TaskHandle_t xMicTaskHandle = NULL;  // Define the task handle
QueueHandle_t xAudioBufferQueue = NULL;  // Define the queue handle


// TAG for logging
static const char *TAG = "I2S_Config";

// Ping-pong buffers for small chunks
 int32_t I2S_Buffer_A[SAMPLE_BUFFER_SIZE];
 int32_t I2S_Buffer_B[SAMPLE_BUFFER_SIZE];

 int32_t *activeBuffer = I2S_Buffer_A;
 int32_t *inactiveBuffer = I2S_Buffer_B;


// ------------------------------------
// I2S READER TASK (Timer-driven, small buffer)
// ------------------------------------
void vTaskI2SReader(void *pvParameters)
{
    size_t bytesRead = 0;

    for (;;)
    {
        // Wait for timer notification
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        esp_err_t err = i2s_channel_read(
            xI2S_RXChanHandle,
            activeBuffer,
            BUFFER_READ_SIZE,
            &bytesRead,
            portMAX_DELAY);

        int samplesRead = bytesRead / sizeof(int32_t);    
        if (err == ESP_OK && bytesRead == BUFFER_READ_SIZE)
        {
            // Swap buffers
            void *tmp = activeBuffer;
            activeBuffer = inactiveBuffer;
            inactiveBuffer = tmp;

            /*// Optional debug
            printf("Starting samples printing\n");
            for (int i = 0; i < samplesRead; i++) {
                    // printf("index %3d:  (%ld)\n", i, samples[i]);   //signed decimal
                    printf("%ld\n", inactiveBuffer[i]);
                }
            printf("Finished samples printing\n");
            */
            
            // Send inactiveBuffer to FFT task via queue
            if (xAudioBufferQueue != NULL)
            {
                BaseType_t xStatus = xQueueSendToBack(xAudioBufferQueue, &inactiveBuffer, 0);  // No wait
                if (xStatus != pdPASS)
                {
                    ESP_LOGW(TAG, "Failed to send buffer to queue");
                }
            }

            
        }
        else
        {
            ESP_LOGE(TAG, "I2S read error: %d, bytes: %u", err, (unsigned)bytesRead);
        }
    }
}


// ------------------------------------
// I2S RX INITIALIZATION
// ------------------------------------
void vI2S_InitRX(void)
{
    esp_err_t xErr;

    // Configure RX channel
    i2s_chan_config_t chanCfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 6,
        .dma_frame_num = 240,  // Smaller for low latency
        .auto_clear = false,
    };

    xErr = i2s_new_channel(&chanCfg, NULL, &xI2S_RXChanHandle);
    ESP_ERROR_CHECK(xErr);

    // Clock config
    i2s_std_clk_config_t clkCfg = I2S_STD_CLK_DEFAULT_CONFIG(I2S_SAMPLE_RATE_HZ);

    // Slot config (Mono, 32-bit)
    i2s_std_slot_config_t slotCfg =
        I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_SAMPLE_BITS, I2S_SLOT_MODE_MONO);

    // GPIO config (adjust pins as needed)
    i2s_std_gpio_config_t gpioCfg = {
        .mclk = I2S_GPIO_UNUSED,
        .bclk = 5,
        .ws   = 6,
        .dout = I2S_GPIO_UNUSED,
        .din  = 4,
        .invert_flags = {0},
    };

    // Standard mode config
    i2s_std_config_t stdCfg = {
        .clk_cfg  = clkCfg,
        .slot_cfg = slotCfg,
        .gpio_cfg = gpioCfg,
    };

    xErr = i2s_channel_init_std_mode(xI2S_RXChanHandle, &stdCfg);
    ESP_ERROR_CHECK(xErr);

    xErr = i2s_channel_enable(xI2S_RXChanHandle);
    ESP_ERROR_CHECK(xErr);

    ESP_LOGI("INMP441", "I2S RX initialized: MONO, %d-bit, %d Hz", I2S_SAMPLE_BITS, I2S_SAMPLE_RATE_HZ);


    // Create audio buffer queue
    xAudioBufferQueue = xQueueCreate(5, sizeof(void*));
}

// ------------------------------------
// MICROPHONE TIMER SETUP

esp_timer_handle_t periodicTimer = NULL;

void timerCallback(void* arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(xMicTaskHandle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void setupMicrophoneTimer(void)
{
    esp_timer_create_args_t timerArgs = {
        .callback = &timerCallback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "MicrophoneTimer"
    };
    esp_timer_create(&timerArgs, &periodicTimer);
    esp_timer_start_periodic(periodicTimer, 20000);  // 20 ms period = 50 Hz
}


// ------------------------------------
// START TASK
// ------------------------------------
void vI2S_StartReaderTask(void)
{
    BaseType_t res = xTaskCreatePinnedToCore(
        vTaskI2SReader,
        TASK_I2S_READER_NAME,
        TASK_I2S_READER_STACK,
        NULL,
        TASK_I2S_READER_PRIORITY,
        &xMicTaskHandle, // Save task handle
        1);  

    configASSERT(res == pdPASS);
}

