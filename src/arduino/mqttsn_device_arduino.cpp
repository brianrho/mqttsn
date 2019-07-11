#if defined(ARDUINO)

#include "mqttsn_device_arduino.h"

#include <stddef.h>
#include <stdint.h>
#include <Arduino.h>


MQTTSNDeviceArduino::MQTTSNDeviceArduino(void)
{

}

uint32_t MQTTSNDeviceArduino::get_millis(void) 
{
    return millis();
}

void MQTTSNDeviceArduino::delay_millis(uint32_t ms) 
{
    delay(ms);
}

void MQTTSNDeviceArduino::cede(void) 
{
    yield();
}

uint32_t MQTTSNDeviceArduino::get_random(uint32_t min, uint32_t max) 
{
    return random(min, max);
}

#endif
