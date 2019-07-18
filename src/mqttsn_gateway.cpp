#include "mqttsn_gateway.h"
#include "mqttsn_defines.h"

#include <string.h>

/********************** MQTTSNInstance ************************/

MQTTSNInstance::MQTTSNInstance(void) :
    msg_inflight_len(0), unicast_timer(0), unicast_counter(0),
    keepalive_interval(0), keepalive_timeout(0), last_in(0), status(MQTTSNInstanceStatus_DISCONNECTED)
{
    client_id[0] = 0;
    address.len = 0;
}

bool MQTTSNInstance::register_(const char * cid, MQTTSNAddress * addr, uint16_t duration, MQTTSNFlags * flags)
{
    if (strlen(cid) > MQTTSN_MAX_CLIENTID_LEN)
        return false;
        
    memcpy(&address, addr, sizeof(MQTTSNAddress));
    keepalive_interval = duration * 1000UL;
    keepalive_timeout = (keepalive_interval > 60000) ? keepalive_interval * 1.1 : keepalive_interval * 1.5;
    
    connect_flags.all = flags->all;
    
    /* mark them all free */
    for (uint16_t i = 0; i < MQTTSN_MAX_INSTANCE_TOPICS; i++) {
        sub_topics[i].tid = MQTTSN_TOPIC_NOTASSIGNED;
        pub_topics[i].tid = MQTTSN_TOPIC_NOTASSIGNED;
    }

    msg_inflight_len = 0;
    status = MQTTSNInstanceStatus_ACTIVE;
    mark_time();
}

void MQTTSNInstance::deregister(void)
{
    client_id[0] = 0;
    address.len = 0;
    status = MQTTSNInstanceStatus_DISCONNECTED;
}

bool MQTTSNInstance::add_sub_topic(uint16_t tid, MQTTSNFlags * flags)
{
    /* check if we're already subbed
       and only update the flags if so */
    for (uint16_t i = 0; i < MQTTSN_MAX_INSTANCE_TOPICS; i++) {
        if (sub_topics[i].tid == tid) {
            sub_topics[i].flags.all = flags->all;
            return true;
        }
    }

    /* else add the new topic to our list */
    for (uint16_t i = 0; i < MQTTSN_MAX_INSTANCE_TOPICS; i++) {
        if (sub_topics[i].tid == MQTTSN_TOPIC_NOTASSIGNED) {
            sub_topics[i].tid = tid;
            sub_topics[i].flags.all = flags->all;
            return true;
        }
    }

    /* no more space */
    return false;
}

bool MQTTSNInstance::add_pub_topic(uint16_t tid)
{
    /* check if its already registered */
    for (uint16_t i = 0; i < MQTTSN_MAX_INSTANCE_TOPICS; i++) {
        if (pub_topics[i].tid == tid) {
            return true;
        }
    }

    /* else add the new topic to our list */
    for (uint16_t i = 0; i < MQTTSN_MAX_INSTANCE_TOPICS; i++) {
        if (pub_topics[i].tid == MQTTSN_TOPIC_NOTASSIGNED) {
            pub_topics[i].tid = tid;
            return true;
        }
    }

    /* no more space */
    return false;
}

void MQTTSNInstance::delete_sub_topic(uint16_t tid)
{
    for (uint16_t i = 0; i < MQTTSN_MAX_INSTANCE_TOPICS; i++) {
        if (sub_topics[i].tid == tid) {
            sub_topics[i].tid = MQTTSN_TOPIC_NOTASSIGNED;
            sub_topics[i].flags.all = 0;
            return true;
        }
    }
}

bool MQTTSNInstance::is_subbed(uint16_t tid)
{
    for (uint16_t i = 0; i < MQTTSN_MAX_INSTANCE_TOPICS; i++) {
        if (sub_topics[i].tid == tid) {
            return true;
        }
    }
    
    return false;
}

MQTTSNInstanceStatus MQTTSNInstance::check_status(MQTTSNTransport * transport)
{
    /* check last time we got a control packet */
    uint32_t curr_time = device->get_millis();
    if (curr_time - last_in > keepalive_timeout) {
        status = MQTTSNInstanceStatus_LOST;
        return status;
    }

    /* no outstanding msgs so return */
    if (msg_inflight_len == 0)
        return status;

    /* check if retry timer is up */
    if (curr_time - unicast_timer < MQTTSN_T_RETRY)
        return status;
        
    unicast_counter++;

    /* check if retry counter is up */
    if (unicast_counter > MQTTSN_N_RETRY):
        status = MQTTSNInstanceStatus_LOST;
        return status;

    /* resend the msg if not */
    transport.write_packet(msg_inflight, msg_inflight_len, &address);
    unicast_timer = curr_time;
    return status;  
}

void MQTTSNInstance::mark_time(void)
{
    last_in = device->get_millis();
}

operator MQTTSNInstance::bool() const
{
    return strlen(client_id) != 0;
}

/********************** MQTTSNGateway ************************/

MQTTSNGateway::MQTTSNGateway(uint8_t gw_id, MQTTSNDevice * device, MQTTSNTransport * transport, MQTTClient * client) :
    gw_id(gw_id), device(device), transport(transport), mqtt_client(client), 
    connected(false), curr_msg_id(0), 
    pub_fifo(pub_fifo_buf, MQTTSN_MAX_QUEUED_PUBLISH, MQTTSN_MAX_MSG_LEN)
{
    
}

void MQTTSNGateway::assign_msg_handlers(void) 
{
    msg_handlers[MQTTSN_SEARCHGW] = &MQTTSNGateway::handle_searchgw;
    msg_handlers[MQTTSN_CONNECT] = &MQTTSNGateway::handle_connect;
    msg_handlers[MQTTSN_REGISTER] = &MQTTSNGateway::handle_register;
    msg_handlers[MQTTSN_PUBLISH] = &MQTTSNGateway::handle_publish;
    msg_handlers[MQTTSN_SUBSCRIBE] = &MQTTSNGateway::handle_subscribe;
    msg_handlers[MQTTSN_UNSUBSCRIBE] = &MQTTSNGateway::handle_unsubscribe;
    msg_handlers[MQTTSN_PINGREQ] = &MQTTSNGateway::handle_pingreq;
}

void MQTTSNGateway::loop(void)
{
    /* handle any messages from clients */
    handle_messages();
    
    /* check keepalive and any inflight messages */
    for (int i = 0; i < MQTTSN_MAX_NUM_CLIENTS; i++) {
        if (clients[i] && clients[i].check_status() == MQTTSNInstanceStatus_LOST) {
            clients[i].deregister();
        }
    }

    /* now distribute any pending publish msgs
       from the queue */
    while (pub_fifo.available()) {
        pub_fifo.dequeue(temp_msg);
        
        /* parse the header so we can get the msg type */
        MQTTSNHeader header;
        uint8_t offset = header.unpack(temp_msg, MQTTSN_MAX_MSG_LEN);
        if (offset == 0 || header.msg_type != MQTTSN_PUBLISH)
            continue;
        
        temp_msg_len = header.length;
        
        /* unpack the message, only QoS 0 for now */
        MQTTSNMessagePublish msg;
        if (!msg.unpack(temp_msg, temp_msg_len) || msg.msg_id != 0x0000)
            continue;
        
        /* dispatch msg to clients */
        uint16_t tid = msg.topic_id;
        for (int i = 0; i < MQTTSN_MAX_NUM_CLIENTS; i++) {
            if (clients[i].is_subbed(tid)) {
                transport->write_packet(temp_msg, temp_msg_len);
            }
        }
    }

    /* just to return something useful */
    return connected;
}

