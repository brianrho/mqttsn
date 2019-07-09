#include "mqttsn_messages.h"

#include <stdint.h>
#include <string.h>

static void reverse_bytes(void *start, uint16_t size) {
    uint8_t *lo = (uint8_t *)start;
    uint8_t *hi = (uint8_t *)start + size - 1;
    uint8_t swap;
    while (lo < hi) {
        swap = *lo;
        *lo++ = *hi;
        *hi-- = swap;
    }
}

/**************** MQTTSNHeader ***************/

MQTTSNHeader::MQTTSNHeader(uint8_t msg_type = NULL) : length(0), msg_type(msg_type)
{

}

uint8_t MQTTSNHeader::pack(uint8_t * buffer, uint8_t buflen, uint8_t datalen)
{
    length = datalen + MQTTSN_HEADER_LEN;
    
    /* check that the buffer is big enough */
    if (buflen < length) {
        return 0;
    }
    
    /* max msg size for now is 255 */
    if (length < 256) {
        buffer[0] = length;
        buffer[1] = msg_type;
        return 2;
    }
    
    return 0;
}

uint8_t MQTTSNHeader::unpack(uint8_t * buffer, uint8_t buflen)
{
    /* check that we have enough to parse */
    if (buflen < MQTTSN_HEADER_LEN || buffer[0] == 0) {
        return 0;
    }
    
    /* multi-byte length (> 255) not supported */
    if (buffer[0] == 1) {
        return 0;
    }
    
    length = buffer[0] - MQTTSN_HEADER_LEN;
    msg_type = buffer[1];
    return MQTTSN_HEADER_LEN;
}

/**************** MQTTSNFlags ***************/

MQTTSNFlags::MQTTSNFlags(void) : 
    dup(0), qos(0), retain(0), will(0), clean_session(0), topicid_type(0), all(0)
{

}

uint8_t MQTTSNFlags::pack(void) 
{
    all = (dup << 7) | (qos << 5) | (retain << 4) | (will << 3) | (clean_session << 2) | topicid_type;
    return all;
}

void MQTTSNFlags::unpack(uint8_t value) 
{
    all = value;
    dup = all >> 7;
    qos = (all >> 5) & 0x3;
    retain = (all >> 4) & 0x1;
    will = (all >> 3) & 0x1;
    clean_session = (all >> 2) & 0x1;
    topicid_type = all & 0x3;
    return 1;
}


/**************** MQTTSNMessageAdvertise ***************/

MQTTSNMessageAdvertise::MQTTSNMessageAdvertise(uint8_t gw_id) : gw_id(gw_id), duration(0)
{
    
}

uint8_t MQTTSNMessageAdvertise::pack(uint8_t * buffer, uint8_t buflen) 
{
    header = MQTTSNHeader(MQTTSN_ADVERTISE);
    offset = header.pack(buffer, buflen, 3);
    if (!offset) {
        return 0;
    }
    
    header[offset++] = gw_id;
    header[offset++] = duration >> 8;
    header[offset++] = duration & 0xff;
    return offset;
}

uint8_t MQTTSNMessageAdvertise::unpack(uint8_t * buffer, uint8_t buflen) 
{
    /* we expect exactly 3 bytes */
    if (buflen != 3) {
        return 0;
    }
    
    gw_id = buffer[0];
    duration = ((uint16_t)buffer[1] << 8) | buffer[2];
    return buflen;
}

/**************** MQTTSNMessageSearchGW ***************/

MQTTSNMessageSearchGW::MQTTSNMessageSearchGW(uint8_t radius) : radius(radius)
{
    
}

uint8_t MQTTSNMessageSearchGW::pack(uint8_t * buffer, uint8_t buflen) 
{
    header = MQTTSNHeader(MQTTSN_SEARCHGW);
    offset = header.pack(buffer, buflen, 1);
    if (!offset) {
        return 0;
    }
    
    header[offset++] = radius;
    return offset;
}

uint8_t MQTTSNMessageSearchGW::unpack(uint8_t * buffer, uint8_t buflen) 
{
    /* we expect 1 byte */
    if (buflen != 1) {
        return 0;
    }
    
    radius = buffer[0];
    return buflen;
}

