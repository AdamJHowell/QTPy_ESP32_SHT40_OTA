/**
 * This sketch is a branch of my PubSubWeather sketch.
 * This sketch will use a HTU21D (SHT20/SHT21 compatible) sensor to measure temperature and humidity.
 * The ESP-32 SDA pin is GPIO21, and SCL is GPIO22.
 * @copyright   Copyright © 2022 Adam Howell
 * @licence     The MIT License (MIT)
 */
#include "WiFi.h"						// This header is part of the standard library.  https://www.arduino.cc/en/Reference/WiFi
#include <Wire.h>						// This header is part of the standard library.  https://www.arduino.cc/en/reference/wire
#include <PubSubClient.h>			// PubSub is the MQTT API.  Author: Nick O'Leary  https://github.com/knolleary/pubsubclient
#include "Adafruit_SHT4x.h"		// Adafruit library for the SHT4x series, such as the SHT40.  https://github.com/adafruit/Adafruit_SHT4X, requires Adafruit_Sensor: https://github.com/adafruit/Adafruit_Sensor
#include <ESPmDNS.h>					// OTA
#include <WiFiUdp.h>					// OTA
#include <ArduinoOTA.h>				// OTA
#include "privateInfo.h"			// I use this file to hide my network information from random people browsing my GitHub repo.
#include <Adafruit_NeoPixel.h>	// The Adafruit NeoPixel library to drive the RGB LED on the QT Py.	https://github.com/adafruit/Adafruit_NeoPixel


// NUMPIXELS sets the number of NeoPixel elements.
#define NUMPIXELS        1
#define RED 0xFF0000
#define ORANGE 0xFFA500
#define YELLOW 0xFFFF00
#define GREEN 0x00FF00
#define BLUE 0x0000FF
#define INDIGO 0x4B0082
#define VIOLET 0xEE82EE
#define PURPLE 0x800080
#define BLACK 0x000000
#define GRAY 0x808080
#define WHITE 0xFFFFFF


/**
 * Declare network variables.
 * Adjust the commented-out variables to match your network and broker settings.
 * The commented-out variables are stored in "privateInfo.h", which I do not upload to GitHub.
 */
//const char *wifiSsid = "yourSSID";				// Typically kept in "privateInfo.h".
//const char *wifiPassword = "yourPassword";		// Typically kept in "privateInfo.h".
//const char *mqttBroker = "yourBrokerAddress";	// Typically kept in "privateInfo.h".
//const int mqttPort = 1883;							// Typically kept in "privateInfo.h".
const char *mqttTopic = "espWeather";
// My topic format is: home/<device MAC>/sensors/<sensor type>/<temperature, humidity, pressure, etc.>
const char *topicRoot = "home";
const char *sketchName = "QTPy_ESP32_SHT40_OTA";
const char *notes = "Adafruit QT Py ESP32-S2 with SHT40 and OTA";
char ipAddress[16];
char macAddress[18];
int loopCount = 0;
int publishDelay = 60000;
uint32_t start;
uint32_t stop;
unsigned long lastPublish = 0;


// Create class objects.
WiFiClient espClient;							// Network client.
PubSubClient mqttClient( espClient );		// MQTT client.
Adafruit_NeoPixel pixels( NUMPIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800 );
Adafruit_SHT4x sht40 = Adafruit_SHT4x();	// SHT40 class object to read temperature and humidity from.


/**
 * The setup() function runs once when the device is booted, and then loop() takes over.
 */
