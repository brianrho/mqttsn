/* Written by Brian Ejike (2019)
 * DIstributed under the MIT License */
 
#ifndef MQTTSN_MESSAGES_H_
#define MQTTSN_MESSAGES_H_

#include "mqttsn_defines.h"
#include <stdint.h>

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
    MQTTSN_WILLMSGRESP,
    
    MQTTSN_NUM_MSG_TYPES
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

/* fixed header */
class MQTTSNHeader {
    public:
    MQTTSNHeader(uint8_t msg_type = 0);
    uint8_t pack(uint8_t * buffer, uint8_t buflen, uint8_t datalen = 0);
    uint8_t unpack(uint8_t * buffer, uint8_t buflen);
    
    uint8_t msg_type, length;
};

/* flags */
typedef struct {
    union {
        uint8_t all;
        struct {
            unsigned topicid_type   : 2;
            unsigned clean_session  : 1;
            unsigned will           : 1;
            unsigned retain         : 1;
            unsigned qos            : 2;
            unsigned dup            : 1;
        };
    };
} MQTTSNFlags;

/* class MQTTSNFlags {
    public:
    MQTTSNFlags(void);
    uint8_t pack(void);
    void unpack(void);
    
    uint8_t dup, qos, retain, will, clean_session, topicid_type;
    uint8_t all;
}; */

/* messages */
class MQTTSNMessage {
    public:
    uint8_t pack(uint8_t * buffer, uint8_t buflen) { return 0; }
    uint8_t unpack(uint8_t * buffer, uint8_t buflen) { return 0; }
    
    MQTTSNHeader header;
};

class MQTTSNMessageAdvertise : public MQTTSNMessage {
    public:
    MQTTSNMessageAdvertise(uint8_t gw_id = 0);
    uint8_t pack(uint8_t * buffer, uint8_t buflen);
    uint8_t unpack(uint8_t * buffer, uint8_t buflen);
    
    uint8_t gw_id;
    uint16_t duration;
};

class MQTTSNMessageSearchGW : public MQTTSNMessage {
    public:
    MQTTSNMessageSearchGW(uint8_t radius = 0);
    uint8_t pack(uint8_t * buffer, uint8_t buflen);
    uint8_t unpack(uint8_t * buffer, uint8_t buflen);
    
    uint8_t radius;
};

class MQTTSNMessageGWInfo : public MQTTSNMessage {
    public:
    MQTTSNMessageGWInfo(uint8_t gw_id = 0);
    uint8_t pack(uint8_t * buffer, uint8_t buflen);
    uint8_t unpack(uint8_t * buffer, uint8_t buflen);
    
    uint8_t gw_id;
    uint8_t * gw_addr;
    uint8_t gw_addr_len;
};

class MQTTSNMessageConnect : public MQTTSNMessage {
    public:
    MQTTSNMessageConnect(uint16_t duration = MQTTSN_DEFAULT_KEEPALIVE);
    uint8_t pack(uint8_t * buffer, uint8_t buflen);
    uint8_t unpack(uint8_t * buffer, uint8_t buflen);
    
    MQTTSNFlags flags;
    uint8_t protocol_id;
    uint16_t duration;
    uint8_t * client_id;
    uint8_t client_id_len;
};

class MQTTSNMessageConnack : public MQTTSNMessage {
    public:
    MQTTSNMessageConnack(uint8_t return_code = MQTTSN_RC_ACCEPTED);
    uint8_t pack(uint8_t * buffer, uint8_t buflen);
    uint8_t unpack(uint8_t * buffer, uint8_t buflen);
    
    uint8_t return_code;
};

class MQTTSNMessageRegister : public MQTTSNMessage {
    public:
    MQTTSNMessageRegister(uint16_t topic_id = 0);
    uint8_t pack(uint8_t * buffer, uint8_t buflen);
    uint8_t unpack(uint8_t * buffer, uint8_t buflen);
    
