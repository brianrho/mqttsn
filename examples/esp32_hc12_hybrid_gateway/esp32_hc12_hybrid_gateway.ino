/* ESP32 gateway with a builtin 'dummy' client */

#include "mqttsn_gateway_includes.h"
#include "mqttsn_client_includes.h"

#include <WiFi.h>
#include <PubSubClient.h>
#include <HardwareSerial.h>
#include <hc12.h>

/* setup HC12 port, gateway address = 10 */
HardwareSerial rfport(1);
HC12 hc12(&rfport);
uint8_t own_addr = 10;

WiFiClient espClient;
PubSubClient pubsub(espClient);

/******************* create gateway MQTTSN components ****************/
MQTTSNDeviceArduino device;

/* 2 transports: one for the HC12 and the other for the dummy client (address: 12) */
MQTTSNTransportHC12 hc12link(&hc12);
MQTTSNTransportDummy gwdummylink(12);

/* MQTT interface */
MQTTClientPubsub mqttc(&pubsub);

/* The gateway instance itself */
MQTTSNGateway gateway(&device, &mqttc);

/* provide MQTT connection details */
const char* ssid = "........";
const char* password = "..........";
const char* mqtt_server = "iot.eclipse.org";
const char* clientId = "ESP32MQTTSNGateway";

/****************** create client MQTTSN components ******************/

/* Transport for this builtin dummy client */
MQTTSNTransportDummy clntdummylink(11);
MQTTSNClient client(&device, &clntdummylink);

/* GW info: GW_ID = 2, GW_ADDR = 12, address is 1-byte long */
MQTTSNAddress gw_addr = {{12}, 1};
MQTTSNGWInfo gw_info = {2, gw_addr};

/* list of client topics */
MQTTSNPubTopic pub_topics[] = { {"stmled"} };
MQTTSNSubTopic sub_topics[] = { {"espled"} };

/*******************************************************************/

void setup_wifi(void) {
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Already connected.");
        return;
    }
    
    delay(10);
    /* We start by connecting to a WiFi network */
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.disconnect(true);
    WiFi.begin(ssid, password);

    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
        return;
    }

    randomSeed(micros());

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

long lastReconnectAttempt = 0;

void reconnect() {
    if (WiFi.status() != WL_CONNECTED) {
        setup_wifi();
        return;
    }
    
    /* check that a connection isnt already in progress */
    if (pubsub.state() != MQTT_CONNECT_INPROGRESS) {
        long now = millis();
        if (now - lastReconnectAttempt < 5000)
            return;

        Serial.print("Attempting MQTT connection...");

        /* Attempt to connect */
        lastReconnectAttempt = now;
        pubsub.beginConnect(clientId);
    }

    /* check the connect status */
    int ret = pubsub.connectStatus();
    switch (ret) {
        case MQTT_CONNECTED:
            Serial.println("connected");
            lastReconnectAttempt = 0;
            break;
        case MQTT_CONNECT_INPROGRESS:
            return;
        default:
            Serial.print("failed! rc = "); Serial.print(ret);
            Serial.println(". Trying again in 5 seconds.");
            break;
    }
}

void setup() {
    Serial.begin(9600);
    pinMode(LED_BUILTIN, OUTPUT);
    
    /* RX = IO16, TX = IO17 */
    rfport.begin(9600, SERIAL_8N1, 16, 17);

    /* set up local transport */
    hc12.begin(own_addr);

    /* setup wifi and MQTT client */
    setup_wifi();
    pubsub.setServer(mqtt_server, 1883);

    Serial.println("Starting gateway.");

    /* start up gateway, GWID = 2 */
    if (!gateway.begin(2)) {
        Serial.println("Init gateway failed.");
        while (1) yield();
    }

    /* register all gw transports here */
    gateway.register_transport(&hc12link);
    gateway.register_transport(&gwdummylink);
    
    /* set topic prefix to the client ID, remove this if you want */
    gateway.set_topic_prefix(clientId);

    /* start the client */
    if (!client.begin("Innerclient")) {
        Serial.println("Init client failed.");
        while (1) yield();
    }

    client.add_gateways(&gw_info, 1);
    client.on_message(publish_callback);
    
    /* async-connect to first available gateway */
    if (!client.connect()) {
        Serial.println("Connect failed.");
        while (1) yield();
    }
}


uint8_t led_state = 0;
uint32_t last_publish = 0;

void loop() {
    /* gateway tasks loop */
    gateway.loop();
    
    /* MQTT tasks loop */
    mqttc.loop();
    
    /* handle MQTT reconnect */
    if (!pubsub.connected()) {
        reconnect();
    }

    client.loop();
    
    if (!client.is_connected())
        return;
        
    /* check if all topics have been registered */
    if (!check_topics())
        return;
        
    /* toggle and publish led state every 5 secs */
    if (millis() - last_publish >= 5000) {
        led_state ^= 1;
        client.publish("stmled", &led_state, sizeof(led_state));
        last_publish = millis();
    }
}

/* set LED to the payload */
void publish_callback(const char * topic, uint8_t * data, uint8_t len, MQTTSNFlags * flags)
{
    printf("\r\nTopic: %s\r\n", topic);
    printf("Payload: ");

    for (int i = 0; i < len; i++) {
        printf("%x ", data[i]);
    }

    printf("\r\n\r\n");
    if (strcmp(topic, "espled") == 0 && len != 0)
        digitalWrite(LED_BUILTIN, data[0]);
}

bool check_topics(void) {
    if (!client.register_topics(pub_topics, sizeof(pub_topics) / sizeof(MQTTSNPubTopic)))
        return false;

    if (!client.subscribe_topics(sub_topics, sizeof(sub_topics) / sizeof(MQTTSNSubTopic)))
        return false;

    return true;
}
