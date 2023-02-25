/**
 * @brief The purpose of this file is to hold network-related functions which are device-agnostic.
 * This is not realistic because of the presence of onReceiveCallback.
 * Ideally, this file could be used by an ESP32, ESP8266, or similar boards.
 * Because memory capacity varies wildly from device to device, buffer sizes are declared as variables in the entry-point file.
 */
#include "NetworkFunctions.h"

/*
 * onReceiveCallback() is a callback function to process MQTT subscriptions.
 * When a message comes in on a topic the MQTT client has subscribed to, this message is called.
 */
void onReceiveCallback( char *topic, byte *payload, unsigned int length )
{
	if( length > 0 )
	{
		callbackCount++;
		// Create a document named callbackJsonDoc to hold the callback payload.
		StaticJsonDocument<JSON_DOC_SIZE> callbackJsonDoc;
		// Deserialize payload into callbackJsonDoc.
		deserializeJson( callbackJsonDoc, payload, length );

		// The command can be: publishTelemetry, changeTelemetryInterval, or publishStatus.
		const char *command = callbackJsonDoc["command"];
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
			unsigned long tempValue = callbackJsonDoc["value"];
			// Only update the value if it is greater than 4 seconds.  This prevents a seconds vs. milliseconds mix-up.
			if( tempValue > 4000 )
				publishInterval = tempValue;
			Serial.print( "MQTT publish interval has been updated to " );
			Serial.println( publishInterval );
			lastPublishTime = 0;
		}
		else if( strcmp( command, "publishStatus" ) == 0 )
		{
			Serial.println( "publishStatus is not yet implemented." );
		}
		else
		{
			Serial.print( "Unknown command: " );
			Serial.println( command );
		}
	}
} // End of onReceiveCallback() function.


/**
 * @brief configureOTA() will configure and initiate Over The Air (OTA) updates for this device.
 *
 */
