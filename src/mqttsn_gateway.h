/* Written by Brian Ejike (2019)
 * DIstributed under the MIT License */
 
#ifndef MQTTSN_GATEWAY_H_
#define MQTTSN_GATEWAY_H_

#include "mqttsn_defines.h"
#include "mqttsn_messages.h"
#include "mqttsn_transport.h"
#include <lite_fifo.h>
#include <stdint.h>

typedef struct {
    uint16_t tid;
} MQTTSNInstancePubTopic;

typedef struct {
    uint16_t tid;
    MQTTSNFlags flags;
} MQTTSNInstanceSubTopic;

typedef enum {
    MQTTSNInstanceStatus_ACTIVE,
    MQTTSNInstanceStatus_LOST,
    MQTTSNInstanceStatus_DISCONNECTED,
    
    MQTTSNInstanceStatus_ASLEEP,
    MQTTSNInstanceStatus_AWAKE,
} MQTTSNInstanceStatus;

class MQTTSNDevice;
class MQTTClient;

class MQTTSNInstance {
    friend class MQTTSNGateway;
    
    MQTTSNInstance(void);
    
    /* insert a new client's info */
    bool register_(uint8_t * cid, uint8_t cid_len, MQTTSNTransport * transport, MQTTSNAddress * addr, uint16_t duration, MQTTSNFlags * flags);
    
    /* delete an existing client, after it gets lost or DISCONNECTed */
    void deregister(void);
    
    /* add a new subscription for the client */
    bool add_sub_topic(uint16_t tid, MQTTSNFlags * flags);
    
    /* add a new topic registered by the client */
    bool add_pub_topic(uint16_t tid);
    
    /* delete a client's subscription */
    void delete_sub_topic(uint16_t tid);
    
    /* check if the client is subs to this topic */
    bool is_subbed(uint16_t tid);
    
    /* re-send any inflight msgs and check the client's status */
    MQTTSNInstanceStatus check_status(uint32_t now);
    
    /* used after a transaction is initiated by the client */
    void mark_time(uint32_t now);
    
    explicit operator bool() const;
    
    /* list of pub and sub topics for this client */
    MQTTSNInstancePubTopic pub_topics[MQTTSN_MAX_INSTANCE_TOPICS];
    MQTTSNInstanceSubTopic sub_topics[MQTTSN_MAX_INSTANCE_TOPICS];
    
    char client_id[MQTTSN_MAX_CLIENTID_LEN + 1];
    MQTTSNFlags connect_flags;
    MQTTSNTransport * transport;
    MQTTSNAddress address;
    
    /* for storing unicast msgs expecting a reply */
    uint8_t msg_inflight[MQTTSN_MAX_MSG_LEN];
    uint8_t msg_inflight_len;
    uint32_t unicast_timer;
    uint8_t unicast_counter;
    
    /* keepalive and (keepalive * 1.5) */
    uint32_t keepalive_interval;
    uint32_t keepalive_timeout;
    
    uint32_t sleep_interval;
    uint32_t sleep_timeout;
    
    /* queue for holding publish messages awaiting dispatch, while client sleeps */
    LiteFifo sleepy_fifo;
    uint8_t sleepy_fifo_buf[MQTTSN_MAX_BUFFERED_MSGS * MQTTSN_MAX_MSG_LEN];
    
    /* track when transactions start or complete */
    uint32_t last_in;
    MQTTSNInstanceStatus status;
};

/* for gateway mapping of topic name to topic ID and type */
typedef struct {
    char name[MQTTSN_MAX_TOPICNAME_LEN + 1];
    uint8_t ttype;
    bool subbed;
    uint8_t sub_qos;
    uint16_t tid;
} MQTTSNTopicMapping;


class MQTTSNGateway {    
    public:
    MQTTSNGateway(MQTTSNDevice * device, MQTTClient * client = NULL);
    
    /* start the gateway with a unique gateway ID */
    bool begin(uint8_t gw_id);
    
    /* register transports that are used to talk to clients */
    bool register_transport(MQTTSNTransport * transport);
    
    void set_advertise_interval(uint16_t seconds);
    
    /* gateway tasks loop */
    bool loop(void);
    