void setup()
{
#if defined( NEOPIXEL_POWER )
	// If this board has a power control pin, we must set it to output and high in order to enable the NeoPixels.
	// We put this in an #ifdef so it can be reused for other boards without compilation errors.
	pinMode( NEOPIXEL_POWER, OUTPUT );
	digitalWrite( NEOPIXEL_POWER, HIGH );
#endif
	// Initialize the NeoPixel.
	pixels.begin();
	pixels.setBrightness( 20 );

	// Set the LED color to gray to indicate setup is underway.
	pixels.fill( GRAY );
	pixels.show();

	// Start the Serial communication to send messages to the computer.
	Serial.begin( 115200 );
	if( !Serial )
		delay( 1000 );
	Serial.println( "Setup is initializing the I2C bus for the Stemma QT port." );
	Wire.setPins( SDA1, SCL1 );	// This is what selects the Stemma QT port, otherwise the two pin headers will be I2C.
	Wire.begin();

	Serial.println( __FILE__ );

	// Set up the sensor.
	setupSHT40();

	// Set the ipAddress char array to a default value.
	snprintf( ipAddress, 16, "127.0.0.1" );

	// Set the MQTT client parameters.
	mqttClient.setServer( mqttBroker, mqttPort );

	// Get the MAC address and store it in macAddress.
	snprintf( macAddress, 18, "%s", WiFi.macAddress().c_str() );

	Serial.println( "Connecting WiFi..." );
	// Set the LED color to yellow.
	pixels.fill( YELLOW );
	pixels.show();

	WiFi.mode( WIFI_STA );
	WiFi.begin( wifiSsid, wifiPassword );
	while( WiFi.waitForConnectResult() != WL_CONNECTED )
	{
		Serial.println( "Connection Failed! Rebooting..." );
		// Set the LED color to red.
		pixels.fill( RED );
		pixels.show();
		delay( 5000 );
		ESP.restart();
	}

	// Port defaults to 3232
	// ArduinoOTA.setPort( 3232 );

	// Hostname defaults to esp3232-[MAC]
	// ArduinoOTA.setHostname( "myesp32" );

	// No authentication by default
	// ArduinoOTA.setPassword( "admin" );

	// Password can be set with it's md5 value as well
	// MD5( admin ) = 21232f297a57a5a743894a0e4a801fc3
	// ArduinoOTA.setPasswordHash( "21232f297a57a5a743894a0e4a801fc3" );

	ArduinoOTA
		.onStart( []()
		{
			String type;
			if( ArduinoOTA.getCommand() == U_FLASH )
				type = "sketch";
			else // U_SPIFFS
				type = "filesystem";

			// NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
			Serial.println( "Start updating " + type );
		} )
		.onEnd( []()
		{
			Serial.println( "\nEnd" );
		} )
		.onProgress( []( unsigned int progress, unsigned int total )
		{
			Serial.printf( "Progress: %u%%\r", ( progress / ( total / 100 ) ) );
		} )
		.onError( []( ota_error_t error )
		{
			Serial.printf( "Error[%u]: ", error );
			if( error == OTA_AUTH_ERROR )
				Serial.println( "Auth Failed" );
			else if( error == OTA_BEGIN_ERROR )
				Serial.println( "Begin Failed" );
			else if( error == OTA_CONNECT_ERROR )
				Serial.println( "Connect Failed" );
			else if( error == OTA_RECEIVE_ERROR )
				Serial.println( "Receive Failed" );
			else if( error == OTA_END_ERROR )
				Serial.println( "End Failed" );
		} );

	ArduinoOTA.begin();

	Serial.println( "Ready" );
	Serial.print( "IP address: " );
	Serial.println( WiFi.localIP() );

	snprintf( ipAddress, 16, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3] );

	// Set the LED color to green.
	pixels.fill( GREEN );
	pixels.show();

	Serial.println( "Setup is complete!" );
} // End of setup() function.


void setupSHT40()
{
	Serial.println( "Initializing the SHT40 using Adafruit's SHT4x library..." );
	// Set the LED color to yellow.
	pixels.fill( YELLOW );
	pixels.show();

	while( !sht40.begin() )
	{
		Serial.println( "Couldn't find SHT4x!" );
		Serial.println( "Retrying in 5 seconds." );
		delay( 5000 );
	}
	Serial.println( "Found SHT4x sensor" );
	Serial.print( "Serial number 0x" );
	Serial.println( sht40.readSerial(), HEX );

	// You can have 3 different precisions, higher precision takes longer.
	sht40.setPrecision( SHT4X_HIGH_PRECISION );
	switch( sht40.getPrecision() )
	{
		case SHT4X_HIGH_PRECISION: 
			Serial.println( "High precision" );
			break;
		case SHT4X_MED_PRECISION: 
			Serial.println( "Med precision" );
			break;
		case SHT4X_LOW_PRECISION: 
			Serial.println( "Low precision" );
			break;
	}

	// You can have 6 different heater settings.
	// Higher heat and longer times uses more power,
	// and reads will take longer.
	sht40.setHeater( SHT4X_NO_HEATER );
	switch( sht40.getHeater() )
	{
		case SHT4X_NO_HEATER: 
			Serial.println("No heater");
			break;
		case SHT4X_HIGH_HEATER_1S: 
			Serial.println("High heat for 1 second");
			break;
		case SHT4X_HIGH_HEATER_100MS: 
			Serial.println("High heat for 0.1 second");
			break;
		case SHT4X_MED_HEATER_1S: 
			Serial.println("Medium heat for 1 second");
			break;
		case SHT4X_MED_HEATER_100MS: 
			Serial.println("Medium heat for 0.1 second");
			break;
		case SHT4X_LOW_HEATER_1S: 
			Serial.println("Low heat for 1 second");
			break;
		case SHT4X_LOW_HEATER_100MS: 
			Serial.println("Low heat for 0.1 second");
			break;
	}
	// Set the LED color to green.
	pixels.fill( GREEN );
	pixels.show();
	Serial.println( "The SHT40 has been configured." );
} // End of setupSHT40 function.


