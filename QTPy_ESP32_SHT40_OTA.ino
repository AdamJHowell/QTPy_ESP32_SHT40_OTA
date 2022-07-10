/**
 * This sketch is a branch of my PubSubWeather sketch.
 * This sketch will use a HTU21D (SHT20/SHT21 compatible) sensor to measure temperature and humidity.
 * The ESP-32 SDA pin is GPIO21, and SCL is GPIO22.
 * @copyright   Copyright © 2022 Adam Howell
 * @licence     The MIT License (MIT)
 */
#ifdef ESP8266
#include "ESP8266WiFi.h"			// This header is installed via board manager.  https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266WiFi
#elif ESP32
#include "WiFi.h"						// This header is installed via board manager.  https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFi/src/WiFi.h
#else
#include "WiFi.h"						// This header is part of the standard library.  https://www.arduino.cc/en/Reference/WiFi
#endif
#include <Wire.h>						// This header is part of the standard library.  https://www.arduino.cc/en/reference/wire
#include <PubSubClient.h>			// PubSub is the MQTT API.  Author: Nick O'Leary  https://github.com/knolleary/pubsubclient
#include <ArduinoJson.h>			// The JSON parsing library used.  Author: Benoît Blanchon  https://arduinojson.org/
#include "Adafruit_SHT4x.h"		// Adafruit library for the SHT4x series, such as the SHT40.  https://github.com/adafruit/Adafruit_SHT4X, requires Adafruit_Sensor: https://github.com/adafruit/Adafruit_Sensor
#include <Adafruit_NeoPixel.h>	// The Adafruit NeoPixel library to drive the RGB LED on the QT Py.	https://github.com/adafruit/Adafruit_NeoPixel
#include <ESPmDNS.h>					// OTA - Multicast DNS.
#include <WiFiUdp.h>					// OTA
#include <ArduinoOTA.h>				// OTA
#include "privateInfo.h"			// I use this file to hide my network information from random people browsing my GitHub repo.


// NeoPixel related values.
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
// My topic format is: home/<device MAC>/sensors/<sensor type>/<temperature, humidity, pressure, etc.>
const char * sketchName = "QTPy_ESP32_SHT40_OTA";
const char * notes = "office/QTPy/ Adafruit QT Py ESP32-S2 with SHT40 and OTA";
const char * hostname = "office-qtpy-sht40";						// The network hostname for this device.  Used by OTA and general networking.
const char * commandTopic = "office/QTPy/command";          // The topic used to subscribe to update commands.  Commands: publishTelemetry, changeTelemetryInterval, publishStatus.
const char * sketchTopic = "office/QTPy/sketch";            // The topic used to publish the sketch name.
const char * macTopic = "office/QTPy/mac";                  // The topic used to publish the MAC address.
const char * ipTopic = "office/QTPy/ip";                    // The topic used to publish the IP address.
const char * rssiTopic = "office/QTPy/rssi";                // The topic used to publish the WiFi Received Signal Strength Indicator.
const char * loopCountTopic = "office/QTPy/loopCount";      // The topic used to publish the loop count.
const char * notesTopic = "office/QTPy/notes";              // The topic used to publish notes relevant to this project.
const char * tempCTopic = "office/QTPy/sht40/tempC";        // The topic used to publish the temperature in Celcius.
const char * tempFTopic = "office/QTPy/sht40/tempF";        // The topic used to publish the temperature in Fahrenheit.
const char * humidityTopic = "office/QTPy/sht40/humidity";  // The topic used to publish the humidity.
const char * mqttTopic = "espWeather";                      // The topic used to publish a single JSON message containing all data.
unsigned long lastPublishTime = 0;                          // This is used to determine the time since last MQTT publish.
unsigned int consecutiveBadTemp = 0;
unsigned int consecutiveBadHumidity = 0;
unsigned long sensorPollDelay = 10000;						// This is the delay between polls of the soil sensor.  This should be greater than 100 milliseconds.
unsigned long lastPollTime = 0;								// This is used to determine the time since last sensor poll.

