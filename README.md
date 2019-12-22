C++ implementation of an MQTT-SN client and gateway
==================================================================

- Minimal MQTT-SN client and aggregating gateway, with gateway discovery supported
- Intended for the Arduino environment, client is light enough to run on an Uno easily
- Completely non-blocking, so other application tasks can happen during protocol transactions
- Modular, so that new transports and HALs/devices can be easily added by implementing the right interface
- Gateway falls back to being a local MQTT-SN broker, in the absence of an MQTT connection
- Zero dynamic allocation, up-front costs only, a plus depending on your application
- Basic functionality complete and tested. No topic wildcards, LWT or message retention supported yet
- Sleeping clients now supported, though completely untested for now

## Dependencies
- A modified [fork](https://github.com/brianrho/pubsubclient) of [knolleary]'s `PubSubClient` provides the default MQTT client interface in examples. This can be replaced by simply sub-classing the `MQTTClient` class to provide your own MQTT client implementation.

- A simple generic [FIFO](https://github.com/brianrho/LiteFifo) library for holding messages

## Tests
- ESP32 gateway and one ESP32 client using RFM69HW transport
- ESP32 gateway with one STM32 client (in Truestudio) using HC12 transport
