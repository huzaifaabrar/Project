#include "i2s_config.h"
#include "fft.h"
#include "test_fft.h"  // Add this

void app_main(void)
{
    //printf("Starting I2S Microphone FFT Application...\n");
    
    vI2S_InitRX();              // Initialize I2S
    vI2S_StartReaderTask();     // Start microphone task
    setupMicrophoneTimer();     // Start periodic timer

    vStartFFTTask(xAudioBufferQueue); // Start FFT processing task

    // test_fft();  // Run minimal FFT test
}
