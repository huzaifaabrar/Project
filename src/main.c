#include "i2s_config.h"
#include "fft.h"
#include "wifi_comm.h"
#include "web_server.h"

void app_main(void)
{
    //printf("Starting I2S Microphone FFT Application...\n");

    wifi_init_sta();            // Initialize Wi-Fi
    while (!wifi_is_connected())
    {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    webserver_start();         // Start web server
    
    vI2S_InitRX();              // Initialize I2S
    vI2S_StartReaderTask();     // Start microphone task
    setupMicrophoneTimer();     // Start periodic timer

    vStartFFTTask(xAudioBufferQueue); // Start FFT processing task

    // test_fft();  // Run minimal FFT test
}
