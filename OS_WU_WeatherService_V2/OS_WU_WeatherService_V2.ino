// ESP32 program used to calculate watering scale for the OpenSprinkler System.
// Current and historical weather data is retrieved from WeatherUnderground 
// by using the free API given to Personal Weather Station (PWS) owners.
// Based on this information, the program performs the Zimmerman calculation
// and sends corresponding API commands to OpenSprinkler in order to update the
// watering percentage parameter. It stops OpenSprinkler watering if there
// is any rainfall detected by the PWS and reported to WU.

// Ideas for this program are derived from the following eBooks and websites:
// Learn ESP32 with Arduino IDE, Rui & Sara Santos, https://rntlab.com/courses 
// Mastering ArduinoJson, Benoit Blanchon, https://arduinojson.org/book/
// Some code related to ESP32 deep sleep mode is under Public Domain License
// by Author: Pranav Cherukupalli, cherukupallip@gmail.com
// 
// Requirements:
//  * an ESP-WROOM-32 NodeMCU (or similar)
//  * a PWS sending data to WeatherUnderground
//  * a PWS owners account on WeatherUnderground
//  * an OpenSprinkler 3.0 with FW Version 2.1.9.

#include <SPI.h>
#include <WiFi.h>
#include <HTTPClient.h>

#include "OS_WU_WeatherService_V2.h"

// Conversion factor for micro seconds to seconds
#define uS_TO_S_FACTOR 1000000
// Time ESP32 will go to sleep (in seconds)
#define TIME_TO_SLEEP 300  // (5 minutes)

// Use ON Board LED to indicate WiFi activity
#define WiFiLEDPin 2 // ON Board LED GPIO 2

// Define boot counter allocated in RTC memory
RTC_DATA_ATTR int bootCount = 0;
const int maxbootCount = 11;

// Define rain delay variables allocated in RTC memory
RTC_DATA_ATTR boolean raindelayActive = false;
RTC_DATA_ATTR int raindelayCount = 0;
const int maxrainCount = 11;

// WiFi and WU IP request parameters
const char *ssid = "yourNetworkName";
const char *passphrase = "yourNetworkPassword";
const char *apiKey = "WeatherUndergroundAPIKey";
const char *stationID = "WeatherUndergroundPWSID";
const char *v2pws7DayHistory = "v2/pws/dailysummary/7day";
const char *beginningofSummaries = "\"summaries\":[";
const char *v2pwsCurrentConditions = "v2/pws/observations/current";
const char *beginningofObservations = "\"observations\":[";


// Define Zimmerman base parameters. Please be aware that the baseline for
// humidity is set to 65%, which reflects local German weather conditions.
// The Zimmerman standard is 30% humidity. The water level calculations are
// based on imperial units. Reason for this are the easier Zimmerman
// calculation and the more accurate weather values reported via the WU API.
const int humidityBase = 65;
const float tempBase = 70.0;
const float precipBase = 0.0;

// Opensprinkler API call parameters
const String OSlocalIP = "local IP"; // e.g. http://192.168.178.77/
const String OSPassword = "device password"; // MD5 hashed
const String OSChangeOpt = "co?pw=";
const String OSControlOpt = "cv?pw=";
// With OS firmware version 2.1.9 the API call parameters have changed in the
// way ‘change options’(/co) is handled: &wl has to be used instead of &o23 to
// update the water level. The previous way of handling binary options is
// obsolete.
const String OSOptionInd = "&wl=";
const String OSParameter = "&rd=";
const int rainDelay = 3;

