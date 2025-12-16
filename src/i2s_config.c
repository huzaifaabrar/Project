#include "i2s_config.h"

// ------------------------------------
// GLOBALS
// ------------------------------------
i2s_chan_handle_t xI2S_RXChanHandle = NULL;
TaskHandle_t xMicTaskHandle = NULL;  // Define the task handle


// Ping-pong buffers for small chunks
static uint8_t I2S_Buffer_A[BUFFER_SIZE_BYTES];
static uint8_t I2S_Buffer_B[BUFFER_SIZE_BYTES];

static void *activeBuffer = I2S_Buffer_A;
static void *inactiveBuffer = I2S_Buffer_B;


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
            BUFFER_SIZE_BYTES,
            &bytesRead,
            portMAX_DELAY);

        if (err == ESP_OK && bytesRead == BUFFER_SIZE_BYTES)
        {
            // Swap buffers
            void *tmp = activeBuffer;
            activeBuffer = inactiveBuffer;
            inactiveBuffer = tmp;

            // Optional debug
            printf("Read %u bytes\n", (unsigned)bytesRead);

            // TODO: Send inactiveBuffer to FFT task via queue
        }
        else
        {
            printf("I2S read error: %d, bytes: %u\n", err, (unsigned)bytesRead);
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
        .dma_desc_num = 8,
        .dma_frame_num = 256,  // Smaller for low latency
        .auto_clear = true,
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
    BaseType_t res = xTaskCreate(
        vTaskI2SReader,
        TASK_I2S_READER_NAME,
        TASK_I2S_READER_STACK,
        NULL,
        TASK_I2S_READER_PRIORITY,
        &xMicTaskHandle);  // Save task handle

    configASSERT(res == pdPASS);
}

