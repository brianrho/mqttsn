#include "mqttsn_messages.h"

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
    MQTTSNInstanceStatus_DISCONNECTED
} MQTTSNInstanceStatus;

class MQTTSNInstance {
    public:
        MQTTSNInstance(void);
        
        /* insert a new client's info */
        bool register_(const char * cid, MQTTSNAddress * addr, uint16_t duration, MQTTSNFlags * flags);
        
        /* delete an existing client, after it gets lost or DISCONNECTed */
        void deregister(void);
        
        /* add a new subscription for the client */
        bool add_sub_topic(uint16_t tid, MQTTSNFlags * flags);
        
        /* add a new topic registered by the client */
        bool add_pub_topic(uint16_t tid, MQTTSNFlags * flags);
        
        /* delete a client's subscription */
        void delete_sub_topic(uint16_t tid);
        
        /* check if the client is subs to this topic */
        bool is_subbed(uint16_t tid);
        
        /* re-send any inflight msgs and check the client's status */
        MQTTSNInstanceStatus check_status(MQTTSNTransport * transport);
        
        /* used after a transaction is initiated by the client */
        void mark_time(void);
        
        operator bool() const;
        
    private:
        MQTTSNInstancePubTopic pub_topics[MQTTSN_MAX_INSTANCE_TOPICS];
        MQTTSNInstanceSubTopic sub_topics[MQTTSN_MAX_INSTANCE_TOPICS];
        
        char client_id[MQTTSN_MAX_CLIENTID_LEN + 1];
        MQTTSNFlags connect_flags;
        MQTTSNAddress address;
        
        /* for storing unicast msgs expecting a reply */
        uint8_t msg_inflight[MQTTSN_MAX_MSG_LEN];
        uint8_t msg_inflight_len;
        uint32_t unicast_timer;
        uint8_t unicast_counter;
        
        /* keepalive and (keepalive * 1.5) */
        uint32_t keepalive_interval;
        uint32_t keepalive_timeout;
        
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
    MQTTSNGateway(uint8_t gw_id, MQTTClient * client, MQTTSNTransport * transport);
    void loop(void);
    
    private:
    void assign_msg_handlers(void);
    void handle_messages(void);
    
    /* add and delete subs from our table of topic mappings */
    void add_subscription(uint16_t tid, uint8_t qos);
    void delete_subscription(uint16_t tid);
    
    uint16_t get_topic_id(const char * name);
    uint16_t get_topic_mapping(uint16_t tid);
    MQTTSNInstance * get_instance(MQTTSNAddress * addr);
    
    /* MQTTSN message handlers */
    void handle_searchgw(uint8_t * data, uint8_t data_len, MQTTSNAddress * src);
    void handle_connect(uint8_t * data, uint8_t data_len, MQTTSNAddress * src);
    void handle_register(uint8_t * data, uint8_t data_len, MQTTSNAddress * src);
    void handle_publish(uint8_t * data, uint8_t data_len, MQTTSNAddress * src);
    void handle_subscribe(uint8_t * data, uint8_t data_len, MQTTSNAddress * src);
    void handle_unsubscribe(uint8_t * data, uint8_t data_len, MQTTSNAddress * src);
    void handle_pingreq(uint8_t * data, uint8_t data_len, MQTTSNAddress * src);
    
    /* MQTT event handlers */
    void handle_mqtt_connect(bool conn_state);
    void handle_mqtt_publish(const char * topic, uint8_t * payload, uint8_t length, MQTTSNFlags * flags);
    
    MQTTSNTopicMapping mappings[MQTTSN_MAX_TOPIC_MAPPINGS];
    MQTTSNInstance clients[MQTTSN_MAX_NUM_CLIENTS];
    
    /* message handlers jump table, used for dispatch */
    void (MQTTSNClient::*msg_handlers[MQTTSN_NUM_MSG_TYPES])(uint8_t *, uint8_t, MQTTSNAddress *);
    
    uint8_t gw_id;
    MQTTSNDevice * device;
    MQTTSNTransport * transport;
    MQTTClient * mqtt_client;
    bool connected;
    uint16_t curr_msg_id;
    
    /* queue for holding publish messages awaiting dispatch */
    LiteFifo pub_fifo;
    uint8_t pub_fifo_buf[MQTTSN_MAX_QUEUED_PUBLISH * MQTTSN_MAX_MSG_LEN];
    
    /* temp buffer for holding serialized msgs */
    uint8_t temp_msg[MQTTSN_MAX_MSG_LEN];
    uint8_t temp_msg_len;
};
};

