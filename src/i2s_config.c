#include "i2s_config.h"
#include <string.h>             // memcpy
#include <stdlib.h>             // malloc, free
#include "esp_heap_caps.h"      // heap_caps_malloc
#include "esp_err.h"

// -------------------------
// GLOBALS
// -------------------------
i2s_chan_handle_t xI2S_RXChanHandle = NULL;
size_t xI2S_uReadBufferSizeBytes = 0;
size_t xI2S_uChunkBytes = 0;
size_t xI2S_uNumChunks = 0;

// Ring buffer pointer (allocated at init)
static uint8_t *pRingBuffer = NULL;

// Small chunk buffers (stack-safe size). CHUNK_BYTES is compile-time safe macro:
#define BYTES_PER_SAMPLE   (I2S_SAMPLE_BITS / 8U)
#define CHUNK_BYTES_STATIC ((I2S_SAMPLE_RATE_HZ * BYTES_PER_SAMPLE * I2S_CHUNK_MS) / 1000U)

static uint8_t chunkA[CHUNK_BYTES_STATIC];
static uint8_t chunkB[CHUNK_BYTES_STATIC];
static uint8_t *pActiveChunk = chunkA;
static uint8_t *pInactiveChunk = chunkB;

// Writer state into ring buffer
static size_t writeOffset = 0;
static size_t chunksFilled = 0;

// Forward declarations
static void prvAllocateRingBufferOrAdjust(void);
static void prvReaderTaskInitI2S(void);

// -------------------------
// Weak callback default
// -------------------------
__attribute__((weak))
void vI2S_WindowReadyCallback(void *pWindowBuffer, size_t uBytes)
{
    // Default implementation: just log. Override in application for FFT processing.
    ESP_LOGI("INMP441", "Window ready callback: %u bytes at %p", (unsigned)uBytes, pWindowBuffer);
}

