/*
 * SmallOLED-PCMonitor - Display Module
 *
 * Display initialization and global display object.
 * Supports both SSD1306 and SH1106 displays via compile-time selection.
 */

#include "display.h"
#include "../config/config.h"
#include <time.h>

// Track last applied brightness to avoid unnecessary updates
static uint8_t lastAppliedBrightness = 255;
static unsigned long lastBrightnessCheck = 0;
const unsigned long BRIGHTNESS_CHECK_INTERVAL = 60000; // Check every minute

// Goodnight animation state tracking
static bool goodnightShown = false;
static unsigned long goodnightPhaseStart = 0;
static int goodnightPhase = 0; // 0=not started, 1=showing text, 2=showing smiley, 3=done

// Initialize display - returns true on success
bool initDisplay() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

  for (int attempt = 0; attempt < 3; attempt++) {
#if DISPLAY_TYPE == 1
    // SH1106: Try 0x3C first (most common), then 0x3D
    byte addrToTry = (attempt == 0) ? DISPLAY_I2C_ADDRESS : 0x3D;
    display.begin(addrToTry);
    display.setContrast(255);
    return true;
#else
    if (display.begin(SSD1306_SWITCHCAPVCC, DISPLAY_I2C_ADDRESS)) {
      return true;
    }
#endif
    delay(500);
  }

  return false;
}

// Apply display brightness from settings
void applyDisplayBrightness() {
#if DISPLAY_TYPE == 1
  // SH1106 has built-in setContrast method
  display.setContrast(settings.displayBrightness);
#else
  // SSD1306/SSD1309: Use library's internal command method
  // Send 0x81 (contrast command) followed by value
  display.ssd1306_command(0x81);
  display.ssd1306_command(settings.displayBrightness);
#endif
}

// Check and apply time-based brightness (scheduled dimming)
void checkScheduledBrightness() {
  // Only check every minute to avoid unnecessary updates
  unsigned long currentTime = millis();
  if (currentTime - lastBrightnessCheck < BRIGHTNESS_CHECK_INTERVAL) {
    return;
  }
  lastBrightnessCheck = currentTime;

  // If scheduled dimming is disabled, ensure normal brightness is applied
  if (!settings.enableScheduledDimming) {
    if (lastAppliedBrightness != settings.displayBrightness) {
      applyDisplayBrightness();
      lastAppliedBrightness = settings.displayBrightness;
    }
    return;
  }

  // Get current time
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return; // Can't get time, skip this check
  }

  uint8_t currentHour = timeinfo.tm_hour;
  uint8_t targetBrightness;

  // Check if current time is within dim period
  // Handle wrap-around case (e.g., 22:00 to 07:00)
  if (settings.dimStartHour < settings.dimEndHour) {
    // Normal case: start and end are in same day
    // e.g., 01:00 to 07:00
    if (currentHour >= settings.dimStartHour && currentHour < settings.dimEndHour) {
      targetBrightness = settings.dimBrightness;
    } else {
      targetBrightness = settings.displayBrightness;
    }
  } else {
    // Wrap-around case: spans midnight
    // e.g., 22:00 to 07:00
    if (currentHour >= settings.dimStartHour || currentHour < settings.dimEndHour) {
      targetBrightness = settings.dimBrightness;
    } else {
      targetBrightness = settings.displayBrightness;
    }
  }

  // Apply brightness only if it changed
  if (lastAppliedBrightness != targetBrightness) {
#if DISPLAY_TYPE == 1
    display.setContrast(targetBrightness);
#else
    display.ssd1306_command(0x81);
    display.ssd1306_command(targetBrightness);
#endif
    lastAppliedBrightness = targetBrightness;
  }
}

// Check if screen should be off based on schedule
// Weekdays (Mon-Fri): Off 23:45-08:00
// Weekend (Sat-Sun): Off 01:00-13:00
// Sunday night -> Monday: Open until 01:00, then off 01:00-13:00
bool isScreenScheduledOff() {
  // Get current time
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return false; // Can't get time, assume screen should be on
  }

  uint8_t currentHour = timeinfo.tm_hour;
  uint8_t currentMinute = timeinfo.tm_min;
  uint8_t currentDayOfWeek = timeinfo.tm_wday; // 0=Sunday, 1=Monday, ..., 6=Saturday

  // Convert current time to minutes since midnight for easier comparison
  uint16_t currentTimeInMinutes = currentHour * 60 + currentMinute;

  // Monday to Friday (1-5): Screen off schedule
  if (currentDayOfWeek >= 1 && currentDayOfWeek <= 5) {
    // Monday: Off from 01:00-13:00 (Sunday night continuation) AND 23:45-00:00
    // Tuesday-Friday: Off from 23:45-13:00
    if (currentDayOfWeek == 1) {
      // Monday: Off 01:00-13:00, also off 23:45-00:00
      if ((currentTimeInMinutes >= 60 && currentTimeInMinutes < 780) ||
          currentTimeInMinutes >= 1425) {
        return true;
      }
    } else {
      // Tuesday-Friday: Off 23:45-13:00
      uint16_t weekdayOffStart = 23 * 60 + 45; // 23:45 = 1425 minutes
      uint16_t weekdayOffEnd = 13 * 60;         // 13:00 = 780 minutes

      // Handle wrap-around (23:45 to midnight to 13:00)
      if (currentTimeInMinutes >= weekdayOffStart || currentTimeInMinutes < weekdayOffEnd) {
        return true;
      }
    }
  }

  // Saturday (6): Screen off from 01:00 to 13:00
  if (currentDayOfWeek == 6) {
    uint16_t weekendOffStart = 1 * 60;        // 01:00 = 60 minutes
    uint16_t weekendOffEnd = 13 * 60;         // 13:00 = 780 minutes

    if (currentTimeInMinutes >= weekendOffStart && currentTimeInMinutes < weekendOffEnd) {
      return true;
    }
  }

  // Sunday (0): Screen open all day until Monday 01:00
  // Screen off only from 01:00 to 08:00 on Monday morning
  if (currentDayOfWeek == 0) {
    // Sunday: Screen is OPEN all day (no off period)
    return false;
  }

  return false;
}

