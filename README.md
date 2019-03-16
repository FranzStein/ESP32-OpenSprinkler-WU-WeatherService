# ESP32-OpenSprinkler-WU-WeatherService

ESP32 program to calculate %Watering for the OpenSprinkler System based on WeatherUndergroud PWS data.
Current and historical weather data is retrieved from WeatherUnderground by using the free API given to non-commercial Personal Weather Station (PWS) owners. Based on this weather data, the program performs the Zimmerman calculation and sends a corresponding API command to OpenSprinkler in order to update the watering percentage parameter. Additionally, it stops OpenSprinkler watering if there is any rainfall detected by the PWS and reported to WeatherUnderground.

Ideas for this program are derived from the following eBooks and websites:
 - Learn ESP32 with Arduino IDE, Rui & Sara Santos, https://rntlab.com/courses
 - Mastering ArduinoJson, Benoit Blanchon, https://arduinojson.org/book/
 - Some code related to ESP32 deep sleep mode is under Public Domain License by Author: Pranav Cherukupalli, cherukupallip@gmail.com

Requirements:
 - an ESP-WROOM-32 NodeMCU (or similar)
 - a PWS sending data to WeatherUnderground
 - a PWS owners account on WeatherUnderground
 - an OpenSprinkler 3.0 to water the garden.

The program is tested within the following environment:
 - Netatmo Weather Station consisting of an indoor and an outdoor module, a rain gauge and a wind gauge. The outdoor module is protected by a sun shield
 - Meteoware Plus account, to connect the Netatmo Weather Station to the "Weather Underground" weather network
 - OpenSprinkler, App Version: 1.8.4, Firmware: 2.1.8, Hardware Version: 3.0 -DC

PLease note that this program is developed as a substitute for the OpenSprinkler weather service changing now to the "OpenWeatherMap" weather network, which is not supported by Meteoware Plus or Netatmo. The program is based on current and historical weather data. Weather forecasts are not taken into account. The OpenSprinkler Weather Adjustment Method has to be set to “Manual” to use this program.
