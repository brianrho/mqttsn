/* Written by Brian Ejike (2019)
 * DIstributed under the MIT License */
 
#ifndef MQTT_CLIENT_PUBSUB_H_
#define MQTT_CLIENT_PUBSUB_H_

#include "mqttx_client.h"

class PubSubClient;

class MQTTClientPubsub : public MQTTClient {
    public:
        MQTTClientPubsub(PubSubClient * client);
        virtual void register_callbacks(void * self, MQTTClientConnectCallback conn_cb, MQTTClientMessageCallback msg_cb);
        virtual void publish(const char * topic, uint8_t * payload, uint8_t length, MQTTSNFlags * flags);
        virtual void subscribe(const char * topic, uint8_t qos);
        virtual void unsubscribe(const char * topic);
        
        void loop(void);
    
    private:
        static void publish_cb(void * which, char * topic, uint8_t * payload, unsigned int length);
        
        void * self;
        MQTTClientConnectCallback connect_cb;
        MQTTClientMessageCallback message_cb;
        PubSubClient * client;
};

#endif

