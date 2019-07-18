/* Written by Brian Ejike (2019)
 * DIstributed under the MIT License */
 
#include "mqttsn_client.h"
#include "mqttsn_messages.h"
#include "mqttsn_device.h"
#include "mqttsn_transport.h"

#include <string.h>
#include <stdlib.h>
#include <stddef.h>

MQTTSNClient::MQTTSNClient(MQTTSNDevice * device, MQTTSNTransport * transport) :
    gateways(NULL), pub_topics(NULL), sub_topics(NULL),
    sub_topics_cnt(0), pub_topics_cnt(0), publish_cb(NULL),
    device(device), transport(transport), client_id(NULL), 
    state(MQTTSNState_DISCONNECTED), curr_gateway(NULL), 
    gateways_capacity(0), connected(false), msg_inflight_len(0),
    keepalive_interval(MQTTSN_DEFAULT_KEEPALIVE_MS), keepalive_timeout(MQTTSN_DEFAULT_KEEPALIVE_MS),
    last_in(0), last_out(0), pingresp_pending(false), 
    pingreq_timer(0), pingreq_counter(0), gwinfo_timer(0), 
    searchgw_interval(MQTTSN_T_SEARCHGW), gwinfo_pending(false),
    curr_msg_id(0), temp_msg_len(0)
{
    
}

bool MQTTSNClient::begin(const char * client_id)
{
    if (!client_id || strlen(client_id) > MQTTSN_MAX_CLIENTID_LEN) {
        MQTTSN_ERROR_PRINTLN("::Invalid Client ID");
        return false;
    }
    
    this->client_id = client_id;
    
    /* 10% tolerance for >1 minute, 50% tolerance otherwise */
    keepalive_timeout = (keepalive_interval > 60000) ? keepalive_interval * 1.1 : keepalive_interval * 1.5;
    
    /* handlers NULL by default */
    for (int i = 0; i < MQTTSN_NUM_MSG_TYPES; i++) {
        msg_handlers[i] = NULL;
    }
    
    for (int i = 0; i < MQTTSNState_NUM_STATES; i++) {
        state_handlers[i] = NULL;
    }
    
    assign_handlers();
}

void MQTTSNClient::assign_handlers(void)
{
    msg_handlers[MQTTSN_ADVERTISE] = &MQTTSNClient::handle_advertise;
    msg_handlers[MQTTSN_SEARCHGW] = &MQTTSNClient::handle_searchgw;
    msg_handlers[MQTTSN_GWINFO] = &MQTTSNClient::handle_gwinfo;
    msg_handlers[MQTTSN_CONNACK] = &MQTTSNClient::handle_connack;
    msg_handlers[MQTTSN_REGACK] = &MQTTSNClient::handle_regack;
    msg_handlers[MQTTSN_SUBACK] = &MQTTSNClient::handle_suback;
    msg_handlers[MQTTSN_UNSUBACK] = &MQTTSNClient::handle_unsuback;
    msg_handlers[MQTTSN_PUBLISH] = &MQTTSNClient::handle_publish;
    msg_handlers[MQTTSN_PINGRESP] = &MQTTSNClient::handle_pingresp;
    
    state_handlers[MQTTSNState_ACTIVE] = &MQTTSNClient::active_handler;
    state_handlers[MQTTSNState_SEARCHING] = &MQTTSNClient::searching_handler;
    state_handlers[MQTTSNState_CONNECTING] = &MQTTSNClient::connecting_handler;
    state_handlers[MQTTSNState_LOST] = &MQTTSNClient::lost_handler;
    //state_handlers[MQTTSNState_DISCONNECTED] = &MQTTSNClient::disconnected_handler;
}

void MQTTSNClient::add_gateways(MQTTSNGWInfo * gateways, uint8_t count)
{
    this->gateways = gateways;
    gateways_capacity = count;
    
    /* mark them all available */
    for (int i = 0; i < gateways_capacity; i++) {
        gateways[i].available = true;
    }
}

bool MQTTSNClient::loop(void)
{
    /* make sure to handle msgs first
       so that the updated states get selected */
    handle_messages();
    
    /* check up on any messages awaiting a reply */
    inflight_handler();
    
    /* run current state handler */
    if (state_handlers[state] != NULL) {
        (this->*state_handlers[state])();
    }
    
    return state == MQTTSNState_ACTIVE;
}

