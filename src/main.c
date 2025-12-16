#include "i2s_config.h"

void app_main(void)
{
    vI2S_InitRX();              // Initialize I2S
    vI2S_StartReaderTask();     // Start microphone task
    setupMicrophoneTimer();     // Start periodic timer
}

