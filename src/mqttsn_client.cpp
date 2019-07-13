#include "mqttsn_client.h"

#include <string.h>
#include <stdlib.h>
#include <stddef.h>

MQTTSNClient::MQTTSNClient(MQTTSNDevice * device, MQTTSNTransport * transport) :
    gateways(NULL), pub_topics(NULL), sub_topics(NULL),
    sub_topics_cnt(0), pub_topics_cnt(0), publish_cb(NULL),
    device(device), transport(transport), client_id(NULL), 
    state(MQTTSNState_DISCONNECTED), curr_gateway(NULL), 
    gateways_capacity(0), connected(false), msg_inflight_len(0),
    keepalive_interval(MQTTSN_DEFAULT_KEEPALIVE), keepalive_timeout(MQTTSN_DEFAULT_KEEPALIVE),
    last_in(0), last_out(0), ping_pending(false), 
    ping_timer(0), ping_counter(0), searchgw_started(0), 
    searchgw_interval(MQTTSN_T_SEARCHGW), searchgw_pending(false),
    curr_msg_id(0)
{
    
}

bool MQTTSNClient::begin(const char * client_id)
{
    if (!client_id || strlen(client_id) > MQTTSN_MAX_CLIENTID_LEN) {
        MQTTSN_ERROR_PRINTLN("Wrong Client ID length.");
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
    state_handlers[MQTTSNState_DISCONNECTED] = &MQTTSNClient::disconnected_handler;
}

void MQTTSNClient::add_gateways(MQTTSNGWInfo * gateways, uint8_t count)
{
    this->gateways = gateways;
    gateways_capacity = count;
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

void MQTTSNClient::discover(void)
{
    searchgw_started = device->get_millis();
    searchgw_interval = device->get_random(0, MQTTSN_T_SEARCHGW);
    searchgw_pending = true;
    state = MQTTSNState_SEARCHING;
    MQTTSN_INFO_PRINTLN("Starting SEARCHGW delay.");
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

bool connect(uint8_t gw_id, MQTTSNFlags * flags, uint16_t duration) 
{
    /* make sure there's no pending transaction */
    if (gateways == NULL || msg_inflight_len != 0)
        return;
        
    /* fill in fields */
    MQTTSNMessageConnect msg;
    msg.flags.all = (flags == NULL) ? 0 : flags->all;
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
    
    /* if none are marked available, lets try them again  */
    for (int i = 0; i < gateways_capacity; i++) {
        gateways[i].available = true;
    }
    
    /* select first available gw */
    for (int i = 0; i < gateways_capacity; i++) {
        if (gateways[i].gw_id)
            return &gateways[i];
    }
    
    /* no valid gateways */
    return NULL;
}
