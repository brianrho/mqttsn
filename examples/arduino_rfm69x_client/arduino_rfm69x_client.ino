/* Arduino client with RFM69HW transport */

#include "mqttsn_client_includes.h"
#include "printf_config.h"
#include <RFM69X.h>

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
MQTTSNPubTopic pub_topics[] = { {"led"} };
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

uint8_t led_state = 0;
uint32_t last_publish = 0;

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
        
    /* toggle and publish led state every 5 secs */
    if (millis() - last_publish >= 5000) {
        led_state ^= 1;
        client.publish("led", &led_state, sizeof(led_state));
        last_publish = millis();
    }
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

/* async-register and async-subscribe to topics defined in the lists */
bool check_topics(void) {
    if (!client.register_topics(pub_topics, sizeof(pub_topics) / sizeof(MQTTSNPubTopic)))
        return false;

    if (!client.subscribe_topics(sub_topics, sizeof(sub_topics) / sizeof(MQTTSNSubTopic)))
        return false;

    return true;
}
