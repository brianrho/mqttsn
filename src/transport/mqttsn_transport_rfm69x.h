#ifndef MQTTSN_TRANSPORT_RFM69X_H_
#define MQTTSN_TRANSPORT_RFM69X_H_

#include <stdint.h>
#include "../mqttsn_transport.h"


class RFM69X;


class MQTTSNTransportRFM69X : public MQTTSNTransport {
    public:
        MQTTSNTransportRFM69X(RFM69X * radio);
        
        virtual uint8_t write_packet(const void * data, uint8_t data_len, MQTTSNAddress * dest);
        virtual int16_t read_packet(void * data, uint8_t data_len, MQTTSNAddress * src);
        virtual uint8_t broadcast(const void * data, uint8_t data_len);
    
    protected:
        RFM69X * radio;
};

#endif