void setup() {
	// Initialize Serial Port
	Serial.begin(115200);
	while (!Serial)
		continue;

	// Initialize the WiFi LED pin as an output
	pinMode(WiFiLEDPin, OUTPUT);
	// Turn WiFiLED, WUInfoLED off
	digitalWrite(WiFiLEDPin, LOW);
	// Print the wakeup reason for ESP32
	print_wakeup_reason();

	// First we configure the wake up source. We set our ESP32 to wake up
	// every 5 minutes. Please note the restricted WU Call volume for PWS
	// owners: 1500/day, 30/minute.
	esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
	Serial.println("Setup ESP32 to sleep for every " 
									+ String(TIME_TO_SLEEP) + " Seconds");

	// Try to connect to Wifi Network
	int no_of_trials = 0;
	WiFi.begin(ssid, passphrase);
	while ((WiFi.status() != WL_CONNECTED) && (no_of_trials < 20)) {
		Serial.println("Connecting to Wifi");
		digitalWrite(WiFiLEDPin, LOW);
		delay(500);
		digitalWrite(WiFiLEDPin, HIGH);
		delay(500);
		no_of_trials++;
	}
	// Go to deep sleep again if connection isn't possible within 20 seconds
	if (no_of_trials >= 20) {
		Serial.println("Connection to the WiFi network failed");
		// Disconnect from WiFi
		WiFi.disconnect(true);
		Serial.println("Disconnected from Wifi");
		Serial.println(WiFi.localIP());

		// Start going to deep sleep.
		Serial.println("Going to deep sleep now");
		delay(1000);
		Serial.flush(); 
		esp_deep_sleep_start();
		Serial.println("This will never be printed");
	}
		
	Serial.println("Connected to the WiFi network");
	Serial.println(WiFi.localIP());
	// Turn WiFiLED on
	digitalWrite(WiFiLEDPin, HIGH);
	
	HTTPClient http;

	// Download the PWS Observations - Current Conditions - v2
	// Please note that the current weather data forwarded from Netatmo weather
	// stations is updated on a 10 min basis by WU.
	Weather currentData[0];
	int m = fetchWUdata(v2pwsCurrentConditions, stationID, apiKey,
									currentData, beginningofObservations, 1);

	Serial.print("Current Weather Conditions: ");
	Serial.print(m);
	Serial.println(" weather object retrieved");
   
	// Be sure that current conditions are retrieved (return code isn't 0)
	if (m > 0) {

		// Print the current data to the serial port  
		Serial.print(currentData[0].obsTimeLocal);
		Serial.print(" -> Precipitation Rate: ");
		Serial.print(currentData[0].precipRate);
		Serial.print(" inch");
		Serial.print(", Precipitation: ");
		Serial.print(currentData[0].precipTotal);
		Serial.println(" inch");

		if (currentData[0].precipRate > 0) {
			raindelayActive = true;

			// Rain detected - stop sprinkler operation for e.g. 3 hours. The
			// Sprinkler operation is stopped for e.g. 3 more hours if it is
			// still raining after one hour.
			// e.g. http://ipadress/cv?pw=mypassword&rd=3
		
			if ((raindelayActive) && (raindelayCount == 0)) {
			
				// Send HTTP request to OpenSprinkler
				http.begin(OSlocalIP + OSControlOpt + OSPassword + OSParameter
																+ rainDelay);
				int httpCode = http.GET();

				Serial.println();
				Serial.print("HTTP request to set a Rain Delay time of ");
				Serial.print(rainDelay);
				Serial.println(" h sent !!!");

				if (httpCode > 0) { //Check for the returning code
					String payload = http.getString();
					Serial.print("HTTP status OK from OS received, Code: ");
					Serial.print(httpCode);
					Serial.print(", ");
					Serial.println(payload);
				}
 
				else {
					Serial.println("Error on HTTP OS rain delay request");
				}
			}
		
			// Increment or reset the rain delay counter
			if (raindelayCount < maxrainCount) {
				++raindelayCount;
			}
			else {
				raindelayCount = 0;
			}
		}

		else {
			// No rain anymore, reset rain delay indicator and rain delay counter
			raindelayActive = false;
			raindelayCount = 0;
		}
		Serial.print("Rain Delay Counter: ");
		Serial.println(raindelayCount);

	}
	
	else {
			Serial.println("No valid Current Data received");
	}

	// Download the PWS Daily Summary - 7 Day History data - v2
	// Please note that the history data is updated on a hourly basis by WU
	// We retrieve the WU historical weather data and update the water level
	// parameter of the OpenSprinkler only once per hour !!!
	//
	// It is currently not clear if there are 7 or 8 weather objects available?
	// There exists a case with two 31st March weather objects. This means
	// that Yesterdays's weather information will be found in structure
	// historyData[7]. It was in structure historyData[6] before. This needs to
	// be further investigated! However, the program handles those cases now!!!

	Serial.println("Boot number: " + String(bootCount));
	
	Weather historyData[8];
	if (bootCount == 0) {
		int n = fetchWUdata(v2pws7DayHistory, stationID, apiKey,
										historyData, beginningofSummaries, 8);
	
		Serial.print("7 Day Weather History: ");
		Serial.print(n);
		Serial.println(" weather objects retrieved");
		// Loop through each weather object
		for (int i = 0; i < n; i++) {
			// Get a reference to the daily data (not a copy)
			Weather &h = historyData[i];
    
			// Print the history data to the serial port
			Serial.print(h.obsTimeLocal);
			Serial.print(" -> Humidity: ");
			Serial.print(h.humidityAvg);
			Serial.print("%, Temperature: ");
			Serial.print(h.tempAvg);
			Serial.print(" F");
			Serial.print(", Precipitation Rate: ");
			Serial.print(h.precipRate);
			Serial.print(" inch");
			Serial.print(", Precipitation: ");
			Serial.print(h.precipTotal);
			Serial.println(" inch");
		}

		// Perform the Zimmerman Calculation based on yesterday's average
		// humidity, temperature data and yesterday's and today's total
		// precipitation data.
		int humidityFactor;
		float tempFactor;
		float precipFactor;
		int percentWatering;

		// Check for plausibility of yeasterdays temperature and humidity data
		// Valid temperatures are 14 to 113 F (-10 to 45 °C), humidity has to be
		// between 0 to 100%, precipitation values have to be greater equal 0.
		bool validhistoryData = true;
		if ((n < 2) || (historyData[n-2].tempAvg < 14) || (historyData[n-2].tempAvg > 113)
				|| (historyData[n-2].humidityAvg < 0) || (historyData[n-2].humidityAvg > 100)
				|| (historyData[n-2].precipRate < 0) || (historyData[n-2].precipTotal < 0)){
			validhistoryData = false;		

			Serial.println("No valid History Data received");
		}
		if (validhistoryData == true) {

			humidityFactor = humidityBase - historyData[n-2].humidityAvg;
			tempFactor = (historyData[n-2].tempAvg - tempBase) * 4;
			precipFactor = (precipBase - historyData[n-2].precipTotal
											- currentData[0].precipTotal)* 200;

			Serial.println();
			Serial.print("HumidityFactor: ");
			Serial.print(humidityFactor);
			Serial.print("%, TemperatureFactor: ");
			Serial.print(tempFactor);
			Serial.print(" %");
			Serial.print(", PrecipitationFactor: ");
			Serial.print(precipFactor);
			Serial.println(" %");

			percentWatering = humidityFactor + tempFactor + precipFactor + 100;

			Serial.println();
			Serial.print("%Watering: ");
			Serial.print(percentWatering);
			Serial.print(" %");

			// Don't run OpenSprinkler if less than 10% watering percentage is 
			// calculated
			if (percentWatering < 10) {
				percentWatering = 0;
				Serial.print(" -> %Watering bounded to: ");
				Serial.print(percentWatering);
				Serial.print(" %");
			}
			Serial.println();
  
			// Change %Watering via API call to OpenSprinkler
			// e.g. http://ipadress:8080/co?pw=mypassword&o23=77


			// Convert integer to string
			String prcntwtr;
			prcntwtr = String(percentWatering);
  
			// Send HTTP request to OpenSprinkler
			http.begin(OSlocalIP + OSChangeOpt + OSPassword + OSOptionInd
																+ prcntwtr);
			int httpCode = http.GET();
 
			Serial.println();
			Serial.println("HTTP request to update the Water Level to OS sent !");

			if (httpCode > 0) { //Check for the returning code
				String payload = http.getString();
				Serial.print("HTTP status OK from OS received, Code: ");
				Serial.print(httpCode);
				Serial.print(", ");
				Serial.println(payload);
			}
 
			else {
				Serial.println("Error on HTTP OS %Watering request");
			}
		}
	}
	
	if (bootCount < maxbootCount) {
		//Increment boot counter
		++bootCount;
	}
	else {
		// Reset boot counter
		bootCount = 0;
	}

	// Disconnect from WiFi
	WiFi.disconnect(true);
	Serial.println("Disconnected from Wifi");
	Serial.println(WiFi.localIP());

	// Start going to deep sleep.
	Serial.println("Going to deep sleep now");
	delay(1000);
	Serial.flush(); 
	esp_deep_sleep_start();
	Serial.println("This will never be printed");
}
void loop() {
	// This is not going to be called
}
