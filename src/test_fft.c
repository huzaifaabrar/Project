#include <stdio.h>    // Required for printf
#include "esp_dsp.h"
#include <math.h>

#define FFT_SIZE 512
// Interleaved buffer: [Real0, Imag0, Real1, Imag1, ...]
static float fft_buffer[FFT_SIZE * 2]; 

void test_fft(void)
{
    // 1. Initialize FFT (uses internal look-up tables)
    esp_err_t ret = dsps_fft2r_init_fc32(NULL, FFT_SIZE);
    if (ret != ESP_OK) {
        printf("Error: Failed to initialize FFT (0x%x)\n", ret);
        return;
    }

    // 2. Fill interleaved buffer with a 50Hz sine wave
    for (int i = 0; i < FFT_SIZE; i++) {
        fft_buffer[i * 2 + 0] = sinf(2.0f * M_PI * 50.0f * i / 16000.0f); // Real
        fft_buffer[i * 2 + 1] = 0.0f;                                    // Imaginary
    }

    // 3. Execute FFT
    dsps_fft2r_fc32(fft_buffer, FFT_SIZE);

    // 4. Bit-Reverse (Orders the frequency bins correctly)
    dsps_bit_rev_fc32(fft_buffer, FFT_SIZE);

    // 5. Calculate and print magnitudes
    printf("FFT Results:\n");
    for (int i = 0; i < FFT_SIZE / 2; i++) {
        float re = fft_buffer[i * 2 + 0];
        float im = fft_buffer[i * 2 + 1];
        
        // Calculate Magnitude: sqrt(re^2 + im^2)
        float magnitude = sqrtf(re * re + im * im) / FFT_SIZE;

        // Print first 10 bins to console
        if (i < 10) {
            printf("Bin %d: %f\n", i, magnitude);
        }
    }
}
