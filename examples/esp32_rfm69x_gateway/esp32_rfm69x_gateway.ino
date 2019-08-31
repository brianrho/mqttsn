/* ESP32 gateway with HC12 transport */

#include "mqttsn_gateway_includes.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <RFM69X.h>

/* create RFM69HW component */
boolean IS_RFM69HW = true;
#define RFM_SS          14
#define RFM_INT         12
uint8_t RFM_INTNUM = digitalPinToInterrupt(RFM_INT);
RFM69X radio(RFM_SS, RFM_INT, IS_RFM69HW, RFM_INTNUM);

/* needed for MQTT */
WiFiClient espClient;
PubSubClient pubsub(espClient);
const char* mqtt_server = "broker.hivemq.com";
const char* clientId = "ESP32MQTTSNGateway";

/* create MQTTSN components */
MQTTSNDeviceArduino device;
MQTTSNTransportRFM69X transport(&radio);
MQTTClientPubsub mqttc(&pubsub);
MQTTSNGateway gateway(&device, &mqttc);

long lastReconnectAttempt = 0;

void reconnect() {
    if (WiFi.status() != WL_CONNECTED) {
        gateway_wifi_setup();
        return;
    }

    /* check that a connection isnt already in progress */
    if (pubsub.state() != MQTT_CONNECT_INPROGRESS) {
        long now = millis();
        if (now - lastReconnectAttempt < 5000)
            return;

        Serial.println("Attempting MQTT connection.");

        /* Attempt to connect */
        lastReconnectAttempt = now;
        pubsub.beginConnect(clientId);
    }

    /* check the connect status */
    int ret = pubsub.connectStatus();
    switch (ret) {
        case MQTT_CONNECTED:
            Serial.println("Connected to broker.");
            lastReconnectAttempt = 0;
            break;
        case MQTT_CONNECT_INPROGRESS:
            return;
        default:
            Serial.printf("MQTT connect failed! rc = %d. Trying again in 5 seconds.\r\n", ret);
            break;
    }
}

void setup() {
    Serial.begin(9600);

    /* set up local transport */
    gateway_local_setup();

    /* setup wifi and MQTT client */
    gateway_wifi_setup();
    pubsub.setServer(mqtt_server, 1883);

    Serial.println("Starting gateway.");

    /* start up gateway, GWID = 2 */
    if (!gateway.begin(2)) {
        Serial.println("Init gateway failed.");
        while (1) yield();
    }

    /* register all transports here, one by one */
    gateway.register_transport(&transport);

    /* set topic prefix to the client ID, remove this if you want */
    gateway.set_topic_prefix(clientId);
}

void gateway_wifi_setup(void) {
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Already connected.");
        return;
    }

    /* provide connection details */
    const char* ssid = "..........";
    const char* password = "..........";

    /* We start by connecting to a WiFi network */
    Serial.printf("\r\nConnecting to %s\r\n", ssid);

    WiFi.disconnect(true);
    WiFi.begin(ssid, password);

    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
        return;
    }

    randomSeed(micros());

    Serial.print("\r\nWiFi connected. IP address: ");
    Serial.println(WiFi.localIP());
}

void gateway_local_setup(void) {
    const uint8_t NODEID = 10;
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

void loop() {
    /* gateway tasks loop */
    gateway.loop();
    /* MQTT tasks loop */
    mqttc.loop();

    /* handle MQTT reconnect */
    if (!pubsub.connected()) {
        reconnect();
    }
}
