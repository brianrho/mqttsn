#include "mqtt_client_pubsub.h"
#include "mqttsn_messages.h"
#include <PubSubClient.h>
#include <stdint.h>

MQTTClientPubsub::MQTTClientPubsub(PubSubClient * client) : client(client), connect_cb(NULL), message_cb(NULL)
{
    
}

void MQTTClientPubsub::register_callbacks(void * self, MQTTClientConnectCallback conn_cb, MQTTClientMessageCallback msg_cb)
{
    this->self = self;
    connect_cb = conn_cb;
    message_cb = msg_cb;
    
    /* publish is received here first before sent to gateway instance */
    client->setMethodCallback(this, MQTTClientPubsub::publish_cb);
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

void MQTTClientPubsub::publish_cb(void * which, char * topic, uint8_t * payload, unsigned int length) {
    /* get the instance */
    MQTTClientPubsub * self = static_cast<MQTTClientPubsub*>(which);
    
    /* call our saved static callback */
    MQTTSNFlags flags;
    flags.all = 0;
    self->message_cb(self->self, topic, payload, length, &flags);
}

void MQTTClientPubsub::loop(void) 
{
    static bool conn_state = false;
    
    if (client->loop() != conn_state) {
        conn_state = !conn_state;
        connect_cb(self, conn_state);
    }
}
