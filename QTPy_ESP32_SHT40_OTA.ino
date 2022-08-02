/**
 * @brief This sketch will use a HTU21D (SHT20/SHT21 compatible) sensor to measure temperature and humidity.
 * The ESP-32 SDA pin is GPIO21, and SCL is GPIO22.
 *
 * My topic formats are:
 *    <location>/<device>/<device reading>
 *    <location>/<device>/<sensor type>/<sensor reading>
 *
 * @copyright   Copyright © 2022 Adam Howell
 * @license     The MIT License (MIT)
 */
#ifdef ESP8266
// These headers are installed when the ESP8266 is installed in board manager.
#include <ESP8266WiFi.h>			// ESP8266 WiFi support.  https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266WiFi
#include <ESP8266mDNS.h>			// OTA - mDNSResponder (Multicast DNS) for the ESP8266 family.
#elif ESP32
// These headers are installed when the ESP32 is installed in board manager.
#include <WiFi.h>						// ESP32 Wifi support.  https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFi/src/WiFi.h
#include <ESPmDNS.h>					// OTA - Multicast DNS for the ESP32.
#else
#include <WiFi.h>						// Arduino Wifi support.  This header is part of the standard library.  https://www.arduino.cc/en/Reference/WiFi
#endif
#include <Wire.h>						// This header is part of the standard library.  https://www.arduino.cc/en/reference/wire
#include <PubSubClient.h>			// PubSub is the MQTT API.  Author: Nick O'Leary  https://github.com/knolleary/pubsubclient
#include <ArduinoJson.h>			// The JSON parsing library used.  Author: Benoît Blanchon  https://arduinojson.org/
#include <WiFiUdp.h>					// OTA
#include <ArduinoOTA.h>				// OTA - The Arduino OTA library.  Specific version of this are installed along with specific boards in board manager.
#include <Adafruit_SHT4x.h>		// Adafruit library for the SHT4x series, such as the SHT40.  https://github.com/adafruit/Adafruit_SHT4X, requires Adafruit_Sensor: https://github.com/adafruit/Adafruit_Sensor
#include <Adafruit_NeoPixel.h>	// The Adafruit NeoPixel library to drive the RGB LED on the QT Py.	https://github.com/adafruit/Adafruit_NeoPixel
#include "privateInfo.h"			// I use this file to hide my network information from random people browsing my GitHub repo.


// NeoPixel related values.
#define NUMPIXELS 1
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


/*
 * Declare network variables.
 * Adjust the commented-out variables to match your network and broker settings.
 * The commented-out variables are stored in "privateInfo.h", which I do not upload to GitHub.
 */
// const char * wifiSsidArray[4] = { "Network1", "Network2", "Network3", "Syrinx" };			// Typically declared in "privateInfo.h".
// const char * wifiPassArray[4] = { "Password1", "Password2", "Password3", "By-Tor" };		// Typically declared in "privateInfo.h".
// const char * mqttBrokerArray[4] = { "Broker1", "Broker2", "Broker3", "192.168.0.2" };		// Typically declared in "privateInfo.h".
// int const mqttPortArray[4] = { 1883, 1883, 1883, 2112 };												// Typically declared in "privateInfo.h".

