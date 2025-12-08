#include "i2s_config.h"

void app_main(void)
{
    vI2S_InitRX();           // Initialize I2S RX
    vI2S_StartReaderTask();  // Start FreeRTOS I2S reader task
}
