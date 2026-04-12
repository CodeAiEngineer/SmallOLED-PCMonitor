/*
 * SmallOLED-PCMonitor - Weather Module Implementation
 *
 * Non-blocking weather fetch using wttr.in API.
 * Uses a single API call to get both temp and description.
 */

#include "weather.h"
#include "../display/display.h"
#include "../config/config.h"
#include <HTTPClient.h>
#include <esp_task_wdt.h>

// Weather state variables
bool weatherAvailable = false;
String weatherTemp = "";
String weatherDesc = "";
unsigned long lastWeatherUpdate = 0;
unsigned long weatherDisplayStart = 0;
bool weatherShowing = false;

// Simple state machine: 0=idle, 1=fetching, 2=show, 3=wait
static int weatherState = 0;
static unsigned long stateStartTime = 0;

// Weather icon mapping
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
  weatherState = 0;
  weatherDesc = "";
  Serial.println("Weather module initialized");
}

void updateWeather() {
  if (!wifiConnected) {
    weatherAvailable = false;
    return;
  }

  unsigned long now = millis();

  switch (weatherState) {
    case 0: // IDLE - Fetch weather
      // Only fetch every 60 seconds
      if (now - lastWeatherUpdate < WEATHER_UPDATE_INTERVAL) {
        return;
      }
      
      Serial.println("Weather: Fetching...");
      {
        // Reset watchdog before HTTP call to prevent boot loop
        esp_task_wdt_reset();
        
        // Use single API call with format: temp|desc
        HTTPClient http;
        http.begin("http://wttr.in/Izmir,Konak?format=%t|%C&lang=tr");
        http.setTimeout(4000);
        
        int httpCode = http.GET();
        
        // Reset watchdog after HTTP call too
        esp_task_wdt_reset();
        if (httpCode == HTTP_CODE_OK) {
          String payload = http.getString();
          payload.trim();
          
          int pipeIndex = payload.indexOf('|');
          if (pipeIndex > 0) {
            weatherTemp = payload.substring(0, pipeIndex);
            weatherDesc = payload.substring(pipeIndex + 1);
            
            if (weatherTemp.length() > 0 && weatherTemp.length() < 15 &&
                weatherDesc.length() > 0 && weatherDesc.length() < 50) {
              weatherAvailable = true;
              Serial.printf("Weather OK: %s | %s\n", weatherTemp.c_str(), weatherDesc.c_str());
              weatherState = 2;
              stateStartTime = now;
              weatherDisplayStart = now;
              weatherShowing = true;
              Serial.println("Weather: Moving to SHOW state");
            } else {
              Serial.println("Weather: Invalid data format");
              weatherAvailable = false;
              weatherState = 3;
              stateStartTime = now;
              lastWeatherUpdate = now;
            }
          } else {
            Serial.printf("Weather: No pipe found, data: %s\n", payload.c_str());
            weatherAvailable = false;
            weatherState = 3;
            stateStartTime = now;
            lastWeatherUpdate = now;
          }
        } else {
          Serial.printf("Weather: HTTP failed: %d\n", httpCode);
          weatherAvailable = false;
          weatherState = 3;
          stateStartTime = now;
          lastWeatherUpdate = now;
        }
        http.end();
      }
      break;
      
    case 2: // SHOW - Display weather (handled by drawWeather)
      // Check if 5 seconds elapsed
      if (now - weatherDisplayStart >= WEATHER_DISPLAY_DURATION) {
        weatherShowing = false;
        weatherState = 3;
        stateStartTime = now;
        Serial.println("Weather: Moving to WAIT state");
      }
      break;
      
    case 3: // WAIT - Wait 60 seconds before next fetch
      if (now - stateStartTime >= WEATHER_UPDATE_INTERVAL) {
        weatherState = 0;
        Serial.println("Weather: Moving to FETCH state");
      }
      break;
  }
}

bool shouldShowWeather() {
  return (weatherState == 2 && weatherAvailable && !weatherShowing);
}

void startWeatherDisplay() {
  if (shouldShowWeather()) {
    weatherShowing = true;
    weatherDisplayStart = millis();
    Serial.printf("Weather display: %s\n", weatherTemp.c_str());
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
    case 2: // Cloudy
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
    case 4: // Storm
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

  unsigned long elapsed = millis() - weatherDisplayStart;
  if (elapsed > WEATHER_DISPLAY_DURATION) {
    weatherShowing = false;
    return;
  }

  display.setTextSize(1);
  display.setTextColor(DISPLAY_WHITE);

  int iconType = getWeatherIcon(weatherDesc);
  drawWeatherIcon(iconType, 4, 4);

  display.setCursor(24, 6);
  display.print("Izmir");

  display.setTextSize(3);
  int tempWidth = weatherTemp.length() * 18;
  display.setCursor(SCREEN_WIDTH - tempWidth - 4, 20);
  display.print(weatherTemp);

  display.setCursor(24, SCREEN_HEIGHT - 10);
  display.print(weatherDesc.length() > 0 ? weatherDesc : "Hava Durumu");

  int barWidth = SCREEN_WIDTH - 8;
  int progress = (int)((elapsed * 100) / WEATHER_DISPLAY_DURATION);
  int fillWidth = (barWidth * progress) / 100;
  display.drawFastHLine(4, 2, barWidth, DISPLAY_WHITE);
  display.drawFastHLine(4, 3, fillWidth, DISPLAY_WHITE);
}
