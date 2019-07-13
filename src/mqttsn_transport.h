#ifndef MQTTSN_TRANSPORT_H_
#define MQTTSN_TRANSPORT_H_

#include <stdint.h>

/* For transport-related addresses, 
 * should be declared statically or explicitly zero-initialized
 */
typedef struct {
    uint8_t bytes[MQTTSN_MAX_ADDR_LEN];
    uint8_t len;
} MQTTSNAddress;

class MQTTSNTransport {
    public:
        
        virtual uint8_t write_packet(const void * data, uint8_t data_len, MQTTSNAddress * dest) = 0;
        virtual int16_t read_packet(void * data, uint8_t data_len, MQTTSNAddress * src) = 0;
        virtual uint8_t broadcast(const void * data, uint8_t data_len) = 0;
        
};

#endif
