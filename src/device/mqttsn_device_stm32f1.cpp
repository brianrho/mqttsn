/* Written by Brian Ejike (2019)
 * DIstributed under the MIT License */
 
#if defined(STM32F10X_MD)

#include "mqttsn_device_stm32f1.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stm32f1xx_it.h>


MQTTSNDeviceSTM32F1::MQTTSNDeviceSTM32F1(void)
{

}

uint32_t MQTTSNDeviceSTM32F1::get_millis(void) 
{
    return millis();
}

void MQTTSNDeviceSTM32F1::delay_millis(uint32_t ms) 
{
    delay_ms(ms);
}

void MQTTSNDeviceSTM32F1::cede(void) 
{

}

uint32_t MQTTSNDeviceSTM32F1::get_random(uint32_t min, uint32_t max) 
{
    return rand() % (max - min) + min;
}

#endif
