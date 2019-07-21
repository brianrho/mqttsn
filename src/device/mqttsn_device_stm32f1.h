#ifndef MQTTSN_DEVICE_STM32F1_H_
#define MQTTSN_DEVICE_STM32F1_H_

#include <stdint.h>
#include "../mqttsn_device.h"

class MQTTSNDeviceSTM32F1 : public MQTTSNDevice {
    public:
        MQTTSNDeviceSTM32F1(void);
        
        virtual uint32_t get_millis(void);
        virtual void cede(void);
        virtual void delay_millis(uint32_t ms);
        virtual uint32_t get_random(uint32_t min, uint32_t max);
};

#endif
