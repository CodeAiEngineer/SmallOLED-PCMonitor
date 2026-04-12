/*
 * SmallOLED-PCMonitor - Mario Clock Implementation
 *
 * Mario clock style with animated Mario character that jumps to change digits.
 */

#include "../config/config.h"
#include "../display/display.h"
#include "clocks.h"
#include "clock_constants.h"
#include "clock_globals.h"

// Forward declarations for helper functions used by Mario clock
void drawTimeWithBounce();
void advanceDisplayedTime();
void updateSpecificDigit(int digitIndex, int newValue);

// Forward declarations for encounter functions
void updateEncounter(struct tm* timeinfo);
void drawEncounterElement();
void startEncounter();

// ========== Draw Time With Bounce Effect ==========
void drawTimeWithBounce() {
  display.setTextSize(3);

  char digits[5];
  
  // 12h/24h format support (v1.5.3)
  int hour = displayed_hour;
  bool isPM = false;
  if (!settings.use24Hour) {
    isPM = (hour >= 12);
    if (hour == 0) hour = 12;
    else if (hour > 12) hour -= 12;
  }
  
  digits[0] = '0' + (hour / 10);
  digits[1] = '0' + (hour % 10);
  digits[2] = shouldShowColon() ? ':' : ' ';  // Blinking colon
  digits[3] = '0' + (displayed_min / 10);
  digits[4] = '0' + (displayed_min % 10);

  for (int i = 0; i < 5; i++) {
    int y = TIME_Y + (int)digit_offset_y[i];
    display.setCursor(DIGIT_X[i], y);
    display.print(digits[i]);
  }
  
  // Draw AM/PM indicator (v1.5.3)
  if (!settings.use24Hour) {
    display.setTextSize(1);
    display.setCursor(115, TIME_Y + 2);
    display.print(isPM ? "P" : "A");
  }
}

// ========== Advance Displayed Time ==========
void advanceDisplayedTime() {
  displayed_min++;
  if (displayed_min >= 60) {
    displayed_min = 0;
    displayed_hour++;
    if (displayed_hour >= 24) {
      displayed_hour = 0;  // Midnight transition: 23:59 -> 00:00
    }
  }
  time_overridden = true;
  time_override_start = millis();
}

// ========== Update Specific Digit ==========
void updateSpecificDigit(int digitIndex, int newValue) {
  // Update the specific digit in displayed_hour or displayed_min
  // digitIndex corresponds to DIGIT_X array: 0=hour tens, 1=hour ones, 3=min tens, 4=min ones
  int hour_tens = displayed_hour / 10;
  int hour_ones = displayed_hour % 10;
  int min_tens = displayed_min / 10;
  int min_ones = displayed_min % 10;

  if (digitIndex == 0) {
    hour_tens = newValue;
    displayed_hour = hour_tens * 10 + hour_ones;
  } else if (digitIndex == 1) {
    hour_ones = newValue;
    displayed_hour = hour_tens * 10 + hour_ones;
  } else if (digitIndex == 3) {
    min_tens = newValue;
    displayed_min = min_tens * 10 + min_ones;
  } else if (digitIndex == 4) {
    min_ones = newValue;
    displayed_min = min_tens * 10 + min_ones;
  }

  time_overridden = true;
  time_override_start = millis();
}