const char * notes = "office/QTPy/ Adafruit QT Py ESP32-S2 with SHT40 and OTA";
const char * hostname = "office-qtpy-sht40";						// The network hostname for this device.  Used by OTA and general networking.
const char * commandTopic = "office/QTPy/command";				// The topic used to subscribe to update commands.  Commands: publishTelemetry, changeTelemetryInterval, publishStatus.
const char * sketchTopic = "office/QTPy/sketch";					// The topic used to publish the sketch name (__FILE__).
const char * macTopic = "office/QTPy/mac";							// The topic used to publish the MAC address.
const char * ipTopic = "office/QTPy/ip";							// The topic used to publish the IP address.
const char * rssiTopic = "office/QTPy/rssi";						// The topic used to publish the WiFi Received Signal Strength Indicator.
const char * publishCountTopic = "office/QTPy/publishCount"; // The topic used to publish the loop count.
const char * notesTopic = "office/QTPy/notes";					// The topic used to publish notes relevant to this project.
const char * tempCTopic = "office/QTPy/sht40/tempC";			// The topic used to publish the temperature in Celsius.
const char * tempFTopic = "office/QTPy/sht40/tempF";			// The topic used to publish the temperature in Fahrenheit.
const char * humidityTopic = "office/QTPy/sht40/humidity";	// The topic used to publish the humidity.
const char * mqttStatsTopic = "espStats";							// The topic this device will publish to upon connection to the broker.
const char * mqttTopic = "espWeather";								// The topic used to publish a single JSON message containing all data.
const int JSON_DOC_SIZE = 512;										// The ArduinoJson document size.
const int LED_PIN = 2;													// The blue LED on the Freenove devkit.
unsigned int consecutiveBadTemp = 0;								// Holds the current number of consecutive invalid temperature readings.
unsigned int consecutiveBadHumidity = 0;							// Holds the current number of consecutive invalid humidity readings.
unsigned int networkIndex = 2112;									// Holds the correct index for the network arrays: wifiSsidArray[], wifiPassArray[], mqttBrokerArray[], and mqttPortArray[].
unsigned long publishInterval = 60000;								// The delay in milliseconds between MQTT publishes.  This prevents "flooding" the broker.
unsigned long telemetryInterval = 10000;							// The delay in milliseconds between polls of the sensor.  This should be greater than 100 milliseconds.
unsigned long lastPublishTime = 0;									// The time since last MQTT publish.
unsigned long lastPollTime = 0;										// The time since last sensor poll.
unsigned long publishCount = 0;										// A count of how many publishes have taken place.
unsigned long wifiConnectionTimeout = 10000;						// The maximum amount of time in milliseconds to wait for a WiFi connection before trying a different SSID.
unsigned long mqttReconnectDelay = 5000;							// How long in milliseconds to delay between MQTT broker connection attempts.
float tempC;																// A global to hold the temperature in Celsius.
float tempF;																// A global to hold the temperature in Fahrenheit.
float humidity;															// A global to hold the relative humidity reading.
long rssi;																	// A global to hold the Received Signal Strength Indicator.
char macAddress[18];														// The MAC address of the WiFi NIC.
char ipAddress[16];														// The IP address given to the device.
char * activeWifiSsid;													// The WiFi SSID currently connected to.
char * activeMqttBroker;													// The MQTT broker currently connected to.
unsigned int activeMqttPort;											// The MQTT port currently in use.


// Create class objects.
WiFiClient espClient;					  // Network client.
PubSubClient mqttClient( espClient ); // MQTT client.
Adafruit_NeoPixel pixels( NUMPIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800 );
Adafruit_SHT4x sht40 = Adafruit_SHT4x(); // SHT40 class object to read temperature and humidity from.
sensors_event_t humidity_sensor;
sensors_event_t temperature_sensor;


