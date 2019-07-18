#include "mqtt_client_pubsub.h"
#include <PubSubClient.h>
#include <stdint.h>

MQTTClientPubsub::MQTTClientPubsub(PubSubClient * client) : client(client), connect_cb(NULL), message_cb(NULL)
{
    
}

void register_callbacks(MQTTClientConnectCallback conn_cb, MQTTClientMessageCallback msg_cb)
{
    connect_cb = conn_cb;
    client->setCallback(msg_cb);
}

void MQTTClientPubsub::publish(const char * topic, uint8_t * payload, uint8_t length, MQTTSNFlags * flags) 
{
    client->publish(topic, payload, length, flags->retain);
}

void MQTTClientPubsub::subscribe(const char * topic, uint8_t qos)
{
    client->subscribe(topic, qos);
}

void MQTTClientPubsub::unsubscribe(const char * topic)
{
    client->unsubscribe(topic);
}

void MQTTClientPubsub::loop(void) 
{
    static bool conn_state = false;
    
    if (client->loop() != conn_state) {
        conn_state = !conn_state;
        connect_cb(conn_state);
    }
}
