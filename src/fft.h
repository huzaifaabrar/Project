#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// FFT Task Config
#define FFT_TASK_NAME         "TaskFFTProcessor"
#define FFT_TASK_STACK        8192
#define FFT_TASK_PRIORITY     6

// Function Prototypes
void vFFTProcessorTask(void* pvParameters);
void vStartFFTTask(QueueHandle_t xAudioBufferQueue);