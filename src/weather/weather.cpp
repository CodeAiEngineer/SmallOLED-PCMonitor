/*
 * SmallOLED-PCMonitor - Weather Module Implementation
 *
 * Simple state machine: FETCH -> SHOW (5s) -> WAIT (55s) -> repeat
 */

#include "weather.h"
#include "../display/display.h"
#include "../config/config.h"
#include <HTTPClient.h>

// Weather state variables
bool weatherAvailable = false;
String weatherTemp = "";
String weatherDesc = "";
unsigned long lastWeatherUpdate = 0;
unsigned long weatherDisplayStart = 0;
bool weatherShowing = false;

// Simple state machine: 0=fetch, 1=show, 2=wait
static int weatherState = 0;

// Weather icon mapping based on description
int getWeatherIcon(String desc) {
  desc.toLowerCase();
  if (desc.indexOf("sun") >= 0 || desc.indexOf("clear") >= 0) return 0;
  if (desc.indexOf("partly cloudy") >= 0 || desc.indexOf("cloud") >= 0) return 1;
  if (desc.indexOf("overcast") >= 0) return 2;
  if (desc.indexOf("rain") >= 0 || desc.indexOf("drizzle") >= 0) return 3;
  if (desc.indexOf("thunder") >= 0 || desc.indexOf("storm") >= 0) return 4;
  if (desc.indexOf("snow") >= 0 || desc.indexOf("blizzard") >= 0) return 5;
  if (desc.indexOf("fog") >= 0 || desc.indexOf("mist") >= 0) return 6;
  return 1;
}

void initWeather() {
  lastWeatherUpdate = 0;
  weatherAvailable = false;
  weatherShowing = false;
  weatherState = 0;  // Start with fetch
  weatherDesc = "";
  Serial.println("Weather module initialized");
}

void updateWeather() {
  // State 0: Fetch weather data
  if (weatherState == 0) {
    if (!wifiConnected) {
      Serial.println("Weather: WiFi not connected, waiting...");
      return;
    }

    Serial.println("Weather: Fetching data...");
    
    // Fetch temperature
    HTTPClient http;
    http.begin(WEATHER_API_URL_TEMP);
    http.setTimeout(5000);
    int httpCode = http.GET();
    
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      payload.trim();
      if (payload.length() > 0 && payload.length() < 15) {
        weatherTemp = payload;
        weatherAvailable = true;
        Serial.printf("Weather temp OK: %s\n", weatherTemp.c_str());
      }
    } else {
      Serial.printf("Weather temp failed: HTTP %d\n", httpCode);
    }
    http.end();

    // Fetch description if temp succeeded
    if (weatherAvailable) {
      HTTPClient httpDesc;
      httpDesc.begin(WEATHER_API_URL_DESC);
      httpDesc.setTimeout(5000);
      int httpCodeDesc = httpDesc.GET();
      if (httpCodeDesc == HTTP_CODE_OK) {
        String descPayload = httpDesc.getString();
        descPayload.trim();
        if (descPayload.length() > 0 && descPayload.length() < 50) {
          weatherDesc = descPayload;
          Serial.printf("Weather desc OK: %s\n", weatherDesc.c_str());
        }
      }
      httpDesc.end();
    }

    // Move to show state
    if (weatherAvailable) {
      weatherState = 1;
      weatherDisplayStart = millis();
      weatherShowing = true;
      Serial.println("Weather: Moving to SHOW state");
    } else {
      // If fetch failed, wait 60s before retry
      lastWeatherUpdate = millis();
      weatherState = 2;
      Serial.println("Weather: Fetch failed, moving to WAIT state");
    }
  }
  // State 2: Wait 60 seconds before next fetch
  else if (weatherState == 2) {
    unsigned long elapsed = millis() - lastWeatherUpdate;
    if (elapsed >= WEATHER_UPDATE_INTERVAL) {
      weatherState = 0;  // Go back to fetch
      Serial.println("Weather: Wait complete, moving to FETCH state");
    }
  }
}

bool shouldShowWeather() {
  // Only show if we're in show state and weather is available
  if (weatherState == 1 && weatherAvailable && !weatherShowing) {
    return true;
  }
  return false;
}

void startWeatherDisplay() {
  if (shouldShowWeather()) {
    weatherShowing = true;
    weatherDisplayStart = millis();
    Serial.printf("Weather display started: %s\n", weatherTemp.c_str());
  }
}

void stopWeatherDisplay() {
  weatherShowing = false;
}