void onReceiveCallback( char *topic, byte *payload, unsigned int length )
{
	char str[length + 1];
	Serial.print( "Message arrived [" );
	Serial.print( topic );
	Serial.print( "] " );
	int i = 0;
	for( i = 0; i < length; i++ )
	{
		Serial.print( ( char )payload[i] );
		str[i] = ( char )payload[i];
	}
	Serial.println();
	// Add the null terminator.
	str[i] = 0;
	StaticJsonDocument<JSON_DOC_SIZE> doc;
	deserializeJson( doc, str );

	// The command can be: publishTelemetry, changeTelemetryInterval, or publishStatus.
	const char *command = doc["command"];
	if( strcmp( command, "publishTelemetry" ) == 0 )
	{
		Serial.println( "Reading and publishing sensor values." );
		// Poll the sensor.
		readTelemetry();
		// Publish the sensor readings.
		publishTelemetry();
		Serial.println( "Readings have been published." );
	}
	else if( strcmp( command, "changeTelemetryInterval" ) == 0 )
	{
		Serial.println( "Changing the publish interval." );
		unsigned long tempValue = doc["value"];
		// Only update the value if it is greater than 4 seconds.  This prevents a seconds vs. milliseconds mixup.
		if( tempValue > 4000 )
			publishInterval = tempValue;
		Serial.printf( "MQTT publish interval has been updated to %lu", publishInterval );
		lastPublishTime = 0;
	}
	else if( strcmp( command, "publishStatus" ) == 0 )
		Serial.println( "publishStatus is not yet implemented." );
	else
		Serial.printf( "Unknown command: %s\n", command );
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
	Serial.println( "\nSetup is initializing hardware and configuring software." );
	Serial.println( "Using the Stemma QT port for I2C." );
	Wire.setPins( SDA1, SCL1 ); // This is what selects the Stemma QT port, otherwise the two pin headers will be I2C.
	Wire.begin();

	Serial.println( __FILE__ );

	// Set up the sensor.
	setupSHT40();

	// Set the ipAddress char array to a default value.
	snprintf( ipAddress, 16, "127.0.0.1" );

	// Get the MAC address and store it in macAddress.
	snprintf( macAddress, 18, "%s", WiFi.macAddress().c_str() );

	// Disable Bluetooth.
	btStop();

	Serial.println( "Connecting WiFi..." );
	// Set the LED color to yellow.
	pixels.fill( YELLOW );
	pixels.show();
	wifiMultiConnect();

	// The networkIndex variable is initialized to 2112.  If it is still 2112 at this point, then WiFi failed to connect.
	if( networkIndex != 2112 )
	{
		// Set the MQTT client parameters.
		mqttClient.setServer( mqttBrokerArray[networkIndex], mqttPortArray[networkIndex] );
		// Assign the onReceiveCallback() function to handle MQTT callbacks.
		mqttClient.setCallback( onReceiveCallback );
		Serial.printf( "Using MQTT broker: %s\n", mqttBrokerArray[networkIndex] );
		Serial.printf( "Using MQTT port: %d\n", mqttPortArray[networkIndex] );
	}

	configureOTA();

	snprintf( ipAddress, 16, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3] );
	Serial.printf( "IP address: %s\n", ipAddress );

	// Set the LED color to green.
	pixels.fill( GREEN );
	pixels.show();

	Serial.println( "Setup is complete!" );
} // End of setup() function.


/**
 * @brief configureOTA() will configure and initiate Over The Air (OTA) updates for this device.
 *
 */
void configureOTA()
{
	Serial.println( "Configuring OTA." );

#ifdef ESP8266
	// The ESP8266 port defaults to 8266
	// ArduinoOTA.setPort( 8266 );
	// The ESP8266 hostname defaults to esp8266-[ChipID]
	ArduinoOTA.setHostname( hostname );
	// Authentication is disabled by default.
	// ArduinoOTA.setPassword( ( const char * )"admin" );
#elif ESP32
	// The ESP32 port defaults to 3232
	// ArduinoOTA.setPort( 3232 );
	// The ESP32 hostname defaults to esp32-[MAC]
	ArduinoOTA.setHostname( hostname );
	// Authentication is disabled by default.
	// ArduinoOTA.setPassword( "admin" );
	// Password can be set with it's md5 value as well
	// MD5( admin ) = 21232f297a57a5a743894a0e4a801fc3
	// ArduinoOTA.setPasswordHash( "21232f297a57a5a743894a0e4a801fc3" );
#else
	// ToDo: Verify how stock Arduino code is meant to handle the port, username, and password.
	ArduinoOTA.setHostname( hostname );
#endif

	Serial.printf( "Using hostname '%s'\n", hostname );

	String type = "filesystem";	// SPIFFS
	if( ArduinoOTA.getCommand() == U_FLASH )
		type = "sketch";

	// Configure the OTA callbacks.
	ArduinoOTA.onStart( []()
	{
		String type = "flash";	// U_FLASH
		if( ArduinoOTA.getCommand() == U_SPIFFS )
			type = "filesystem";
		// NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
		Serial.printf( "OTA is updating the %s\n", type );
	} );
	ArduinoOTA.onEnd( []() { Serial.println( "\nTerminating OTA communication." ); } );
	ArduinoOTA.onProgress( []( unsigned int progress, unsigned int total ){ Serial.printf( "OTA progress: %u%%\r", ( progress / ( total / 100 ) ) ); } );
	ArduinoOTA.onError( []( ota_error_t error ){
		Serial.printf( "Error[%u]: ", error );
		if( error == OTA_AUTH_ERROR ) Serial.println( "OTA authentication failed!" );
		else if( error == OTA_BEGIN_ERROR ) Serial.println( "OTA transmission failed to initiate properly!" );
		else if( error == OTA_CONNECT_ERROR ) Serial.println( "OTA connection failed!" );
		else if( error == OTA_RECEIVE_ERROR ) Serial.println( "OTA client was unable to properly receive data!" );
		else if( error == OTA_END_ERROR ) Serial.println( "OTA transmission failed to terminate properly!" ); } );

	// Start listening for OTA commands.
	ArduinoOTA.begin();

	Serial.println( "OTA is configured and ready." );
} // End of the configureOTA() function.


