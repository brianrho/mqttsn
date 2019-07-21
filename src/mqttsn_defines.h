/* Written by Brian Ejike (2019)
 * DIstributed under the MIT License */
 
#ifndef MQTTSN_DEFINES_H_
#define MQTTSN_DEFINES_H_

/* maximum length of any transport's address */
#define MQTTSN_MAX_ADDR_LEN             10

/* this is the maximum MQTTSN message size,
 * should be defined according to the packet payload size of your transport
 */
#define MQTTSN_MAX_MSG_LEN              32

/* maximum payload length in any single PUBLISH; 
 * 7 bytes for the remaining fields in a PUBLISH */
#define MQTTSN_MAX_PAYLOAD_LEN          (MQTTSN_MAX_MSG_LEN - 7)

/* length of fixed header */
#define MQTTSN_HEADER_LEN               2
#define MQTTSN_MAX_CLIENTID_LEN         23

/* Unassigned topic IDs set to 0 for convenience,
   Unsubscribed topics set to max value */
#define MQTTSN_TOPIC_NOTASSIGNED        0x0000
#define MQTTSN_TOPIC_UNSUBSCRIBED       0xFFFF

/* maximum length of a topic name;
 * 6 bytes for the remaining fields in a REGISTER */
#define MQTTSN_MAX_TOPICNAME_LEN        (MQTTSN_MAX_MSG_LEN - 6)

#define MQTTSN_DEFAULT_KEEPALIVE        30
#define MQTTSN_DEFAULT_KEEPALIVE_MS     (MQTTSN_DEFAULT_KEEPALIVE * 1000UL)

/* timeout for all unicasted messages to GW in ms */
#define MQTTSN_T_RETRY                  5000UL
#define MQTTSN_N_RETRY                  3

/* max delay before sending first SEARCHGW in milliseconds */
#define MQTTSN_T_SEARCHGW               5000UL
/* max delay between consecutive SEARCHGWs in milliseconds */ 
#define MQTTSN_MAX_T_SEARCHGW           (30UL * 60 * 1000)

/********** For gateways *************/

/* max number of publish OR subscribe topics for a client
   For instance, 10 here means a max of 10 pub topics and a max of 10 sub topics = 20 topics total */
#define MQTTSN_MAX_INSTANCE_TOPICS      10

/* max number of unique topics in publications or subscriptions */
#define MQTTSN_MAX_TOPIC_MAPPINGS       20

#define MQTTSN_MAX_NUM_CLIENTS          10

/* max number of queued publish messages yet to be delivered to MQTTSN clients */
#define MQTTSN_MAX_QUEUED_PUBLISH       64


#include "mqttsn_debug.h"

#endif