void MQTTSNClient::start_discovery(void)
{
    if (gwinfo_pending)
        return;
        
    gwinfo_timer = device->get_millis();
    gwinfo_pending = true;
    searchgw_interval = device->get_random(0, MQTTSN_T_SEARCHGW);
    state = MQTTSNState_SEARCHING;
    MQTTSN_INFO_PRINTLN("::Starting SEARCHGW delay");
}

uint8_t MQTTSNClient::gateway_count(void)
{
    /* count gateways with valid IDs */
    uint8_t count = 0;
    for (int i = 0; i < gateways_capacity; i++) {
        if (gateways[i].gw_id != 0) {
            count++;
        }
    }
    
    return count;
}

bool MQTTSNClient::connect(uint8_t gw_id, MQTTSNFlags * flags, uint16_t duration) 
{
    /* make sure there's no pending transaction */
    if (gateways == NULL || msg_inflight_len != 0)
        return false;
        
    /* fill in fields, save flags for later */
    MQTTSNMessageConnect msg;
    msg.flags.all = (flags == NULL) ? 0 : flags->all;
    connect_flags.all = msg.flags.all;
    
    msg.client_id = (uint8_t *)client_id;
    msg.client_id_len = strlen(client_id);
    msg.duration = duration;
    
    keepalive_interval = duration * 1000UL;
    keepalive_timeout = (keepalive_interval > 60000) ? keepalive_interval * 1.1 : keepalive_interval * 1.5;
    
    /* serialize and store for later */
    msg_inflight_len = msg.pack(msg_inflight, MQTTSN_MAX_MSG_LEN);
    if (msg_inflight_len == 0) {
        return false;
    }
    
    /* get the gateway */
    curr_gateway = select_gateway(gw_id);
    if (curr_gateway == NULL) {
        msg_inflight_len = 0;
        return false;
    }
    
    transport->write_packet(msg_inflight, msg_inflight_len, &curr_gateway->gw_addr);
    
    /* now we wait for a reply */
    connected = false;
    state = MQTTSNState_CONNECTING;
    
    /* start unicast timer */
    last_out = device->get_millis();
    unicast_timer = device->get_millis();
    unicast_counter = 0;
    
    MQTTSN_INFO_PRINT("::CONNECT sent to ID "); MQTTSN_INFO_HEXLN(curr_gateway->gw_id);
    return true;
}

MQTTSNGWInfo * MQTTSNClient::select_gateway(uint8_t gw_id)
{
    /* if a valid ID was passed, search our list */
    if (gw_id) {
        for (int i = 0; i < gateways_capacity; i++) {
            if (gateways[i].gw_id == gw_id)
                return &gateways[i];
        }
        
        return NULL;
    }
    
    /* next, try to select any available gw */
    for (int i = 0; i < gateways_capacity; i++) {
        if (gateways[i].gw_id && gateways[i].available)
            return &gateways[i];
    }
    
    /* They're all marked unavailable, lets try them again  */
    for (int i = 0; i < gateways_capacity; i++) {
        gateways[i].available = true;
    }
    
    /* select first valid gw */
    for (int i = 0; i < gateways_capacity; i++) {
        if (gateways[i].gw_id)
            return &gateways[i];
    }
    
    /* no valid gateways */
    return NULL;
}

bool MQTTSNClient::register_topics(MQTTSNPubTopic * topics, uint16_t len)
{
    pub_topics = topics;
    pub_topics_cnt = len;
    
    /* if we're not connected or theres a pending reply */
    if (!connected || msg_inflight_len) {
        return false;
    }
    
    /* register any unregistered topics */
    for (int i = 0; i < pub_topics_cnt; i++) {
        if (pub_topics[i].tid == MQTTSN_TOPIC_NOTASSIGNED) {
            register_(&pub_topics[i]);
            return false;
        }
    }
    
    return true;
}