void setupSHT40()
{
	int retryCountdown = 5;
	Serial.println( "Initializing the SHT40 using Adafruit's SHT4x library..." );
	// Set the LED color to yellow.
	pixels.fill( YELLOW );
	pixels.show();

	while( !sht40.begin() )
	{
		if( retryCountdown > 0 )
		{
			Serial.println( "Couldn't find a SHT4x!" );
			Serial.println( "Retrying in 5 seconds." );
			retryCountdown--;
			delay( 5000 );
		}
		else
		{
			Serial.println( "Unable to find the sensor in 5 attempts, restarting the device!" );
			ESP.restart();
		}
	}
	Serial.println( "Found SHT4x sensor" );
	Serial.printf( "Serial number 0x%X\n", sht40.readSerial() );

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
			Serial.println( "No heater" );
			break;
		case SHT4X_HIGH_HEATER_1S:
			Serial.println( "High heat for 1 second" );
			break;
		case SHT4X_HIGH_HEATER_100MS:
			Serial.println( "High heat for 0.1 second" );
			break;
		case SHT4X_MED_HEATER_1S:
			Serial.println( "Medium heat for 1 second" );
			break;
		case SHT4X_MED_HEATER_100MS:
			Serial.println( "Medium heat for 0.1 second" );
			break;
		case SHT4X_LOW_HEATER_1S:
			Serial.println( "Low heat for 1 second" );
			break;
		case SHT4X_LOW_HEATER_100MS:
			Serial.println( "Low heat for 0.1 second" );
			break;
	}
	// Set the LED color to green.
	pixels.fill( GREEN );
	pixels.show();
	Serial.println( "The SHT40 has been configured." );
} // End of setupSHT40 function.


/*
 * wifiMultiConnect() will iterate through 'wifiSsidArray[]', attempting to connect with the password stored at the same index in 'wifiPassArray[]'.
 *
 */
