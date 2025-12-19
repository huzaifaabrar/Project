#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// FFT Task Config
#define FFT_TASK_NAME         "TaskFFTProcessor"
#define FFT_TASK_STACK        8192
#define FFT_TASK_PRIORITY     4

// FFT Settings
#define FFT_SIZE              2048  // Must be power of 2 and <= SAMPLE_BUFFER_SIZE
#define SAMPLE_RATE           I2S_SAMPLE_RATE_HZ 

// Function Prototypes
void vFFTProcessorTask(void* pvParameters);
void vStartFFTTask(QueueHandle_t xAudioBufferQueue);