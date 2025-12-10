#pragma once
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// I2S buffer length configuration
#define I2S_BUFFER_TIME_SEC    1.0f   // buffer duration in seconds


// I2S config constants
#define I2S_SAMPLE_RATE_HZ    48000
#define I2S_SAMPLE_BITS       32

// FreeRTOS task config
#define TASK_I2S_READER_NAME      "TaskI2SReader"
#define TASK_I2S_READER_STACK     4096
#define TASK_I2S_READER_PRIORITY  5

// Channel handle
extern i2s_chan_handle_t xI2S_RXChanHandle;

// Read buffer size in bytes
extern size_t xI2S_uReadBufferSizeBytes;


// Function prototypes
void vI2S_InitRX(void);
void vI2S_StartReaderTask(void);