char ipAddress[16];
char macAddress[18];
unsigned long loopCount = 0;
int publishDelay = 60000;
uint32_t start;
uint32_t stop;
unsigned long lastPublish = 0;							// In milliseconds, this sets a limit at 49.7 days of time.
unsigned long mqttReconnectDelay = 5000;				// An unsigned long can hold values from 0-4,294,967,295.  In milliseconds, this sets a limit at 49.7 days of time.


// Create class objects.
WiFiClient espClient;							// Network client.
PubSubClient mqttClient( espClient );		// MQTT client.
Adafruit_NeoPixel pixels( NUMPIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800 );
Adafruit_SHT4x sht40 = Adafruit_SHT4x();	// SHT40 class object to read temperature and humidity from.
sensors_event_t humidity;
sensors_event_t tempC;


void onReceiveCallback( char * topic, byte * payload, unsigned int length )
{
	char str[length + 1];
	Serial.print( "Message arrived [" );
	Serial.print( topic );
	Serial.print( "] " );
	int i=0;
	for( i = 0; i < length; i++ )
	{
		Serial.print( ( char ) payload[i] );
		str[i] = ( char )payload[i];
	}
	Serial.println();
	// Add the null terminator.
	str[i] = 0;
	StaticJsonDocument <256> doc;
	deserializeJson( doc, str );

	// The command can be: publishTelemetry, changeTelemetryInterval, or publishStatus.
	const char * command = doc["command"];
	if( strcmp( command, "publishTelemetry") == 0 )
	{
		Serial.println( "Reading and publishing sensor values." );
		// Poll the sensor.
		readTelemetry();
		// Publish the sensor readings.
		publishTelemetry();
		Serial.println( "Readings have been published." );
	}
	else if( strcmp( command, "changeTelemetryInterval") == 0 )
	{
		Serial.println( "Changing the publish interval." );
		unsigned long tempValue = doc["value"];
		// Only update the value if it is greater than 4 seconds.  This prevents a seconds vs. milliseconds mixup.
		if( tempValue > 4000 )
			publishDelay = tempValue;
		Serial.print( "MQTT publish interval has been updated to " );
		Serial.println( publishDelay );
		lastPublishTime = 0;
	}
	else if( strcmp( command, "publishStatus") == 0 )
	{
		Serial.println( "publishStatus is not yet implemented." );
	}
	else
	{
		Serial.print( "Unknown command: " );
		Serial.println( command );
	}
} // End of onReceiveCallback() function.


/**
 * The setup() function runs once when the device is booted, and then loop() takes over.
 */