void MQTTSNClient::register_(MQTTSNPubTopic * topic)
{
    MQTTSNMessageRegister msg;
    msg.topic_name = (uint8_t *)topic->name;
    msg.topic_id = 0;
    
    /* 0 is reserved for message IDs */
    curr_msg_id = (curr_msg_id == 0) ? 1 : curr_msg_id;
    msg.msg_id = curr_msg_id;

    /* serialize and store for later */
    msg_inflight_len = msg.pack(msg_inflight, MQTTSN_MAX_MSG_LEN);
    if (msg_inflight_len == 0) {
        return;
    }
    
    transport->write_packet(msg_inflight, msg_inflight_len, &curr_gateway->gw_addr);

    /* start unicast timer */
    last_out = device->get_millis();
    unicast_timer = device->get_millis();
    unicast_counter = 0;

    /* advance for next transaction */
    curr_msg_id = curr_msg_id + 1;
}

bool MQTTSNClient::publish(const char * topic, uint8_t * data, uint8_t len, MQTTSNFlags * flags)
{
    /* if we're not connected */
    if (!connected)
        return false;

    /* get the topic id */
    MQTTSNMessagePublish msg;
    msg.topic_id = 0;
    
    for (int i = 0; i < pub_topics_cnt; i++) {
        if (strcmp(pub_topics[i].name, topic) == 0) {
            msg.topic_id = pub_topics[i].tid;
            break;
        }
    }
    
    /* if we didnt find it */
    if (msg.topic_id == 0)
        return false;

    msg.flags.all = (flags == NULL) ? 0 : flags->all;

    /* msgid = 0 for qos 0 */
    if (msg.flags.qos == 1 || msg.flags.qos == 2) {
        curr_msg_id = curr_msg_id == 0 ? 1 : curr_msg_id;
        msg.msg_id = curr_msg_id;

        /* increment */
        curr_msg_id = curr_msg_id + 1;
    }

    msg.data = data;
    uint8_t temp_msg_len = msg.pack(temp_msg, MQTTSN_MAX_MSG_LEN);
    transport->write_packet(temp_msg, temp_msg_len, &curr_gateway->gw_addr);

    /* TODO: need to note last_out for QoS 1 publish */

    return true;
}

bool MQTTSNClient::subscribe_topics(MQTTSNSubTopic * topics, uint16_t len)
{
    sub_topics = topics;
    sub_topics_cnt = len;
    
    /* if we're not connected or theres a pending reply */
    if (!connected || msg_inflight_len) {
        return false;
    }
    
    /* register any unregistered topics */
    for (int i = 0; i < sub_topics_cnt; i++) {
        if (sub_topics[i].tid == MQTTSN_TOPIC_NOTASSIGNED) {
            subscribe(&sub_topics[i]);
            return false;
        }
    }
    
    return true;
}

void MQTTSNClient::subscribe(MQTTSNSubTopic * topic)
{
    MQTTSNMessageSubscribe msg;
    msg.topic_name = (uint8_t *)topic->name;
    
    /* 0 is reserved for message IDs */
    curr_msg_id = (curr_msg_id == 0) ? 1 : curr_msg_id;
    msg.msg_id = curr_msg_id;
    msg.flags.all = topic->flags.all;
    
    /* serialize and store for later */
    msg_inflight_len = msg.pack(msg_inflight, MQTTSN_MAX_MSG_LEN);
    if (msg_inflight_len == 0) {
        return;
    }
    
    transport->write_packet(msg_inflight, msg_inflight_len, &curr_gateway->gw_addr);

    /* start unicast timer */
    last_out = device->get_millis();
    unicast_timer = device->get_millis();
    unicast_counter = 0;

    /* advance for next transaction */
    curr_msg_id = curr_msg_id + 1;
}