// Draw weather pixel art icon (16x16)
void drawWeatherIcon(int iconType, int x, int y) {
  switch (iconType) {
    case 0: // Sun
      display.fillCircle(x + 8, y + 8, 5, DISPLAY_WHITE);
      display.drawLine(x + 8, y, x + 8, y + 2, DISPLAY_WHITE);
      display.drawLine(x + 8, y + 14, x + 8, y + 16, DISPLAY_WHITE);
      display.drawLine(x, y + 8, x + 2, y + 8, DISPLAY_WHITE);
      display.drawLine(x + 14, y + 8, x + 16, y + 8, DISPLAY_WHITE);
      display.drawLine(x + 2, y + 2, x + 4, y + 4, DISPLAY_WHITE);
      display.drawLine(x + 12, y + 12, x + 14, y + 14, DISPLAY_WHITE);
      display.drawLine(x + 14, y + 2, x + 12, y + 4, DISPLAY_WHITE);
      display.drawLine(x + 2, y + 14, x + 4, y + 12, DISPLAY_WHITE);
      break;
    case 1: // Partly Cloudy
      display.fillCircle(x + 5, y + 5, 4, DISPLAY_WHITE);
      display.fillCircle(x + 7, y + 10, 4, DISPLAY_WHITE);
      display.fillCircle(x + 12, y + 10, 3, DISPLAY_WHITE);
      display.fillRect(x + 7, y + 10, 8, 4, DISPLAY_WHITE);
      break;
    case 2: // Cloudy/Overcast
      display.fillCircle(x + 5, y + 7, 4, DISPLAY_WHITE);
      display.fillCircle(x + 11, y + 7, 4, DISPLAY_WHITE);
      display.fillCircle(x + 8, y + 6, 5, DISPLAY_WHITE);
      display.fillRect(x + 3, y + 9, 10, 4, DISPLAY_WHITE);
      break;
    case 3: // Rain
      display.fillCircle(x + 5, y + 5, 4, DISPLAY_WHITE);
      display.fillCircle(x + 11, y + 5, 4, DISPLAY_WHITE);
      display.fillCircle(x + 8, y + 4, 5, DISPLAY_WHITE);
      display.fillRect(x + 3, y + 7, 10, 3, DISPLAY_WHITE);
      display.drawLine(x + 4, y + 12, x + 3, y + 15, DISPLAY_WHITE);
      display.drawLine(x + 8, y + 11, x + 7, y + 15, DISPLAY_WHITE);
      display.drawLine(x + 12, y + 12, x + 11, y + 15, DISPLAY_WHITE);
      break;
    case 4: // Thunder/Storm
      display.fillCircle(x + 5, y + 4, 4, DISPLAY_WHITE);
      display.fillCircle(x + 11, y + 4, 4, DISPLAY_WHITE);
      display.fillCircle(x + 8, y + 3, 5, DISPLAY_WHITE);
      display.fillRect(x + 3, y + 6, 10, 3, DISPLAY_WHITE);
      display.drawLine(x + 9, y + 9, x + 7, y + 12, DISPLAY_WHITE);
      display.drawLine(x + 7, y + 12, x + 10, y + 12, DISPLAY_WHITE);
      display.drawLine(x + 10, y + 12, x + 8, y + 15, DISPLAY_WHITE);
      break;
    case 5: // Snow
      display.fillCircle(x + 5, y + 5, 4, DISPLAY_WHITE);
      display.fillCircle(x + 11, y + 5, 4, DISPLAY_WHITE);
      display.fillCircle(x + 8, y + 4, 5, DISPLAY_WHITE);
      display.fillRect(x + 3, y + 7, 10, 3, DISPLAY_WHITE);
      display.drawPixel(x + 4, y + 12, DISPLAY_WHITE);
      display.drawPixel(x + 8, y + 11, DISPLAY_WHITE);
      display.drawPixel(x + 12, y + 12, DISPLAY_WHITE);
      display.drawPixel(x + 6, y + 14, DISPLAY_WHITE);
      display.drawPixel(x + 10, y + 14, DISPLAY_WHITE);
      break;
    case 6: // Fog
      display.fillCircle(x + 5, y + 5, 3, DISPLAY_WHITE);
      display.fillCircle(x + 11, y + 5, 3, DISPLAY_WHITE);
      display.fillCircle(x + 8, y + 4, 4, DISPLAY_WHITE);
      display.fillRect(x + 4, y + 6, 8, 2, DISPLAY_WHITE);
      display.drawLine(x + 2, y + 10, x + 14, y + 10, DISPLAY_WHITE);
      display.drawLine(x + 3, y + 12, x + 13, y + 12, DISPLAY_WHITE);
      display.drawLine(x + 2, y + 14, x + 14, y + 14, DISPLAY_WHITE);
      break;
    default:
      drawWeatherIcon(1, x, y);
      break;
  }
}

void drawWeather() {
  if (!weatherShowing || !displayAvailable) return;

  // Check if display duration exceeded (5 seconds)
  unsigned long elapsed = millis() - weatherDisplayStart;
  if (elapsed > WEATHER_DISPLAY_DURATION) {
    weatherShowing = false;
    weatherState = 2;  // Move to wait state
    lastWeatherUpdate = millis();
    Serial.println("Weather: Display finished, moving to WAIT state");
    return;
  }

  // Draw weather info
  display.setTextSize(1);
  display.setTextColor(DISPLAY_WHITE);

  // Draw weather icon (16x16) on the left
  int iconType = getWeatherIcon(weatherDesc);
  drawWeatherIcon(iconType, 4, 4);

  // Location label (right side)
  display.setCursor(24, 6);
  display.print("Izmir");

  // Temperature (large, centered right)
  display.setTextSize(3);
  int tempWidth = weatherTemp.length() * 18;
  display.setCursor(SCREEN_WIDTH - tempWidth - 4, 20);
  display.print(weatherTemp);

  // Weather description at bottom
  display.setCursor(24, SCREEN_HEIGHT - 10);
  display.print(weatherDesc.length() > 0 ? weatherDesc : "Hava Durumu");

  // Progress bar at top
  int barWidth = SCREEN_WIDTH - 8;
  int progress = (int)((elapsed * 100) / WEATHER_DISPLAY_DURATION);
  int fillWidth = (barWidth * progress) / 100;
  display.drawFastHLine(4, 2, barWidth, DISPLAY_WHITE);
  display.drawFastHLine(4, 3, fillWidth, DISPLAY_WHITE);
}
