/* Written by Brian Ejike (2019)
 * DIstributed under the MIT License */
 
#ifndef MQTTSN_GATEWAY_INCLUDES_H_
#define MQTTSN_GATEWAY_INCLUDES_H_

/* uncomment to select a device */

    #define MQTTSN_INCLUDE_DEVICE_ARDUINO
    
/* uncomment to select transport(s) */

    #define MQTTSN_INCLUDE_TRANSPORT_HC12
    #define MQTTSN_INCLUDE_TRANSPORT_DUMMY
    
/* uncomment to select an MQTT client lib */

    #define MQTTSN_INCLUDE_MQTTCLIENT_PUBSUB
    
    
/*************** IGNORE EVERYTHING BELOW *******************/

#if defined(MQTTSN_INCLUDE_DEVICE_ARDUINO)
    #include "device/mqttsn_device_arduino.h"
#endif

/* possible to use multiple transports */
#if defined(MQTTSN_INCLUDE_TRANSPORT_HC12)
    #include "transport/mqttsn_transport_hc12.h"
#endif

#if defined(MQTTSN_INCLUDE_TRANSPORT_DUMMY)
    #include "mqttsn_transport_dummy.h"
#endif

#if defined(MQTTSN_INCLUDE_MQTTCLIENT_PUBSUB)
    #include "mqtt/mqtt_client_pubsub.h"
#endif

#include "mqttsn_defines.h"
#include "mqttsn_gateway.h"

#endif