bool MQTTSNClient::unsubscribe(const char * topic, MQTTSNFlags * flags)
{
    /* if we're not connected or theres a pending reply */
    if (!connected || msg_inflight_len) {
        return false;
    }

    MQTTSNMessageUnsubscribe msg;

    /* check our list of subs for this topic */
    for (int i = 0; i < sub_topics_cnt; i++) {
        if (strcmp(sub_topics[i].name, topic) == 0) {
            msg.topic_name = (uint8_t *)topic;
            break;
        }
    }

    /* 0 is reserved */
    curr_msg_id = curr_msg_id == 0 ? 1 : curr_msg_id;
    msg.msg_id = curr_msg_id;
    msg.flags.all = flags->all;

    /* serialize and store for later */
    msg_inflight_len = msg.pack(msg_inflight, MQTTSN_MAX_MSG_LEN);
    if (msg_inflight_len == 0) {
        return false;
    }
    
    transport->write_packet(msg_inflight, msg_inflight_len, &curr_gateway->gw_addr);

    /* start unicast timer */
    last_out = device->get_millis();
    unicast_timer = device->get_millis();
    unicast_counter = 0;

    /* advance for next transaction */
    curr_msg_id = curr_msg_id + 1;
    return true;
}

bool MQTTSNClient::ping(void)
{
    /* if we're not connected */
    if (!connected) {
        return false;
    }
    
    MQTTSNMessagePingreq msg;
    uint8_t temp_msg_len = msg.pack(temp_msg, MQTTSN_MAX_MSG_LEN);
    transport->write_packet(temp_msg, temp_msg_len, &curr_gateway->gw_addr);

    last_out = device->get_millis();
    pingreq_timer = device->get_millis();
    return true;
}

bool MQTTSNClient::transaction_pending(void)
{
    /* if there's nothing waiting */
    if (msg_inflight_len == 0) {
        return false;
    }
    
    /* maybe we've gotten a response, so consume it */
    loop();
    return msg_inflight_len != 0;
}

bool MQTTSNClient::is_connected(void)
{
    return connected;
}

bool MQTTSNClient::disconnect(void)
{
    if (!connected) {
        return false;
    }
    
    MQTTSNMessageDisconnect msg;
    uint8_t temp_msg_len = msg.pack(temp_msg, MQTTSN_MAX_MSG_LEN);
    transport->write_packet(temp_msg, temp_msg_len, &curr_gateway->gw_addr);

    connected = false;
    state = MQTTSNState_DISCONNECTED;
    return true;
}

void MQTTSNClient::on_message(MQTTSNPublishCallback callback)
{
    publish_cb = callback;
}

MQTTSNState MQTTSNClient::status(void) 
{
    return state;
}

void MQTTSNClient::handle_messages(void)
{
    while (true) {
        /* try to read a packet */
        MQTTSNAddress src;
        int16_t rlen = transport->read_packet(temp_msg, MQTTSN_MAX_MSG_LEN, &src);
        if (rlen <= 0)
            return;
            
        /* get the msg type */
        MQTTSNHeader header;
        uint8_t offset = header.unpack(temp_msg, rlen);
        if (offset == 0)
            continue;
        
        /* make sure there's a handler */
        uint8_t idx = header.msg_type;
        if (idx >= MQTTSN_NUM_MSG_TYPES || msg_handlers[idx] == NULL)
            continue;
        
        /* call the handler */
        (this->*msg_handlers[idx])(&temp_msg[offset], rlen, &src);
        
        device->cede();
    }
}

void MQTTSNClient::inflight_handler(void)
{
    /* if there's nothing waiting */
    if (msg_inflight_len == 0) {
        return;
    }
    
    /* do we still have time? */
    if (device->get_millis() - unicast_timer < MQTTSN_T_RETRY) {
        return;
    }
    
    unicast_timer = device->get_millis();
    unicast_counter += 1;
    
    /* too many retries? */
    if (unicast_counter >= MQTTSN_N_RETRY) {
        connected = false;
        msg_inflight_len = 0;
        state = MQTTSNState_LOST;
        
        /* Mark the gateway as unavailable */
        curr_gateway->available = false;
        return;
    }
    
    /* resend the msg */
    transport->write_packet(msg_inflight, msg_inflight_len, &curr_gateway->gw_addr);
}

void MQTTSNClient::handle_advertise(uint8_t * data, uint8_t data_len, MQTTSNAddress * src)
{
    
    MQTTSNMessageAdvertise msg;
    if (!msg.unpack(data, data_len))
        return;
    
    /* check if we already have it */
    MQTTSNGWInfo * ret = select_gateway(msg.gw_id);
    if (ret != NULL) {
        return;
    }
    
    /* find a free slot, add the new GW info */
    for (int i = 0; i < gateways_capacity; i++) {
        if (gateways[i].gw_id == 0) {
            gateways[i].gw_id = msg.gw_id;
            memcpy(&gateways[i].gw_addr, src, sizeof(MQTTSNAddress));
            return;
        }
    }
}

