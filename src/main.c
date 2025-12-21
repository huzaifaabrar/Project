#include "i2s_config.h"
#include "fft.h"
#include "wifi_comm.h"
#include "web_server.h"

void app_main(void)
{
    // 1. Initialize I2S microphone
    vI2S_InitRX();              
    vI2S_StartReaderTask();     
    setupMicrophoneTimer();     

    // 2. Start FFT processing task
    vStartFFTTask(xAudioBufferQueue); 

    // 3. Initialize Wi-Fi 
    vWifiInitSta();            

    // 4. Start web server (it will only serve if Wi-Fi is connected)
    vWebServerStart();          

}

