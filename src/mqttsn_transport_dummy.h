/* Written by Brian Ejike (2019)
 * DIstributed under the MIT License */
 
#ifndef MQTTSN_TRANSPORT_DUMMY_H_
#define MQTTSN_TRANSPORT_DUMMY_H_

#include "mqttsn_defines.h"
#include "mqttsn_transport.h"
#include <lite_fifo.h>

/* enough to buffer 8 messages for each dummy */
#define MQTTSN_TRANSPORT_DUMMY_QUEUED_MSGS      8

class MQTTSNTransportDummy : public MQTTSNTransport {
    public:
        MQTTSNTransportDummy(uint8_t addr);
        virtual uint8_t write_packet(const void * data, uint8_t data_len, MQTTSNAddress * dest);
        virtual int16_t read_packet(void * data, uint8_t data_len, MQTTSNAddress * src);
        virtual uint8_t broadcast(const void * data, uint8_t data_len);
    
    private:
        uint8_t address;
        LiteFifo read_fifo;
        uint8_t read_buf[MQTTSN_TRANSPORT_DUMMY_QUEUED_MSGS * (MQTTSN_MAX_MSG_LEN + 1)];
        
        /* +1 is for address */
        static uint8_t tempbuf[MQTTSN_MAX_MSG_LEN + 1];
        /* keep a reference to each dummy client, +1 for the gateway too */
        static MQTTSNTransportDummy * dummies[MQTTSN_MAX_DUMMY_TRANSPORTS];
};

#endif