void wifiMultiConnect()
{
	digitalWrite( LED_PIN, HIGH ); // Turn the LED off to show a connection is being made.

	Serial.println( "\nEntering wifiMultiConnect()" );
	for( size_t networkArrayIndex = 0; networkArrayIndex < sizeof( wifiSsidArray ); networkArrayIndex++ )
	{
		// Get the details for this connection attempt.
		const char *wifiSsid = wifiSsidArray[networkArrayIndex];
		const char *wifiPassword = wifiPassArray[networkArrayIndex];

		// Announce the WiFi parameters for this connection attempt.
		Serial.print( "Attempting to connect to SSID \"" );
		Serial.print( wifiSsid );
		Serial.println( "\"" );

		// Don't even try to connect if the SSID cannot be found.
		if( checkForSSID( wifiSsid ) )
		{
			// Attempt to connect to this WiFi network.
			Serial.printf( "Wi-Fi mode set to WIFI_STA %s\n", WiFi.mode( WIFI_STA ) ? "" : "Failed!" );
			WiFi.begin( wifiSsid, wifiPassword );

			unsigned long wifiConnectionStartTime = millis();
			Serial.printf( "Waiting up to %lu seconds for a connection.\n", wifiConnectionTimeout / 1000 );
			/*
			WiFi.status() return values:
			WL_IDLE_STATUS      = 0,
			WL_NO_SSID_AVAIL    = 1,
			WL_SCAN_COMPLETED   = 2,
			WL_CONNECTED        = 3,
			WL_CONNECT_FAILED   = 4,
			WL_CONNECTION_LOST  = 5,
			WL_WRONG_PASSWORD   = 6,
			WL_DISCONNECTED     = 7
			*/
			while( WiFi.status() != WL_CONNECTED && ( millis() - wifiConnectionStartTime < wifiConnectionTimeout ) )
			{
				Serial.print( "." );
				delay( 1000 );
			}
			Serial.println( "" );

			if( WiFi.status() == WL_CONNECTED )
			{
				digitalWrite( LED_PIN, LOW ); // Turn the LED on to show the connection was successful.
				Serial.print( "IP address: " );
				snprintf( ipAddress, 16, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3] );
				Serial.println( ipAddress );
				networkIndex = networkArrayIndex;
				// Print that WiFi has connected.
				Serial.println( "\nWiFi connection established!" );
				return;
			}
			else
				Serial.println( "Unable to connect to WiFi!" );
		}
		else
			Serial.println( "That network was not found!" );
	}
	Serial.println( "Exiting wifiMultiConnect()\n" );
} // End of wifiMultiConnect() function.


/*
 * checkForSSID() is used by wifiMultiConnect() to avoid attempting to connect to SSIDs which are not in range.
 * Returns 1 if 'ssidName' can be found.
 * Returns 0 if 'ssidName' cannot be found.
 */
int checkForSSID( const char *ssidName )
{
	byte networkCount = WiFi.scanNetworks();
	if( networkCount == 0 )
		Serial.println( "No WiFi SSIDs are in range!" );
	else
	{
		Serial.printf( "%d networks found.\n", networkCount );
		for( int i = 0; i < networkCount; ++i )
		{
			// Check to see if this SSID matches the parameter.
			if( strcmp( ssidName, WiFi.SSID( i ).c_str() ) == 0 )
				return 1;
			// Alternately, the String compareTo() function can be used like this: if( WiFi.SSID( i ).compareTo( ssidName ) == 0 )
		}
	}
	Serial.printf( "SSID '%s' was not found!\n", ssidName );
	return 0;
} // End of checkForSSID() function.


/*
 * mqttMultiConnect() will:
 * 1. Check the WiFi connection, and reconnect WiFi as needed.
 * 2. Attempt to connect the MQTT client designated in 'mqttBrokerArray[networkIndex]' up to 'maxAttempts' number of times.
 * 3. Subscribe to the topic defined in 'commandTopic'.
 * If the broker connection cannot be made, an error will be printed to the serial port.
 */