/**************** MQTTSNMessageGWInfo ***************/

MQTTSNMessageGWInfo::MQTTSNMessageGWInfo(uint8_t gw_id) : 
    gw_id(gw_id), gw_addr(NULL), gw_addr_len(0)
{
    
}

uint8_t MQTTSNMessageGWInfo::pack(uint8_t * buffer, uint8_t buflen) 
{
    header = MQTTSNHeader(MQTTSN_GWINFO);
    offset = header.pack(buffer, buflen, 1 + gw_addr_len);
    if (!offset) {
        return 0;
    }
    
    header[offset++] = gw_id;
    memcpy(&buffer[offset], gw_addr, gw_addr_len);
    offset += gw_addr_len;
    return offset;
}

uint8_t MQTTSNMessageGWInfo::unpack(uint8_t * buffer, uint8_t buflen) 
{
    /* we expect 1 bytes at least for gw_id, gw_addr is optional */
    if (buflen == 0) {
        return 0;
    }
    
    gw_id = buffer[0];
    gw_addr = (buflen > 1) ? &buffer[1] : NULL;
    return buflen;
}

/**************** MQTTSNMessageConnect ***************/

MQTTSNMessageConnect::MQTTSNMessageConnect(uint8_t duration) : 
    protocol_id(1), duration(duration), client_id(NULL), client_id_len(0)
{
    
}

uint8_t MQTTSNMessageConnect::pack(uint8_t * buffer, uint8_t buflen) 
{
    header = MQTTSNHeader(MQTTSN_CONNECT);
    offset = header.pack(buffer, buflen, 1 + 1 + 2 + client_id_len);
    if (!offset) {
        return 0;
    }
    
    /* fill in the message content */
    header[offset++] = flags.pack();
    header[offset++] = protocol_id;
    header[offset++] = duration >> 8;
    header[offset++] = duration & 0xff;
    
    memcpy(&buffer[offset], client_id, client_id_len);
    offset += client_id_len;
    return offset;
}

uint8_t MQTTSNMessageConnect::unpack(uint8_t * buffer, uint8_t buflen) 
{
    /* we expect 5 bytes at least, client ID is not optional */
    uint8_t fixed_len = 1 + 1 + 2;
    if (buflen < fixed_len + 1) {
        return 0;
    }
    
    flags.unpack(buffer[0]);
    protocol_id = buffer[1];
    duration = ((uint16_t)buffer[2] << 8) | buffer[3];
    client_id = &buffer[4];
    client_id_len = buflen - fixed_len;
    
    return buflen;
}

/**************** MQTTSNMessageConnack ***************/

MQTTSNMessageConnack::MQTTSNMessageConnack(uint8_t return_code) : return_code(return_code)
{
    
}

uint8_t MQTTSNMessageConnack::pack(uint8_t * buffer, uint8_t buflen) 
{
    header = MQTTSNHeader(MQTTSN_CONNACK);
    offset = header.pack(buffer, buflen, 1);
    if (!offset) {
        return 0;
    }
    
    header[offset++] = return_code;
    return offset;
}

uint8_t MQTTSNMessageConnack::unpack(uint8_t * buffer, uint8_t buflen) 
{
    /* we expect 1 byte */
    if (buflen != 1) {
        return 0;
    }
    
    return_code = buffer[0];
    return buflen;
}

/**************** MQTTSNMessageRegister ***************/

MQTTSNMessageRegister::MQTTSNMessageRegister(uint16_t topic_id) : 
    topic_id(topic_id), msg_id(0), topic_name(NULL), topic_name_len(0)
{
    
}

uint8_t MQTTSNMessageRegister::pack(uint8_t * buffer, uint8_t buflen) 
{
    header = MQTTSNHeader(MQTTSN_REGISTER);
    offset = header.pack(buffer, buflen, 2 + 2 + topic_name_len);
    if (!offset) {
        return 0;
    }
    
    /* fill in the message content */
    header[offset++] = topic_id >> 8;
    header[offset++] = topic_id & 0xff;
    header[offset++] = msg_id >> 8;
    header[offset++] = msg_id & 0xff;
    
    memcpy(&buffer[offset], topic_name, topic_name_len);
    offset += topic_name_len;
    return offset;
}

