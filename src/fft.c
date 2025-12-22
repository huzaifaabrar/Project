#include "fft.h"
#include "i2s_config.h"
#include "esp_timer.h"
#include <stdio.h>
#include "esp_dsp.h"
#include "esp_log.h"
#include <math.h>
#include "web_server.h"
#include "config.h"


// TAG for logging
static const char *TAG = "FFT";

static float vReal[SAMPLE_BUFFER_SIZE];  // Interleaved: [Real, Imag, Real, Imag, ...]

#define BIN_START                   ((int)((Freq_START_HZ * FFT_SIZE) / I2S_SAMPLE_RATE_HZ))   // calculate start bin
#define BIN_END                     ((int)((Freq_END_HZ   * FFT_SIZE) / I2S_SAMPLE_RATE_HZ))   // calculate end bin
#if USE_LONG_WINDOW
    #define DETECT_COUNT             2      // Number of consecutive detections to trigger alarm
#else
    #define DETECT_COUNT             5      // Number of consecutive detections to trigger alarm
#endif

#define NUM_BINS                    (FFT_SIZE / 2)  // Number of FFT bins for real FFT
#define LONG_WINDOW_SECONDS         4.0f   // 1 second
#define LONG_WINDOW_FRAMES          ((int)ceil(LONG_WINDOW_SECONDS * I2S_SAMPLE_RATE_HZ / FFT_SIZE))


static float avgSpectrum[NUM_BINS];
static int frameCount = 0;
static int detectionCounter = 0;

// Forward declarations
static int  normalizeBuffer(int32_t* buffer, float* real, int length);
static void applyWindow(float* data, int length);
static void analyzeBins(float* fftData, int startBin, int endBin, float threshold);

// Windowing reference sum for normalization
static float window_sum = 0.0f;
static bool window_initialized = false;

static void initWindowRef(void)
{
    if (window_initialized)
        return;

    for (int n = 0; n < FFT_SIZE; n++)
    {
        float w = 0.54f - 0.46f * cosf((2.0f * M_PI * n) / (FFT_SIZE - 1)); // Hamming
        window_sum += w;
    }

    window_initialized = true;
}



// FFT Task
void vFFTProcessorTask(void* pvParameters)
{
    QueueHandle_t xAudioBufferQueue = (QueueHandle_t)pvParameters;
    void* buffer;

    // Initialize FFT (once)
    esp_err_t ret = dsps_fft2r_init_fc32(NULL, FFT_SIZE);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize FFT: %d", ret);
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
            
            
            // Optional debug
            // printf("Starting samples printing\n");
            // for (int i = 0; i < SAMPLE_BUFFER_SIZE; i += 8) {
            //         printf("index %3d:  (%f)\n", i, vReal[i]);   //signed decimal
            //         // printf("%f\n", vReal[i]);
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
    for (int i = 0; i < length; i ++)
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
    bool detected = false;

#if USE_LONG_WINDOW
    // -------------------------------
    // Long-window averaging
    // -------------------------------
    for (int i = 0; i < NUM_BINS; i++)
    {
        float re = fftData[2*i];
        float im = fftData[2*i + 1];
            if (isnan(im)) {
                im = 0.0f;
            }
            if (isnan(re)) {
                re = 0.0f;
            }
        float mag = sqrtf(re*re + im*im);

        if (i != 0 && i != (FFT_SIZE / 2))
            mag *= 2.0f;

        avgSpectrum[i] = (avgSpectrum[i] * frameCount + mag) / (frameCount + 1);
    }

    frameCount++;

    if (frameCount >= LONG_WINDOW_FRAMES)
    {
        for (int i = startBin; i <= endBin; i++)
        {
            float powerDB = 20.0f * log10f((avgSpectrum[i] / window_sum) + 1e-12f);
            if (powerDB > threshold_dB)
            {
                detected = true;
                break;
            }
        }

        if (detected)
            detectionCounter++;
        else
            detectionCounter = 0;

        if (detectionCounter >= DETECT_COUNT)
        {
            detectionCounter = 0;
        
            // Send event to web server queue
            if (xFireAlarmEventQueue != NULL)
            {
                web_event_t event;
                event.type = EVENT_FIRE_ALARM;
                event.timestamp_ms = esp_timer_get_time() / 1000; // ms since boot 
                BaseType_t xStatus = xQueueSend(xFireAlarmEventQueue, &event, 0); // non-blocking
                if (xStatus != pdPASS)
                {
                    ESP_LOGW(TAG, "Failed to send fire alarm event to queue");
                }
            }        
        }

        frameCount = 0;
        memset(avgSpectrum, 0, sizeof(avgSpectrum));
    }

#else
    // -------------------------------
    // Short-window analysis
    // -------------------------------
    for (int i = startBin; i <= endBin; i++)
    {
        float re = fftData[2*i];
        float im = fftData[2*i + 1];
            if (isnan(im)) {
                im = 0.0f;
            }
            if (isnan(re)) {
                re = 0.0f;
            }
        float mag = sqrtf(re*re + im*im);

        if (i != 0 && i != (FFT_SIZE / 2))
            mag *= 2.0f;

        float powerDB = 20.0f * log10f((mag / window_sum) + 1e-12f);

        // Optional debug 
            // printf("index %3d:  (%f)\n", i, powerDB);   //signed decimal    
            // printf("index %3d:  (%f)\n", i, im);   //signed decimal
            // printf("%f\n", vReal[i]);

        if (powerDB > threshold_dB)
        {
            detected = true;
            break;
        }
    }

    if (detected) 
        detectionCounter++;
    else if (detectionCounter > 0)
        detectionCounter--;

    if (detectionCounter >= DETECT_COUNT)
    {
        detectionCounter = 0;
        
        // Send event to web server queue
        if (xFireAlarmEventQueue != NULL)
        {
            web_event_t event;
            event.type = EVENT_FIRE_ALARM;
            event.timestamp_ms = esp_timer_get_time() / 1000; // ms since boot 
            BaseType_t xStatus = xQueueSend(xFireAlarmEventQueue, &event, 0); // non-blocking
            if (xStatus != pdPASS)
            {
                ESP_LOGW(TAG, "Failed to send fire alarm event to queue");
            }
        }
        
    }
#endif
}


// Start FFT Task
void vStartFFTTask(QueueHandle_t xAudioBufferQueue)
{
    initWindowRef();  // precompute window reference
    xTaskCreatePinnedToCore(
        vFFTProcessorTask,       // Task function
        FFT_TASK_NAME,           // Name
        FFT_TASK_STACK,          // Stack size
        xAudioBufferQueue,       // Parameters
        FFT_TASK_PRIORITY,       // Priority
        NULL,                    // Task handle
        0                        // Core 0
    );
}

