// Interface functions to retrieve weather data from WU API server

// Ideas for this program are derived from the following eBooks and websites:
// Learn ESP32 with Arduino IDE, Rui & Sara Santos, https://rntlab.com/courses 
// Mastering ArduinoJson, Benoit Blanchon, https://arduinojson.org/book/
// Some code related to the ESP32 Deepsleep mode is under Public Domain License
// by Author: Pranav Cherukupalli, cherukupallip@gmail.com
// 
#include <WiFi.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

#include "OS_WU_WeatherService_V2.h"

// Function to send the HTTP request
static bool sendRequest(Print &client, const char *request,  const char *ID,
															const char *key) {
	client.print("GET https://api.weather.com/");
	client.print(request);
	client.print("?stationId=");
	client.print(ID);
	client.print("&format=json&units=e&apiKey=");
	client.print(key);
	client.print(" HTTP/1.0");
	client.println("Host: api.weather.com");
	client.println("Connection: close");
	return client.println() != 0;
}
// Function to verify the HTTP status
static bool checkResponse(Stream &client) {
	char status[32];
	client.readBytesUntil('\r', status, sizeof(status));
		
	// should be "HTTP/1.0 200 OK"
	return memcmp(status + 9, "200 OK", 6) == 0;
}
// Function to skip all bytes until we are in the beginning of the array
  static bool jumpTostart(Stream &stream, const char *beginningOfarray) {
	return stream.find(beginningOfarray);
}
// Function to skip all bytes until we found a comma of a closing bracket
static bool jumpToNextElement(Stream &stream) {
	char comma[] = ",";
	char endOfArray[] = "]";
	return stream.findUntil(comma, endOfArray);
}
// Function to deserialize the JSON objects			  
static bool deserializeWeatherData(Stream &stream, Weather &w) {
	StaticJsonDocument<2048> doc;
	DeserializationError err = deserializeJson(doc, stream);
	if (err) return false;
	
	strlcpy(w.obsTimeLocal, doc["obsTimeLocal"] | "N/A",
												sizeof(w.obsTimeLocal));
	w.humidityAvg = doc["humidityAvg"];
	w.tempAvg = doc["imperial"]["tempAvg"];
	w.precipRate = doc["imperial"]["precipRate"];
	w.precipTotal = doc["imperial"]["precipTotal"];
	return true;
}
// Function to indicate the reason by which ESP32 has been awaken from sleep
void print_wakeup_reason() {
	esp_sleep_wakeup_cause_t wakeup_reason;

	wakeup_reason = esp_sleep_get_wakeup_cause();

	switch(wakeup_reason) {
		case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
		case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
		case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
		case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
		case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
		default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
    }
}
// Function invoked by the main program to retrieve weather data objects
int fetchWUdata(const char *WUrequest, const char *stationID,
							const char *apiKey,	Weather *weatherData,
							const char *beginningofArray, int maxData) {
						  
	WiFiClientSecure client;
		
	// Connect to host
	if (!client.connect("api.weather.com", 443)) {
		Serial.println("Failed to connect to WU API server");
		return 0;
	}
	Serial.println("Connected to server");
	
	// Send HTTPS request
	if (!sendRequest(client, WUrequest, stationID, apiKey)) {
		Serial.println("Failed to send WU API request");
		return 0;
	}
		Serial.println("HTTPS request to WU API server sent");
		
	// Check response code
	if (!checkResponse(client)) {
		Serial.print("Unexpected HTTPS status from WU API server received");
		return 0;
	}
	
	Serial.println("HTTPS status OK from WU API server received");

	// The JSON response is gigantic, but we are only interested in the array
	// "summaries" or "observations=.
	//
	// So we skip all the response until we find the start of the array.
	if (!jumpTostart(client, beginningofArray)) {
		Serial.println("Array \"summaries\" or \"observations\" is missing");
		return 0;
	}
						  
	int n = 0;
  // We are now in the array, we can read the objects one after the other
	while (n < maxData) {
		if (!deserializeWeatherData(client, weatherData[n++])) {
		  Serial.println("Failed to parse weather data");
		  break;
		}
		// After reading a weather data object, the next character is either a
		// comma (,) or the closing bracket (])
		if (!jumpToNextElement(client)) break;
	}
	client.stop();
	return n;
}
