/* Written by Brian Ejike (2019)
 * DIstributed under the MIT License */
 
#include "mqttsn_gateway.h"
#include "mqttsn_messages.h"
#include "mqttsn_defines.h"
#include "mqttsn_device.h"
#include "mqttsn_transport.h"
#include "mqttx_client.h"

#include <lite_fifo.h>
#include <string.h>
#include <stdlib.h>

/********************** MQTTSNInstance ************************/

MQTTSNInstance::MQTTSNInstance(void) :
    transport(NULL), msg_inflight_len(0), unicast_timer(0), unicast_counter(0),
    keepalive_interval(0), keepalive_timeout(0), sleep_interval(0), sleep_timeout(0),
    sleepy_fifo(sleepy_fifo_buf, MQTTSN_MAX_BUFFERED_MSGS, MQTTSN_MAX_MSG_LEN),
    last_in(0), status(MQTTSNInstanceStatus_DISCONNECTED)
{
    client_id[0] = 0;
    memset(&address, 0, sizeof(MQTTSNAddress));
}

bool MQTTSNInstance::register_(uint8_t * cid, uint8_t cid_len, MQTTSNTransport * transport, MQTTSNAddress * addr, uint16_t duration, MQTTSNFlags * flags)
{
    if (cid_len > MQTTSN_MAX_CLIENTID_LEN || transport == NULL || addr == NULL)
        return false;
    
    memcpy(client_id, cid, cid_len);
    client_id[cid_len] = 0;
    
    /* copy the address */
    this->transport = transport;
    memcpy(&address.bytes, addr->bytes, addr->len);
    address.len = addr->len;
    
    MQTTSN_INFO_PRINT("Address: ");
    for (int i = 0; i < address.len; i++) {
        MQTTSN_INFO_PRINT("%X ", address.bytes[i]);
    }
    MQTTSN_INFO_PRINT("\r\n");
        
    keepalive_interval = duration * 1000UL;
    keepalive_timeout = (keepalive_interval > 60000) ? keepalive_interval * 1.1 : keepalive_interval * 1.5;
    
    connect_flags.all = flags == NULL ? 0 : flags->all;
    
    /* mark them all free */
    for (uint16_t i = 0; i < MQTTSN_MAX_INSTANCE_TOPICS; i++) {
        sub_topics[i].tid = MQTTSN_TOPICID_NOTASSIGNED;
        pub_topics[i].tid = MQTTSN_TOPICID_NOTASSIGNED;
    }

    msg_inflight_len = 0;
    status = MQTTSNInstanceStatus_ACTIVE;
    return true;
}