// ========== Display Clock With Mario ==========
void displayClockWithMario() {
  struct tm timeinfo;
  if(!getTimeWithTimeout(&timeinfo)) {
    display.setTextSize(1);
    display.setCursor(20, 28);
    if (!ntpSynced) {
      display.print("Syncing time...");
    } else {
      display.print("Time Error");
    }
    return;
  }

  // Always use NTP time unless actively animating
  bool isAnimating = (mario_state != MARIO_IDLE);
  if (!isAnimating || !time_overridden) {
    displayed_hour = timeinfo.tm_hour;
    displayed_min = timeinfo.tm_min;
    time_overridden = false;
  }

  // Date at top
  display.setTextSize(1);
  char dateStr[12];

  switch (settings.dateFormat) {
    case 0:
      sprintf(dateStr, "%02d/%02d/%04d", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
      break;
    case 1:
      sprintf(dateStr, "%02d/%02d/%04d", timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_year + 1900);
      break;
    case 2:
      sprintf(dateStr, "%04d-%02d-%02d", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
      break;
  }

  int date_x = (SCREEN_WIDTH - DATE_DISPLAY_WIDTH) / 2;
  display.setCursor(date_x, 4);
  display.print(dateStr);

  updateDigitBounce();
  drawTimeWithBounce();

  updateMarioAnimation(&timeinfo);
  
  // Update and draw idle encounters (v1.5.2)
  if (settings.marioIdleEncountersEnabled) {
    updateEncounter(&timeinfo);
  }

  int mario_draw_y = mario_base_y + (int)mario_jump_y;
  bool isJumping = (mario_state == MARIO_JUMPING);
  drawMario((int)mario_x, mario_draw_y, mario_facing_right, mario_walk_frame, isJumping);
  
  // Draw encounter element if active
  drawEncounterElement();

  // Draw no-WiFi icon if disconnected
  if (!wifiConnected) {
    drawNoWiFiIcon(0, 0);
  }
}

// ========== Update Mario Animation ==========
void updateMarioAnimation(struct tm* timeinfo) {
  unsigned long currentMillis = millis();

  if (currentMillis - last_mario_update < MARIO_ANIM_SPEED) {
    return;
  }
  last_mario_update = currentMillis;

  int seconds = timeinfo->tm_sec;
  int current_minute = timeinfo->tm_min;

  if (current_minute != last_minute) {
    last_minute = current_minute;
    animation_triggered = false;
  }

  if (seconds >= MARIO_ANIMATION_TRIGGER_SECOND && !animation_triggered && mario_state == MARIO_IDLE) {
    animation_triggered = true;
    calculateTargetDigits(displayed_hour, displayed_min);
    if (num_targets > 0) {
      current_target_index = 0;
      mario_x = MARIO_START_X;
      mario_state = MARIO_WALKING;
      mario_facing_right = true;
      digit_bounce_triggered = false;
    }
  }

  switch (mario_state) {
    case MARIO_IDLE:
      mario_walk_frame = 0;
      mario_x = MARIO_START_X;
      
      // Check for idle encounter trigger (v1.5.2)
      if (settings.marioIdleEncountersEnabled && !encounter_active && !animation_triggered) {
        unsigned long currentMillis2 = millis();
        if (currentMillis2 >= encounter_cooldown_end) {
          startEncounter();
        }
      }
      break;

    case MARIO_WALKING:
      if (current_target_index < num_targets) {
        int target = target_x_positions[current_target_index];

        if (abs(mario_x - target) > MARIO_TARGET_PROXIMITY) {
          float walkSpeed = settings.marioWalkSpeed / 10.0f;
          if (mario_x < target) {
            mario_x += walkSpeed;
            mario_facing_right = true;
          } else {
            mario_x -= walkSpeed;
            mario_facing_right = false;
          }
          int frameCount = settings.marioSmoothAnimation ? 4 : 2;
          mario_walk_frame = (mario_walk_frame + 1) % frameCount;
        } else {
          mario_x = target;
          mario_state = MARIO_JUMPING;
          jump_velocity = JUMP_POWER;
          mario_jump_y = 0;
          digit_bounce_triggered = false;
        }
      } else {
        mario_state = MARIO_WALKING_OFF;
        mario_facing_right = true;
      }
      break;

    case MARIO_JUMPING:
      {
        jump_velocity += GRAVITY;
        mario_jump_y += jump_velocity;

        int mario_head_y = mario_base_y + (int)mario_jump_y - MARIO_HEAD_OFFSET;

        if (!digit_bounce_triggered && mario_head_y <= DIGIT_BOTTOM) {
          digit_bounce_triggered = true;
          triggerDigitBounce(target_digit_index[current_target_index]);

          // Update only the specific digit that Mario just hit
          updateSpecificDigit(target_digit_index[current_target_index],
                             target_digit_values[current_target_index]);

          jump_velocity = MARIO_BOUNCE_VELOCITY;
        }

        if (mario_jump_y >= 0) {
          mario_jump_y = 0;
          jump_velocity = 0;

          current_target_index++;

          if (current_target_index < num_targets) {
            mario_state = MARIO_WALKING;
            mario_facing_right = (target_x_positions[current_target_index] > mario_x);
            digit_bounce_triggered = false;
          } else {
            mario_state = MARIO_WALKING_OFF;
            mario_facing_right = true;
          }
        }
      }
      break;

    case MARIO_WALKING_OFF:
      mario_x += settings.marioWalkSpeed / 10.0f;
      {
        int frameCount = settings.marioSmoothAnimation ? 4 : 2;
        mario_walk_frame = (mario_walk_frame + 1) % frameCount;
      }

      if (mario_x > SCREEN_WIDTH + 15) {
        mario_state = MARIO_IDLE;
        mario_x = MARIO_START_X;
      }
      break;
  }
}

// ========== Draw Mario Sprite ==========
void drawMario(int x, int y, bool facingRight, int frame, bool jumping) {
  if (x < -10 || x > SCREEN_WIDTH + 10) return;

  int sx = x - 4;
  int sy = y - 10;

  if (jumping) {
    display.fillRect(sx + 2, sy, 4, 3, DISPLAY_WHITE);
    display.fillRect(sx + 2, sy + 3, 4, 3, DISPLAY_WHITE);
    display.drawPixel(sx + 1, sy + 2, DISPLAY_WHITE);
    display.drawPixel(sx + 6, sy + 2, DISPLAY_WHITE);
    display.drawPixel(sx + 0, sy + 1, DISPLAY_WHITE);
    display.drawPixel(sx + 7, sy + 1, DISPLAY_WHITE);
    display.fillRect(sx + 2, sy + 6, 2, 3, DISPLAY_WHITE);
    display.fillRect(sx + 4, sy + 6, 2, 3, DISPLAY_WHITE);
  } else {
    display.fillRect(sx + 2, sy, 4, 3, DISPLAY_WHITE);
    if (facingRight) {
      display.drawPixel(sx + 6, sy + 1, DISPLAY_WHITE);
    } else {
      display.drawPixel(sx + 1, sy + 1, DISPLAY_WHITE);
    }

    display.fillRect(sx + 2, sy + 3, 4, 3, DISPLAY_WHITE);

    if (settings.marioSmoothAnimation) {
      // 4-frame mode: both arms animate in opposite phase
      if (facingRight) {
        display.drawPixel(sx + 1, sy + 4 - (frame % 2), DISPLAY_WHITE);  // Back arm (opposite phase)
        display.drawPixel(sx + 6, sy + 3 + (frame % 2), DISPLAY_WHITE);  // Front arm
      } else {
        display.drawPixel(sx + 6, sy + 4 - (frame % 2), DISPLAY_WHITE);  // Back arm (opposite phase)
        display.drawPixel(sx + 1, sy + 3 + (frame % 2), DISPLAY_WHITE);  // Front arm
      }

      // 4-frame walk cycle for smoother animation
      switch (frame % 4) {
        case 0:  // Legs together (neutral)
          display.fillRect(sx + 2, sy + 6, 2, 3, DISPLAY_WHITE);
          display.fillRect(sx + 4, sy + 6, 2, 3, DISPLAY_WHITE);
          break;
        case 1:  // Left leg forward
          display.fillRect(sx + 1, sy + 6, 2, 3, DISPLAY_WHITE);
          display.fillRect(sx + 4, sy + 6, 2, 3, DISPLAY_WHITE);
          break;
        case 2:  // Legs apart (full stride)
          display.fillRect(sx + 1, sy + 6, 2, 3, DISPLAY_WHITE);
          display.fillRect(sx + 5, sy + 6, 2, 3, DISPLAY_WHITE);
          break;
        case 3:  // Right leg forward
          display.fillRect(sx + 2, sy + 6, 2, 3, DISPLAY_WHITE);
          display.fillRect(sx + 5, sy + 6, 2, 3, DISPLAY_WHITE);
          break;
      }
    } else {
      // 2-frame mode (original): back arm static, front arm moves
      if (facingRight) {
        display.drawPixel(sx + 1, sy + 4, DISPLAY_WHITE);
        display.drawPixel(sx + 6, sy + 3 + (frame % 2), DISPLAY_WHITE);
      } else {
        display.drawPixel(sx + 6, sy + 4, DISPLAY_WHITE);
        display.drawPixel(sx + 1, sy + 3 + (frame % 2), DISPLAY_WHITE);
      }

      // 2-frame walk cycle (original)
      if (frame == 0) {
        display.fillRect(sx + 2, sy + 6, 2, 3, DISPLAY_WHITE);
        display.fillRect(sx + 4, sy + 6, 2, 3, DISPLAY_WHITE);
      } else {
        display.fillRect(sx + 1, sy + 6, 2, 3, DISPLAY_WHITE);
        display.fillRect(sx + 5, sy + 6, 2, 3, DISPLAY_WHITE);
      }
    }
  }
}

// ========== Mario Idle Encounters (v1.5.2) ==========

// Draw Goomba sprite (8x8)
static void drawGoomba(int x, int y, int frame) {
  if (x < -12 || x > SCREEN_WIDTH + 12) return;
  // Body (brown-like shape)
  display.fillRect(x + 1, y + 2, 6, 4, DISPLAY_WHITE);
  display.fillRect(x + 0, y + 3, 8, 3, DISPLAY_WHITE);
  // Eyes
  display.drawPixel(x + 2, y + 3, DISPLAY_BLACK);
  display.drawPixel(x + 5, y + 3, DISPLAY_BLACK);
  // Feet (animated)
  if (frame % 2 == 0) {
    display.fillRect(x + 0, y + 6, 3, 2, DISPLAY_WHITE);
    display.fillRect(x + 5, y + 6, 3, 2, DISPLAY_WHITE);
  } else {
    display.fillRect(x + 1, y + 6, 3, 2, DISPLAY_WHITE);
    display.fillRect(x + 4, y + 6, 3, 2, DISPLAY_WHITE);
  }
}

// Draw Koopa Troopa sprite (8x10), facing direction of movement
static void drawKoopa(int x, int y, int dir, int frame) {
  if (x < -12 || x > SCREEN_WIDTH + 12) return;
  // Shell
  display.fillRect(x + 1, y + 2, 6, 6, DISPLAY_WHITE);
  // Head (facing direction of movement)
  if (dir > 0) {
    display.fillRect(x + 4, y + 0, 4, 3, DISPLAY_WHITE);
    display.drawPixel(x + 6, y + 1, DISPLAY_BLACK);  // Eye
  } else {
    display.fillRect(x + 0, y + 0, 4, 3, DISPLAY_WHITE);
    display.drawPixel(x + 1, y + 1, DISPLAY_BLACK);  // Eye
  }
  // Feet (animated)
  if (frame % 2 == 0) {
    display.fillRect(x + 1, y + 8, 2, 2, DISPLAY_WHITE);
    display.fillRect(x + 5, y + 8, 2, 2, DISPLAY_WHITE);
  } else {
    display.fillRect(x + 2, y + 8, 2, 2, DISPLAY_WHITE);
    display.fillRect(x + 4, y + 8, 2, 2, DISPLAY_WHITE);
  }
}

// Start a random encounter
void startEncounter() {
  encounter_active = true;
  encounter_type = (random(2) == 0) ? GOOMBA_TYPE : KOOPA_TYPE;
  
  // Random direction (left-to-right or right-to-left)
  encounter_element_dir = (random(2) == 0) ? 1 : -1;
  encounter_element_x = (encounter_element_dir > 0) ? -10 : SCREEN_WIDTH + 10;
  encounter_element_y = ENCOUNTER_ELEMENT_Y;
  encounter_element_frame = 0;
  encounter_start_time = millis();
  
  // Set cooldown based on frequency setting
  unsigned long cooldown;
  switch (settings.marioEncounterFrequency) {
    case 0: cooldown = random(ENCOUNTER_FREQ_RARE_MIN, ENCOUNTER_FREQ_RARE_MAX); break;
    case 1: cooldown = random(ENCOUNTER_FREQ_NORMAL_MIN, ENCOUNTER_FREQ_NORMAL_MAX); break;
    case 2: cooldown = random(ENCOUNTER_FREQ_FREQUENT_MIN, ENCOUNTER_FREQ_FREQUENT_MAX); break;
    case 3: cooldown = random(ENCOUNTER_FREQ_CHAOTIC_MIN, ENCOUNTER_FREQ_CHAOTIC_MAX); break;
    default: cooldown = random(ENCOUNTER_FREQ_NORMAL_MIN, ENCOUNTER_FREQ_NORMAL_MAX); break;
  }
  encounter_cooldown_end = millis() + cooldown;
  
  Serial.printf("Mario encounter started! Type: %s, Dir: %s\n",
                encounter_type == GOOMBA_TYPE ? "Goomba" : "Koopa",
                encounter_element_dir > 0 ? "L->R" : "R->L");
}

// Update encounter state
void updateEncounter(struct tm* timeinfo) {
  // Auto-abort at :56s to prioritize minute animation
  if (timeinfo->tm_sec >= ENCOUNTER_AUTO_ABORT_SECOND) {
    encounter_active = false;
    return;
  }

  if (!encounter_active) return;

  unsigned long currentMillis = millis();
  unsigned long encounter_elapsed = currentMillis - encounter_start_time;
  
  // Max encounter duration: 8 seconds
  if (encounter_elapsed > 8000) {
    encounter_active = false;
    Serial.println("Mario encounter ended (timeout)");
    return;
  }

  // Get encounter speed based on setting
  float speed;
  switch (settings.marioEncounterSpeed) {
    case 0: speed = ENCOUNTER_SPEED_SLOW / 10.0f; break;
    case 1: speed = ENCOUNTER_SPEED_NORMAL / 10.0f; break;
    case 2: speed = ENCOUNTER_SPEED_FAST / 10.0f; break;
    default: speed = ENCOUNTER_SPEED_NORMAL / 10.0f; break;
  }

  // Move encounter element
  encounter_element_x += speed * encounter_element_dir;
  encounter_element_frame++;

  // Check if element went off screen
  if ((encounter_element_dir > 0 && encounter_element_x > SCREEN_WIDTH + 15) ||
      (encounter_element_dir < 0 && encounter_element_x < -15)) {
    encounter_active = false;
    Serial.println("Mario encounter ended (element left screen)");
  }
}

// Draw encounter element
void drawEncounterElement() {
  if (!encounter_active) return;

  int ex = (int)encounter_element_x;
  int ey = (int)encounter_element_y;

  if (encounter_type == GOOMBA_TYPE) {
    drawGoomba(ex, ey, encounter_element_frame);
  } else {
    drawKoopa(ex, ey, encounter_element_dir, encounter_element_frame);
  }
}
