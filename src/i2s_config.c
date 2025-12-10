#include "i2s_config.h"

// -------------------------
// GLOBALS
// -------------------------

i2s_chan_handle_t xI2S_RXChanHandle = NULL;

size_t xI2S_uReadBufferSizeBytes = 0;
size_t xI2S_uNumSamples = 0;

// Double buffers (ping–pong)
int32_t *activeBuffer = NULL;
int32_t *inactiveBuffer = NULL;

// I2S reader task
static void vTaskI2SReader(void *pvParameters)
{
    size_t bytesRead = 0;

    for (;;)
    {
        // Blocking read fills activeBuffer
        esp_err_t err = i2s_channel_read(
            xI2S_RXChanHandle,
            activeBuffer,
            xI2S_uReadBufferSizeBytes,
            &bytesRead,
            portMAX_DELAY);

        if (err == ESP_OK && bytesRead > 0)
        {
            // Swap ping–pong buffers
            int32_t *temp = activeBuffer;
            activeBuffer  = inactiveBuffer;
            inactiveBuffer = temp;

            // Now inactiveBuffer has a FULL 4-second window
            printf("Received %.2f sec audio (%u bytes)\n",
                   I2S_BUFFER_TIME_SEC, (unsigned)bytesRead);

            printf("First sample: %ld\n", inactiveBuffer[0]);
        }
    }
}


// Initialize I2S RX channel
void vI2S_InitRX(void)
{
    esp_err_t xErr;

    // Calculate read buffer size based on time duration
    xI2S_uReadBufferSizeBytes =
        (size_t)(I2S_SAMPLE_RATE_HZ *
                 (I2S_SAMPLE_BITS / 8) *
                 1 *
                 I2S_BUFFER_TIME_SEC);

    xI2S_uNumSamples = xI2S_uReadBufferSizeBytes / sizeof(int32_t);

    ESP_LOGI("INMP441",
        "Allocating %.2f seconds (%u bytes, %u samples)",
        I2S_BUFFER_TIME_SEC,
        (unsigned)xI2S_uReadBufferSizeBytes,
        (unsigned)xI2S_uNumSamples);

    // Allocate ping-pong buffers
    activeBuffer   = malloc(xI2S_uReadBufferSizeBytes);
    inactiveBuffer = malloc(xI2S_uReadBufferSizeBytes);

    configASSERT(activeBuffer != NULL);
    configASSERT(inactiveBuffer != NULL);


    // 1) RX channel config
    i2s_chan_config_t xChanCfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 8,
        .dma_frame_num = 512,
        .auto_clear = pdTRUE,
    };

    // 2) Create RX channel
    xErr = i2s_new_channel(&xChanCfg, NULL, &xI2S_RXChanHandle);
    ESP_ERROR_CHECK(xErr);

    // 3) Standard mode clock config
    i2s_std_clk_config_t xClkCfg = I2S_STD_CLK_DEFAULT_CONFIG(I2S_SAMPLE_RATE_HZ);

    // 4) Slot config (Philips mono)
    i2s_std_slot_config_t xSlotCfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
        I2S_SAMPLE_BITS, I2S_SLOT_MODE_MONO
    );

    // 5) GPIO config
    i2s_std_gpio_config_t xGPIOCfg = {
        .mclk = I2S_GPIO_UNUSED,
        .bclk = 5,
        .ws   = 6,
        .dout = I2S_GPIO_UNUSED,
        .din  = 4,
        .invert_flags = {0},
    };

    // 6) Standard mode config
    i2s_std_config_t xStdCfg = {
        .clk_cfg = xClkCfg,
        .slot_cfg = xSlotCfg,
        .gpio_cfg = xGPIOCfg,
    };

    // 7) Initialize channel
    xErr = i2s_channel_init_std_mode(xI2S_RXChanHandle, &xStdCfg);
    ESP_ERROR_CHECK(xErr);

    // 8) Enable RX channel
    xErr = i2s_channel_enable(xI2S_RXChanHandle);
    ESP_ERROR_CHECK(xErr);

    


    ESP_LOGI("INMP441", "I2S RX initialized (Philips mono, 32-bit, %d Hz).", I2S_SAMPLE_RATE_HZ);
}

// Start the reader task
void vI2S_StartReaderTask(void)
{
    BaseType_t xReturned = xTaskCreate(
        vTaskI2SReader,
        TASK_I2S_READER_NAME,
        TASK_I2S_READER_STACK,
        NULL,
        TASK_I2S_READER_PRIORITY,
        NULL
    );

    configASSERT(xReturned == pdPASS);
}
