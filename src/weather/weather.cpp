/*
 * SmallOLED-PCMonitor - Weather Module Implementation
 *
 * Fetches weather data from wttr.in for Izmir Konak.
 * Displays temperature for 5 seconds every 60 seconds.
 */

#include "weather.h"
#include "../display/display.h"
#include "../config/config.h"
#include <HTTPClient.h>

// Weather state variables
bool weatherAvailable = false;
String weatherTemp = "";
unsigned long lastWeatherUpdate = 0;
unsigned long weatherDisplayStart = 0;
bool weatherShowing = false;
unsigned long weatherShowTimer = 0;  // Timer for tracking show cycle
bool weatherShownThisCycle = false;   // Track if we've shown weather in current cycle

void initWeather() {
  lastWeatherUpdate = 0;  // Force immediate update
  weatherAvailable = false;
  weatherShowing = false;
  weatherShowTimer = 0;
  weatherShownThisCycle = false;
  Serial.println("Weather module initialized");
}

void updateWeather() {
  if (!wifiConnected) {
    weatherAvailable = false;
    return;
  }

  unsigned long currentMillis = millis();
  
  // Fetch weather every 60 seconds
  if (currentMillis - lastWeatherUpdate < WEATHER_UPDATE_INTERVAL) {
    return;  // Not time yet
  }

  lastWeatherUpdate = currentMillis;
  weatherShownThisCycle = false;  // Reset cycle flag

  HTTPClient http;
  http.begin(WEATHER_API_URL);
  http.setTimeout(5000);  // 5 second timeout

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    payload.trim();

    // wttr.in returns something like "+25°C" or "-3°C"
    if (payload.length() > 0 && payload.length() < 15) {
      weatherTemp = payload;
      weatherAvailable = true;
      Serial.printf("Weather updated: %s\n", weatherTemp.c_str());
    } else {
      Serial.printf("Weather API returned invalid data: %s\n", payload.c_str());
      weatherAvailable = false;
    }
  } else {
    Serial.printf("Weather API failed: HTTP %d\n", httpCode);
    weatherAvailable = false;
  }

  http.end();
}

bool shouldShowWeather() {
  // Don't show if weather not available
  if (!weatherAvailable || weatherTemp.length() == 0) return false;
  
  // Don't show if already showing
  if (weatherShowing) return false;
  
  // Don't show if already shown this cycle
  if (weatherShownThisCycle) return false;

  // Show weather 2 seconds after each weather fetch, for 5 seconds
  unsigned long sinceLastUpdate = millis() - lastWeatherUpdate;
  if (sinceLastUpdate >= 2000) {
    return true;
  }

  return false;
}

void startWeatherDisplay() {
  if (shouldShowWeather() && !weatherShowing) {
    weatherShowing = true;
    weatherDisplayStart = millis();
    weatherShownThisCycle = true;  // Mark as shown for this cycle
    Serial.printf("Weather display started - showing for 5 seconds: %s\n", weatherTemp.c_str());
  }
}

void stopWeatherDisplay() {
  weatherShowing = false;
}

void drawWeather() {
  if (!weatherShowing || !displayAvailable) return;

  // Check if display duration exceeded (5 seconds)
  unsigned long elapsed = millis() - weatherDisplayStart;
  if (elapsed > WEATHER_DISPLAY_DURATION) {
    weatherShowing = false;
    Serial.println("Weather display finished");
    return;
  }

  // Draw weather info
  display.setTextSize(1);
  display.setTextColor(DISPLAY_WHITE);

  // Location label
  int locWidth = 6 * 10;  // "Izmir Konak" ~ 10 chars * 6px
  display.setCursor((SCREEN_WIDTH - locWidth) / 2, 8);
  display.print("Izmir Konak");

  // Temperature (large)
  display.setTextSize(3);
  int tempWidth = weatherTemp.length() * 18;  // Each char ~18px at size 3
  display.setCursor((SCREEN_WIDTH - tempWidth) / 2, 22);
  display.print(weatherTemp);

  // "Hava Durumu" label at bottom
  display.setTextSize(1);
  int labelWidth = 9 * 6;  // ~9 chars
  display.setCursor((SCREEN_WIDTH - labelWidth) / 2, 52);
  display.print("Hava Durumu");

  // Update flag when done
  if (millis() - weatherDisplayStart > WEATHER_DISPLAY_DURATION - 500) {
    // About to expire, mark for next cycle
  }
}
