/*
 * SmallOLED-PCMonitor - Weather Module
 *
 * Fetches weather data from wttr.in for Izmir Konak.
 * Displays temperature for 3 seconds every 60 seconds.
 */

#ifndef WEATHER_H
#define WEATHER_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>

// Weather update interval (60 seconds)
#define WEATHER_UPDATE_INTERVAL 60000
// Weather display duration (3 seconds)
#define WEATHER_DISPLAY_DURATION 3000

// Weather API URL (wttr.in - no API key needed)
#define WEATHER_API_URL "http://wttr.in/Izmir,Konak?format=%t&lang=tr"

// Weather state
extern bool weatherAvailable;
extern String weatherTemp;
extern unsigned long lastWeatherUpdate;
extern unsigned long weatherDisplayStart;
extern bool weatherShowing;

// Initialize weather module
void initWeather();

// Fetch weather data (non-blocking)
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
