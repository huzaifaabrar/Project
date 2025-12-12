#pragma once
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stddef.h>

/*
 * Option B: chunked acquisition + ring buffer assembly
 *
 * Behavior:
 * - I2S_CHUNK_MS: each DMA read will request this many milliseconds of audio
 * - I2S_BUFFER_TIME_SEC: total window to assemble (1.0f or 4.0f)
 *
 * The ring buffer will be: SAMPLE_RATE * bytes_per_sample * I2S_BUFFER_TIME_SEC
 * The chunk size will be: SAMPLE_RATE * bytes_per_sample * I2S_CHUNK_MS/1000
 *
 * The code will try PSRAM first and fall back to heap if PSRAM is not available.
 */

// Window duration (seconds) to assemble for the FFT window (1.0 or 4.0)
#define I2S_BUFFER_TIME_SEC    1.0f

// Chunk length (ms) per DMA read. Keep small for low-latency, e.g. 10-50 ms.
#define I2S_CHUNK_MS           20u

// I2S config constants
#define I2S_SAMPLE_RATE_HZ     48000u
#define I2S_SAMPLE_BITS        32u     // 32-bit I2S slots holding 24-bit audio

// Task config
#define TASK_I2S_READER_NAME      "TaskI2SReader"
#define TASK_I2S_READER_STACK     4096
#define TASK_I2S_READER_PRIORITY  5

// Exposed globals
extern i2s_chan_handle_t xI2S_RXChanHandle;
extern size_t xI2S_uReadBufferSizeBytes; // total bytes in the assembled window
extern size_t xI2S_uChunkBytes;          // bytes per chunk (I2S_CHUNK_MS)
extern size_t xI2S_uNumChunks;           // number of chunks per window

// Lifecycle
void vI2S_InitRX(void);
void vI2S_StartReaderTask(void);

// Callback invoked when a full window is assembled.
// Weak symbol — implement this in your app or override it.
void vI2S_WindowReadyCallback(void *pWindowBuffer, size_t uBytes);
