#include "i2s_config.h"

// Global channel handle
i2s_chan_handle_t xI2S_RXChanHandle = NULL;

// I2S reader task
static void vTaskI2SReader(void *pvParameters)
{
    // Properly aligned buffer for 32-bit reads
    int32_t plReadBuf[I2S_READ_BUFFER_LEN / sizeof(int32_t)];
    size_t xSamplesRead = 0;

    while (1) {
        esp_err_t xErr = i2s_channel_read(
            xI2S_RXChanHandle,
            (uint8_t *)plReadBuf,
            I2S_READ_BUFFER_LEN,
            &xSamplesRead,
            portMAX_DELAY
        );

        if (xErr == ESP_OK && xSamplesRead > 0) {
            size_t xNumSamples = xSamplesRead / sizeof(int32_t);

            // Process first sample (or add processing loop)
            int32_t lFirstSample = plReadBuf[0];
            printf("Mic Sample: %ld\n", lFirstSample);

            // Optional: loop over all samples
            // for (size_t x = 0; x < xNumSamples; x++) { ... }
        }
    }
}

// Initialize I2S RX channel
void vI2S_InitRX(void)
{
    esp_err_t xErr;

    // 1) RX channel config
    i2s_chan_config_t xChanCfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 4,
        .dma_frame_num = 128,
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
