#ifndef MQTTSN_TRANSPORT_HC12_H_
#define MQTTSN_TRANSPORT_HC12_H_

#include <stdint.h>
#include "../mqttsn_transport.h"


class HC12;


class MQTTSNTransportHC12 : public MQTTSNTransport {
    public:
        MQTTSNTransportHC12(HC12 * port);
        
        virtual uint8_t write_packet(const void * data, uint8_t data_len, MQTTSNAddress * dest);
        virtual int16_t read_packet(void * data, uint8_t data_len, MQTTSNAddress * src);
        virtual uint8_t broadcast(const void * data, uint8_t data_len);
    
    protected:
        HC12 * port;
};

#endif