void MQTTSNClient::handle_searchgw(uint8_t * data, uint8_t data_len, MQTTSNAddress * src)
{
    MQTTSNMessageSearchGW msg;
    if (!msg.unpack(data, data_len))
        return;

    /* If someone already sent one, we just reset our timer */
    if (gwinfo_pending) {
        gwinfo_timer = device->get_millis();
    }
    
    /* TODO: Send GWINFO from clients */
}

void MQTTSNClient::handle_gwinfo(uint8_t * data, uint8_t data_len, MQTTSNAddress * src)
{
    MQTTSNMessageGWInfo msg;
    if (!msg.unpack(data, data_len))
        return;
    
    /* check if we already have it */
    MQTTSNGWInfo * ret = select_gateway(msg.gw_id);
    if (ret != NULL) {
        gwinfo_pending = false;
        return;
    }
    
    /* else add it to our list */
    for (int i = 0; i < gateways_capacity; i++) {
        if (gateways[i].gw_id == 0) {
            gateways[i].gw_id = msg.gw_id;
            
            /* check if a gw or client sent the GWINFO */
            if (msg.gw_addr != NULL && msg.gw_addr_len <= MQTTSN_MAX_ADDR_LEN) {
                memcpy(gateways[i].gw_addr.bytes, msg.gw_addr, msg.gw_addr_len);
            }
            else {
                memcpy(gateways[i].gw_addr.bytes, src->bytes, src->len);
            }
            break;
        }
    }
    
    /* cancel any pending wait */
    gwinfo_pending = false;
}

void MQTTSNClient::handle_connack(uint8_t * data, uint8_t data_len, MQTTSNAddress * src)
{
    /* make sure its from the solicited gateway */
    if (curr_gateway == NULL || memcmp(src->bytes, curr_gateway->gw_addr.bytes, curr_gateway->gw_addr.len) != 0) {
        return;
    }
    
    if (msg_inflight_len == 0)
        return;
        
    /* parse our stored msg */
    MQTTSNHeader header;
    uint8_t offset = header.unpack(msg_inflight, msg_inflight_len);
    if (offset == 0) {
        msg_inflight_len = 0;
        return;
    }
    
    /* we have no pending CONNECT */
    if (header.msg_type != MQTTSN_CONNECT)
        return;
    
    /* unpack the original CONNECT */
    MQTTSNMessageConnect sent;
    if (!sent.unpack(&msg_inflight[offset], msg_inflight_len - offset)) {
        msg_inflight_len = 0;
        return;
    }
        
    /* now unpack the reply */
    MQTTSNMessageConnack msg;
    if (!msg.unpack(data, data_len))
        return;
    
    if (msg.return_code != MQTTSN_RC_ACCEPTED) {
        msg_inflight_len = 0;
        state = MQTTSNState_DISCONNECTED;
        return;
    }

    /* we are now connected */
    connected = true;
    msg_inflight_len = 0;
    pingresp_pending = false;
    last_in = device->get_millis();
    
    /* re-register and re-sub topics */
    for (int i = 0; i < pub_topics_cnt; i++) {
        pub_topics[i].tid = 0;
    }
    for (int i = 0; i < sub_topics_cnt; i++) {
        sub_topics[i].tid = 0;
    }
}

