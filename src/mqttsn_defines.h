/* Written by Brian Ejike (2019)
 * DIstributed under the MIT License */
 
#ifndef MQTTSN_DEFINES_H_
#define MQTTSN_DEFINES_H_

#define MQTTSN_MAX_ADDR_LEN         10

/* this is the present maximum message/packet size,
 * deemed enough for our pruposes, but can be greater if needed.
 * this also happens to be the minimum message/packet size to be supported by most hardware 
 * in order to accommodate complete clientIDs etc
 */
#define MQTTSN_MAX_MSG_LEN          32

#define MQTTSN_HEADER_LEN           2
#define MQTTSN_MAX_CLIENTID_LEN     23

/* Unassigned topic IDs set to 0 for convenience,
   Unsubscribed topics set to max value */
#define MQTTSN_TOPIC_NOTASSIGNED    0x0000
#define MQTTSN_TOPIC_UNSUBSCRIBED   0xFFFF

/* in milliseconds */
#define MQTTSN_DEFAULT_KEEPALIVE    30000UL

/* timeout for all unicasted messages to GW in ms */
#define MQTTSN_T_RETRY              5000UL
#define MQTTSN_N_RETRY              3

/* max delay before sending first SEARCHGW in milliseconds */
#define MQTTSN_T_SEARCHGW           5000UL
/* max delay between consecutive SEARCHGWs in milliseconds */ 
#define MQTTSN_MAX_T_SEARCHGW       (30UL * 60 * 1000)

/********** For gateways *************/

#define MQTTSN_MAX_INSTANCE_TOPICS  10
#define MQTTSN_MAX_GATEWAY_TOPICS   60

#define MQTTSN_MAX_NUM_CLIENTS      10

#define MQTTSN_MAX_QUEUED_PUBLISH   64

#endif
