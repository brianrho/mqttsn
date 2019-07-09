#ifndef MQTTSN_DEFINES_H_
#define MQTTSN_DEFINES_H_

#define GW_ADDR_LENGTH              2

/* this is the present maximum message/packet size,
 * deemed enough for our pruposes, but can be greater if needed.
 * this also happens to be the minimum message/packet size to be supported by most hardware 
 * in order to accommodate complete clientIDs etc
 */
#define MQTTSN_MAX_MSG_LEN          32

#define MQTTSN_HEADER_LEN           2
#define MQTTSN_MAX_CLIENTID_LEN     23

/* in seconds */
#define MQTTSN_DEFAULT_KEEPALIVE    30

/* timeout for all unicasted messages to GW in ms */
#define MQTTSN_T_RETRY              5000
#define MQTTSN_N_RETRY              3

/* max delay between SEARCHGWs in milliseconds */
#define MQTTSN_T_SEARCHGW           5000

/********** For gateways *************/

#define MQTTSN_MAX_INSTANCE_TOPICS  10
#define MQTTSN_MAX_GATEWAY_TOPICS   60

#define MQTTSN_MAX_NUM_CLIENTS      10

#define MQTTSN_MAX_QUEUED_PUBLISH   64

#endif
