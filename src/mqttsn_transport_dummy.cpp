/* Written by Brian Ejike (2019)
 * DIstributed under the MIT License */

#include "mqttsn_excludes.h"

#if !defined(MQTTSN_EXCLUDE_TRANSPORT_DUMMY)

#include "mqttsn_transport_dummy.h"
#include <lite_fifo.h>
#include <string.h>

MQTTSNTransportDummy * MQTTSNTransportDummy::dummies[MQTTSN_MAX_DUMMY_TRANSPORTS] = {NULL};
uint8_t MQTTSNTransportDummy::tempbuf[MQTTSN_MAX_MSG_LEN + 1] = {0};

MQTTSNTransportDummy::MQTTSNTransportDummy(uint8_t addr) : 
    address(addr), read_fifo(read_buf, MQTTSN_TRANSPORT_DUMMY_QUEUED_MSGS, MQTTSN_MAX_MSG_LEN)
{
    for (int i = 0; i < MQTTSN_MAX_DUMMY_TRANSPORTS; i++) {
        /* save the instance for later */
        if (dummies[i] == NULL) {
            dummies[i] = this;
            return;
        }
    }
}

/* Format is: {1-byte address}{MQTTSN message payload} 
 * First byte of an MQTTSN message is always the length, so we use that */
 
uint8_t MQTTSNTransportDummy::write_packet(const void * data, uint8_t data_len, MQTTSNAddress * dest)
{
    /* address should be just one byte in size */
    if (dest->len != 1 || data_len > MQTTSN_MAX_MSG_LEN) 
        return 0;
    
    /* copy src address and data into buffer */
    tempbuf[0] = address;
    memcpy(tempbuf + 1, data, data_len);
    
    for (int i = 0; i < MQTTSN_MAX_DUMMY_TRANSPORTS; i++) {
        MQTTSNTransportDummy * dummy = dummies[i];
        
        /* check that the message is meant for this dummy */
        if (dummy != NULL && (dummy->address == dest->bytes[0])) {
            return dummy->read_fifo.enqueue(tempbuf) ? data_len : 0;
        }
    }
    
    return 0;
}

int16_t MQTTSNTransportDummy::read_packet(void * data, uint8_t data_len, MQTTSNAddress * src)
{
    /* at least 1 byte capacity */
    if (MQTTSN_MAX_ADDR_LEN == 0) return 0;
    
    /* make sure there's something to read */
    if (read_fifo.available() == 0)
        return -1;
    
    /* get the real length of the msg */
    read_fifo.peek(tempbuf);
    uint8_t real_len = tempbuf[1];
    
    /* not enough space to hold it */
    if (data_len < real_len)
        return 0;
    
    read_fifo.dequeue(tempbuf);
    
    /* copy address and data into user buffer */
    src->bytes[0] = tempbuf[0];
    src->len = 1;
    memcpy(data, tempbuf + 1, real_len);
    return real_len;
}

uint8_t MQTTSNTransportDummy::broadcast(const void * data, uint8_t data_len)
{
    if (data_len > MQTTSN_MAX_MSG_LEN) 
        return 0;
        
    memcpy(tempbuf, data, data_len);
    
    for (int i = 0; i < MQTTSN_MAX_DUMMY_TRANSPORTS; i++) {
        MQTTSNTransportDummy * dummy = dummies[i];
        
        /* (almost) everybody gets this msg */
        if (dummy != NULL && dummy != this) {
            dummy->read_fifo.enqueue(tempbuf);
        }
    }
    
    return data_len;
}

#endif

