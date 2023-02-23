#ifndef QTPY_ESP32_SHT40_OTA_QTPY_ESP32_SHT40_OTA_H
#define QTPY_ESP32_SHT40_OTA_QTPY_ESP32_SHT40_OTA_H

#include <Wire.h>					 // This header is part of the standard library.  https://www.arduino.cc/en/reference/wire
#include "PubSubClient.h"		 // PubSub is the MQTT API.  Author: Nick O'Leary  https://github.com/knolleary/pubsubclient
#include <ArduinoJson.h>		 // The JSON parsing library used.  Author: Benoît Blanchon  https://arduinojson.org/
#include <WiFiUdp.h>				 // OTA
#include <ArduinoOTA.h>			 // OTA - The Arduino OTA library.  Specific version of this are installed along with specific boards in board manager.
#include <Adafruit_SHT4x.h>	 // Adafruit library for the SHT4x series, such as the SHT40.  https://github.com/adafruit/Adafruit_SHT4X, requires Adafruit_Sensor: https://github.com/adafruit/Adafruit_Sensor
#include <Adafruit_NeoPixel.h> // The Adafruit NeoPixel library to drive the RGB LED on the QT Py.	https://github.com/adafruit/Adafruit_NeoPixel
#include "privateInfo.h"		 // I use this file to hide my network information from random people browsing my GitHub repo.
#include "NetworkFunctions.h"


// NeoPixel related values.
#define NUM_PIXELS 1
#define RED			 0xFF0000
#define ORANGE		 0xFFA500
#define YELLOW		 0xFFFF00
#define GREEN		 0x00FF00
#define BLUE		 0x0000FF
#define INDIGO		 0x4B0082
#define VIOLET		 0xEE82EE
#define PURPLE		 0x800080
#define BLACK		 0x000000
#define GRAY		 0x808080
#define WHITE		 0xFFFFFF


/*
 * Declare network variables.
 * Adjust the commented-out variables to match your network and broker settings.
 * The commented-out variables are stored in "privateInfo.h", which I do not upload to GitHub.
 */
// const char * wifiSsidArray[4] = { "Network1", "Network2", "Network3", "Syrinx" };			// Typically declared in "privateInfo.h".
// const char * wifiPassArray[4] = { "Password1", "Password2", "Password3", "By-Tor" };		// Typically declared in "privateInfo.h".
// const char * mqttBrokerArray[4] = { "Broker1", "Broker2", "Broker3", "192.168.0.2" };		// Typically declared in "privateInfo.h".
// int const mqttPortArray[4] = { 1883, 1883, 1883, 2112 };												// Typically declared in "privateInfo.h".

const char *notes = "office/QTPy/ Adafruit QT Py ESP32-S2 with SHT40 and OTA";
const char *hostName = "office-qtpy-sht40";						// The network hostName for this device.  Used by OTA and general networking.
const char *commandTopic = "office/QTPy/command";				// The topic used to subscribe to update commands.  Commands: publishTelemetry, changeTelemetryInterval, publishStatus.
const char *sketchTopic = "office/QTPy/sketch";					// The topic used to publish the sketch name (__FILE__).
const char *macTopic = "office/QTPy/mac";							// The topic used to publish the MAC address.
const char *ipTopic = "office/QTPy/ip";							// The topic used to publish the IP address.
const char *rssiTopic = "office/QTPy/rssi";						// The topic used to publish the WiFi Received Signal Strength Indicator.
const char *publishCountTopic = "office/QTPy/publishCount"; // The topic used to publish the loop count.
const char *notesTopic = "office/QTPy/notes";					// The topic used to publish notes relevant to this project.
const char *tempCTopic = "office/QTPy/sht40/tempC";			// The topic used to publish the temperature in Celsius.
const char *tempFTopic = "office/QTPy/sht40/tempF";			// The topic used to publish the temperature in Fahrenheit.
const char *humidityTopic = "office/QTPy/sht40/humidity";	// The topic used to publish the humidity.
const int JSON_DOC_SIZE = 1024;										// The ArduinoJson document size.
unsigned int consecutiveBadTemp = 0;								// Holds the current number of consecutive invalid temperature readings.
unsigned int consecutiveBadHumidity = 0;							// Holds the current number of consecutive invalid humidity readings.
unsigned int networkIndex = 2112;									// Holds the correct index for the network arrays: wifiSsidArray[], wifiPassArray[], mqttBrokerArray[], and mqttPortArray[].
unsigned long publishInterval = 60000;								// The delay in milliseconds between MQTT publishes.  This prevents "flooding" the broker.
unsigned long telemetryInterval = 10000;							// The delay in milliseconds between polls of the sensor.  This should be greater than 100 milliseconds.
unsigned long lastPublishTime = 0;									// The time since last MQTT publish.
unsigned long lastPollTime = 0;										// The time since last sensor poll.
unsigned long publishCount = 0;										// A count of how many publishes have taken place.
unsigned long wifiConnectionTimeout = 10000;						// The maximum amount of time in milliseconds to wait for a WiFi connection before trying a different SSID.
unsigned long bootTime = millis();									// The boot time of the device.
unsigned long mqttReconnectInterval = 5000;						// The time between MQTT connection attempts.
float tempC;																// A global to hold the temperature in Celsius.
float tempF;																// A global to hold the temperature in Fahrenheit.
float humidity;															// A global to hold the relative humidity reading.


// Create class objects.
Adafruit_NeoPixel pixels( NUM_PIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800 );
Adafruit_SHT4x sht40 = Adafruit_SHT4x(); // SHT40 class object to read temperature and humidity from.
sensors_event_t humidity_sensor;
sensors_event_t temperature_sensor;

#endif //QTPY_ESP32_SHT40_OTA_QTPY_ESP32_SHT40_OTA_H