// mqttConnect() will attempt to (re)connect the MQTT client.
void mqttConnect( int maxAttempts )
{
	int i = 0;
	// Loop until MQTT has connected.
	while( !mqttClient.connected() && i < maxAttempts )
	{
		Serial.print( "Attempting MQTT connection..." );
		// Set the LED color to orange while we reconnect.
		pixels.fill( ORANGE );
		pixels.show();

		// Connect to the broker using the MAC address for a clientID.  This guarantees that the clientID is unique.
		if( mqttClient.connect( macAddress ) )
		{
			Serial.println( "connected!" );
			// Set the LED color to green.
			pixels.fill( GREEN );
			pixels.show();
		}
		else
		{
			Serial.print( " failed, return code: " );
			Serial.print( mqttClient.state() );
			Serial.println( " try again in 2 seconds" );
			// Wait 5 seconds before retrying.
			delay( 5000 );
		}
		i++;
	}
	mqttClient.setBufferSize( 512 );
	char mqttString[512];
	snprintf( mqttString, 512, "{\n\t\"sketch\": \"%s\",\n\t\"mac\": \"%s\",\n\t\"ip\": \"%s\"\n}", sketchName, macAddress, ipAddress );
	mqttClient.publish( "espConnect", mqttString );
} // End of mqttConnect() function.


/**
 * The loop() function begins after setup(), and repeats as long as the unit is powered.
 */
void loop()
{
	// Set the LED color to orange while we check the MQTT connection state.
	pixels.fill( ORANGE );
	pixels.show();
	delay( 10 );

	// Check the mqttClient connection state.
	if( !mqttClient.connected() )
	{
		// Reconnect to the MQTT broker.
		mqttConnect( 10 );
	}
	// The loop() function facilitates the receiving of messages and maintains the connection to the broker.
	mqttClient.loop();

	ArduinoOTA.handle();
	yield();

	// Set color to blue while we read the sensor.
	pixels.fill( BLUE );
	pixels.show();
	delay( 500 );

	unsigned long time = millis();
	if( ( time > publishDelay ) && ( time - publishDelay ) > lastPublish )
	{
		loopCount++;
		Serial.println();
		Serial.println( sketchName );
		Serial.println( __FILE__ );

		// Create objects to hold sensor data.  See Adafruit_Sensor.h for details on this type.
		sensors_event_t humidity;
		sensors_event_t temp;

		uint32_t timestamp = millis();
		// Populate temp and humidity objects with fresh data.
		sht40.getEvent( &humidity, &temp );
		timestamp = millis() - timestamp;

		Serial.print( "Temperature: " );
		Serial.print( temp.temperature );
		Serial.println( " degrees C" );
		Serial.print( "Humidity: " );
		Serial.print( humidity.relative_humidity );
		Serial.println( "% rH" );
		Serial.print( "Read duration (ms): " );
		Serial.println( timestamp );

		// Print the signal strength:
		long rssi = WiFi.RSSI();
		Serial.print( "WiFi RSSI: " );
		Serial.println( rssi );

		// Prepare a String to hold the JSON.
		char mqttString[512];
		// Write the readings to the String in JSON format.
		snprintf( mqttString, 512, "{\n\t\"sketch\": \"%s\",\n\t\"mac\": \"%s\",\n\t\"ip\": \"%s\",\n\t\"tempC\": %.2f,\n\t\"humidity\": %.2f,\n\t\"rssi\": %ld,\n\t\"uptime\": %d,\n\t\"notes\": \"%s\"\n}", sketchName, macAddress, ipAddress, temp.temperature, humidity.relative_humidity, rssi, loopCount, notes );
		if( mqttClient.connected() )
		{
			// Publish the JSON to the MQTT broker.
			mqttClient.publish( mqttTopic, mqttString );
			Serial.print( "Published to topic " );
			Serial.println( mqttTopic );
		}
		else
		{
			Serial.println( "Lost connection to the MQTT broker between the start of this loop and now!" );
		}
		// Print the JSON to the Serial port.
		Serial.println( mqttString );

		String logString = "loopCount: ";
		logString += loopCount;
		Serial.println( logString );

		lastPublish = millis();
		Serial.print( "Next publish in " );
		Serial.print( publishDelay / 1000 );
		Serial.println( " seconds.\n" );
	}

	// Set the LED color to green.
	pixels.fill( GREEN );
	pixels.show();
	delay( 10 );
} // End of loop() function.