void configureOTA()
{
	Serial.println( "Configuring OTA." );

#ifdef ESP8266
	// The ESP8266 hostName defaults to esp8266-[ChipID]
	// The ESP8266 port defaults to 8266
	// ArduinoOTA.setPort( 8266 );
	// Authentication is disabled by default.
	 ArduinoOTA.setPassword( otaPass );
#else
	// The ESP32 hostName defaults to esp32-[MAC]
	// The ESP32 port defaults to 3232
	// ArduinoOTA.setPort( 3232 );
	// Authentication is disabled by default.
	 ArduinoOTA.setPassword( otaPass );
	// Password can be set with it's md5 value as well
	// MD5( admin ) = 21232f297a57a5a743894a0e4a801fc3
	// ArduinoOTA.setPasswordHash( "21232f297a57a5a743894a0e4a801fc3" );
#endif
	ArduinoOTA.setHostname( hostName );

	Serial.printf( "Using hostName '%s'\n", hostName );

	String type = "filesystem"; // SPIFFS
	if( ArduinoOTA.getCommand() == U_FLASH )
		type = "sketch";

	// Configure the OTA callbacks.
	ArduinoOTA.onStart( []()
							  {
		String type = "flash";	// U_FLASH
		if( ArduinoOTA.getCommand() == U_SPIFFS )
			type = "filesystem";
		// NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
		Serial.printf( "OTA is updating the %s\n", type ); } );
	ArduinoOTA.onEnd( []()
							{ Serial.println( "\nTerminating OTA communication." ); } );
	ArduinoOTA.onProgress( []( unsigned int progress, unsigned int total )
								  { Serial.printf( "OTA progress: %u%%\r", ( progress / ( total / 100 ) ) ); } );
	ArduinoOTA.onError( []( ota_error_t error )
							  {
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


/**
 * @brief checkForSSID() will scan for all visible SSIDs, see if any match 'ssidName',
 * and return a count of how many matches were found.
 *
 * @param ssidName the SSID name to search for.
 * @return int the count of SSIDs which match the passed parameter.
 */
int checkForSSID( const char *ssidName )
{
	int ssidCount = 0;
	byte networkCount = WiFi.scanNetworks();
	if( networkCount == 0 )
		Serial.println( "No WiFi SSIDs are in range!" );
	else
	{
		//      Serial.printf( "WiFi SSIDs in range: %d\n", networkCount );
		for( int i = 0; i < networkCount; ++i )
		{
			// Check to see if this SSID matches the parameter.
			if( strcmp( ssidName, WiFi.SSID( i ).c_str() ) == 0 )
				ssidCount++;
		}
	}
	return ssidCount;
} // End of checkForSSID() function.


/**
 * @brief wifiConnect() will attempt to connect to a single SSID.
 */
bool wifiConnect( const char *ssid, const char *password )
{
	wifiConnectCount++;

	Serial.printf( "Attempting to connect to Wi-Fi SSID '%s'", ssid );
	WiFi.mode( WIFI_STA );
	//   WiFi.setHostname( hostName );
	WiFi.begin( ssid, password );

	unsigned long wifiConnectionStartTime = millis();

	// Loop until connected, or until wifiConnectionTimeout.
	while( WiFi.status() != WL_CONNECTED && ( millis() - wifiConnectionStartTime < wifiConnectionTimeout ) )
	{
		Serial.print( "." );
		delay( 1000 );
	}
	Serial.println( "" );

	if( WiFi.status() == WL_CONNECTED )
	{
		// Print that Wi-Fi has connected.
		Serial.println( "Wi-Fi connection established!" );
		snprintf( ipAddress, 16, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3] );
		// Set the LED color to green to show that all is well in Zion.
		pixels.fill( GREEN );
		pixels.show();
		return true;
	}
	Serial.println( "Wi-Fi failed to connect in the timeout period.\n" );
	return false;
} // End of the wifiConnect() function.


/**
 * @brief wifiMultiConnect() will iterate through the SSIDs in 'wifiSsidList[]', and then use checkForSSID() determine which are in range.
 * When a SSID is in range, wifiConnect() will be called with that SSID and password.
 */
void wifiMultiConnect()
{
	long time = millis();
	if( lastWifiConnectTime == 0 || ( time > wifiCoolDownInterval && ( time - wifiCoolDownInterval ) > lastWifiConnectTime ) )
	{
		Serial.println( "\nEntering wifiMultiConnect()" );
		// Set the LED color to yellow to show that Wi-Fi is connecting.
		pixels.fill( YELLOW );
		pixels.show();

		for( size_t networkArrayIndex = 0; networkArrayIndex < sizeof( wifiSsidArray ); networkArrayIndex++ )
		{
			// Get the details for this connection attempt.
			const char *wifiSsid = wifiSsidArray[networkArrayIndex];
			const char *wifiPassword = wifiPassArray[networkArrayIndex];

			// Announce the Wi-Fi parameters for this connection attempt.
			Serial.print( "Attempting to connect to to SSID \"" );
			Serial.print( wifiSsid );
			Serial.println( "\"" );

			// Don't even try to connect if the SSID cannot be found.
			int ssidCount = checkForSSID( wifiSsid );
			if( ssidCount > 0 )
			{
				// This is useful for detecting multiples APs.
				if( ssidCount > 1 )
					Serial.printf( "Found %d SSIDs matching '%s'.\n", ssidCount, wifiSsid );

				// If the Wi-Fi connection is successful, set the mqttClient broker parameters.
				if( wifiConnect( wifiSsid, wifiPassword ) )
				{
					mqttClient.setServer( mqttBrokerArray[networkArrayIndex], mqttPortArray[networkArrayIndex] );
					return;
				}
			}
			else
				Serial.printf( "Network '%s' was not found!\n\n", wifiSsid );
		}
		Serial.println( "Exiting wifiMultiConnect()\n" );
		lastWifiConnectTime = millis();
	}
} // End of wifiMultiConnect() function.


/*
 * mqttMultiConnect() will:
 * 1. Check the WiFi connection, and reconnect WiFi as needed.
 * 2. Attempt to connect the MQTT client designated in 'mqttBrokerArray[networkIndex]' up to 'maxAttempts' number of times.
 * 3. Subscribe to the topic defined in 'commandTopic'.
 * If the broker connection cannot be made, an error will be printed to the serial port.
 */
bool mqttMultiConnect( int maxAttempts )
{
	Serial.println( "\nFunction mqttMultiConnect() has initiated." );
	if( WiFi.status() != WL_CONNECTED )
		wifiMultiConnect();

	mqttClient.setCallback( onReceiveCallback );

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
			Serial.println( " connected." );
			if( !mqttClient.setBufferSize( JSON_DOC_SIZE ) )
			{
				Serial.printf( "Unable to create a buffer %d bytes long!\n", JSON_DOC_SIZE );
				Serial.println( "Restarting the device!" );
				ESP.restart();
			}
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

			Serial.printf( "Trying again in %lu seconds.\n\n", mqttReconnectInterval / 1000 );
			delay( mqttReconnectInterval );
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