uint8_t MQTTSNMessageRegister::unpack(uint8_t * buffer, uint8_t buflen) 
{
    /* we expect 5 bytes at least, topic name is not optional */
    uint8_t fixed_len = 2 + 2;
    if (buflen < fixed_len + 1) {
        return 0;
    }
    
    topic_id = ((uint16_t)buffer[0] << 8) | buffer[1];
    msg_id = ((uint16_t)buffer[2] << 8) | buffer[3];
    topic_name = &buffer[4];
    topic_name_len = buflen - fixed_len;
    
    return buflen;
}

/**************** MQTTSNMessageRegack ***************/

MQTTSNMessageRegack::MQTTSNMessageRegack(uint8_t return_code) : 
    topic_id(0), msg_id(0), return_code(return_code)
{
    
}

uint8_t MQTTSNMessageRegack::pack(uint8_t * buffer, uint8_t buflen) 
{
    header = MQTTSNHeader(MQTTSN_REGACK);
    offset = header.pack(buffer, buflen, 2 + 2 + 1);
    if (!offset) {
        return 0;
    }
    
    /* fill in the message content */
    header[offset++] = topic_id >> 8;
    header[offset++] = topic_id & 0xff;
    header[offset++] = msg_id >> 8;
    header[offset++] = msg_id & 0xff;
    header[offset++] = return_code;
    
    return offset;
}

uint8_t MQTTSNMessageRegack::unpack(uint8_t * buffer, uint8_t buflen) 
{
    /* we expect 5 bytes at least, topic name is not optional */
    uint8_t fixed_len = 2 + 2 + 1;
    if (buflen != fixed_len) {
        return 0;
    }
    
    topic_id = ((uint16_t)buffer[0] << 8) | buffer[1];
    msg_id = ((uint16_t)buffer[2] << 8) | buffer[3];
    return_code = buffer[4];
    
    return buflen;
}

/**************** MQTTSNMessagePublish ***************/

MQTTSNMessagePublish::MQTTSNMessagePublish(uint16_t msg_id) : 
    topic_id(0), msg_id(msg_id), data(NULL), data_len(0)
{
    
}

uint8_t MQTTSNMessagePublish::pack(uint8_t * buffer, uint8_t buflen) 
{
    header = MQTTSNHeader(MQTTSN_PUBLISH);
    offset = header.pack(buffer, buflen, 1 + 2 + 2 + data_len);
    if (!offset) {
        return 0;
    }
    
    /* fill in the message content */
    header[offset++] = flags.pack();
    header[offset++] = topic_id >> 8;
    header[offset++] = topic_id & 0xff;
    header[offset++] = msg_id >> 8;
    header[offset++] = msg_id & 0xff;
    
    memcpy(&buffer[offset], data, data_len);
    offset += data_len;
    return offset;
}

uint8_t MQTTSNMessagePublish::unpack(uint8_t * buffer, uint8_t buflen) 
{
    /* we expect 5 bytes at least, payload is optional */
    uint8_t fixed_len = 1 + 2 + 2;
    if (buflen < fixed_len) {
        return 0;
    }
    
    flags.unpack(buffer[0]);
    topic_id = ((uint16_t)buffer[1] << 8) | buffer[2];
    msg_id = ((uint16_t)buffer[3] << 8) | buffer[4];
    data = (buflen > fixed_len) ? &buffer[5] : NULL;
    data_len = buflen - fixed_len;
    
    return buflen;
}

/**************** MQTTSNMessagePuback ***************/

MQTTSNMessagePuback::MQTTSNMessagePuback(uint8_t return_code) : 
    topic_id(0), msg_id(0), return_code(return_code)
{
    
}

uint8_t MQTTSNMessagePuback::pack(uint8_t * buffer, uint8_t buflen) 
{
    header = MQTTSNHeader(MQTTSN_PUBACK);
    offset = header.pack(buffer, buflen, 2 + 2 + 1);
    if (!offset) {
        return 0;
    }
    
    /* fill in the message content */
    header[offset++] = topic_id >> 8;
    header[offset++] = topic_id & 0xff;
    header[offset++] = msg_id >> 8;
    header[offset++] = msg_id & 0xff;
    header[offset++] = return_code;
    
    return offset;
}

