cmake_minimum_required( VERSION 3.23 )
project( QTPy_ESP32_SHT40_OTA )

set( CMAKE_CXX_STANDARD 14 )

include_directories( . )

add_executable( QTPy_ESP32_SHT40_OTA
		privateInfo.h
		QTPy_ESP32_SHT40_OTA.h
		QTPy_ESP32_SHT40_OTA.ino
		NetworkFunctions.h
		NetworkFunctions.ino )

set_target_properties( QTPy_ESP32_SHT40_OTA PROPERTIES LINKER_LANGUAGE CXX )