    /* Set a prefix for every client topic
       e.g. Topic "lights" could be prefixed with the client's name "home" to yield "home/lights" */ 
    bool set_topic_prefix(const char * prefix);
    
    private:
    void advertise(void);
    void assign_msg_handlers(void);
    void handle_messages(void);
    
    /* add and delete subs from our table of topic mappings */
    void add_subscription(uint16_t tid, uint8_t qos);
    void delete_subscription(uint16_t tid);
    
    uint16_t get_topic_id(const uint8_t * name, uint8_t name_len);
    MQTTSNTopicMapping * get_topic_mapping(uint16_t tid);
    MQTTSNInstance * get_client(MQTTSNTransport * transport, MQTTSNAddress * addr);
    MQTTSNInstance * get_client(const char * cid, uint8_t cid_len);
    bool get_mqtt_topic_name(const char * name, char * mqtt_name, uint16_t mqtt_name_sz);
    
    /* MQTTSN message handlers */
    void handle_searchgw(uint8_t * data, uint8_t data_len, MQTTSNTransport * transport, MQTTSNAddress * src);
    void handle_connect(uint8_t * data, uint8_t data_len, MQTTSNTransport * transport, MQTTSNAddress * src);
    void handle_register(uint8_t * data, uint8_t data_len, MQTTSNTransport * transport, MQTTSNAddress * src);
    void handle_publish(uint8_t * data, uint8_t data_len, MQTTSNTransport * transport, MQTTSNAddress * src);
    void handle_subscribe(uint8_t * data, uint8_t data_len, MQTTSNTransport * transport, MQTTSNAddress * src);
    void handle_unsubscribe(uint8_t * data, uint8_t data_len, MQTTSNTransport * transport, MQTTSNAddress * src);
    void handle_pingreq(uint8_t * data, uint8_t data_len, MQTTSNTransport * transport, MQTTSNAddress * src);
    void handle_disconnect(uint8_t * data, uint8_t data_len, MQTTSNTransport * transport, MQTTSNAddress * src);
    
    /* MQTT event handlers */
    static void handle_mqtt_connect(void * which, bool conn_state);
    static void handle_mqtt_publish(void * which, const char * topic, uint8_t * payload, uint8_t length, MQTTSNFlags * flags);
    
    /* prepended to every MQTTSN client topic, except those that begin with a $ */
    char topic_prefix[MQTTSN_MAX_TOPICPREFIX_LEN + 1];
    
    /* for holding complete MQTT topic name during publish/subscribe */
    char topic_name_full[MQTTSN_MAX_MQTT_TOPICNAME_LEN + 1];
    
    /* table of topic mappings */
    MQTTSNTopicMapping mappings[MQTTSN_MAX_TOPIC_MAPPINGS];
    
    /* list of clients connected to this gateway */
    MQTTSNInstance clients[MQTTSN_MAX_NUM_CLIENTS];
    
    /* message handlers jump table, used for dispatch */
    void (MQTTSNGateway::*msg_handlers[MQTTSN_NUM_MSG_TYPES])(uint8_t *, uint8_t, MQTTSNTransport *, MQTTSNAddress *);
    
    uint8_t gw_id;
    MQTTSNDevice * device;
    MQTTSNTransport * transports[MQTTSN_MAX_NUM_TRANSPORTS];
    MQTTClient * mqtt_client;
    
    /* for MQTT connection state */
    bool connected;
    /* for unicast msgs */
    uint16_t curr_msg_id;
    
    /* interval between ADVERTISE messages in millisecs */
    uint32_t advert_interval;
    uint32_t last_advert;
    
    /* queue for holding publish messages awaiting dispatch */
    LiteFifo pub_fifo;
    uint8_t pub_fifo_buf[MQTTSN_MAX_QUEUED_PUBLISH * MQTTSN_MAX_MSG_LEN];
    
    /* buffer for incoming packets */
    uint8_t in_msg[MQTTSN_MAX_MSG_LEN];
    uint8_t in_msg_len;
    
    /* buffer for outgoing packets */
    uint8_t out_msg[MQTTSN_MAX_MSG_LEN];
    uint8_t out_msg_len;
};

#endif

