#include "mqttsn_messages.h"

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

/*********MQTTSNHeader**************/
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

void MQTTSNFlags::pack(void) 
{
    all = (dup << 7) | (qos << 5) | (retain << 4) | (will << 3) | (clean_session << 2) | topicid_type;
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


/**************** MQTTSNAdvertise ***************/
MQTTSNAdvertise::MQTTSNAdvertise(uint8_t gw_id) : gw_id(gw_id), duration(0)
{
    
}

uint8_t MQTTSNAdvertise::pack(uint8_t * buffer, uint8_t buflen) 
{
    header = MQTTSNHeader(MQTTSN_ADVERTISE);
    offset = header.pack(buffer, buflen);
    if (!offset) {
        return 0;
    }
    
    header[offset++] = gw_id;
    header[offset++] = duration >> 8;
    header[offset++] = duration & 0xff;
    return offset;
}

uint8_t MQTTSNAdvertise::unpack(uint8_t * buffer, uint8_t buflen) 
{
    /* we expect 3 bytes */
    if (buflen != 3) {
        return 0;
    }
    
    gw_id = buffer[0];
    duration = ((uint16_t)buffer[1] << 8) | buffer[2];
    return 3;
}

