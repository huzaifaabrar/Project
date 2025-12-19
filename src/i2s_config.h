#pragma once

#include "driver/i2s_std.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// I2S config constants
#define I2S_SAMPLE_RATE_HZ    48000   // Match your mic specs
#define I2S_SAMPLE_BITS       I2S_DATA_BIT_WIDTH_32BIT      // or 16

// FFT-related settings
#define SAMPLE_BUFFER_SIZE    2048     // Number of samples per FFT frame
#define BYTES_PER_SAMPLE      (I2S_SAMPLE_BITS / 8)
#define BUFFER_SIZE_BYTES     (SAMPLE_BUFFER_SIZE * BYTES_PER_SAMPLE)
#define BUFFER_READ_SIZE      SAMPLE_BUFFER_SIZE * sizeof(int32_t)

// Task config
#define TASK_I2S_READER_NAME      "TaskI2SReader"
#define TASK_I2S_READER_STACK     4096
#define TASK_I2S_READER_PRIORITY  5

// External handles
extern i2s_chan_handle_t xI2S_RXChanHandle;
extern TaskHandle_t xMicTaskHandle;
extern QueueHandle_t xAudioBufferQueue;

// Prototypes
void vI2S_InitRX(void);
void vI2S_StartReaderTask(void);
void setupMicrophoneTimer(void);