//
// Created by Adam on 2023-02-22.
//

#ifndef QTPY_ESP32_SHT40_OTA_NETWORKFUNCTIONS_H
#define QTPY_ESP32_SHT40_OTA_NETWORKFUNCTIONS_H


#include "QTPy_ESP32_SHT40_OTA.h"

#ifdef ESP8266
// These headers are installed when the ESP8266 is installed in board manager.
#include <ESP8266WiFi.h> // ESP8266 WiFi support.  https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266WiFi
#include <ESP8266mDNS.h> // OTA - mDNSResponder (Multicast DNS) for the ESP8266 family.
#else
// These headers are installed when the ESP32 is installed in board manager.
#include <WiFi.h>		// ESP32 Wifi support.  https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFi/src/WiFi.h
#include <ESPmDNS.h> // OTA - Multicast DNS for the ESP32.
#endif


long rssi;				// A global to hold the Received Signal Strength Indicator.
char macAddress[18]; // The MAC address of the WiFi NIC.
char ipAddress[16];	// The IP address given to the device.


WiFiClient espClient;					  // Network client.
PubSubClient mqttClient( espClient ); // MQTT client.


#endif //QTPY_ESP32_SHT40_OTA_NETWORKFUNCTIONS_H
