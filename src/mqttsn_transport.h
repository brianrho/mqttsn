/* Written by Brian Ejike (2019)
 * DIstributed under the MIT License */
 
#ifndef MQTTSN_TRANSPORT_H_
#define MQTTSN_TRANSPORT_H_

#include <stdint.h>
#include "mqttsn_defines.h"

/* For transport-related data
   bytes: the transport-specific data i.e. address(es), sync words, etc
   len: the occupied length of 'bytes' */
typedef struct {
    uint8_t bytes[MQTTSN_MAX_ADDR_LEN];
    uint8_t len;
} MQTTSNAddress;

/* interface for any transport */
class MQTTSNTransport {
    public:
        
        /* return how many bytes were written, 0 if any error occurred */
        virtual uint8_t write_packet(const void * data, uint8_t data_len, MQTTSNAddress * dest) = 0;
        
        /* return -1 if there's nothing to read, 0 for too-long msgs, otherwise the payload length */
        virtual int16_t read_packet(void * data, uint8_t data_len, MQTTSNAddress * src) = 0;
        
        /* return how many bytes were written, 0 if any error occurred */
        virtual uint8_t broadcast(const void * data, uint8_t data_len) = 0;
        
};

#endif