bool mqttMultiConnect( int maxAttempts )
{
	Serial.println( "Function mqttMultiConnect() has initiated.\n" );
	if( WiFi.status() != WL_CONNECTED )
		wifiMultiConnect();

	digitalWrite( LED_PIN, HIGH ); // Turn the LED off to show a connection is being made.

	/*
	 * The networkIndex variable is initialized to 2112.
	 * If it is still 2112 at this point, then WiFi failed to connect.
	 * This is only needed to display the name and port of the broker being used.
	 */
	if( networkIndex != 2112 )
		Serial.printf( "Attempting to connect to the MQTT broker at '%s:%d' up to %d times.\n", mqttBrokerArray[networkIndex], mqttPortArray[networkIndex], maxAttempts );
	else
		Serial.printf( "Attempting to connect to the MQTT broker up to %d times.\n", maxAttempts );


	int attemptNumber = 0;
	// Loop until MQTT has connected.
	while( !mqttClient.connected() && attemptNumber < maxAttempts )
	{
		// Put the macAddress and random number into clientId.
		char clientId[22];
		snprintf( clientId, 22, "%s-%03ld", macAddress, random( 999 ) );
		// Connect to the broker using the MAC address for a clientID.  This guarantees that the clientID is unique.
		Serial.printf( "Connecting with client ID '%s'.\n", clientId );
		Serial.printf( "Attempt # %d....", ( attemptNumber + 1 ) );
		if( mqttClient.connect( clientId ) )
		{
			digitalWrite( LED_PIN, LOW ); // Turn the LED on to show the connection was successful.
			Serial.println( " connected." );
			if( !mqttClient.setBufferSize( JSON_DOC_SIZE ) )
			{
				Serial.printf( "Unable to create a buffer %d bytes long!\n", JSON_DOC_SIZE );
				Serial.println( "Restarting the device!" );
				ESP.restart();
			}
			publishStats();
			// Subscribe to the command topic.
			if( mqttClient.subscribe( commandTopic ) )
				Serial.printf( "Successfully subscribed to topic '%s'.\n", commandTopic );
			else
				Serial.printf( "Failed to subscribe to topic '%s'!\n", commandTopic );
		}
		else
		{
			int mqttState = mqttClient.state();
			/*
				Possible values for client.state():
				MQTT_CONNECTION_TIMEOUT     -4		// Note: This also comes up when the clientID is already in use.
				MQTT_CONNECTION_LOST        -3
				MQTT_CONNECT_FAILED         -2
				MQTT_DISCONNECTED           -1
				MQTT_CONNECTED               0
				MQTT_CONNECT_BAD_PROTOCOL    1
				MQTT_CONNECT_BAD_CLIENT_ID   2
				MQTT_CONNECT_UNAVAILABLE     3
				MQTT_CONNECT_BAD_CREDENTIALS 4
				MQTT_CONNECT_UNAUTHORIZED    5
			*/
			Serial.printf( " failed!  Return code: %d", mqttState );
			if( mqttState == -4 )
				Serial.println( " - MQTT_CONNECTION_TIMEOUT" );
			else if( mqttState == 2 )
				Serial.println( " - MQTT_CONNECT_BAD_CLIENT_ID" );
			else
				Serial.println( "" );

			Serial.printf( "Trying again in %lu seconds.\n\n", mqttReconnectDelay / 1000 );
			delay( mqttReconnectDelay );
		}
		attemptNumber++;
	}

	if( !mqttClient.connected() )
	{
		Serial.println( "Unable to connect to the MQTT broker!" );
		return false;
	}

	Serial.println( "Function mqttMultiConnect() has completed.\n" );
	return true;
} // End of mqttMultiConnect() function.


/*
 * readTelemetry() will:
 * 1. read from all available sensors
 * 2. store legitimate values in global variables
 * 3. increment a counter if any value is invalid
 */
