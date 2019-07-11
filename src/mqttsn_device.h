#ifndef MQTTSN_DEVICE_H_
#define MQTTSN_DEVICE_H_

#include <stdint.h>

class MQTTSNDevice {
    public:
        /* get system millisecond tick */
        virtual uint32_t get_millis(void) = 0;
        
        /* delay in millisecs */
        virtual void delay_millis(uint32_t ms) = 0;
        
        /* get a random value within some range */
        virtual uint32_t get_random(uint32_t min, uint32_t max) = 0;
        
        /* Yield control to the device for background tasks,
         * necessary for devices like the ESP8266 which have WiFi stuff to handle
         */
        virtual void cede(void) = 0;
};

#endif
