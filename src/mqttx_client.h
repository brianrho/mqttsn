/* Written by Brian Ejike (2019)
 * DIstributed under the MIT License */
 
#ifndef MQTTX_CLIENT_H_
#define MQTTX_CLIENT_H_

#include "mqttsn_messages.h"
#include <stdint.h>

/* callback signatures for connect/disconnect and publish events
   first argument is a pointer to the callee instance i.e. 'this' */
typedef void (*MQTTClientConnectCallback)(void * self, bool conn_state);
typedef void (*MQTTClientMessageCallback)(void * self, const char * topic, uint8_t * payload, uint8_t length, MQTTSNFlags * flags);

class MQTTClient {
    public:
    virtual void register_callbacks(void * self, MQTTClientConnectCallback conn_cb, MQTTClientMessageCallback msg_cb) = 0;
    virtual void publish(const char * topic, uint8_t * payload, uint8_t length, MQTTSNFlags * flags) = 0;
    virtual void subscribe(const char * topic, uint8_t qos) = 0;
    virtual void unsubscribe(const char * topic) = 0;
};

#endif

