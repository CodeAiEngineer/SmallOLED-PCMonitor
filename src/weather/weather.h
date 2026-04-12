/*
 * SmallOLED-PCMonitor - Weather Module
 *
 * Fetches weather data from wttr.in for Izmir Konak.
 * Displays temperature + icon for 5 seconds every 60 seconds.
 */

#ifndef WEATHER_H
#define WEATHER_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>

// Weather update interval (60 seconds)
#define WEATHER_UPDATE_INTERVAL 60000
// Weather display duration (8 seconds)
#define WEATHER_DISPLAY_DURATION 8000

// Weather API URLs (wttr.in - no API key needed)
#define WEATHER_API_URL_TEMP "http://wttr.in/Izmir,Konak?format=%t&lang=tr"
#define WEATHER_API_URL_DESC "http://wttr.in/Izmir,Konak?format=%C&lang=tr"

// Weather state
extern bool weatherAvailable;
extern String weatherTemp;
extern String weatherDesc;
extern unsigned long lastWeatherUpdate;
extern unsigned long weatherDisplayStart;
extern bool weatherShowing;

// Initialize weather module
void initWeather();

// Fetch weather data (state machine)
void updateWeather();

// Check if we should display weather now
bool shouldShowWeather();

// Start showing weather
void startWeatherDisplay();

// Stop showing weather
void stopWeatherDisplay();

// Draw weather info on display
void drawWeather();

#endif // WEATHER_H