void MQTTSNInstance::deregister(void)
{
    client_id[0] = 0;
    address.len = 0;
    transport = NULL;
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
        if (sub_topics[i].tid == MQTTSN_TOPICID_NOTASSIGNED) {
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
        if (pub_topics[i].tid == MQTTSN_TOPICID_NOTASSIGNED) {
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
            sub_topics[i].tid = MQTTSN_TOPICID_NOTASSIGNED;
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

MQTTSNInstanceStatus MQTTSNInstance::check_status(uint32_t now)
{ 
    /* check last time we got a control packet */
    if (now - last_in > keepalive_timeout) {
        status = MQTTSNInstanceStatus_LOST;
        return status;
    }
    
    /* if there are any outstanding msgs awaiting a response */
    if (msg_inflight_len != 0) {
        /* check if retry timer is up */
        if (now - unicast_timer < MQTTSN_T_RETRY)
            return status;
            
        unicast_counter++;

        /* check if retry counter is up */
        if (unicast_counter > MQTTSN_N_RETRY) {
            status = MQTTSNInstanceStatus_LOST;
            return status;
        }

        /* resend the msg if not */
        transport->write_packet(msg_inflight, msg_inflight_len, &address);
        unicast_timer = now;
    }
    
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

MQTTSNGateway::MQTTSNGateway(MQTTSNDevice * device, MQTTClient * client) :
    gw_id(0), device(device), mqtt_client(client), 
    connected(false), curr_msg_id(0), 
    advert_interval(MQTTSN_DEFAULT_ADVERTISE_INTERVAL * 1000UL), last_advert(0),
    pub_fifo(pub_fifo_buf, MQTTSN_MAX_QUEUED_PUBLISH, MQTTSN_MAX_MSG_LEN)
{
    topic_prefix[0] = 0;
    for (int i = 0; i < MQTTSN_MAX_NUM_TRANSPORTS; i++) {
        transports[i] = NULL;
    }
}

bool MQTTSNGateway::begin(uint8_t gw_id)
{
    this->gw_id = gw_id;
    if (mqtt_client) {
        mqtt_client->register_callbacks(this, MQTTSNGateway::handle_mqtt_connect, MQTTSNGateway::handle_mqtt_publish);
    }
    assign_msg_handlers();
}

bool MQTTSNGateway::register_transport(MQTTSNTransport * transport)
{
    for (int i = 0; i < MQTTSN_MAX_NUM_TRANSPORTS; i++) {
        if (transports[i] == NULL) {
            transports[i] = transport;
            return true;
        }
    }
    
    return false;
}

void MQTTSNGateway::set_advertise_interval(uint16_t seconds) 
{
    advert_interval = seconds * 1000UL;
}

void MQTTSNGateway::advertise(void) 
{
    MQTTSNTransport * transport;
    MQTTSNMessageAdvertise msg;
    out_msg_len = msg.pack(out_msg, MQTTSN_MAX_MSG_LEN);
    
    /* advertise on all transports */
    for (int i = 0; i < MQTTSN_MAX_NUM_TRANSPORTS; i++) {
        transport = transports[i];
        
        if (transport == NULL)
            continue;
            
        transport->broadcast(out_msg, out_msg_len);
    }
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
    msg_handlers[MQTTSN_DISCONNECT] = &MQTTSNGateway::handle_disconnect;
}

bool MQTTSNGateway::set_topic_prefix(const char * prefix) 
{
    if (strlen(prefix) > MQTTSN_MAX_TOPICPREFIX_LEN) {
        return false;
    }
    
    strcpy(topic_prefix, prefix);
    return true;
}

bool MQTTSNGateway::loop(void)
{
    /* handle any messages from clients */
    handle_messages();
    
    /* check status of each client */
    for (MQTTSNInstance &clnt : clients) {
        if (!clnt)
            continue;
        
        /* delete any LOST clients */
        if (clnt.check_status(device->get_millis()) == MQTTSNInstanceStatus_LOST) {
            MQTTSN_INFO_PRINTLN("Client %s is lost.", clnt.client_id);
            clnt.deregister();
        }
        
        /* if the client is now AWAKE, send any buffered msgs */
        if (clnt.status == MQTTSNInstanceStatus_AWAKE) {
            /* send PINGRESP if there are no msgs left */
            if (clnt.sleepy_fifo.available() == 0) {
                MQTTSNMessagePingresp reply;
                out_msg_len = reply.pack(out_msg, MQTTSN_MAX_MSG_LEN);
                clnt.transport->write_packet(out_msg, out_msg_len, &clnt.address);
                
                clnt.status = MQTTSNInstanceStatus_ASLEEP;
                clnt.mark_time(device->get_millis());
                continue;
            }
            
            clnt.sleepy_fifo.dequeue(out_msg);
            
            /* parse the header so we can get the length */
            MQTTSNHeader header;
            header.unpack(out_msg, MQTTSN_MAX_MSG_LEN);
            out_msg_len = header.length;
        
            clnt.transport->write_packet(out_msg, out_msg_len, &clnt.address);
        }
    }

    /* now distribute any pending publish msgs from the publish queue */    
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
        
        uint16_t tid = msg.topic_id;
        
        /* dispatch msg to clients */
        for (MQTTSNInstance &clnt : clients) {
            /* skip if this client isnt subscribed to this topic */
            if (!clnt.is_subbed(tid))
                continue;
                
            /* buffer the msg if the client is asleep, else send it now */
            if (clnt.status == MQTTSNInstanceStatus_ASLEEP) {
                clnt.sleepy_fifo.enqueue(out_msg);
            }
            else {
                clnt.transport->write_packet(out_msg, out_msg_len, &clnt.address);
            }
        }
    }
    
    /* advertise if its time */
    if (device->get_millis() - last_advert > advert_interval) {
        advertise();
        last_advert = device->get_millis();
    }
    
    /* just to return something useful */
    return connected;
}

void MQTTSNGateway::handle_messages(void)
{
    MQTTSNTransport * transport;
    
    for (int i = 0; i < MQTTSN_MAX_NUM_TRANSPORTS; i++) {
        transport = transports[i];
        
        if (transport == NULL)
            continue;
            
        while (true) {
            /* try to read something, return if theres nothing */
            MQTTSNAddress src;
            int16_t rlen = transport->read_packet(in_msg, MQTTSN_MAX_MSG_LEN, &src);
            if (rlen <= 0)
                break;
            
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
            (this->*msg_handlers[idx])(&in_msg[offset], rlen - offset, transport, &src);
            
            device->cede();
        }
    }
}

bool MQTTSNGateway::get_mqtt_topic_name(const char * name, char * mqtt_name, uint16_t mqtt_name_sz) {
    if (strlen(name) + 1 > mqtt_name_sz)
        return false;
    
    /* prepend the prefix if its not a special topic */
    if (name[0] != '$' && strlen(topic_prefix) != 0) {
        int rc = snprintf(mqtt_name, mqtt_name_sz, "%s/%s", topic_prefix, name);
        if (rc < 0 || rc >= mqtt_name_sz)
            return false;
    }
    else {
        strcpy(mqtt_name, name);
    }
    
    return true;
}

void MQTTSNGateway::add_subscription(uint16_t tid, uint8_t qos)
{
    MQTTSNTopicMapping * mapping = get_topic_mapping(tid);
    if (mapping == NULL)
        return;
        
    /* if there's no MQTT sub yet */
    if (!mapping->subbed) {
        mapping->subbed = true;
        mapping->sub_qos = qos;
        if (mqtt_client != NULL && connected) {
            if (!get_mqtt_topic_name(mapping->name, topic_name_full, MQTTSN_MAX_MQTT_TOPICNAME_LEN + 1))
                return;
            
            mqtt_client->subscribe(topic_name_full, mapping->sub_qos);
            MQTTSN_INFO_PRINTLN("MQTT SUBSCRIBE to %s.", topic_name_full);
        }
    }
    /* or if the new sub has a higher qos */
    else if (mapping->sub_qos < qos) {
        mapping->sub_qos = qos;
        if (mqtt_client != NULL && connected) {
            if (!get_mqtt_topic_name(mapping->name, topic_name_full, MQTTSN_MAX_MQTT_TOPICNAME_LEN + 1))
                return;
            
            mqtt_client->subscribe(topic_name_full, mapping->sub_qos);
            MQTTSN_INFO_PRINTLN("MQTT SUBSCRIBE to %s.", topic_name_full);
        }
    }
}

void MQTTSNGateway::delete_subscription(uint16_t tid)
{
    MQTTSNTopicMapping * mapping = get_topic_mapping(tid);
    if (mapping == NULL)
        return;
    
    mapping->subbed = false;
    mapping->sub_qos = 0;
    /* if we're connected, unsubscribe from this topic with the MQTT broker */
    if (mqtt_client != NULL && connected) {
        if (!get_mqtt_topic_name(mapping->name, topic_name_full, MQTTSN_MAX_MQTT_TOPICNAME_LEN + 1))
            return;
            
        mqtt_client->unsubscribe(topic_name_full);
        MQTTSN_INFO_PRINTLN("MQTT UNSUBSCRIBE to %s.", topic_name_full);
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
            while (mapping->tid == MQTTSN_TOPICID_UNSUBSCRIBED || mapping->tid == MQTTSN_TOPICID_NOTASSIGNED)
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

MQTTSNInstance * MQTTSNGateway::get_client(MQTTSNTransport * transport, MQTTSNAddress * addr)
{
    MQTTSNInstance * clnt;
    
    for (int i = 0; i < MQTTSN_MAX_NUM_CLIENTS; i++) {
        clnt = &clients[i];
        
        /* check that the transports and addresses match */
        if (*clnt && clnt->transport == transport && clnt->address.len == addr->len 
            && memcmp(&clnt->address.bytes, addr->bytes, addr->len) == 0)
        {
            return clnt;
        }
    }
    
    return NULL;
}

MQTTSNInstance * MQTTSNGateway::get_client(const char * cid, uint8_t cid_len)
{
    MQTTSNInstance * clnt;
    
    for (int i = 0; i < MQTTSN_MAX_NUM_CLIENTS; i++) {
        clnt = &clients[i];
        
        /* find a client with the specified name */
        if (*clnt && strlen(clnt->client_id) == cid_len && strncmp(clnt->client_id, cid, cid_len) == 0) {
            return clnt;
        }
    }
    
    return NULL;
}

void MQTTSNGateway::handle_searchgw(uint8_t * data, uint8_t data_len, MQTTSNTransport * transport, MQTTSNAddress * src)
{
    MQTTSN_INFO_PRINTLN("Got SEARCHGW.");
    MQTTSNMessageSearchGW msg;
    if (!msg.unpack(data, data_len))
        return;
    
    MQTTSNMessageGWInfo reply;
    reply.gw_id = gw_id;
    out_msg_len = reply.pack(out_msg, MQTTSN_MAX_MSG_LEN);
    transport->broadcast(out_msg, out_msg_len);
    MQTTSN_INFO_PRINTLN("GWINFO broadcast.\r\n");
}

void MQTTSNGateway::handle_connect(uint8_t * data, uint8_t data_len, MQTTSNTransport * transport, MQTTSNAddress * src)
{
    MQTTSN_INFO_PRINTLN("Got CONNECT.");
    
    MQTTSNMessageConnect msg;
    if (!msg.unpack(data, data_len) || msg.client_id_len == 0)
        return;
    
    /* prepare connack */
    MQTTSNMessageConnack reply;
    reply.return_code = MQTTSN_RC_CONGESTION;
    
    /* discard any existing client with the same name or address */
    for (MQTTSNInstance &clnt : clients) {
        if (!clnt)
            continue;
        
        bool same_name = strlen(clnt.client_id) == msg.client_id_len && memcmp(clnt.client_id, msg.client_id, msg.client_id_len) == 0;
        bool same_address = clnt.transport == transport && clnt.address.len == src->len && memcmp(clnt.address.bytes, src->bytes, src->len) == 0;
        
        if (same_name || same_address) {
            MQTTSN_INFO_PRINTLN("Discarding duplicate client: %s", clnt.client_id);
            clnt.deregister();
        }
    }
        
    /* now add the client to our list, if there's room */
    for (MQTTSNInstance &clnt : clients) {
        if (!clnt) {
            clnt.register_(msg.client_id, msg.client_id_len, transport, src, msg.duration, &msg.flags);
            clnt.mark_time(device->get_millis());
            reply.return_code = MQTTSN_RC_ACCEPTED;
            
            MQTTSN_INFO_PRINTLN("New client: %s", clnt.client_id);
            break;
        }
    }
    
    out_msg_len = reply.pack(out_msg, MQTTSN_MAX_MSG_LEN);
    transport->write_packet(out_msg, out_msg_len, src);
    MQTTSN_INFO_PRINTLN("CONNACK sent.\r\n");
}

void MQTTSNGateway::handle_register(uint8_t * data, uint8_t data_len, MQTTSNTransport * transport, MQTTSNAddress * src)
{
    MQTTSN_INFO_PRINTLN("Got REGISTER.");
    
    /* identify the client */
    MQTTSNInstance * clnt = get_client(transport, src);
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
    
    /* get an ID, create a new mapping if needed */
    uint16_t tid = get_topic_id(msg.topic_name, msg.topic_name_len);
    if (tid == 0)
        return;
        
    /* add the topic to the instance */
    if (!clnt->add_pub_topic(tid)) {
        reply.return_code = MQTTSN_RC_CONGESTION;
    }
    else {
        reply.topic_id = tid;
        MQTTSN_INFO_PRINTLN("Topic name: %.*s, ID: %X", msg.topic_name_len, msg.topic_name, tid);
    }

    /* now send our reply */
    out_msg_len = reply.pack(out_msg, MQTTSN_MAX_MSG_LEN);
    transport->write_packet(out_msg, out_msg_len, src);
    MQTTSN_INFO_PRINTLN("REGACK sent.\r\n");
}

void MQTTSNGateway::handle_publish(uint8_t * data, uint8_t data_len, MQTTSNTransport * transport, MQTTSNAddress * src)
{
    MQTTSN_INFO_PRINTLN("Got PUBLISH.");
    
    /* check that we know this client */
    MQTTSNInstance * clnt = get_client(transport, src);
    if (clnt == NULL)
        return;
        
    /* now unpack the message, only QoS 0 for now */
    MQTTSNMessagePublish msg;
    if (!msg.unpack(data, data_len) || msg.msg_id != 0x0000)
        return;

    MQTTSNTopicMapping * mapping = get_topic_mapping(msg.topic_id);
    if (mapping == NULL)
        return;

    /* if we're connected to the MQTT broker, just pass on the PUBLISH */
    if (mqtt_client != NULL && connected) {
        if (!get_mqtt_topic_name(mapping->name, topic_name_full, MQTTSN_MAX_MQTT_TOPICNAME_LEN + 1))
            return;
        
        mqtt_client->publish(topic_name_full, msg.data, msg.data_len, &msg.flags);
        MQTTSN_INFO_PRINTLN("MQTT PUBLISH to %s", topic_name_full);
    }
    else {
        /* if no client is subbed to this topic */
        if (!mapping->subbed)
            return;
            
        /* we're on our own, add the msg to our queue
           so we'll distribute it locally as broker */
        msg.pack(out_msg, MQTTSN_MAX_MSG_LEN);
        if (!pub_fifo.enqueue(out_msg)) {
            MQTTSN_ERROR_PRINTLN("Publish FIFO is full!");
        }
        else {
            MQTTSN_INFO_PRINTLN("Message queued.");
        }
    }
    
    MQTTSN_INFO_PRINT("\r\n");
}

void MQTTSNGateway::handle_subscribe(uint8_t * data, uint8_t data_len, MQTTSNTransport * transport, MQTTSNAddress * src)
{
    MQTTSN_INFO_PRINTLN("Got SUBSCRIBE.");
    
    /* check that we know this client */
    MQTTSNInstance * clnt = get_client(transport, src);
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

    /* get the ID, create a new mapping if needed */
    uint16_t tid = get_topic_id(msg.topic_name, msg.topic_name_len);
    if (tid == 0)
        return;
    
    reply.return_code = MQTTSN_RC_ACCEPTED;
    /* add the topic to the instance */
    if (!clnt->add_sub_topic(tid, &msg.flags)) {
        reply.return_code = MQTTSN_RC_CONGESTION;
        MQTTSN_ERROR_PRINTLN("Topic congestion!");
    }
    else {
        MQTTSN_INFO_PRINTLN("Topic name: %.*s, ID: %X", msg.topic_name_len, msg.topic_name, tid);
        reply.topic_id = tid;
    }

    /* now send our reply */
    out_msg_len = reply.pack(out_msg, MQTTSN_MAX_MSG_LEN);
    transport->write_packet(out_msg, out_msg_len, src);
    
    MQTTSN_INFO_PRINTLN("SUBACK sent.");
    
    /* send the new sub to MQTT broker */
    if (reply.return_code == MQTTSN_RC_ACCEPTED)
        add_subscription(tid, msg.flags.qos);
        
    MQTTSN_INFO_PRINTLN();
}

void MQTTSNGateway::handle_unsubscribe(uint8_t * data, uint8_t data_len, MQTTSNTransport * transport, MQTTSNAddress * src)
{
    MQTTSN_INFO_PRINTLN("Got UNSUBSCRIBE.");
    
    /* check that we know this client */
    MQTTSNInstance * clnt = get_client(transport, src);
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

void MQTTSNGateway::handle_pingreq(uint8_t * data, uint8_t data_len, MQTTSNTransport * transport, MQTTSNAddress * src)
{
    MQTTSN_INFO_PRINTLN("Got PINGREQ.");
    
    /* check that we know this client */
    MQTTSNInstance * clnt = get_client(transport, src);
    if (clnt == NULL)
        return;
    
    /* check we have something to parse */
    if (data_len != 0) {
        MQTTSNMessagePingreq msg;
        if (!msg.unpack(data, data_len))
            return;
        
        MQTTSNInstance * clnt = get_client((char *)msg.client_id, msg.client_id_len);
        if (clnt == NULL)
            return;
        
        /* if we got a PING from a sleeping client */
        if (clnt->status == MQTTSNInstanceStatus_ASLEEP) {
            clnt->status = MQTTSNInstanceStatus_AWAKE;
            clnt->mark_time(device->get_millis());
            return;
        }
    }
    
    clnt->mark_time(device->get_millis());

    /* now send our reply */
    MQTTSNMessagePingresp reply;
    out_msg_len = reply.pack(out_msg, MQTTSN_MAX_MSG_LEN);
    transport->write_packet(out_msg, out_msg_len, src);
}

void MQTTSNGateway::handle_disconnect(uint8_t * data, uint8_t data_len, MQTTSNTransport * transport, MQTTSNAddress * src)
{
    MQTTSN_INFO_PRINTLN("Got DISCONNECT.");
    
    /* check that we know this client */
    MQTTSNInstance * clnt = get_client(transport, src);
    if (clnt == NULL)
        return;
    
    /* make sure we have something to parse */
    if (data_len != 0) {
        MQTTSNMessageDisconnect msg;
        if (!msg.unpack(data, data_len))
            return;
        
        /* client wants to sleep, so note duration and clear msg buffer */
        clnt->keepalive_interval = msg.duration * 1000UL;
        clnt->keepalive_timeout = (clnt->keepalive_interval > 60000) ? clnt->keepalive_interval * 1.1 : clnt->keepalive_interval * 1.5;
        clnt->status = MQTTSNInstanceStatus_ASLEEP;
        clnt->sleepy_fifo.clear();
    }

    /* now send our reply */
    MQTTSNMessageDisconnect reply;
    out_msg_len = reply.pack(out_msg, MQTTSN_MAX_MSG_LEN);
    transport->write_packet(out_msg, out_msg_len, src);
    clnt->mark_time(device->get_millis());
}

void MQTTSNGateway::handle_mqtt_connect(void * which, bool conn_state)
{
    MQTTSN_INFO_PRINTLN("MQTT connect status: %s", conn_state ? "connected" : "disconnected");
    
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
        if (!mapping->subbed)
            continue;
          
        MQTTSNInstance * clnt;
        bool sub_exists = false;
        for (int i = 0; i < MQTTSN_MAX_NUM_CLIENTS; i++) {
            clnt = &self->clients[i];
            
            /* check that at least one client is subbed to this topic */
            if (*clnt && clnt->is_subbed(mapping->tid)) {
                sub_exists = true;
                break;
            }
        }
        
        if (!sub_exists) {
            mapping->subbed = false;
            continue;
        }
        
        if (!self->get_mqtt_topic_name(mapping->name, self->topic_name_full, MQTTSN_MAX_MQTT_TOPICNAME_LEN + 1))
            return;
            
        self->mqtt_client->subscribe(self->topic_name_full, mapping->sub_qos);
        MQTTSN_INFO_PRINTLN("MQTT SUBSCRIBE to %s.", self->topic_name_full);
    }
    
    MQTTSN_INFO_PRINT("\r\n");
}

void MQTTSNGateway::handle_mqtt_publish(void * which, const char * topic, uint8_t * payload, uint8_t length, MQTTSNFlags * flags)
{
    MQTTSNGateway * self = static_cast<MQTTSNGateway*>(which);
    
    /* TODO: adapt for qos 1 later with msg id */
    
    /* craft a message */
    MQTTSNMessagePublish msg;
    
    msg.data = payload;
    msg.data_len = length;
    
    MQTTSN_INFO_PRINTLN("Got MQTT PUBLISH to %s.", topic);
    
    /* if its not a special topic and a topic prefix is in use */
    uint16_t prefix_len = strlen(self->topic_prefix);
    if (topic[0] != '$' && prefix_len != 0) {
        /* check for the prefix and following slash */
        if (strncmp(self->topic_prefix, topic, prefix_len) != 0 || topic[prefix_len] != '/')
            return;
        
        topic += prefix_len + 1;
    }
    
    uint16_t topic_id = self->get_topic_id((uint8_t *)topic, strlen(topic));

    if (topic_id == 0)
        return;
    
    msg.topic_id = topic_id;
    msg.flags.all = flags->all;
    
    /* serialize and add to our pub queue */
    msg.pack(self->out_msg, MQTTSN_MAX_MSG_LEN);
    if (!self->pub_fifo.enqueue(self->out_msg)) {
        MQTTSN_ERROR_PRINTLN("Publish FIFO is full!");
    }
}