void MQTTSNClient::handle_regack(uint8_t * data, uint8_t data_len, MQTTSNAddress * src)
{
    /* if this is to be used as proof of connectivity,
       then we must verify that the gateway is the right one */
    if (curr_gateway == NULL || memcmp(src->bytes, curr_gateway->gw_addr.bytes, curr_gateway->gw_addr.len) != 0) {
        return;
    }
    
    /* no pending transactions */
    if (msg_inflight_len == 0)
        return;
        
    /* parse our stored msg */
    MQTTSNHeader header;
    uint8_t offset = header.unpack(msg_inflight, msg_inflight_len);
    if (offset == 0) {
        msg_inflight_len = 0;
        return;
    }
    
    /* we have no pending REGISTER */
    if (header.msg_type != MQTTSN_REGISTER)
        return;
    
    /* unpack the original REGISTER */
    MQTTSNMessageRegister sent;
    if (!sent.unpack(&msg_inflight[offset], msg_inflight_len - offset)) {
        msg_inflight_len = 0;
        return;
    }
    
    /* now unpack the reply */
    MQTTSNMessageRegack msg;
    if (!msg.unpack(data, data_len))
        return;
    
    if (msg.msg_id != sent.msg_id || msg.return_code != MQTTSN_RC_ACCEPTED) {
        return;
    }
    
    /* check our list and put in the ID */
    bool topic_found = false;
    for (int i = 0; i < pub_topics_cnt; i++) {
        if (strcmp(pub_topics[i].name, (char *)sent.topic_name) == 0) {
            pub_topics[i].tid = msg.topic_id;
            topic_found = true;
            break;
        }
    }
    
    if (!topic_found)
        return;

    msg_inflight_len = 0;
    last_in = device->get_millis();
}

void MQTTSNClient::handle_publish(uint8_t * data, uint8_t data_len, MQTTSNAddress * src)
{
    /* wont check the gw address,
       have faith that only our connected gw will send us msgs */
    if (curr_gateway == NULL || !connected) {
        return;
    }

    /* now unpack the message, only QoS 0 for now */
    MQTTSNMessagePublish msg;
    if (!msg.unpack(data, data_len) || msg.msg_id != 0x0000)
        return;

    /* get the topic name */
    const char * topic_name = NULL;
    for (int i = 0; i < pub_topics_cnt; i++) {
        if (pub_topics[i].tid == msg.topic_id) {
            topic_name = pub_topics[i].name;
            break;
        }
    }
    
    if (topic_name == NULL)
        return;

    /* call user handler */
    if (publish_cb != NULL)
        publish_cb(topic_name, msg.data, msg.data_len, &msg.flags);
}

void MQTTSNClient::handle_suback(uint8_t * data, uint8_t data_len, MQTTSNAddress * src)
{
    /* if this is to be used as proof of connectivity,
       then we must verify that the gateway is the right one */
    if (curr_gateway == NULL || memcmp(src->bytes, curr_gateway->gw_addr.bytes, curr_gateway->gw_addr.len) != 0) {
        return;
    }

    /* no pending transactions */
    if (msg_inflight_len == 0)
        return;
        
    /* parse our stored msg */
    MQTTSNHeader header;
    uint8_t offset = header.unpack(msg_inflight, msg_inflight_len);
    if (offset == 0) {
        msg_inflight_len = 0;
        return;
    }
    
    /* we have no pending SUBSCRIBE */
    if (header.msg_type != MQTTSN_SUBSCRIBE)
        return;
        
    /* unpack the original SUBSCRIBE */
    MQTTSNMessageSubscribe sent;
    if (!sent.unpack(&msg_inflight[offset], msg_inflight_len - offset)) {
        msg_inflight_len = 0;
        return;
    }
    
    /* now unpack the reply */
    MQTTSNMessageSuback msg;
    if (!msg.unpack(data, data_len))
        return;
        
    if (msg.msg_id != sent.msg_id || msg.return_code != MQTTSN_RC_ACCEPTED) {
        return;
    }
    
    /* check our list and put in the ID */
    bool topic_found = false;
    for (int i = 0; i < sub_topics_cnt; i++) {
        if (strcmp(sub_topics[i].name, (char *)sent.topic_name) == 0) {
            sub_topics[i].tid = msg.topic_id;
            topic_found = true;
            break;
        }
    }
    
    if (!topic_found)
        return;

    msg_inflight_len = 0;
    last_in = device->get_millis();
}

