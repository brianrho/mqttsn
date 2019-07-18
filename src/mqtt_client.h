#include <stdint.h>

/* callback signatures for connect/disconnect and publish events */
typedef void (*MQTTClientConnectCallback)(bool);
typedef void (*MQTTClientMessageCallback)(const char *, uint8_t * payload, uint8_t length, MQTTSNFlags * flags);

class MQTTSNFlags;


class MQTTClient {
    public:
    virtual void register_callbacks(MQTTClientConnectCallback conn_cb, MQTTClientMessageCallback msg_cb) = 0;
    virtual void publish(const char * topic, uint8_t * payload, uint8_t length, MQTTSNFlags * flags) = 0;
    virtual void subscribe(const char * topic, uint8_t qos) = 0;
    virtual void unsubscribe(const char * topic) = 0;
    
    protected:
    MQTTClientConnectCallback connect_cb;
    MQTTClientMessageCallback message_cb;
};

