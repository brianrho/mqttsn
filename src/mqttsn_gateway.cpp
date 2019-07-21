/* Written by Brian Ejike (2019)
 * DIstributed under the MIT License */
 
#include "mqttsn_gateway.h"
#include "mqttsn_messages.h"
#include "mqttsn_defines.h"
#include "mqttsn_device.h"
#include "mqttsn_transport.h"
#include "mqttx_client.h"

#include <string.h>
#include <stdlib.h>

/********************** MQTTSNInstance ************************/

MQTTSNInstance::MQTTSNInstance(void) :
    msg_inflight_len(0), unicast_timer(0), unicast_counter(0),
    keepalive_interval(0), keepalive_timeout(0), last_in(0), status(MQTTSNInstanceStatus_DISCONNECTED)
{
    client_id[0] = 0;
    memset(&address, 0, sizeof(MQTTSNAddress));
}

bool MQTTSNInstance::register_(uint8_t * cid, uint8_t cid_len, MQTTSNAddress * addr, uint16_t duration, MQTTSNFlags * flags)
{
    if (cid_len > MQTTSN_MAX_CLIENTID_LEN)
        return false;
    
    memcpy(client_id, cid, cid_len);
    client_id[cid_len] = 0;
    
    /* copy the address */
    memcpy(&address.bytes, addr->bytes, addr->len);
    address.len = addr->len;
    
    MQTTSN_INFO_PRINT("Address: ");
    for (int i = 0; i < address.len; i++) {
        MQTTSN_INFO_PRINT("%X ", address.bytes[i]);
    }
    MQTTSN_INFO_PRINTLN();
        
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
    return true;
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
            return;
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

MQTTSNInstanceStatus MQTTSNInstance::check_status(MQTTSNDevice * device, MQTTSNTransport * transport)
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
    if (unicast_counter > MQTTSN_N_RETRY) {
        status = MQTTSNInstanceStatus_LOST;
        return status;
    }

    /* resend the msg if not */
    transport->write_packet(msg_inflight, msg_inflight_len, &address);
    unicast_timer = curr_time;
    return status;  
}

void MQTTSNInstance::mark_time(uint32_t now)
{
    last_in = now;
}

MQTTSNInstance::operator bool() const
{
    return strlen(client_id) != 0;
}

/********************** MQTTSNGateway ************************/

MQTTSNGateway::MQTTSNGateway(MQTTSNDevice * device, MQTTSNTransport * transport, MQTTClient * client) :
    gw_id(0), device(device), transport(transport), mqtt_client(client), 
    connected(false), curr_msg_id(0), 
    pub_fifo(pub_fifo_buf, MQTTSN_MAX_QUEUED_PUBLISH, MQTTSN_MAX_MSG_LEN)
{
    
}

