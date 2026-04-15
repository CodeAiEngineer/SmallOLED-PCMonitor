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

// 90s cycle: display(10s) + wait(80s), alternating weather/clock
#define WEATHER_UPDATE_INTERVAL 80000    // Wait after display phase
#define WEATHER_DISPLAY_DURATION 10000   // Weather display duration (10 seconds)
#define CLOCK_OVERLAY_DURATION 10000     // Fullscreen clock duration (10 seconds)

// Open-Meteo API (free, no key needed, accurate ECMWF data)
// Izmir Konak coordinates: 38.42, 27.14
#define WEATHER_API_URL "http://api.open-meteo.com/v1/forecast?latitude=38.42&longitude=27.14&current=temperature_2m,weather_code"

// Weather state
extern bool weatherAvailable;
extern String weatherTemp;
extern String weatherDesc;
extern unsigned long lastWeatherUpdate;
extern unsigned long weatherDisplayStart;
extern bool weatherShowing;
extern bool clockOverlayShowing;

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

// Draw fullscreen clock overlay (shown after weather)
void drawClockOverlay();

// Check if clock overlay is active
bool isClockOverlayShowing();

#endif // WEATHER_H
