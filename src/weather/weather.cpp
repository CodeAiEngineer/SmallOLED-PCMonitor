/*
 * SmallOLED-PCMonitor - Weather Module Implementation
 *
 * Fetches weather data from wttr.in for Izmir Konak.
 * Displays temperature for 3 seconds every 60 seconds.
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

void initWeather() {
  lastWeatherUpdate = millis();
  weatherAvailable = false;
  weatherShowing = false;
  Serial.println("Weather module initialized");
}

void updateWeather() {
  if (!wifiConnected) {
    weatherAvailable = false;
    return;
  }

  unsigned long currentMillis = millis();
  if (currentMillis - lastWeatherUpdate < WEATHER_UPDATE_INTERVAL) {
    return;  // Not time yet
  }

  lastWeatherUpdate = currentMillis;

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
  // Don't show weather if PC is online (show stats instead)
  if (metricData.online) return false;

  // Don't show if weather not available
  if (!weatherAvailable || weatherTemp.length() == 0) return false;

  // Check if it's time to show weather
  unsigned long sinceLastUpdate = millis() - lastWeatherUpdate;
  if (sinceLastUpdate > WEATHER_UPDATE_INTERVAL + 2000) {
    // Give a 2 second grace period after update before showing
    return true;
  }

  return false;
}

void startWeatherDisplay() {
  if (shouldShowWeather() && !weatherShowing) {
    weatherShowing = true;
    weatherDisplayStart = millis();
    Serial.println("Weather display started");
  }
}

void stopWeatherDisplay() {
  weatherShowing = false;
}

void drawWeather() {
  if (!weatherShowing || !displayAvailable) return;

  // Check if display duration exceeded
  if (millis() - weatherDisplayStart > WEATHER_DISPLAY_DURATION) {
    weatherShowing = false;
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