void readTelemetry()
{
	rssi = WiFi.RSSI();
	sensors_event_t temporaryHumidity;
	sensors_event_t temporaryTemperature;
	// Populate temp and humidity objects with fresh data.
	sht40.getEvent( &temporaryHumidity, &temporaryTemperature );

	// Define the valid temperature range (in Celsius) for this sensor.
	if( temporaryTemperature.temperature > -30 || temporaryTemperature.temperature < 90 )
	{
		tempC = temporaryTemperature.temperature;
		tempF = ( tempC * 9 / 5 ) + 32;
		// Clear the consecutive bad count.
		consecutiveBadTemp = 0;
	}
	else
	{
		Serial.printf( "Temperature reading of %.2f C is out of bounds and is being marked as invalid!\n", temporaryTemperature.temperature );
		consecutiveBadTemp++;
	}

	// Define the valid humidity range for this sensor.
	if( temporaryHumidity.relative_humidity >= 0 || temporaryHumidity.relative_humidity <= 100 )
	{
		humidity = temporaryHumidity.relative_humidity;
		// Clear the consecutive bad count.
		consecutiveBadHumidity = 0;
	}
	else
	{
		Serial.printf( "Humidity reading of %.2f is out of bounds and is being marked as invalid!\n", temporaryHumidity.relative_humidity );
		consecutiveBadHumidity++;
	}

	if( consecutiveBadTemp > 4 || consecutiveBadHumidity > 4 )
	{
		Serial.println( "\n\n\n\n" );
		Serial.printf( "%u consecutive bad temperature readings!\n", consecutiveBadTemp );
		Serial.printf( "%u consecutive bad humidity readings!\n", consecutiveBadHumidity );
		Serial.println( "Resetting the device!\n\n\n" );
		ESP.restart(); // Reset the device.
	}
} // End of readTelemetry() function.


/*
 * printTelemetry() will print the sensor and device data to the serial port.
 */
void printTelemetry()
{
	Serial.printf( "WiFi SSID: %s\n", wifiSsidArray[networkIndex] );
	Serial.printf( "Broker: %s:%d\n", mqttBrokerArray[networkIndex], mqttPortArray[networkIndex] );
	Serial.printf( "Temperature: %.2f C\n", tempC );
	Serial.printf( "Temperature: %.2f F\n", tempF );
	Serial.printf( "Humidity: %.2f %%\n", humidity );
	Serial.printf( "WiFi RSSI: %ld\n", rssi );
} // End of printTelemetry() function.


/*
 * printUptime() will print the uptime to the serial port.
 */
void printUptime()
{
	Serial.print( "Uptime in " );
	long seconds = ( millis() - bootTime ) / 1000;
	long minutes = seconds / 60;
	long hours = minutes / 60;
	if( seconds < 601 )
	{
		Serial.print( "seconds: " );
		Serial.println( seconds );
	}
	else if( minutes < 121 )
	{
		Serial.print( "minutes: " );
		Serial.println( minutes );
	}
	else
	{
		Serial.print( "hours: " );
		Serial.println( hours );
	}
} // End of printUptime() function.


/*
 * publishStats() is called by mqttMultiConnect() every time the device (re)connects to the broker, and every publishInterval milliseconds thereafter.
 * It is also called by the callback when the "publishStats" command is received.
 */
void publishStats()
{
	char mqttStatsString[JSON_DOC_SIZE];
	// Create a JSON Document on the stack.
	StaticJsonDocument<JSON_DOC_SIZE> doc;
	// Add data: __FILE__, macAddress, ipAddress, rssi, publishCount
	doc["sketch"] = __FILE__;
	doc["mac"] = macAddress;
	doc["ip"] = ipAddress;
	doc["rssi"] = rssi;
	doc["publishCount"] = publishCount;

	// Serialize the JSON into mqttStatsString, with indentation and line breaks.
	serializeJsonPretty( doc, mqttStatsString );

	Serial.printf( "Publishing stats to the '%s' topic.\n", mqttStatsTopic );

	if( mqttClient.connected() )
	{
		if( mqttClient.connected() && mqttClient.publish( mqttStatsTopic, mqttStatsString, false ) )
			Serial.printf( "Published to this broker and port: %s:%d, and to this topic '%s':\n%s\n", mqttBrokerArray[networkIndex], mqttPortArray[networkIndex], mqttStatsTopic, mqttStatsString );
		else
			Serial.println( "\n\nPublish failed!\n\n" );
	}
} // End of publishStats() function.


/*
 * publishTelemetry() will publish the sensor and device data over MQTT.
 */
