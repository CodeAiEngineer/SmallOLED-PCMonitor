/*
 * SmallOLED-PCMonitor - Weather Module Implementation
 *
 * Non-blocking weather fetch using Open-Meteo API (free, no key).
 * Uses a single API call to get temperature and WMO weather code.
 */

#include "weather.h"
#include "../display/display.h"
#include "../config/config.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>

// Weather state variables
bool weatherAvailable = false;
String weatherTemp = "";
String weatherDesc = "";
unsigned long lastWeatherUpdate = 0;
unsigned long weatherDisplayStart = 0;
bool weatherShowing = false;
bool clockOverlayShowing = false;
static unsigned long clockOverlayStart = 0;

// Simple state machine: 0=idle, 1=fetching, 2=show, 3=wait
static int weatherState = 0;
static unsigned long stateStartTime = 0;

// Convert Turkish characters to ASCII
String toAscii(String input) {
  input.replace("ç", "c");
  input.replace("Ç", "C");
  input.replace("ğ", "g");
  input.replace("Ğ", "G");
  input.replace("ı", "i");
  input.replace("İ", "I");
  input.replace("ö", "o");
  input.replace("Ö", "O");
  input.replace("ş", "s");
  input.replace("Ş", "S");
  input.replace("ü", "u");
  input.replace("Ü", "U");
  return input;
}

// Map WMO weather code to simple Turkish description
String getWeatherDescFromCode(int code) {
  if (code == 0) return "GUNESLI";
  if (code == 1) return "AZ BULUTLU";
  if (code == 2) return "BULUTLU";
  if (code == 3) return "KAPALI";
  if (code == 45 || code == 48) return "SISLI";
  if (code >= 51 && code <= 57) return "CISELEYEN";
  if (code >= 61 && code <= 67) return "YAGMURLU";
  if (code >= 71 && code <= 77) return "KARLI";
  if (code >= 80 && code <= 82) return "SAGANAK";
  if (code >= 85 && code <= 86) return "KAR SAGANAK";
  if (code >= 95 && code <= 99) return "FIRTINA";
  return "GUNESLI";
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
      
      Serial.println("Weather: Fetching from Open-Meteo...");
      {
        esp_task_wdt_reset();

        HTTPClient http;
        // Open-Meteo: Izmir Konak coords (38.42, 27.14), get current temp + weather code
        http.begin("http://api.open-meteo.com/v1/forecast?latitude=38.42&longitude=27.14&current=temperature_2m,weather_code");
        http.setTimeout(5000);

        int httpCode = http.GET();
        esp_task_wdt_reset();

        if (httpCode == HTTP_CODE_OK) {
          String payload = http.getString();

          JsonDocument doc;
          DeserializationError err = deserializeJson(doc, payload);

          if (!err && doc["current"].is<JsonObject>()) {
            float temp = doc["current"]["temperature_2m"];
            int wmoCode = doc["current"]["weather_code"];

            // Round temperature to integer
            int tempInt = (int)(temp + 0.5f);
            if (temp < 0) tempInt = (int)(temp - 0.5f);

            weatherTemp = String(tempInt);
            weatherDesc = getWeatherDescFromCode(wmoCode);

            weatherAvailable = true;
            Serial.printf("Weather OK: %d°C | %s (WMO:%d)\n", tempInt, weatherDesc.c_str(), wmoCode);
            weatherState = 2;
            stateStartTime = now;
            weatherDisplayStart = now;
            weatherShowing = true;
            Serial.println("Weather: Moving to SHOW state");
          } else {
            Serial.printf("Weather: JSON parse error: %s\n", err.c_str());
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
      // Check if display duration elapsed
      {
        unsigned long elapsed = now - weatherDisplayStart;
        if (elapsed >= WEATHER_DISPLAY_DURATION) {
          weatherShowing = false;
          display.invertDisplay(false);
          // Transition to fullscreen clock phase
          weatherState = 4;
          clockOverlayStart = now;
          clockOverlayShowing = true;
          stateStartTime = now;
          Serial.printf("Weather: Moving to CLOCK state after %lums\n", elapsed);
        }
      }
      break;

    case 4: // CLOCK - Show fullscreen clock after weather
      {
        unsigned long elapsed = now - clockOverlayStart;
        if (elapsed >= CLOCK_OVERLAY_DURATION) {
          clockOverlayShowing = false;
          weatherState = 3;
          stateStartTime = now;
          lastWeatherUpdate = now;
          Serial.printf("Clock overlay: Moving to WAIT state after %lums\n", elapsed);
        }
      }
      break;

    case 3: // WAIT - Wait before next fetch cycle
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

bool isClockOverlayShowing() {
  return clockOverlayShowing;
}

void drawClockOverlay() {
  if (!clockOverlayShowing || !displayAvailable) return;

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 10)) return;

  display.clearDisplay();
  display.setTextColor(DISPLAY_WHITE);

  // Large time - size 4 (fills screen nicely)
  display.setTextSize(4);
  char timeStr[6];
  // Blink colon every second
  char sep = (timeinfo.tm_sec % 2 == 0) ? ':' : ' ';
  sprintf(timeStr, "%02d%c%02d", timeinfo.tm_hour, sep, timeinfo.tm_min);

  int time_x = (SCREEN_WIDTH - 120) / 2;  // 5 chars * 24px = 120
  display.setCursor(time_x, 4);
  display.print(timeStr);

  // Date at bottom
  display.setTextSize(1);
  char dateStr[12];
  sprintf(dateStr, "%02d/%02d/%04d", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
  int date_x = (SCREEN_WIDTH - 60) / 2;
  display.setCursor(date_x, 54);
  display.print(dateStr);
}

void drawWeather() {
  if (!weatherShowing || !displayAvailable) return;

  unsigned long elapsed = millis() - weatherDisplayStart;
  if (elapsed > WEATHER_DISPLAY_DURATION) {
    weatherShowing = false;
    display.invertDisplay(false);
    return;
  }

  // First 2 seconds: flash by toggling invert every 500ms
  if (elapsed < 2000) {
    bool inv = ((elapsed / 500) % 2) == 0;
    display.invertDisplay(inv);
  } else {
    display.invertDisplay(true);
  }

  display.clearDisplay();
  display.setTextColor(DISPLAY_WHITE);

  // weatherTemp is already clean integer string from Open-Meteo
  String cleanTemp = weatherTemp;
  String condText = weatherDesc;

  // Top: temperature big centered (size 3)
  String tempStr = cleanTemp + "C";
  display.setTextSize(3);
  int tw = tempStr.length() * 18;
  int tx = (SCREEN_WIDTH - tw) / 2;
  if (tx < 0) tx = 0;
  display.setCursor(tx, 4);
  display.print(tempStr);
  // Degree symbol
  display.drawCircle(tx + cleanTemp.length() * 18 - 2, 2, 2, DISPLAY_WHITE);

  // Separator
  display.drawLine(0, 34, 128, 34, DISPLAY_WHITE);

  // Bottom: condition centered (size 2)
  display.setTextSize(2);
  int cw = condText.length() * 12;
  int cx = (SCREEN_WIDTH - cw) / 2;
  if (cx < 0) cx = 0;
  display.setCursor(cx, 44);
  display.print(condText);
}