uint8_t MQTTSNMessagePuback::unpack(uint8_t * buffer, uint8_t buflen) 
{
    /* we expect 5 bytes at least, topic name is not optional */
    uint8_t fixed_len = 2 + 2 + 1;
    if (buflen != fixed_len) {
        return 0;
    }
    
    topic_id = ((uint16_t)buffer[0] << 8) | buffer[1];
    msg_id = ((uint16_t)buffer[2] << 8) | buffer[3];
    return_code = buffer[4];
    
    return buflen;
}

/**************** MQTTSNMessageSubscribe ***************/

MQTTSNMessageSubscribe::MQTTSNMessageSubscribe(uint16_t topic_id) : 
    msg_id(0), topic_name(NULL), topic_name_len(0)
{
    
}

uint8_t MQTTSNMessageSubscribe::pack(uint8_t * buffer, uint8_t buflen) 
{
    header = MQTTSNHeader(MQTTSN_SUBSCRIBE);
    offset = header.pack(buffer, buflen, 1 + 2 + topic_name_len);
    if (!offset) {
        return 0;
    }
    
    /* fill in the message content */
    header[offset++] = flags.pack();
    header[offset++] = msg_id >> 8;
    header[offset++] = msg_id & 0xff;
    
    memcpy(&buffer[offset], topic_name, topic_name_len);
    offset += topic_name_len;
    return offset;
}

uint8_t MQTTSNMessageSubscribe::unpack(uint8_t * buffer, uint8_t buflen) 
{
    /* we expect 4 bytes at least, topic name/ID is not optional */
    uint8_t fixed_len = 1 + 2;
    if (buflen < fixed_len + 1) {
        return 0;
    }
    
    flags.unpack(buffer[0]);
    msg_id = ((uint16_t)buffer[1] << 8) | buffer[2];
    topic_name = &buffer[3];
    topic_name_len = buflen - fixed_len;
    
    return buflen;
}

/**************** MQTTSNMessageUnsubscribe ***************/

MQTTSNMessageUnsubscribe::MQTTSNMessageUnsubscribe(uint16_t topic_id) : 
    msg_id(0), topic_name(NULL), topic_name_len(0)
{
    
}

uint8_t MQTTSNMessageUnsubscribe::pack(uint8_t * buffer, uint8_t buflen) 
{
    header = MQTTSNHeader(MQTTSN_UNSUBSCRIBE);
    offset = header.pack(buffer, buflen, 1 + 2 + topic_name_len);
    if (!offset) {
        return 0;
    }
    
    /* fill in the message content */
    header[offset++] = flags.pack();
    header[offset++] = msg_id >> 8;
    header[offset++] = msg_id & 0xff;
    
    memcpy(&buffer[offset], topic_name, topic_name_len);
    offset += topic_name_len;
    return offset;
}

uint8_t MQTTSNMessageUnsubscribe::unpack(uint8_t * buffer, uint8_t buflen) 
{
    /* we expect 4 bytes at least, topic name/ID is not optional */
    uint8_t fixed_len = 1 + 2;
    if (buflen < fixed_len + 1) {
        return 0;
    }
    
    flags.unpack(buffer[0]);
    msg_id = ((uint16_t)buffer[1] << 8) | buffer[2];
    topic_name = &buffer[3];
    topic_name_len = buflen - fixed_len;
    
    return buflen;
}

/**************** MQTTSNMessageSuback ***************/

MQTTSNMessageSuback::MQTTSNMessageSuback(uint8_t return_code) : 
    topic_id(0), msg_id(0), return_code(return_code)
{
    
}

uint8_t MQTTSNMessageSuback::pack(uint8_t * buffer, uint8_t buflen) 
{
    header = MQTTSNHeader(MQTTSN_REGACK);
    offset = header.pack(buffer, buflen, 1 + 2 + 2 + 1);
    if (!offset) {
        return 0;
    }
    
    /* fill in the message content */
    header[offset++] = flags.pack();
    header[offset++] = topic_id >> 8;
    header[offset++] = topic_id & 0xff;
    header[offset++] = msg_id >> 8;
    header[offset++] = msg_id & 0xff;
    header[offset++] = return_code;
    
    return offset;
}

