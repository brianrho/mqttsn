/* Written by Brian Ejike (2019)
 * DIstributed under the MIT License */
 
#include "../mqttsn_excludes.h"

#if !defined(MQTTSN_EXCLUDE_TRANSPORT_HC12)

#include "mqttsn_transport_hc12.h"

#include <stdint.h>
#include <hc12.h>

MQTTSNTransportHC12::MQTTSNTransportHC12(HC12 * port) : port(port)
{
    
}

uint8_t MQTTSNTransportHC12::write_packet(const void * data, uint8_t data_len, MQTTSNAddress * dest) 
{
    /* address should be just one byte in size */
    if (dest->len != 1) return 0;
    
    return port->send(data, data_len, dest->bytes[0]);
}

int16_t MQTTSNTransportHC12::read_packet(void * data, uint8_t data_len, MQTTSNAddress * src) 
{
    /* at least 1 byte capacity */
    if (MQTTSN_MAX_ADDR_LEN == 0) return 0;
    
    src->len = 1;
    return port->recv(data, data_len, src->bytes);
}

uint8_t MQTTSNTransportHC12::broadcast(const void * data, uint8_t data_len) 
{
    return port->broadcast(data, data_len);
}

#endif

