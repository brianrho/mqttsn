/* Arduino client with RFM69HW transport
 *  This example tests the basic support for a sleeping client.
 *  
 *  The client connects, subscribes to a topic as usual and then 
 *  informs the gateway it will be going to sleep for a specified interval.
 *  After confirmation from the gateway, it goes to bed 
 *  and only wakes up after each sleep interval to check for any messages,
 *  before returning to sleep.
 */

#include "mqttsn_client_includes.h"
#include "printf_config.h"
#include <RFM69X.h>

/* sleep duration in seconds */
#define SLEEP_DURATION  10

/* create RFM69HW component */
boolean IS_RFM69HW = true;
#define RFM_SS          14
#define RFM_INT         12
uint8_t RFM_INTNUM = digitalPinToInterrupt(RFM_INT);
RFM69X radio(RFM_SS, RFM_INT, IS_RFM69HW, RFM_INTNUM);

/* create MQTTSN components */
MQTTSNDeviceArduino device;
MQTTSNTransportRFM69X transport(&radio);
MQTTSNClient client(&device, &transport);

/* Provide a buffer to hold info of discovered gateways (default max = 3),
 * Must have static storage duration
 */
const int GATEWAYS_SZ = 3;
MQTTSNGWInfo gateways[GATEWAYS_SZ];

/* list pub and sub topics */
MQTTSNSubTopic sub_topics[] = { {"led"} };

void setup() {
    Serial.begin(9600);

    /* set up local transport */
    client_local_setup();
    
    Serial.println("Starting client.");
    
    if (!client.begin("Pubclient")) {
        Serial.println("Init client failed.");
        while (1) yield();
    }

    init_gateway_list();
    
    /* register the gateway info buffer */
    client.add_gateways(gateways, GATEWAYS_SZ);

    /* set up publish callback */
    client.on_message(publish_callback);
    
    /* async-connect to first available gateway */
    if (!client.connect()) {
        Serial.println("Connect failed.");
        while (1) yield();
    }
}

void client_local_setup(void) {
    const uint8_t NODEID = 9;
    const uint8_t NETWORKID = 100;
    const uint8_t FREQUENCY = RF69_868MHZ;
    const char ENCRYPTKEY[] = "mqttsntestdevice";

    if (!radio.initialize(FREQUENCY, NODEID, NETWORKID)) {
        Serial.println ("RFM69 init failed. Halting.");
        while (1);
    }

    if (IS_RFM69HW) {
        radio.setHighPower();      // Only for RFM69HW!
    }
    
    radio.encrypt(ENCRYPTKEY);
    radio.promiscuous(false);
    Serial.println("Using RFM69 Extended mode");
}

void init_gateway_list(void) {
    /* list pre-defined gateway address(es) */
    MQTTSNAddress gw_addr;
    gw_addr.bytes[0] = 10;
    gw_addr.len = 1;

    /* add them to the list, along with their GWID.
     * Here, only the first slot (of 3) is used.
     */
    gateways[0].gw_id = 2;
    gateways[0].gw_addr = gw_addr;
}

void loop(void) {
    /* handle tasks */
    client.loop();
    
    /* only proceed if we're connected */
    if (!client.is_connected())
        return;
        
    /*  check if all topics have been registered and subbed.
     *  No need to wait for ALL of them really, just the ones you need immediately
     */
    if (!check_topics())
        return;

    /* if we're done registering topics, and we're still in the ACTIVE state,
     *  inform the GW we want to sleep, non-blocking as usual
     */
    MQTTSNState ret = client.status();
    if (ret == MQTTSNState_ACTIVE) {
        client.sleep(SLEEP_DURATION);     /* multiple calls are already handled internally */
    }
    /* once GW sends confirmation and the client enters the ASLEEP state, 
     * then actually send the device to sleep */
    else if (ret == MQTTSNState_ASLEEP) {
        slumber();
    }
}

void slumber(void) {
    /* we will wake up after SLEEP_DURATION secs */
    esp_sleep_enable_timer_wakeup(SLEEP_DURATION * 1000000UL);

    Serial.println("Entering light sleep.");

    /* Get timestamp before entering sleep */
    int64_t t_before_us = micros();

    /* Enter sleep mode */
    Serial.flush();
    esp_light_sleep_start();

    /* Get timestamp after waking up from sleep */
    int64_t t_after_us = micros();
        
    /* Determine wake up reason */
    const char* wakeup_reason;
    switch (esp_sleep_get_wakeup_cause()) {
        case ESP_SLEEP_WAKEUP_TIMER:
            wakeup_reason = "timer";
            break;
        case ESP_SLEEP_WAKEUP_GPIO:
            wakeup_reason = "pin";
            break;
        default:
            wakeup_reason = "other";
            break;
    }

    printf("\r\nReturned from light sleep, reason: %s, t=%lld ms, slept for %lld ms\n",
            wakeup_reason, t_after_us / 1000, (t_after_us - t_before_us) / 1000);
}

void publish_callback(const char * topic, uint8_t * data, uint8_t len, MQTTSNFlags * flags)
{
    if (strcmp(topic, "led") == 0 && len != 0) {
        printf("\r\nTopic: %s\r\n", topic);
        printf("Payload: ");

        for (int i = 0; i < len; i++) {
            printf("%x ", data[i]);
        }

        printf("\r\n\r\n");

        /* set LED to the payload */
        digitalWrite(LED_BUILTIN, data[0]);
    }
}

/* async-subscribe to topics defined in the list */
bool check_topics(void) {
    if (!client.subscribe_topics(sub_topics, sizeof(sub_topics) / sizeof(MQTTSNSubTopic)))
        return false;

    return true;
}
