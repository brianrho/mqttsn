/* Written by Brian Ejike (2019)
 * DIstributed under the MIT License */
 
#include "../mqttsn_excludes.h"

#if !defined(MQTTSN_EXCLUDE_TRANSPORT_RFM69X)

#include "mqttsn_transport_rfm69x.h"

#include <stdint.h>
#include <RFM69X.h>

MQTTSNTransportRFM69X::MQTTSNTransportRFM69X(RFM69X * radio) : radio(radio)
{
    
}

uint8_t MQTTSNTransportRFM69X::write_packet(const void * data, uint8_t data_len, MQTTSNAddress * dest) 
{
    /* dest address should be just one byte in size */
    if (dest->len != 1) return 0;
    
    /* don't request for ack */
    radio->send(dest->bytes[0], data, data_len, false);
    return data_len;
}

int16_t MQTTSNTransportRFM69X::read_packet(void * data, uint8_t data_len, MQTTSNAddress * src) 
{
    /* at least 1 byte capacity */
    if (MQTTSN_MAX_ADDR_LEN == 0) return 0;
    
    if (!radio->receiveDone())
        return -1;
        
    if (data_len < radio->DATALEN)
        return 0;
        
    src->len = 1;
    src->bytes[0] = radio->SENDERID;
    memcpy(data, (const void *)radio->DATA, radio->DATALEN);
    return radio->DATALEN;
}

uint8_t MQTTSNTransportRFM69X::broadcast(const void * data, uint8_t data_len) 
{
    /* don't request for ack */
    radio->send(RF69_BROADCAST_ADDR, data, data_len, false);
    return data_len;
}

#endif

