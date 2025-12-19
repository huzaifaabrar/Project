#include "fft.h"
#include "i2s_config.h"
#include <stdio.h>
#include "esp_dsp.h"
#include <math.h>


static float vReal[SAMPLE_BUFFER_SIZE];  // Interleaved: [Real, Imag, Real, Imag, ...]
static float vImag[SAMPLE_BUFFER_SIZE];

// Example bin range to monitor (e.g., 2 kHz to 3 kHz)
#define BIN_START       42   // 42 * (48000 / 1024) = 2000 Hz
#define BIN_END         64   // 64 * (48000 / 1024) = 3000 Hz

#define THRESHOLD_DB    -50.0f  // dB threshold for detection
#define DETECT_COUNT    5      // Number of consecutive detections to trigger alarm

static int detectionCounter = 0;

// Forward declarations
static int  normalizeBuffer(int32_t* buffer, float* real, int length);
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
            int n = normalizeBuffer(audioBuffer, vReal, SAMPLE_BUFFER_SIZE);

            // Apply windowing
            applyWindow(vReal, n);

            // Convert real → complex (interleaved)
            for (int i = n - 1; i >= 0; i--)
            {
                vReal[2*i + 0] = vReal[i];
                vReal[2*i + 1] = 0.0f;
            }

            // Perform FFT
            dsps_fft2r_fc32(vReal, n);       // Real FFT
            dsps_bit_rev_fc32(vReal, n);     // Bit-reversal
            
            // // Optional debug
            // printf("Starting samples printing\n");
            // for (int i = 0; i < SAMPLE_BUFFER_SIZE; i++) {
            //         // printf("index %3d:  (%ld)\n", i, samples[i]);   //signed decimal
            //         printf("%f\n", vReal[i]);
            //     }
            // printf("Finished samples printing\n");
            


            // Analyze bins
            analyzeBins(vReal, BIN_START, BIN_END, THRESHOLD_DB);
        }
    }
}

// Normalize 32-bit samples to float [-1.0, 1.0]
static int normalizeBuffer(int32_t *buffer, float *real, int length)
{
    const float scale = 1.0f / 2147483648.0f;   // 2³¹
    // const float scale = 1.0f / 16777216.0f;   // 2²⁴
    // const float scale = 1.0f / 8388608.0f;   // 2²³
    int out = 0;
    for (int i = 0; i < length; i += 2)
    {
        // int32_t sample24 = buffer[i] >> 8;  // Convert 32-bit to 24-bit
        int32_t sample24 = buffer[i];  // Use full 32-bit
        real[out++] = (float)sample24 * scale;
    }
    
    return out;
}

// Apply Hamming window
static void applyWindow(float *data, int length)
{
    for (int i = 0; i < length; i++)
    {
        float w = 0.54f - 0.46f * cosf((2.0f * M_PI * i) / (length - 1));
        data[i] *= w;
    }
}


// Analyze FFT bins for threshold crossing
static void analyzeBins(float *fftData, int startBin, int endBin, float threshold_dB)
{
    const float window_gain = 0.54f;     // Hamming
    const float ref = FFT_SIZE * window_gain;
    bool detected = false;

    
    for (int i = startBin; i <= endBin; i++)
    {
        float re = fftData[2*i];
        float im = fftData[2*i + 1];

        float mag = sqrtf(re * re + im * im);

        // dBFS (safe log)
        float powerDB = 20.0f * log10f((mag / ref) + 1e-12f);

        // Debug (optional)
        // printf("Bin %d: %.2f dBFS\n", i, powerDB);

        if (powerDB > threshold_dB)
        {
            detected = true;
            // printf("Bin %d: %.2f dBFS\n", i, powerDB);
            break;
        }
    }

    if (detected)
        detectionCounter++;
    else if (detectionCounter > 0)
        detectionCounter--;  // decay instead of reset

    if (detectionCounter >= DETECT_COUNT)
    {
        printf("🚨 Alarm triggered!\n");
        
        detectionCounter = 0;
    }
}

// Start FFT Task
void vStartFFTTask(QueueHandle_t xAudioBufferQueue)
{
    xTaskCreate(vFFTProcessorTask, FFT_TASK_NAME, FFT_TASK_STACK, xAudioBufferQueue, FFT_TASK_PRIORITY, NULL);
}