    uint16_t topic_id;
    uint16_t msg_id;
    uint8_t * topic_name;
    uint8_t topic_name_len;
};

class MQTTSNMessageRegack : public MQTTSNMessage {
    public:
    MQTTSNMessageRegack(uint8_t return_code = MQTTSN_RC_ACCEPTED);
    uint8_t pack(uint8_t * buffer, uint8_t buflen);
    uint8_t unpack(uint8_t * buffer, uint8_t buflen);
    
    uint16_t topic_id;
    uint16_t msg_id;
    uint8_t return_code;
};

class MQTTSNMessagePublish : public MQTTSNMessage {
    public:
    MQTTSNMessagePublish(uint16_t msg_id = 0);
    uint8_t pack(uint8_t * buffer, uint8_t buflen);
    uint8_t unpack(uint8_t * buffer, uint8_t buflen);
    
    uint16_t topic_id;
    uint16_t msg_id;
    MQTTSNFlags flags;
    uint8_t * data;
    uint8_t data_len;
};

class MQTTSNMessagePuback : public MQTTSNMessage {
    public:
    MQTTSNMessagePuback(uint8_t return_code = MQTTSN_RC_ACCEPTED);
    uint8_t pack(uint8_t * buffer, uint8_t buflen);
    uint8_t unpack(uint8_t * buffer, uint8_t buflen);
    
    uint16_t topic_id;
    uint16_t msg_id;
    uint8_t return_code;
};

class MQTTSNMessageSubscribe : public MQTTSNMessage {
    public:
    MQTTSNMessageSubscribe(void);
    uint8_t pack(uint8_t * buffer, uint8_t buflen);
    uint8_t unpack(uint8_t * buffer, uint8_t buflen);
    
    uint16_t msg_id;
    MQTTSNFlags flags;
    uint8_t * topic_name;
    uint8_t topic_name_len;
};

class MQTTSNMessageUnsubscribe : public MQTTSNMessage {
    public:
    MQTTSNMessageUnsubscribe(void);
    uint8_t pack(uint8_t * buffer, uint8_t buflen);
    uint8_t unpack(uint8_t * buffer, uint8_t buflen);
    
    uint16_t msg_id;
    MQTTSNFlags flags;
    uint8_t * topic_name;
    uint8_t topic_name_len;
};

class MQTTSNMessageSuback : public MQTTSNMessage {
    public:
    MQTTSNMessageSuback(uint8_t return_code = MQTTSN_RC_ACCEPTED);
    uint8_t pack(uint8_t * buffer, uint8_t buflen);
    uint8_t unpack(uint8_t * buffer, uint8_t buflen);
    
    uint16_t topic_id;
    uint16_t msg_id;
    MQTTSNFlags flags;
    uint8_t return_code;
};

class MQTTSNMessageUnsuback : public MQTTSNMessage {
    public:
    MQTTSNMessageUnsuback(void);
    uint8_t pack(uint8_t * buffer, uint8_t buflen);
    uint8_t unpack(uint8_t * buffer, uint8_t buflen);
    
    uint16_t msg_id;
};

class MQTTSNMessagePingreq : public MQTTSNMessage {
    public:
    MQTTSNMessagePingreq(void);
    uint8_t pack(uint8_t * buffer, uint8_t buflen);
    uint8_t unpack(uint8_t * buffer, uint8_t buflen);
    
    uint8_t * client_id;
    uint8_t client_id_len;
};

class MQTTSNMessagePingresp : public MQTTSNMessage {
    public:
    MQTTSNMessagePingresp(void);
    uint8_t pack(uint8_t * buffer, uint8_t buflen);
    uint8_t unpack(uint8_t * buffer, uint8_t buflen);
};

class MQTTSNMessageDisconnect : public MQTTSNMessage {
    public:
    MQTTSNMessageDisconnect(void);
    uint8_t pack(uint8_t * buffer, uint8_t buflen);
    uint8_t unpack(uint8_t * buffer, uint8_t buflen);
    
    uint16_t duration;
};

#endif
