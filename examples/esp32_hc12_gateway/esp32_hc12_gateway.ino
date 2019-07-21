#include "mqttsn_gateway_includes.h"
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

/* create MQTTSN components */
MQTTSNDeviceArduino device;
MQTTSNTransportHC12 transport(&hc12);
MQTTClientPubsub mqttc(&pubsub);

MQTTSNGateway gateway(&device, &transport, &mqttc);

const char* ssid = "..........";
const char* password = "............";
const char* mqtt_server = "iot.eclipse.org";

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

        /* Create a random client ID */
        String clientId = "ESP32Client-";
        clientId += String(random(0xffff), HEX);

        /* Attempt to connect */
        lastReconnectAttempt = now;
        pubsub.beginConnect(clientId.c_str());
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