uint8_t MQTTSNMessageSuback::unpack(uint8_t * buffer, uint8_t buflen) 
{
    /* we expect exactly 6 bytes */
    uint8_t fixed_len = 1 + 2 + 2 + 1;
    if (buflen != fixed_len) {
        return 0;
    }
    
    flags.unpack(buffer[0]);
    topic_id = ((uint16_t)buffer[1] << 8) | buffer[2];
    msg_id = ((uint16_t)buffer[3] << 8) | buffer[4];
    return_code = buffer[5];
    
    return buflen;
}

/**************** MQTTSNMessageUnsuback ***************/

MQTTSNMessageUnsuback::MQTTSNMessageUnsuback(uint16_t topic_id) : msg_id(0)
{
    
}

uint8_t MQTTSNMessageUnsuback::pack(uint8_t * buffer, uint8_t buflen) 
{
    header = MQTTSNHeader(MQTTSN_UNSUBACK);
    offset = header.pack(buffer, buflen, 2);
    if (!offset) {
        return 0;
    }
    
    /* fill in the message content */
    header[offset++] = msg_id >> 8;
    header[offset++] = msg_id & 0xff;
    
    return offset;
}

uint8_t MQTTSNMessageUnsuback::unpack(uint8_t * buffer, uint8_t buflen) 
{
    /* we expect exactly 2 bytes */
    uint8_t fixed_len = 2;
    if (buflen != fixed_len) {
        return 0;
    }
    
    msg_id = ((uint16_t)buffer[0] << 8) | buffer[1];
    return buflen;
}

/**************** MQTTSNMessagePingreq ***************/

/* only used when the client ID is present */
MQTTSNMessagePingreq::MQTTSNMessagePingreq(void) : 
    client_id(NULL), client_id_len(0)
{
    
}

uint8_t MQTTSNMessagePingreq::pack(uint8_t * buffer, uint8_t buflen) 
{
    header = MQTTSNHeader(MQTTSN_PINGREQ);
    offset = header.pack(buffer, buflen, client_id_len);
    if (!offset) {
        return 0;
    }
    
    /* fill in the message content */
    memcpy(&buffer[offset], client_id, client_id_len);
    offset += client_id_len;
    
    return offset;
}

uint8_t MQTTSNMessagePingreq::unpack(uint8_t * buffer, uint8_t buflen) 
{
    /* empty payload not allowed, client ID must be present */
    if (buflen == 0) {
        return 0;
    }
    
    client_id = buffer;
    client_id_len = buflen;
    
    return buflen;
}

/**************** MQTTSNMessagePingresp ***************/

/* not really used for now */
MQTTSNMessagePingresp::MQTTSNMessagePingresp(void)
{
    
}

uint8_t MQTTSNMessagePingresp::pack(uint8_t * buffer, uint8_t buflen) 
{
    header = MQTTSNHeader(MQTTSN_PINGRESP);
    offset = header.pack(buffer, buflen);
    if (!offset) {
        return 0;
    }
    
    return offset;
}

uint8_t MQTTSNMessagePingresp::unpack(uint8_t * buffer, uint8_t buflen) 
{    
    return 0;
}

/**************** MQTTSNMessageDisconnect ***************/

/* only used when there's a non-zero sleep duration */
MQTTSNMessageDisconnect::MQTTSNMessageDisconnect(uint8_t gw_id) : duration(0)
{
    
}

uint8_t MQTTSNMessageDisconnect::pack(uint8_t * buffer, uint8_t buflen) 
{
    header = MQTTSNHeader(MQTTSN_DISCONNECT);
    offset = header.pack(buffer, buflen, duration ? 2 : 0);
    if (!offset) {
        return 0;
    }
    
    if (duration) {
        header[offset++] = duration >> 8;
        header[offset++] = duration & 0xff;
    }
    
    return offset;
}

uint8_t MQTTSNMessageDisconnect::unpack(uint8_t * buffer, uint8_t buflen) 
{
    /* we expect exactly 2 bytes */
    if (buflen != 2) {
        return 0;
    }
    
    duration = ((uint16_t)buffer[0] << 8) | buffer[1];
    return buflen;
}


