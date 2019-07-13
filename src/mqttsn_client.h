#ifndef MQTTSN_CLIENT_H_
#define MQTTSN_CLIENT_H_

#include "mqttsn_messages.h"
#include "mqttsn_transport.h"

#include <stdint.h>
#include <stddef.h>

/* client state */
typedef enum {
    MQTTSNState_ACTIVE,
    MQTTSNState_LOST,
    MQTTSNState_ASLEEP,
    MQTTSNState_AWAKE,
    MQTTSNState_DISCONNECTED,
    MQTTSNState_CONNECTING,
    MQTTSNState_SEARCHING,
    
    MQTTSNState_NUM_STATES
} MQTTSNState;

/* The following structs should be declared statically
 * or explicitly zero-initialized, this is mainly for unused fields */

/* For holding gateway info */
typedef struct {
    uint8_t gw_id;
    MQTTSNAddress gw_addr;
} MQTTSNGWInfo;

/* For holding publish topics */
typedef struct {
    const char * name;
    uint16_t tid;
} MQTTSNPubTopic;

/* For holding subscribe topics */
typedef struct {
    const char * name;
    MQTTSNFlags flags;
    uint16_t tid;
} MQTTSNSubTopic;

/* template for publish message handler */
typedef void (*MQTTSNPublishHandler)(const char * topic, uint8_t * data, uint8_t len, MQTTSNFlags * flags);

class MQTTSNClient {
    public:
        MQTTSNClient(MQTTSNDevice * device, MQTTSNTransport * transport);
        
        /* return true if we setup successfully */
        bool begin(const char * clnt_id);
        
        /* Supply a list of gateways manually */
        void add_gateways(MQTTSNGWInfo * gateways, uint8_t count);
        
        /* client loop, should be called regularly to handle tasks */
        bool loop(void);
        
        /* send a SEARCHGW to find a gateway */
        void discover(void);
        
        /* return the total number of gateways added manually or discovered */
        uint8_t gateway_count(void);
        
        /* Connect a specific gateway,
         * returns true if the message was sent */
        bool connect(uint8_t gw_id, MQTTSNFlags * flags = NULL, uint16_t duration = MQTTSN_DEFAULT_KEEPALIVE);
        
        /* Register a list of topics with the gateway,
         * returns true if all topics in the list have been registered */
        bool register_topics(MQTTSNPubTopic * topics, uint16_t len);
        
        /* Publish data to a topic, returns true if the message was sent */
        bool publish(const char * topic, uint8_t * data, uint8_t len, MQTTSNFlags * flags = NULL);
        
        /* Subscribe to a list of topics with the gateway,
         * returns true if all topics in the list have been subscribed to */
        bool subscribe_topics(MQTTSNSubTopic * topics, uint16_t len);
        
        /* Unsubscribe to a topic, returns true if the message was sent */
        bool unsubscribe(const char * topic, MQTTSNFlags * flags = NULL);
        
        /* check if there's a pending transaction,
         * like a REGISTER, SUBSCRIBE or QoS>0 PUBLISH.
         * Only one transaction can be pending at any given time.
         */
        bool transaction_pending(void);
        
        /* check if we're connected to a gateway */
        bool is_connected(void);
        
        /* Disconnect from the gateway, return true if the message was sent */
        bool disconnect(void);
        
        /* Register a handler for all publish messages from the gateway */
        void on_message(MQTTSNPublishHandler handler);
        
        /* return client status */
        MQTTSNState status(void);
        
    private:
        void assign_handlers(void);
        void handle_messages(void);
        void inflight_handler(void);
        void _register(MQTTSNPubTopic * topic);
        void subscribe(MQTTSNSubTopic * topic);
        bool ping(void);
        MQTTSNGWInfo * select_gateway(uint8_t gw_id);
        
        /* message handlers */
        void handle_advertise(uint8_t * data, uint8_t data_len, MQTTSNAddress * src);
        void handle_searchgw(uint8_t * data, uint8_t data_len, MQTTSNAddress * src);
        void handle_gwinfo(uint8_t * data, uint8_t data_len, MQTTSNAddress * src);
        void handle_connack(uint8_t * data, uint8_t data_len, MQTTSNAddress * src);
        void handle_regack(uint8_t * data, uint8_t data_len, MQTTSNAddress * src);
        void handle_publish(uint8_t * data, uint8_t data_len, MQTTSNAddress * src);
        void handle_suback(uint8_t * data, uint8_t data_len, MQTTSNAddress * src);
        void handle_unsuback(uint8_t * data, uint8_t data_len, MQTTSNAddress * src);
        void handle_pingresp(uint8_t * data, uint8_t data_len, MQTTSNAddress * src);
        
        /* client state handlers */
        void searching_handler(void);
        void connecting_handler(void);
        void lost_handler(void);
        void disconnected_handler(void);
        void active_handler(void);
        
        /* message handlers jump table, used for dispatch */
        void (MQTTSNClient::*msg_handlers[MQTTSN_NUM_MSG_TYPES])(void);
        
        /* state handlers jump table, used for dispatch */
        void (MQTTSNClient::*state_handlers[MQTTSNState_NUM_STATES])(void);
        
        /* point to user-provided gateway list */
        MQTTSNGWInfo * gateways;
        uint8_t gateways_capacity;
        
        /* point to user-provided list of pub/sub topics */
        MQTTSNPubTopic * pub_topics;
        MQTTSNSubTopic * sub_topics;
        uint16_t sub_topics_cnt, pub_topics_cnt;
        
        /* user-provided handler/callback for publish msgs */
        MQTTSNPublishHandler publish_cb;
        
        MQTTSNDevice * device;
        MQTTSNTransport * transport;
        const char * client_id;
        MQTTSNState state;
        
        /* keep track of current gateway */
        MQTTSNGWInfo * curr_gateway;
        
        /* keep track of connection to gw */
        bool connected;
        
        /* for storing unicast msgs expecting a reply */
        uint8_t msg_inflight[MQTTSN_MAX_MSG_LEN];
        uint8_t msg_inflight_len;
        uint32_t unicast_timer;
        uint8_t unicast counter;
        
        /* keepalive and (keepalive * 1.5) */
        uint32_t keepalive_interval;
        uint32_t keepalive_timeout;
        
        /* track when transactions start or complete */
        uint32_t last_in, last_out;
        
        /* for tracking PINGREQs */
        bool ping_pending;
        uint32_t ping_timer;
        uint8_t ping_counter;
        
        /* for tracking SEARCHGWs */
        uint32_t searchgw_started, searchgw_interval;
        bool searchgw_pending;
        
        /* msg ID counter for transactions */
        uint16_t curr_msg_id;
        
};

#endif