bool MQTTSNGateway::begin(uint8_t gw_id)
{
    this->gw_id = gw_id;
    if (mqtt_client) {
        mqtt_client->register_callbacks(this, MQTTSNGateway::handle_mqtt_connect, MQTTSNGateway::handle_mqtt_publish);
    }
    assign_msg_handlers();
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

bool MQTTSNGateway::loop(void)
{
    /* handle any messages from clients */
    handle_messages();
    
    /* check keepalive and any inflight messages */
    for (int i = 0; i < MQTTSN_MAX_NUM_CLIENTS; i++) {
        if (clients[i] && clients[i].check_status(device, transport) == MQTTSNInstanceStatus_LOST) {
            MQTTSN_INFO_PRINTLN("Client lost: %s", clients[i].client_id);
            clients[i].deregister();
        }
    }

    /* now distribute any pending publish msgs
       from the queue */    
    while (pub_fifo.available()) {
        MQTTSN_INFO_PRINTLN("Dispatching msgs.");
        
        pub_fifo.dequeue(out_msg);
        
        /* parse the header so we can get the msg type */
        MQTTSNHeader header;
        uint8_t offset = header.unpack(out_msg, MQTTSN_MAX_MSG_LEN);
        if (offset == 0 || header.msg_type != MQTTSN_PUBLISH)
            continue;
        
        out_msg_len = header.length;
        
        /* unpack the message, only QoS 0 for now */
        MQTTSNMessagePublish msg;
        if (!msg.unpack(&out_msg[offset], out_msg_len - offset) || msg.msg_id != 0x0000)
            continue;
        
        /* dispatch msg to clients */
        uint16_t tid = msg.topic_id;
        for (int i = 0; i < MQTTSN_MAX_NUM_CLIENTS; i++) {
            if (clients[i].is_subbed(tid)) {
                transport->write_packet(out_msg, out_msg_len, &clients[i].address);
            }
        }
    }

    /* just to return something useful */
    return connected;
}

void MQTTSNGateway::handle_messages(void)
{
    while (true) {        
        /* try to read something, return if theres nothing */
        MQTTSNAddress src;
        int16_t rlen = transport->read_packet(in_msg, MQTTSN_MAX_MSG_LEN, &src);
        if (rlen <= 0)
            return;
        
        /* get the msg type */
        MQTTSNHeader header;
        uint8_t offset = header.unpack(in_msg, rlen);
        if (offset == 0)
            continue;
        
        /* make sure there's a handler */
        uint8_t idx = header.msg_type;
        if (idx >= MQTTSN_NUM_MSG_TYPES || msg_handlers[idx] == NULL)
            continue;
        
        /* call the handler */
        (this->*msg_handlers[idx])(&in_msg[offset], rlen - offset, &src);
        
        device->cede();
    }
}

void MQTTSNGateway::add_subscription(uint16_t tid, uint8_t qos)
{
    MQTTSNTopicMapping * mapping = get_topic_mapping(tid);

    /* if there's no MQTT sub yet */
    if (!mapping->subbed) {
        mapping->subbed = true;
        mapping->sub_qos = qos;
        if (mqtt_client != NULL && connected) {
            mqtt_client->subscribe(mapping->name, mapping->sub_qos);
        }
    }
    /* or if the new sub has a higher qos */
    else if (mapping->sub_qos < qos) {
        mapping->sub_qos = qos;
        if (mqtt_client != NULL && connected) {
            mqtt_client->subscribe(mapping->name, mapping->sub_qos);
        }
    }
}

void MQTTSNGateway::delete_subscription(uint16_t tid)
{
    MQTTSNTopicMapping * mapping = get_topic_mapping(tid);
    mapping->subbed = false;
    mapping->sub_qos = 0;
    if (mqtt_client != NULL && connected) {
        mqtt_client->unsubscribe(mapping->name);
    }
}

uint16_t MQTTSNGateway::get_topic_id(const uint8_t * name, uint8_t name_len)
{
    if (name_len > MQTTSN_MAX_TOPICNAME_LEN)
        return 0;
        
    /* check if we already have that topic */
    for (uint16_t i = 0; i < MQTTSN_MAX_TOPIC_MAPPINGS; i++) {
        MQTTSNTopicMapping * mapping = &mappings[i];
        
        if (strlen(mapping->name) == name_len && memcmp(mapping->name, name, name_len) == 0) {
            return mapping->tid;
        }
    }
        
    /* else add it */
    for (uint16_t i = 0; i < MQTTSN_MAX_TOPIC_MAPPINGS; i++) {
        MQTTSNTopicMapping * mapping = &mappings[i];
        
        if (strlen(mapping->name) == 0) {
            memcpy(mapping->name, name, name_len);
            mapping->name[name_len] = 0;
            
            mapping->tid = i + 1;
            while (mapping->tid == MQTTSN_TOPIC_UNSUBSCRIBED || mapping->tid == MQTTSN_TOPIC_NOTASSIGNED)
                mapping->tid++;
            return mapping->tid;
        }
    }
    
    return 0;
}

MQTTSNTopicMapping * MQTTSNGateway::get_topic_mapping(uint16_t tid)
{
    /* check if we already have that topic */
    for (uint16_t i = 0; i < MQTTSN_MAX_TOPIC_MAPPINGS; i++) {
        if (mappings[i].tid == tid) {
            return &mappings[i];
        }
    }
    
    return NULL;
}

MQTTSNInstance * MQTTSNGateway::get_client(MQTTSNAddress * addr)
{
    for (int i = 0; i < MQTTSN_MAX_NUM_CLIENTS; i++) {
        if (clients[i] && clients[i].address.len == addr->len && memcmp(&clients[i].address.bytes, addr->bytes, addr->len) == 0)
            return &clients[i];
    }
    
    return NULL;
}

void MQTTSNGateway::handle_searchgw(uint8_t * data, uint8_t data_len, MQTTSNAddress * src)
{
    MQTTSNMessageSearchGW msg;
    if (!msg.unpack(data, data_len))
        return;
    
    MQTTSNMessageGWInfo reply;
    reply.gw_id = gw_id;
    out_msg_len = reply.pack(out_msg, MQTTSN_MAX_MSG_LEN);
    transport->broadcast(out_msg, out_msg_len);
}

void MQTTSNGateway::handle_connect(uint8_t * data, uint8_t data_len, MQTTSNAddress * src)
{
    MQTTSN_INFO_PRINTLN("Got CONNECT.");
    
    MQTTSNMessageConnect msg;
    if (!msg.unpack(data, data_len) || msg.client_id_len == 0)
        return;
    
    /* prepare connack */
    MQTTSNMessageConnack reply;
    reply.return_code = MQTTSN_RC_CONGESTION;

    /* check if a client already exists with this same address */
    MQTTSNInstance * clnt = get_client(src);
    if (clnt != NULL) {
        /* if we do have an existing session, overwrite it */
        clnt->register_(msg.client_id, msg.client_id_len, src, msg.duration, &msg.flags);
        clnt->mark_time(device->get_millis());
        reply.return_code = MQTTSN_RC_ACCEPTED;
        
        MQTTSN_INFO_PRINTLN("Existing client: %s", clnt->client_id);
    }
    else {
        /* else create a new instance */
        for (int i = 0; i < MQTTSN_MAX_NUM_CLIENTS; i++) {
            if (!clients[i]) {
                clients[i].register_(msg.client_id, msg.client_id_len, src, msg.duration, &msg.flags);
                clients[i].mark_time(device->get_millis());
                reply.return_code = MQTTSN_RC_ACCEPTED;
                
                MQTTSN_INFO_PRINTLN("New client: %s", clients[i].client_id);
                break;
            }
        }
    }
    
    out_msg_len = reply.pack(out_msg, MQTTSN_MAX_MSG_LEN);
    transport->write_packet(out_msg, out_msg_len, src);
    MQTTSN_INFO_PRINTLN("CONNACK sent.");
}

void MQTTSNGateway::handle_register(uint8_t * data, uint8_t data_len, MQTTSNAddress * src)
{
    MQTTSN_INFO_PRINTLN("Got REGISTER.");
    
    MQTTSNInstance * clnt = get_client(src);
    if (clnt == NULL)
        return;
        
    /* unpack the msg */
    MQTTSNMessageRegister msg;
    if (!msg.unpack(data, data_len) || msg.topic_id != 0x0000 || msg.topic_name_len == 0)
        return;
        
    clnt->mark_time(device->get_millis());

    /* construct REGACK response */
    MQTTSNMessageRegack reply;
    reply.msg_id = msg.msg_id;
    reply.return_code = MQTTSN_RC_ACCEPTED;
    
    /* get an ID and add the topic to the instance */
    uint16_t tid = get_topic_id(msg.topic_name, msg.topic_name_len);
    if (tid == 0)
        return;
        
    if (!clnt->add_pub_topic(tid)) {
        reply.return_code = MQTTSN_RC_CONGESTION;
    }
    else {
        reply.topic_id = tid;
        MQTTSN_INFO_PRINTLN("Topic ID: %X", tid);
    }

    /* now send our reply */
    out_msg_len = reply.pack(out_msg, MQTTSN_MAX_MSG_LEN);
    transport->write_packet(out_msg, out_msg_len, src);
}

void MQTTSNGateway::handle_publish(uint8_t * data, uint8_t data_len, MQTTSNAddress * src)
{
    MQTTSN_INFO_PRINTLN("Got PUBLISH.");
    
    /* check that we know this client */
    MQTTSNInstance * clnt = get_client(src);
    if (clnt == NULL)
        return;
        
    /* now unpack the message, only QoS 0 for now */
    MQTTSNMessagePublish msg;
    if (!msg.unpack(data, data_len) || msg.msg_id != 0x0000)
        return;

    /* get the topic name */
    MQTTSNTopicMapping * mapping = get_topic_mapping(msg.topic_id);
    if (mapping == NULL)
        return;

    /* if we're connected to the MQTT broker, just pass on the PUBLISH */
    if (mqtt_client != NULL && connected) {
        mqtt_client->publish(mapping->name, msg.data, msg.data_len, &msg.flags);
        MQTTSN_INFO_PRINTLN("Publishing to broker.");
    }
    else {
        /* else we're on our own, add the msg to our queue
           so we'll distribute it locally as broker */
        msg.pack(out_msg, MQTTSN_MAX_MSG_LEN);
        pub_fifo.enqueue(out_msg);
        
        MQTTSN_INFO_PRINTLN("Message queued.");
    }
}

void MQTTSNGateway::handle_subscribe(uint8_t * data, uint8_t data_len, MQTTSNAddress * src)
{
    MQTTSN_INFO_PRINTLN("Got SUBSCRIBE.");
    
    /* check that we know this client */
    MQTTSNInstance * clnt = get_client(src);
    if (clnt == NULL)
        return;

    /* unpack the msg */
    MQTTSNMessageSubscribe msg;
    if (!msg.unpack(data, data_len))
        return;

    clnt->mark_time(device->get_millis());

    /* construct suback response */
    MQTTSNMessageSuback reply;
    reply.msg_id = msg.msg_id;
    reply.return_code = MQTTSN_RC_ACCEPTED;

    /* get an ID */
    uint16_t tid = get_topic_id(msg.topic_name, msg.topic_name_len);
    if (tid == 0)
        return;
    
    reply.return_code = MQTTSN_RC_ACCEPTED;
    /* add the topic to the instance */
    if (!clnt->add_sub_topic(tid, &msg.flags)) {
        reply.return_code = MQTTSN_RC_CONGESTION;
        MQTTSN_INFO_PRINTLN("Topic congestion.");
    }
    else {
        MQTTSN_INFO_PRINTLN("ID assigned: %d", tid);
        reply.topic_id = tid;
    }

    /* now send our reply */
    out_msg_len = reply.pack(out_msg, MQTTSN_MAX_MSG_LEN);
    transport->write_packet(out_msg, out_msg_len, src);
    
    MQTTSN_INFO_PRINTLN("SUBACK sent.");
    
    /* send the new sub to MQTT broker */
    if (reply.return_code == MQTTSN_RC_ACCEPTED)
        add_subscription(tid, msg.flags.qos);
}

void MQTTSNGateway::handle_unsubscribe(uint8_t * data, uint8_t data_len, MQTTSNAddress * src)
{
    /* check that we know this client */
    MQTTSNInstance * clnt = get_client(src);
    if (clnt == NULL)
        return;

    /* unpack the msg */
    MQTTSNMessageUnsubscribe msg;
    if (!msg.unpack(data, data_len))
        return;

    clnt->mark_time(device->get_millis());

    /* construct unsuback response */
    MQTTSNMessageUnsuback reply;
    reply.msg_id = msg.msg_id;

    /* get the topic ID first */
    uint16_t tid = get_topic_id(msg.topic_name, msg.topic_name_len);
    if (tid == 0)
        return;

    /* delete the topic from the instance */
    clnt->delete_sub_topic(tid);

    /* now send our reply */
    out_msg_len = reply.pack(out_msg, MQTTSN_MAX_MSG_LEN);
    transport->write_packet(out_msg, out_msg_len, src);

    /* check if anybody's still subscribed */
    for (int i = 0; i < MQTTSN_MAX_NUM_CLIENTS; i++) {
        if (clients[i] && clients[i].is_subbed(tid))
            return;
    }

    /* if not, delete the sub from MQTT broker */
    delete_subscription(tid);
}

void MQTTSNGateway::handle_pingreq(uint8_t * data, uint8_t data_len, MQTTSNAddress * src)
{
    MQTTSN_INFO_PRINTLN("Got PINGREQ.");
    
    /* check that we know this client */
    MQTTSNInstance * clnt = get_client(src);
    if (clnt == NULL)
        return;
    
    /* make sure we have something to parse */
    if (data_len != 0) {
        MQTTSN_INFO_PRINTLN("Inside PINGREQ.");
        MQTTSNMessagePingreq msg;
        if (!msg.unpack(data, data_len))
            return;
    }
    
    clnt->mark_time(device->get_millis());

    /* now send our reply */
    MQTTSNMessagePingresp reply;
    out_msg_len = reply.pack(out_msg, MQTTSN_MAX_MSG_LEN);
    //while (1) {
    //device->delay_millis(1000);
    transport->write_packet(out_msg, out_msg_len, src);
    //}
}

void MQTTSNGateway::handle_mqtt_connect(void * which, bool conn_state)
{
    MQTTSNGateway * self = static_cast<MQTTSNGateway*>(which);
    
    /* now we know we're no longer connected to MQTT broker */
    if (!conn_state) {
        self->connected = false;
        return;
    }

    if (self->connected)
        return;
        
    /* now that we just reconnected to MQTT broker,
       re-subscribe to all sub topics of all our MQTT-SN clients */
    self->connected = true;
    for (uint16_t i = 0; i < MQTTSN_MAX_TOPIC_MAPPINGS; i++) {
        MQTTSNTopicMapping * mapping = &self->mappings[i];
        if (mapping->subbed)
            self->mqtt_client->subscribe(mapping->name, mapping->sub_qos);
    }
}

void MQTTSNGateway::handle_mqtt_publish(void * which, const char * topic, uint8_t * payload, uint8_t length, MQTTSNFlags * flags)
{
    MQTTSNGateway * self = static_cast<MQTTSNGateway*>(which);
    
    /* TODO: adapt for qos 1 later with msg id */
    
    /* craft a message */
    MQTTSNMessagePublish msg;
    
    msg.data = payload;
    msg.data_len = length;
    uint16_t topic_id = self->get_topic_id((uint8_t *)topic, strlen(topic));

    if (topic_id == 0)
        return;
    
    msg.topic_id = topic_id;
    msg.flags.all = flags->all;
    
    /* serialize and add to our pub queue */
    msg.pack(self->out_msg, MQTTSN_MAX_MSG_LEN);
    self->pub_fifo.enqueue(self->out_msg);
}

