/* Written by Brian Ejike (2019)
 * DIstributed under the MIT License */
 
#ifndef MQTTSN_INCLUDES_H
#define MQTTSN_INCLUDES_H

/* uncomment to select a device */

    #define MQTTSN_INCLUDE_DEVICE_ARDUINO
    
/* uncomment to select a transport */

    #define MQTTSN_INCLUDE_TRANSPORT_HC12
    
    
/* ignore */
#if defined(MQTTSN_INCLUDE_DEVICE_ARDUINO)
    #include "arduino/mqttsn_device_arduino.h"
#endif

#if defined(MQTTSN_INCLUDE_TRANSPORT_HC12)
    #include "arduino/mqttsn_transport_hc12.h"
#endif

#include "mqttsn_defines.h"
#include "mqttsn_client.h"

#endif
