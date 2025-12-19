#include "fft.h"
#include "i2s_config.h"
#include <stdio.h>
#include "esp_dsp.h"
#include <math.h>

// Constants
#define FFT_SIZE        SAMPLE_BUFFER_SIZE  // 512
#define SAMPLE_RATE     I2S_SAMPLE_RATE_HZ  // 16000

static float vReal[FFT_SIZE * 2];  // Interleaved: [Real, Imag, Real, Imag, ...]

// Example bin range to monitor (e.g., 2 kHz to 3 kHz)
#define BIN_START       64   // 64 * (16000 / 512) = 2000 Hz
#define BIN_END         96   // 96 * (16000 / 512) = 3000 Hz

#define THRESHOLD_DB    10.0f  // dB threshold for detection
#define DETECT_COUNT    5      // Number of consecutive detections to trigger alarm

static int detectionCounter = 0;

// Forward declarations
static void normalizeBuffer(int32_t* buffer, float* real, int length);
static void applyWindow(float* data, int length);
static void analyzeBins(float* fftData, int startBin, int endBin, float threshold);

// FFT Task
void vFFTProcessorTask(void* pvParameters)
{
    QueueHandle_t xAudioBufferQueue = (QueueHandle_t)pvParameters;
    void* buffer;

    // Initialize FFT (once)
    esp_err_t ret = dsps_fft2r_init_fc32(NULL, FFT_SIZE);
    if (ret != ESP_OK)
    {
        printf("Error: Failed to initialize FFT: %d\n", ret);
        vTaskDelete(NULL);
    }

    for (;;)
    {
        if (xQueueReceive(xAudioBufferQueue, &buffer, portMAX_DELAY) == pdTRUE)
        {
            int32_t* audioBuffer = (int32_t*)buffer;



            // Normalize samples to float
            normalizeBuffer(audioBuffer, vReal, FFT_SIZE);

            // Apply windowing
            applyWindow(vReal, FFT_SIZE);

            // Clear imaginary parts
            for (int i = 0; i < FFT_SIZE; i++)
            {
                vReal[i * 2 + 1] = 0.0f;
            }

            // Perform FFT
            dsps_fft2r_fc32(vReal, FFT_SIZE);       // Real FFT
            dsps_bit_rev_fc32(vReal, FFT_SIZE);     // Bit-reversal
    
            // Analyze bins
            analyzeBins(vReal, BIN_START, BIN_END, THRESHOLD_DB);
        }
    }
}

// Normalize 32-bit samples to float [-1.0, 1.0]
static void normalizeBuffer(int32_t* buffer, float* real, int length)
{
    const float scale = 1.0f / 2147483647.0f;   // 2³¹‑1
    for (int i = 0; i < length; i++)
    {
        //printf("index %d: Buffer value original: %.4ld\n", i, buffer[i]);
        real[i] = buffer[i] * scale;
        //real[i * 2 + 0] = (float)(buffer[i] >> 16) / 32768.0f;  // Real
        // real[i * 2 + 0] = (float)(buffer[i]) / 32768.0f;  // Real
        //printf("index %d: Buffer value real: %.4f\n", i, real[i]);
        //real[i * 2 + 1] = 0.0f;  // Imag = 0
    }
}

// Apply Hamming window
static void applyWindow(float* data, int length)
{
    for (int i = 0; i < length; i++)
    {
        data[i * 2] *= 0.54f - 0.46f * cosf(2 * M_PI * i / (length - 1));
    }
}

// Analyze FFT bins for threshold crossing
static void analyzeBins(float* fftData, int startBin, int endBin, float threshold)
{
    bool detected = false;

    for (int i = startBin; i <= endBin; i++)
    {
        float re = fftData[i * 2 + 0];
        float im = fftData[i * 2 + 1];
        float magnitude = sqrtf(re * re + im * im) / FFT_SIZE;
        printf("Bin %d: %.4f\n", i, magnitude);
        float powerDB = 10.0f * log10f(magnitude * magnitude + 1e-12f);
        printf("Bin %d: %.4f\n", i, powerDB);
        if (powerDB > threshold)
        {
            detected = true;
            break;
        }
    }

    if (detected)
    {
        detectionCounter++;
    }
    else
    {
        detectionCounter = 0;
    }

    if (detectionCounter >= DETECT_COUNT)
    {
        printf("🚨 Alarm triggered!\n");
        detectionCounter = 0;  // Reset
    }
}

// Start FFT Task
void vStartFFTTask(QueueHandle_t xAudioBufferQueue)
{
    xTaskCreate(vFFTProcessorTask, FFT_TASK_NAME, FFT_TASK_STACK, xAudioBufferQueue, FFT_TASK_PRIORITY, NULL);
}
