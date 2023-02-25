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


long rssi;											 // A global to hold the Received Signal Strength Indicator.
char macAddress[18];								 // The MAC address of the WiFi NIC.
char ipAddress[16];								 // The IP address given to the device.
unsigned int callbackCount = 0;				 // The number of times a callback has been processed.
unsigned int wifiConnectCount = 0;			 // The number of times Wi-Fi has (re)connected.
unsigned long lastWifiConnectTime = 0;		 // The most recent Wi-Fi connection time.
unsigned int wifiCoolDownInterval = 20000; // The minimum time between Wi-Fi connection attempts.

void onReceiveCallback( char *topic, byte *payload, unsigned int length );
void configureOTA();
int checkForSSID( const char *ssidName );
bool wifiConnect( const char *ssid, const char *password );
void wifiMultiConnect();
bool mqttMultiConnect( int maxAttempts );


WiFiClient espClient;					  // Network client.
PubSubClient mqttClient( espClient ); // MQTT client.


#endif //QTPY_ESP32_SHT40_OTA_NETWORKFUNCTIONS_H