// -------------------------
// READER TASK
// -------------------------
static void vTaskI2SReader(void *pvParameters)
{
    size_t bytesRead = 0;

    while (1)
    {
        // Read one chunk into the active small chunk buffer
        esp_err_t err = i2s_channel_read(
            xI2S_RXChanHandle,
            pActiveChunk,
            xI2S_uChunkBytes,
            &bytesRead,
            portMAX_DELAY);

        if (err != ESP_OK) {
            ESP_LOGE("INMP441", "i2s_channel_read failed: %d", err);
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if (bytesRead == 0) {
            // no data, try again
            continue;
        }

        // Copy chunk into the ring buffer at current write offset
        if (pRingBuffer != NULL) {
            memcpy(pRingBuffer + writeOffset, pActiveChunk, bytesRead);
        }

        // Advance write offset and chunk counters
        writeOffset += bytesRead;
        chunksFilled++;

        // Swap small chunk buffers (ping-pong)
        uint8_t *tmp = pActiveChunk;
        pActiveChunk = pInactiveChunk;
        pInactiveChunk = tmp;

        // If we've filled the configured number of chunks -> window complete
        if (chunksFilled >= xI2S_uNumChunks) {
            // Call the weak callback with the start of the ring buffer
            vI2S_WindowReadyCallback((void *)pRingBuffer, xI2S_uReadBufferSizeBytes);

            // Reset for next accumulation
            writeOffset = 0;
            chunksFilled = 0;
        }
    }
}

// -------------------------
// Helper: allocate ring buffer in PSRAM if possible,
// otherwise try heap; if allocation fails reduce window to 1s and retry.
// -------------------------
static void prvAllocateRingBufferOrAdjust(void)
{
    // compute target sizes
    xI2S_uChunkBytes = (I2S_SAMPLE_RATE_HZ * BYTES_PER_SAMPLE * I2S_CHUNK_MS) / 1000U;
    xI2S_uReadBufferSizeBytes = (size_t)(I2S_SAMPLE_RATE_HZ * BYTES_PER_SAMPLE * I2S_BUFFER_TIME_SEC);
    xI2S_uNumChunks = xI2S_uReadBufferSizeBytes / xI2S_uChunkBytes;

    ESP_LOGI("INMP441", "Requesting ring buffer: %.2f sec -> %u bytes (%u chunks of %u bytes)",
             I2S_BUFFER_TIME_SEC,
             (unsigned)xI2S_uReadBufferSizeBytes,
             (unsigned)xI2S_uNumChunks,
             (unsigned)xI2S_uChunkBytes);

    // Try PSRAM first (if present)
    pRingBuffer = (uint8_t *) heap_caps_malloc(xI2S_uReadBufferSizeBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (pRingBuffer != NULL) {
        ESP_LOGI("INMP441", "Allocated ring buffer in PSRAM at %p", pRingBuffer);
        return;
    }

    // Fallback: try regular heap
    pRingBuffer = (uint8_t *) malloc(xI2S_uReadBufferSizeBytes);
    if (pRingBuffer != NULL) {
        ESP_LOGI("INMP441", "Allocated ring buffer in heap at %p", pRingBuffer);
        return;
    }

    // Allocation failed. Try to reduce window to 1.0 sec and try again.
    if (I2S_BUFFER_TIME_SEC > 1.0f) {
        size_t newBytes = (size_t)(I2S_SAMPLE_RATE_HZ * BYTES_PER_SAMPLE * 1.0f);
        ESP_LOGW("INMP441", "Ring allocation failed. Trying 1.0 sec window (%u bytes).", (unsigned)newBytes);

        // Update globals for 1s
        xI2S_uReadBufferSizeBytes = newBytes;
        xI2S_uNumChunks = xI2S_uReadBufferSizeBytes / xI2S_uChunkBytes;

        // retry psram then heap
        pRingBuffer = (uint8_t *) heap_caps_malloc(xI2S_uReadBufferSizeBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (pRingBuffer == NULL) {
            pRingBuffer = (uint8_t *) malloc(xI2S_uReadBufferSizeBytes);
        }

        if (pRingBuffer != NULL) {
            ESP_LOGI("INMP441", "Allocated reduced ring buffer at %p (%u bytes)", pRingBuffer, (unsigned)xI2S_uReadBufferSizeBytes);
            return;
        }
    }

    // Still failed — fatal for this feature
    ESP_LOGE("INMP441", "Failed to allocate ring buffer. Acquisition cannot proceed.");
    configASSERT(pRingBuffer != NULL);
}

// -------------------------
// I2S RX initialization (unchanged semantics)
// -------------------------
void vI2S_InitRX(void)
{
    esp_err_t xErr;

    // compute chunk and window sizes and allocate ring buffer
    prvAllocateRingBufferOrAdjust();

    // Configure channel
    i2s_chan_config_t chanCfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 8,
        .dma_frame_num = 512,
        .auto_clear = pdTRUE,
    };

    xErr = i2s_new_channel(&chanCfg, NULL, &xI2S_RXChanHandle);
    ESP_ERROR_CHECK(xErr);

    i2s_std_clk_config_t clkCfg = I2S_STD_CLK_DEFAULT_CONFIG(I2S_SAMPLE_RATE_HZ);

    i2s_std_slot_config_t slotCfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_SAMPLE_BITS, I2S_SLOT_MODE_MONO);

    i2s_std_gpio_config_t gpioCfg = {
        .mclk = I2S_GPIO_UNUSED,
        .bclk = 5,
        .ws   = 6,
        .dout = I2S_GPIO_UNUSED,
        .din  = 4,
        .invert_flags = {0},
    };

    i2s_std_config_t stdCfg = {
        .clk_cfg = clkCfg,
        .slot_cfg = slotCfg,
        .gpio_cfg = gpioCfg,
    };

    xErr = i2s_channel_init_std_mode(xI2S_RXChanHandle, &stdCfg);
    ESP_ERROR_CHECK(xErr);

    xErr = i2s_channel_enable(xI2S_RXChanHandle);
    ESP_ERROR_CHECK(xErr);

    ESP_LOGI("INMP441", "I2S RX initialized (mono, %u-bit, %u Hz). Chunk %u bytes, window %u bytes",
             (unsigned)I2S_SAMPLE_BITS,
             (unsigned)I2S_SAMPLE_RATE_HZ,
             (unsigned)xI2S_uChunkBytes,
             (unsigned)xI2S_uReadBufferSizeBytes);
}

// -------------------------
// Start reader task
// -------------------------
void vI2S_StartReaderTask(void)
{
    BaseType_t res = xTaskCreate(
        vTaskI2SReader,
        TASK_I2S_READER_NAME,
        TASK_I2S_READER_STACK,
        NULL,
        TASK_I2S_READER_PRIORITY,
        NULL);

    configASSERT(res == pdPASS);
}
