C++ implementation of an MQTT-SN client and gateway
==================================================================

- Minimal MQTT-SN client and aggregate gateway, with gateway discovery included.
- C++, Arduino-compatible. Client can run on an Uno.
- Completely non-blocking, so other application tasks can happen during transactions
- Modular, so that new transports and HALs/devices can be easily added by replacing the right interface
- Gateway falls back to being a local MQTT-SN broker, in the absence of an MQTT connection
- Zero dynamic allocation
- Basic functionality complete and tested, but no topic wildcards or LWT supported yet

## Dependencies
- A modified [fork](https://github.com/brianrho/pubsubclient) of [knolleary]'s `PubSubClient` provides the default MQTT client interface in examples. This can be replaced by simply sub-classing the `MQTTClient` class to provide your own MQTT client implementation.

- A simple generic [FIFO](https://github.com/brianrho/LiteFifo) library for holding messages

## Tests
- ESP32 (+ HC12 RF module) gateway with one STM32 client (using Atollic Truestudio)
