#include "mqtt_client.h"

class MQTTClientPubsub : public MQTTClient {
    public:
        MQTTClientPubsub(PubSubClient * client);
        virtual void register_callbacks(MQTTClientConnectCallback conn_cb, MQTTClientMessageCallback msg_cb);
        virtual void publish(const char * topic, uint8_t * payload, uint8_t length, MQTTSNFlags * flags);
        virtual void subscribe(const char * topic, uint8_t qos);
        virtual void unsubscribe(const char * topic);
        
        void loop(void);
    
    private:
        PubSubClient * client;
        
};