void publishTelemetry()
{
	// Create a JSON Document on the stack.
	StaticJsonDocument<JSON_DOC_SIZE> doc;
	// Add data: __FILE__, macAddress, ipAddress, tempC, tempF, humidity, rssi, publishCount, notes
	doc["sketch"] = __FILE__;
	doc["mac"] = macAddress;
	doc["ip"] = ipAddress;
	doc["tempC"] = tempC;
	doc["tempF"] = tempF;
	doc["humidity"] = humidity;
	doc["rssi"] = rssi;
	doc["publishCount"] = publishCount;
	doc["notes"] = notes;

	// Prepare a String to hold the JSON.
	char mqttString[JSON_DOC_SIZE];
	// Serialize the JSON into mqttString, with indentation and line breaks.
	serializeJsonPretty( doc, mqttString );
	// Publish the JSON to the MQTT broker.
	bool success = mqttClient.publish( mqttTopic, mqttString, false );
	if( success )
	{
		Serial.println( "Successfully published to:" );
		char buffer[20];
		// New format: <location>/<device>/<sensor>/<metric>
		if( mqttClient.publish( sketchTopic, __FILE__, false ) )
			Serial.printf( "  %s\n", sketchTopic );
		if( mqttClient.publish( macTopic, macAddress, false ) )
			Serial.printf( "  %s\n", macTopic );
		if( mqttClient.publish( ipTopic, ipAddress, false ) )
			Serial.printf( "  %s\n", ipTopic );
		ltoa( rssi, buffer, 10 );
		if( mqttClient.publish( rssiTopic, buffer, false ) )
			Serial.printf( "  %s\n", rssiTopic );
		ltoa( publishCount, buffer, 10 );
		if( mqttClient.publish( publishCountTopic, buffer, false ) )
			Serial.printf( "  %s\n", publishCountTopic );
		if( mqttClient.publish( notesTopic, notes, false ) )
			Serial.printf( "  %s\n", notesTopic );
		dtostrf( tempC, 1, 3, buffer );
		if( mqttClient.publish( tempCTopic, buffer, false ) )
			Serial.printf( "  %s\n", tempCTopic );
		dtostrf( ( tempF ), 1, 3, buffer );
		if( mqttClient.publish( tempFTopic, buffer, false ) )
			Serial.printf( "  %s\n", tempFTopic );
		dtostrf( ( humidity ), 1, 3, buffer );
		if( mqttClient.publish( humidityTopic, buffer, false ) )
			Serial.printf( "  %s\n", humidityTopic );

		Serial.printf( "Successfully published to '%s', this JSON:\n", mqttTopic );
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
		mqttMultiConnect( 10 );
	// The loop() function facilitates the receiving of messages and maintains the connection to the broker.
	mqttClient.loop();

	ArduinoOTA.handle();
	yield();

	unsigned long time = millis();
	if( lastPollTime == 0 || ( ( time > telemetryInterval ) && ( time - telemetryInterval ) > lastPollTime ) )
	{
		readTelemetry();
		Serial.printf( "Temperature: %.2f C\n", tempC );
		float tempF = ( tempC * 9 / 5 ) + 32;
		Serial.printf( "Temperature: %.2f F\n", tempF );
		Serial.printf( "Humidity: %.2f %%\n", humidity );
		lastPollTime = millis();
		Serial.printf( "Next telemetry poll in %lu seconds\n\n", telemetryInterval / 1000 );
	}

	time = millis();
	if( ( time > publishInterval ) && ( time - publishInterval ) > lastPublishTime )
	{
		publishCount++;
		Serial.println();
		Serial.println( __FILE__ );

		// Populate tempC and humidity objects with fresh data.
		readTelemetry();

		Serial.printf( "Temperature: %.2f C\n", tempC );
		float tempF = ( tempC * 9 / 5 ) + 32;
		Serial.printf( "Temperature: %.2f F\n", tempF );
		Serial.printf( "Humidity: %.2f %%\n", humidity );

		printUptime();
		printTelemetry();
		publishTelemetry();

		Serial.printf( "publishCount: %lu\n", publishCount );

		lastPublishTime = millis();
		Serial.printf( "Next MQTT publish in %lu seconds.\n\n", publishInterval / 1000 );
	}
} // End of loop() function.