void setup()
{
	// Start the Serial communication to send messages to the computer.
	Serial.begin( 115200 );
	delay( 1000 );

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

	// Disable Bluetooth.
	btStop();

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
		// Tell the SDK to reboot.  Sometimes more reliable than ESP.reset().
		ESP.restart();
	}

	// Port defaults to 3232
	// ArduinoOTA.setPort( 3232 );

	// Hostname defaults to esp32-[MAC]
	ArduinoOTA.setHostname( hostname );

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
bool mqttConnect( int maxAttempts )
{
	int i = 0;
	// Loop until MQTT has connected.
	while( !mqttClient.connected() && i < maxAttempts )
	{
		Serial.print( "Attempting MQTT connection..." );
		// Set the LED color to orange while we reconnect.
		pixels.fill( ORANGE );
		pixels.show();

		char clientId[22];
		// Put the macAddress and a random number into clientId.  The random number suffix prevents brokers from rejecting a clientID as already in use.
		snprintf( clientId, 22, "%s-%03d", macAddress, random( 999 ) );
		Serial.print( "Connecting with client ID '" );
		Serial.print( clientId );
		Serial.print( "' " );

		// Connect to the broker using the pseudo-random clientId.
		if( mqttClient.connect( clientId ) )
		{
			Serial.println( "connected!" );
			// Set the LED color to green.
			pixels.fill( GREEN );
			pixels.show();
		}
		else
		{
			int mqttState = mqttClient.state();
			/*
				Possible values for client.state():
				#define MQTT_CONNECTION_TIMEOUT     -4		// Note: This also comes up when the clientID is already in use.
				#define MQTT_CONNECTION_LOST        -3
				#define MQTT_CONNECT_FAILED         -2
				#define MQTT_DISCONNECTED           -1
				#define MQTT_CONNECTED               0
				#define MQTT_CONNECT_BAD_PROTOCOL    1
				#define MQTT_CONNECT_BAD_CLIENT_ID   2
				#define MQTT_CONNECT_UNAVAILABLE     3
				#define MQTT_CONNECT_BAD_CREDENTIALS 4
				#define MQTT_CONNECT_UNAUTHORIZED    5
			*/
			Serial.print( " failed!  Return code: " );
			Serial.print( mqttState );
			if( mqttState == -4 )
			{
				Serial.println( " - MQTT_CONNECTION_TIMEOUT" );
			}
			else if( mqttState == 2 )
			{
				Serial.println( " - MQTT_CONNECT_BAD_CLIENT_ID" );
			}
			else
			{
				Serial.println( "" );
			}

			Serial.print( "Trying again in " );
			Serial.print( mqttReconnectDelay / 1000 );
			Serial.println( " seconds." );
			delay( mqttReconnectDelay );
		}
		i++;
	}
	if( mqttClient.connected() )
	{
		// Subscribe to backYard/Lolin8266/command, which will respond to publishTelemetry and publishStatus
		mqttClient.subscribe( commandTopic );
		mqttClient.setBufferSize( 512 );
		char connectString[512];
		snprintf( connectString, 512, "{\n\t\"sketch\": \"%s\",\n\t\"mac\": \"%s\",\n\t\"ip\": \"%s\"\n}", sketchName, macAddress, ipAddress );
		mqttClient.publish( "espConnect", connectString );
		// Set the LED color to green.
		pixels.fill( GREEN );
		pixels.show();
	}
	else
	{
		Serial.println( "Unable to connect to the MQTT broker!" );
		return false;
	}

	Serial.println( "Function mqttConnect() has completed." );
	return true;
} // End of mqttConnect() function.


/*
 * readTelemetry() will:
 * 1. read from all available sensors
 * 2. store legitimate values in global variables
 * 3. increment a counter if any value is invalid
 */
void readTelemetry()
{
	sensors_event_t temporaryHumidity;
	sensors_event_t temporaryTemperature;
	// Populate temp and humidity objects with fresh data.
	sht40.getEvent( &temporaryHumidity, &temporaryTemperature );

	// Define the valid temperature range (in Celsius) for this sensor.
	if( temporaryTemperature.temperature > -30 || temporaryTemperature.temperature < 90 )
	{
		tempC = temporaryTemperature;
		// Clear the consecutive bad count.
		consecutiveBadTemp = 0;
	}
	else
		consecutiveBadTemp++;

	// Define the valid humidity range for this sensor.
	if( temporaryHumidity.relative_humidity >= 0 || temporaryHumidity.relative_humidity <= 100 )
	{
		humidity = temporaryHumidity;
		// Clear the consecutive bad count.
		consecutiveBadHumidity = 0;
	}
	else
		consecutiveBadHumidity++;
} // End of readTelemetry() function.


/*
 * publishTelemetry() will publish the sensor and device data over MQTT.
 */
