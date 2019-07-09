#ifndef MQTTSN_MESSAGES_H_
#define MQTTSN_MESSAGES_H_

/* message types */
enum {
    MQTTSN_ADVERTISE,
    MQTTSN_SEARCHGW,
    MQTTSN_GWINFO,
    MQTTSN_CONNECT = 4,
    MQTTSN_CONNACK,
    MQTTSN_WILLTOPICREQ,
    MQTTSN_WILLTOPIC,
    MQTTSN_WILLMSGREQ,
    MQTTSN_WILLMSG,
    MQTTSN_REGISTER,
    MQTTSN_REGACK,
    MQTTSN_PUBLISH,
    MQTTSN_PUBACK,
    MQTTSN_PUBCOMP,
    MQTTSN_PUBREC,
    MQTTSN_PUBREL,
    MQTTSN_SUBSCRIBE = 18,
    MQTTSN_SUBACK,
    MQTTSN_UNSUBSCRIBE,
    MQTTSN_UNSUBACK,
    MQTTSN_PINGREQ,
    MQTTSN_PINGRESP,
    MQTTSN_DISCONNECT,
    MQTTSN_WILLTOPICUPD = 26,
    MQTTSN_WILLTOPICRESP,
    MQTTSN_WILLMSGUPD,
    MQTTSN_WILLMSGRESP
};

/* topic options */
enum {
    MQTTSN_TOPIC_NORMAL, 
    MQTTSN_TOPIC_PREDEFINED, 
    MQTTSN_TOPIC_SHORTNAME
};

/* return codes */
enum {
    MQTTSN_RC_ACCEPTED,
    MQTTSN_RC_CONGESTION,
    MQTTSN_RC_INVALIDTID,
    MQTTSN_RC_NOTSUPPORTED
};

class MQTTSNHeader {
    public:
    MQTTSNHeader(uint8_t msg_type = NULL);
    uint8_t pack(uint8_t * buffer, uint8_t buflen, uint8_t datalen = 0);
    uint8_t unpack(uint8_t * buffer, uint8_t buflen);
    
    uint8_t msg_type, length;
};

class MQTTSNFlags {
    public:
    MQTTSNFlags(void);
    void pack(void);
    void unpack(void);
    
    uint8_t dup, qos, retain, will, clean_session, topicid_type;
    uint8_t all;
};

class MQTTSNMessage {
    public:
    virtual uint8_t pack(uint8_t * buffer, uint8_t buflen) = 0;
    virtual uint8_t unpack(uint8_t * buffer, uint8_t buflen) = 0;
    
    MQTTSNHeader header;
};

class MQTTSNMessageAdvertise : public MQTTSNMessage {
    public:
    MQTTSNMessageAdvertise(uint8_t gw_id = 0);
    virtual uint8_t pack(uint8_t * buffer, uint8_t buflen);
    virtual uint8_t unpack(uint8_t * buffer, uint8_t buflen);
    
    uint8_t gw_id;
    uint16_t duration;
}


#endif
