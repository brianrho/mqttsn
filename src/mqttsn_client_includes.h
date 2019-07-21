/* Written by Brian Ejike (2019)
 * DIstributed under the MIT License */
 
#ifndef MQTTSN_CLIENT_INCLUDES_H_
#define MQTTSN_CLIENT_INCLUDES_H_

/* uncomment to select a device */

    #define MQTTSN_INCLUDE_DEVICE_ARDUINO
    //#define MQTTSN_INCLUDE_DEVICE_STM32F1

/* uncomment to select a transport */

    #define MQTTSN_INCLUDE_TRANSPORT_HC12
    
    
/* ignore */
#if defined(MQTTSN_INCLUDE_DEVICE_ARDUINO)
    #include "device/mqttsn_device_arduino.h"
#elif defined(MQTTSN_INCLUDE_DEVICE_STM32F1)
	#include "device/mqttsn_device_stm32f1.h"
#endif

#if defined(MQTTSN_INCLUDE_TRANSPORT_HC12)
    #include "transport/mqttsn_transport_hc12.h"
#endif

#include "mqttsn_defines.h"
#include "mqttsn_client.h"

#endif