void publishTelemetry()
{
	// Print the signal strength:
	long rssi = WiFi.RSSI();
	Serial.print( "WiFi RSSI: " );
	Serial.println( rssi );
	float tempF = ( tempC.temperature * 9 / 5 ) + 32;
	// Prepare a String to hold the JSON.
	char mqttString[512];
	// Write the readings to the String in JSON format.
	snprintf( mqttString, 512, "{\n\t\"sketch\": \"%s\",\n\t\"mac\": \"%s\",\n\t\"ip\": \"%s\",\n\t\"tempC\": %.2f,\n\t\"tempF\": %.2f,\n\t\"humidity\": %.2f,\n\t\"rssi\": %ld,\n\t\"loopCount\": %d,\n\t\"notes\": \"%s\"\n}", sketchName, macAddress, ipAddress, tempC.temperature, tempF, humidity.relative_humidity, rssi, loopCount, notes );
	// Publish the JSON to the MQTT broker.
	bool success = mqttClient.publish( mqttTopic, mqttString, false );
	if( success )
	{
		Serial.println( "Succsefully published to:" );
		char buffer[20];
		// New format: <location>/<device>/<sensor>/<metric>
		if( mqttClient.publish( sketchTopic, sketchName, false ) )
			Serial.println( sketchTopic );
		if( mqttClient.publish( macTopic, macAddress, false ) )
			Serial.println( macTopic );
		if( mqttClient.publish( ipTopic, ipAddress, false ) )
			Serial.println( ipTopic );
		if( mqttClient.publish( rssiTopic, ltoa( rssi, buffer, 10 ), false ) )
			Serial.println( rssiTopic );
		if( mqttClient.publish( loopCountTopic, ltoa( loopCount, buffer, 10 ), false ) )
			Serial.println( loopCountTopic );
		if( mqttClient.publish( notesTopic, notes, false ) )
			Serial.println( notesTopic );
		dtostrf( tempC.temperature, 1, 3, buffer );
		if( mqttClient.publish( tempCTopic, buffer, false ) )
			Serial.println( tempCTopic );
		dtostrf( ( tempF ), 1, 3, buffer );
		if( mqttClient.publish( tempFTopic, buffer, false ) )
			Serial.println( tempFTopic );
		if( mqttClient.publish( humidityTopic, ltoa( humidity.relative_humidity, buffer, 10 ), false ) )
			Serial.println( humidityTopic );
		if( consecutiveBadTemp > 1 )
		{
			Serial.print( "\n\n\n\n" );
			Serial.print( consecutiveBadTemp );
			Serial.println( " consecutive bad temperature readings!\nResetting the device!\n\n" );
			ESP.restart();		// Reset the device.
		}
		if( consecutiveBadHumidity > 1 )
		{
			Serial.print( "\n\n\n\n" );
			Serial.print( consecutiveBadHumidity );
			Serial.println( " consecutive bad humidity readings!\nResetting the device!\n\n" );
			ESP.restart();		// Reset the device.
		}

		Serial.print( "Successfully published to '" );
		Serial.print( mqttTopic );
		Serial.println( "', this JSON:" );
	}
	else
		Serial.println( "MQTT publish failed!  Attempted to publish this JSON to the broker:" );
	// Print the JSON to the Serial port.
	Serial.println( mqttString );
	lastPublishTime = millis();
} // End of publishTelemetry() function.


void loop()
{
	// Check the mqttClient connection state.
	if( !mqttClient.connected() )
		mqttConnect( 10 );
	// The loop() function facilitates the receiving of messages and maintains the connection to the broker.
	mqttClient.loop();

	ArduinoOTA.handle();
	yield();

	unsigned long time = millis();
	if( lastPollTime == 0 || ( ( time > sensorPollDelay ) && ( time - sensorPollDelay ) > lastPollTime ) )
	{
		readTelemetry();
		Serial.printf( "Temperature: %.2f C\n", tempC.temperature );
		float tempF = ( tempC.temperature * 9 / 5 ) + 32;
		Serial.printf( "Temperature: %.2f F\n", tempF );
		Serial.printf( "Humidity: %.2f %%\n", humidity.relative_humidity );
		lastPollTime = millis();
		Serial.printf( "Next telemetry poll in %ul seconds\n\n", sensorPollDelay / 1000 );
	}

	time = millis();
	if( ( time > publishDelay ) && ( time - publishDelay ) > lastPublish )
	{
		loopCount++;
		Serial.println();
		Serial.println( sketchName );
		Serial.println( __FILE__ );

		// Populate tempC and humidity objects with fresh data.
		readTelemetry();

		Serial.printf( "Temperature: %.2f C\n", tempC.temperature );
		float tempF = ( tempC.temperature * 9 / 5 ) + 32;
		Serial.printf( "Temperature: %.2f F\n", tempF );
		Serial.printf( "Humidity: %.2f %%\n", humidity.relative_humidity );

		publishTelemetry();

		Serial.printf( "loopCount: %ul\n", loopCount );

		lastPublish = millis();
		Serial.printf( "Next MQTT publish in %ul seconds.\n\n", publishDelay / 1000 );
	}
} // End of loop() function.
