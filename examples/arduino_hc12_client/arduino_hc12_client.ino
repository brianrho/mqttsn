#include "mqttsn_client_includes.h"
#include <SoftwareSerial.h>
#include <hc12.h>
#include "printf_config.h"

/* A list of 3 gateways
 * The first is initialized to GW_ID = 2, GW_ADDR = 10, address is 1-byte long */
const int GATEWAYS_SZ = 3;
MQTTSNAddress gw_addr = {{10}, 1};
MQTTSNGWInfo gateways[GATEWAYS_SZ] = { {2, gw_addr}  };

/* setup HC12 port */
SoftwareSerial rfport(4, 5);
HC12 hc12(&rfport);
uint8_t own_addr = 9;

/* create MQTTSN components */
MQTTSNDeviceArduino device;
MQTTSNTransportHC12 transport(&hc12);
MQTTSNClient client(&device, &transport);

/* list topics */
MQTTSNPubTopic pub_topics[] = { {"led"} };
MQTTSNSubTopic sub_topics[] = { {"led"} };

void setup(void) {
    Serial.begin(9600);
    rfport.begin(9600);
    printf_begin();
    
    hc12.begin(own_addr);

    Serial.println("Starting client.");
    
    if (!client.begin("Pubclient")) {
        Serial.println("Init client failed.");
        while (1) yield();
    }

    client.add_gateways(gateways, GATEWAYS_SZ);
    client.on_message(publish_callback);
    
    /* async-connect to first available gateway */
    if (!client.connect()) {
        Serial.println("Connect failed.");
        while (1) yield();
    }
}

uint8_t led_state = 0;
uint32_t last_publish = 0;

void loop(void) {
    client.loop();
    
    if (!client.is_connected())
        return;
        
    /* check if all topics have been registered */
    if (!check_topics())
        return;
        
    /* toggle and publish led state every 5 secs */
    if (millis() - last_publish >= 5000) {
        led_state ^= 1;
        client.publish("led", &led_state, sizeof(led_state));
        last_publish = millis();
    }
}

/* set LED to the payload */
void publish_callback(const char * topic, uint8_t * data, uint8_t len, MQTTSNFlags * flags)
{
    if (strcmp(topic, "led") == 0 && len != 0) {
        printf("\r\nTopic: %s\r\n", topic);
        printf("Payload: ");

        for (int i = 0; i < len; i++) {
            printf("%x ", data[i]);
        }

        printf("\r\n\r\n");
        digitalWrite(LED_BUILTIN, data[0]);
    }
}

bool check_topics(void) {
    if (!client.register_topics(pub_topics, sizeof(pub_topics) / sizeof(MQTTSNPubTopic)))
        return false;

    if (!client.subscribe_topics(sub_topics, sizeof(sub_topics) / sizeof(MQTTSNSubTopic)))
        return false;

    return true;
}

