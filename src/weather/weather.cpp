/*
 * SmallOLED-PCMonitor - Weather Module Implementation
 *
 * Fetches weather data from wttr.in for Izmir Konak.
 * Displays temperature + weather icon for 5 seconds every 60 seconds.
 */

#include "weather.h"
#include "../display/display.h"
#include "../config/config.h"
#include <HTTPClient.h>

// Weather state variables
bool weatherAvailable = false;
String weatherTemp = "";
String weatherDesc = "";  // Weather description
unsigned long lastWeatherUpdate = 0;
unsigned long weatherDisplayStart = 0;
bool weatherShowing = false;
unsigned long weatherShowTimer = 0;  // Timer for tracking show cycle
bool weatherShownThisCycle = false;   // Track if we've shown weather in current cycle

// Weather icon mapping based on description
int getWeatherIcon(String desc) {
  desc.toLowerCase();
  if (desc.indexOf("sun") >= 0 || desc.indexOf("clear") >= 0) return 0;      // ☀️ Sunny
  if (desc.indexOf("partly cloudy") >= 0 || desc.indexOf("cloud") >= 0) return 1;  // ⛅ Partly Cloudy
  if (desc.indexOf("overcast") >= 0) return 2;  // ☁️ Overcast
  if (desc.indexOf("rain") >= 0 || desc.indexOf("drizzle") >= 0) return 3;   // 🌧️ Rain
  if (desc.indexOf("thunder") >= 0 || desc.indexOf("storm") >= 0) return 4;  // ⛈️ Thunder
  if (desc.indexOf("snow") >= 0 || desc.indexOf("blizzard") >= 0) return 5;  // ❄️ Snow
  if (desc.indexOf("fog") >= 0 || desc.indexOf("mist") >= 0) return 6;       // 🌫️ Fog
  return 1;  // Default: Partly cloudy
}

void initWeather() {
  lastWeatherUpdate = 0;  // Force immediate update
  weatherAvailable = false;
  weatherShowing = false;
  weatherShowTimer = 0;
  weatherShownThisCycle = false;
  weatherDesc = "";
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
      Serial.printf("Weather temp: %s\n", weatherTemp.c_str());
    } else {
      Serial.printf("Weather API returned invalid temp data: %s\n", payload.c_str());
      weatherAvailable = false;
    }
  } else {
    Serial.printf("Weather temp API failed: HTTP %d\n", httpCode);
    weatherAvailable = false;
  }
  http.end();

  // Fetch weather description
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
        Serial.printf("Weather desc: %s\n", weatherDesc.c_str());
      }
    } else {
      Serial.printf("Weather desc API failed: HTTP %d\n", httpCodeDesc);
    }
    httpDesc.end();
  }
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

// Draw weather pixel art icon (16x16)
void drawWeatherIcon(int iconType, int x, int y) {
  switch (iconType) {
    case 0: // Sun
      display.fillCircle(x + 8, y + 8, 5, DISPLAY_WHITE);
      // Rays
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
      // Sun (partial)
      display.fillCircle(x + 5, y + 5, 4, DISPLAY_WHITE);
      // Cloud
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
      // Cloud
      display.fillCircle(x + 5, y + 5, 4, DISPLAY_WHITE);
      display.fillCircle(x + 11, y + 5, 4, DISPLAY_WHITE);
      display.fillCircle(x + 8, y + 4, 5, DISPLAY_WHITE);
      display.fillRect(x + 3, y + 7, 10, 3, DISPLAY_WHITE);
      // Rain drops
      display.drawLine(x + 4, y + 12, x + 3, y + 15, DISPLAY_WHITE);
      display.drawLine(x + 8, y + 11, x + 7, y + 15, DISPLAY_WHITE);
      display.drawLine(x + 12, y + 12, x + 11, y + 15, DISPLAY_WHITE);
      break;
      
    case 4: // Thunder/Storm
      // Cloud
      display.fillCircle(x + 5, y + 4, 4, DISPLAY_WHITE);
      display.fillCircle(x + 11, y + 4, 4, DISPLAY_WHITE);
      display.fillCircle(x + 8, y + 3, 5, DISPLAY_WHITE);
      display.fillRect(x + 3, y + 6, 10, 3, DISPLAY_WHITE);
      // Lightning bolt
      display.drawLine(x + 9, y + 9, x + 7, y + 12, DISPLAY_WHITE);
      display.drawLine(x + 7, y + 12, x + 10, y + 12, DISPLAY_WHITE);
      display.drawLine(x + 10, y + 12, x + 8, y + 15, DISPLAY_WHITE);
      break;
      
    case 5: // Snow
      // Cloud
      display.fillCircle(x + 5, y + 5, 4, DISPLAY_WHITE);
      display.fillCircle(x + 11, y + 5, 4, DISPLAY_WHITE);
      display.fillCircle(x + 8, y + 4, 5, DISPLAY_WHITE);
      display.fillRect(x + 3, y + 7, 10, 3, DISPLAY_WHITE);
      // Snowflakes (dots)
      display.drawPixel(x + 4, y + 12, DISPLAY_WHITE);
      display.drawPixel(x + 8, y + 11, DISPLAY_WHITE);
      display.drawPixel(x + 12, y + 12, DISPLAY_WHITE);
      display.drawPixel(x + 6, y + 14, DISPLAY_WHITE);
      display.drawPixel(x + 10, y + 14, DISPLAY_WHITE);
      break;
      
    case 6: // Fog
      // Cloud (lighter)
      display.fillCircle(x + 5, y + 5, 3, DISPLAY_WHITE);
      display.fillCircle(x + 11, y + 5, 3, DISPLAY_WHITE);
      display.fillCircle(x + 8, y + 4, 4, DISPLAY_WHITE);
      display.fillRect(x + 4, y + 6, 8, 2, DISPLAY_WHITE);
      // Fog lines
      display.drawLine(x + 2, y + 10, x + 14, y + 10, DISPLAY_WHITE);
      display.drawLine(x + 3, y + 12, x + 13, y + 12, DISPLAY_WHITE);
      display.drawLine(x + 2, y + 14, x + 14, y + 14, DISPLAY_WHITE);
      break;
      
    default: // Default: Partly cloudy
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
    Serial.println("Weather display finished");
    return;
  }

  // Draw weather info
  display.setTextSize(1);
  display.setTextColor(DISPLAY_WHITE);

  // Draw weather icon (16x16) on the left
  int iconType = getWeatherIcon(weatherDesc);
  drawWeatherIcon(iconType, 4, 4);

  // Location label (right side)
  display.setTextSize(1);
  display.setCursor(24, 6);
  display.print("Izmir");

  // Temperature (large, centered right)
  display.setTextSize(3);
  int tempWidth = weatherTemp.length() * 18;  // Each char ~18px at size 3
  display.setCursor(SCREEN_WIDTH - tempWidth - 4, 20);
  display.print(weatherTemp);

  // Weather description at bottom
  display.setTextSize(1);
  display.setCursor(24, SCREEN_HEIGHT - 10);
  display.print(weatherDesc.length() > 0 ? weatherDesc : "Hava Durumu");

  // Progress bar at top (shows how long until weather display ends)
  int barWidth = SCREEN_WIDTH - 8;
  int progress = (int)((elapsed * 100) / WEATHER_DISPLAY_DURATION);
  int fillWidth = (barWidth * progress) / 100;
  display.drawFastHLine(4, 2, barWidth, DISPLAY_WHITE);
  display.drawFastHLine(4, 3, fillWidth, DISPLAY_WHITE);
}