// Handle goodnight sequence: "İyi Geceler" for 5s, then smiley for 5s
// Returns true if sequence is still running, false if done
bool handleGoodnightSequence() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return false;
  }

  uint8_t currentHour = timeinfo.tm_hour;
  uint8_t currentMinute = timeinfo.tm_min;
  uint8_t currentDayOfWeek = timeinfo.tm_wday;

  uint16_t currentTimeInMinutes = currentHour * 60 + currentMinute;

  // Check if we're at the goodnight trigger time (23:45 on weekdays)
  bool shouldTriggerGoodnight = false;
  if (currentDayOfWeek >= 1 && currentDayOfWeek <= 5) {
    // Trigger at exactly 23:45-23:59
    if (currentHour == 23 && currentMinute >= 45) {
      shouldTriggerGoodnight = true;
    }
  }

  // Reset goodnight state if it's a new day (after 08:00)
  if (currentHour >= 8 && currentHour < 23) {
    goodnightShown = false;
    goodnightPhase = 0;
    return false;
  }

  // If not trigger time and sequence not started, return
  if (!shouldTriggerGoodnight && goodnightPhase == 0) {
    return false;
  }

  // If we've already shown goodnight and past the time, reset
  if (goodnightPhase == 3 && currentHour >= 0 && currentHour < 8) {
    return false; // Already shown, waiting for morning reset
  }

  // Start goodnight sequence if triggered and not yet shown today
  if (shouldTriggerGoodnight && !goodnightShown && goodnightPhase == 0) {
    goodnightPhase = 1;
    goodnightPhaseStart = millis();
    goodnightShown = true;
    Serial.println("Goodnight sequence started");
  }

  // Process goodnight sequence
  if (goodnightPhase > 0 && goodnightPhase < 3) {
    unsigned long elapsed = millis() - goodnightPhaseStart;
    
    if (goodnightPhase == 1) {
      // Phase 1: Show "İyi Geceler" for 5 seconds
      if (elapsed < 5000) {
        display.clearDisplay();
        display.setTextColor(DISPLAY_WHITE);
        
        // Draw "İyi Geceler" centered
        display.setTextSize(2);
        const char* text = "Iyi Geceler";
        int16_t x1, y1;
        uint16_t w, h;
        display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
        display.setCursor((SCREEN_WIDTH - w) / 2, 20);
        display.print(text);
        
        // Draw moon icon
        display.setTextSize(1);
        display.setCursor(10, 8);
        display.print("C"); // Moon character
        
        display.display();
        return true; // Still running
      } else {
        // Move to phase 2 (smiley)
        goodnightPhase = 2;
        goodnightPhaseStart = millis();
        Serial.println("Goodnight phase 2: Smiley");
      }
    }
    
    if (goodnightPhase == 2) {
      // Phase 2: Show smiley face for 5 seconds
      if (elapsed < 5000) {
        display.clearDisplay();
        display.setTextColor(DISPLAY_WHITE);
        
        // Draw big smiley face
        // Circle
        int centerX = SCREEN_WIDTH / 2;
        int centerY = SCREEN_HEIGHT / 2;
        int radius = 24;
        display.drawCircle(centerX, centerY, radius, DISPLAY_WHITE);
        
        // Eyes
        display.fillCircle(centerX - 8, centerY - 8, 3, DISPLAY_WHITE);
        display.fillCircle(centerX + 8, centerY - 8, 3, DISPLAY_WHITE);
        
        // Smile (arc)
        display.drawCircle(centerX, centerY - 2, 14, DISPLAY_WHITE);
        
        // Draw "Uyku modu" text below
        display.setTextSize(1);
        const char* sleepText = "Uyku modu";
        int16_t x1, y1;
        uint16_t w, h;
        display.getTextBounds(sleepText, 0, 0, &x1, &y1, &w, &h);
        display.setCursor((SCREEN_WIDTH - w) / 2, SCREEN_HEIGHT - 12);
        display.print(sleepText);
        
        display.display();
        return true; // Still running
      } else {
        // Phase 3: Done - screen will turn off
        goodnightPhase = 3;
        display.clearDisplay();
        display.display();
        Serial.println("Goodnight sequence completed, screen off");
        return false;
      }
    }
  }

  // If sequence completed
  if (goodnightPhase == 3) {
    return false;
  }

  return false;
}
