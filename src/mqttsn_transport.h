#ifndef MQTTSN_TRANSPORT_H_
#define MQTTSN_TRANSPORT_H_

#include <stdint.h>

class MQTTSNTransport {
    public:
        
        virtual uint8_t write_packet(const void * data, uint8_t data_len, uint8_t * dest, uint8_t dest_len) = 0;
        virtual int16_t read_packet(void * data, uint8_t data_len, uint8_t * src, uint8_t src_len) = 0;
        virtual uint8_t broadcast(const void * data, uint8_t data_len) = 0;
        
};

#endif
