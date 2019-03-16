// Data declarations to retrieve weather data from WU API server

// Ideas for this program are derived from the following eBooks and websites:
// Learn ESP32 with Arduino IDE, Rui & Sara Santos, https://rntlab.com/courses 
// Mastering ArduinoJson, Benoit Blanchon, https://arduinojson.org/book/
// Some code related to the ESP32 Deepsleep mode is under Public Domain License
// by Author: Pranav Cherukupalli, cherukupallip@gmail.com

struct Weather {
	char obsTimeLocal[64];
	int humidityAvg;
	float tempAvg;
	float precipRate;
	float precipTotal;
};

void print_wakeup_reason();
int fetchWUdata(const char * WUrequest, const char *stationID,
const char *apiKey, Weather *weatherData, const char *beginningofArray,
																int maxData);
