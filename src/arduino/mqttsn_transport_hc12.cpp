#include "mqttsn_transport_hc12.h"

#include <stdint.h>
#include <HC12.h>

MQTTSNTransportHC12::MQTTSNTransportHC12(HC12 * port) : port(port)
{
    
}

uint8_t MQTTSNTransportHC12::write_packet(const void * data, uint8_t data_len, uint8_t * dest, uint8_t dest_len) 
{
    /* address should be just one byte in size */
    if (dest_len != 1) return 0;
    
    return port->send(data, data_len, *dest);
}

int16_t MQTTSNTransportHC12::read_packet(void * data, uint8_t data_len, uint8_t * src, uint8_t src_len) 
{
    /* at least 1 byte capacity */
    if (src_len == 0) return 0;
    
    return port->recv(data, data_len, src);
}

uint8_t MQTTSNTransportHC12::broadcast(const void * data, uint8_t data_len) 
{
    return port->broadcast(data, data_len);
}