void MQTTSNClient::handle_unsuback(uint8_t * data, uint8_t data_len, MQTTSNAddress * src)
{
    /* if this is to be used as proof of connectivity,
       then we must verify that the gateway is the right one */
    if (curr_gateway == NULL || memcmp(src->bytes, curr_gateway->gw_addr.bytes, curr_gateway->gw_addr.len) != 0) {
        return;
    }

    /* no pending transactions */
    if (msg_inflight_len == 0)
        return;

    /* parse our stored msg */
    MQTTSNHeader header;
    uint8_t offset = header.unpack(msg_inflight, msg_inflight_len);
    if (offset == 0) {
        msg_inflight_len = 0;
        return;
    }
    
    /* we have no pending UNSUBSCRIBE */
    if (header.msg_type != MQTTSN_UNSUBSCRIBE)
        return;

    /* unpack the original UNSUBSCRIBE */
    MQTTSNMessageUnsubscribe sent;
    if (!sent.unpack(&msg_inflight[offset], msg_inflight_len - offset)) {
        msg_inflight_len = 0;
        return;
    }
    
    /* now unpack the reply */
    MQTTSNMessageUnsuback msg;
    if (!msg.unpack(data, data_len) || msg.msg_id != sent.msg_id)
        return;
        
    /* check our list and delete the ID */
    bool topic_found = false;
    for (int i = 0; i < sub_topics_cnt; i++) {
        if (strcmp(sub_topics[i].name, (char *)sent.topic_name) == 0) {
            sub_topics[i].tid = MQTTSN_TOPIC_UNSUBSCRIBED;
            topic_found = true;
            break;
        }
    }

    if (!topic_found)
        return;

    msg_inflight_len = 0;
    last_in = device->get_millis();
}

void MQTTSNClient::handle_pingresp(uint8_t * data, uint8_t data_len, MQTTSNAddress * src)
{
    /* if this is to be used as proof of connectivity,
       then we must verify that the gateway is the right one */
    if (curr_gateway == NULL || memcmp(src->bytes, curr_gateway->gw_addr.bytes, curr_gateway->gw_addr.len) != 0) {
        return;
    }
    
    /* if theres no pending PINGRESP or the payload isnt empty */
    if (!pingresp_pending || data_len != 0)
        return;

    pingresp_pending = false;
    last_in = device->get_millis();
}

void MQTTSNClient::searching_handler(void)
{
    /* if we're still waiting for a GWINFO and the wait interval is over */
    if (gwinfo_pending && (uint32_t)(device->get_millis() - gwinfo_timer) >= searchgw_interval) {
        /* broadcast it and start waiting again */
        MQTTSNMessageSearchGW msg;
        temp_msg_len = msg.pack(temp_msg, MQTTSN_MAX_MSG_LEN);
        transport->broadcast(temp_msg, temp_msg_len);
        gwinfo_timer = device->get_millis();
        
        /* increase exponentially */
        searchgw_interval = (searchgw_interval < MQTTSN_MAX_T_SEARCHGW) ? searchgw_interval * 2 : MQTTSN_MAX_T_SEARCHGW;
    }
}

void MQTTSNClient::connecting_handler(void)
{
    if (connected) {
        state = MQTTSNState_ACTIVE;
    }
}

void MQTTSNClient::lost_handler(void)
{
    MQTTSN_ERROR_PRINTLN("::Gateway lost.");
    /* try to re-connect any available gateway */
    connect(0, &connect_flags, keepalive_interval / 1000);
}


void MQTTSNClient::active_handler(void)
{
    uint32_t curr_time = device->get_millis();
    
    /* check if its been too long since we finished a transaction */
    if ((uint32_t)(curr_time - last_out) < keepalive_interval && (uint32_t)(curr_time - last_in) < keepalive_interval)
        return;
    
    /* if we havent sent a ping */
    if (!pingresp_pending) {
        ping();
        pingresp_pending = true;
        return;
    }
    
    /* if there's still time for a reply */
    if ((uint32_t)(curr_time - pingreq_timer) < MQTTSN_T_RETRY)
        return;
        
    /* if we've hit the keepalive limit, we're lost */
    if ((uint32_t)(curr_time - last_in) >= keepalive_timeout) {
        state = MQTTSNState_LOST;
        curr_gateway->available = false;
        curr_gateway = NULL;
        
        connected = false;
        pingresp_pending = false;
    }
    else {
        /* if we still have time, keep pinging */
        ping();
        pingresp_pending = true;
    }
}

