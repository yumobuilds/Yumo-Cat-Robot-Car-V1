/*
 * ╔══════════════════════════════════════════════════════════════════╗
 * ║                                                                  ║
 * ║                      YUMO  BUILDS                                ║
 * ║                    —  CAT CAR  —                                 ║
 * ║                                                                  ║
 * ║   ESP32-C3 Super Mini  |  v1.0                                   ║
 * ║   Desk Cat Pet · Neon Startup · Servo Expression Engine          ║
 * ║                                                                  ║
 * ╚══════════════════════════════════════════════════════════════════╝
 *
 * Yumo Cat Car - desk cat pet, startup
 *
 * Power on (STARTUP - plays once, in order, never triggered again after
 * this):
 *   1. Neon "YUMO BUILDS" sign flickers on the round screen.
 *   2. The cat wakes up: Sleepy (groggy sway), Surprised (jolt),
 *      Curious (look around), Happy (one full 360 spin in place),
 *      Happy (wheel twerk), Love (gentle rock forward and back).
 *   3. Settles into Idle.
 *
 * After startup (touch behavior):
 *   - Touching while Idle plays one reaction from touchReactions[]:
 *     Surprised (flashing "!!"), Curious, Happy (twerk, no spin), Love,
 *     Angry, Sad - then returns to Idle. reactCursor advances by one each
 *     touch and wraps around, so every touch is a different reaction
 *     from the last, cycling through all of them before repeating.
 *   - Sleepy and Idle are NOT touch reactions - Sleepy only happens
 *     after IDLE_HOLD_BEFORE_SLEEP_MS with no touch, and Idle is just
 *     the resting state. The Happy+360-spin combo is startup-only too;
 *     touch gets the plain Happy twerk instead.
 *   - A touch while asleep wakes it back to Idle (no reaction spent).
 *
 * Face and wheels always start and stop together: each step's wheel
 * movement runs for exactly as long as its face animation plays.
 *
 * Tuning: SPIN_360_MS is how long the spin takes for one full turn.
 * If the car turns short of 360, raise it; if it overshoots, lower it.
 *
 * Frame files needed: cat, happy, surprised, curious, love, sleepy, angry,
 * sad.
 *
 * Runs on: the CAR's ESP32-C3 Super Mini. The remote has its own, separate
 * ESP32-C3 Super Mini that runs Yumo_Remote.ino and talks to this one over
 * ESP-NOW (see README.md).
 *
 * Power: 2x 18650 3.7V lithium batteries. (The remote has its own small
 * 3.7V lithium battery.)
 *
 * Board settings (Arduino IDE) - for the CAR's ESP32:
 *   Board "ESP32C3 Dev Module", Flash size 4MB, Partition scheme "Huge APP"
 *   (the 120 animation frames need about 1.5 MB of flash)
 *
 * Tested with (CAR's ESP32):
 *   ESP32 board package (Arduino core) 2.0.18-arduino.5
 *
 * Libraries needed (tested versions):
 *   - "GFX Library for Arduino" by moononournation     1.6.9
 *   - "Adafruit SSD1306"                               2.5.17
 *   - "Adafruit GFX Library"                           1.12.6
 *   - "TJpg_Decoder" by Bodmer                         1.1.0
 *   - "ESP32Servo" by Kevin Harrington/Dlloydev        3.2.1
 *
 * ──────────────────────────────────────────────────────────────────────
 *  LICENSE
 * ──────────────────────────────────────────────────────────────────────
 *
 * Licensed under the PolyForm Noncommercial License 1.0.0.
 * See LICENSE.md in the repository root, or
 * https://polyformproject.org/licenses/noncommercial/1.0.0
 *
 * Required Notice: Copyright yumobuilds (https://github.com/yumobuilds)
 *
 */

#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TJpg_Decoder.h>
#include <ESP32Servo.h>
#include <math.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>   // ADDED: explicit ESP-NOW channel selection
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include "cat_frames.h"
#include "happy_frames.h"
#include "surprised_frames.h"
#include "sleepy_frames.h"
#include "angry_frames.h"
#include "sad_frames.h"
#include "curious_frames.h"
#include "love_frames.h"

#define TFT_BLK   2
#define TFT_DC    3
#define TOUCH_PIN 4
#define TFT_RST   5
#define TFT_SCLK  6
#define TFT_MOSI  7
#define I2C_SDA   8
#define I2C_SCL   9
#define TFT_CS    10

const int PIN_SERVO_FL = 0;
const int PIN_SERVO_BL = 1;
const int PIN_SERVO_BR = 20;
const int PIN_SERVO_FR = 21;

Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI, -1);
Arduino_GFX *gfx = new Arduino_GC9A01(bus, TFT_RST, 2, true);

#define OLED_W 128
#define OLED_H 32
#define OLED_ADDR_1 0x3C
#define OLED_ADDR_2 0x3D

Adafruit_SSD1306 oled1(OLED_W, OLED_H, &Wire, -1);
Adafruit_SSD1306 oled2(OLED_W, OLED_H, &Wire, -1);

Servo servoFL, servoFR, servoBL, servoBR;

const int SERVO_STOP       = 1500;
const int SERVO_TINY_FWD   = 1560;
const int SERVO_TINY_REV   = 1440;
const int SERVO_FWD_GENTLE = 1620;
const int SERVO_FWD_FAST   = 1750;
const int SERVO_REV_GENTLE = 1380;
const int SERVO_REV_FAST   = 1250;

// Dedicated turn speed for tankTurnCW() only - slower than SERVO_FWD_FAST/
// SERVO_REV_FAST so the wheels grip the desk instead of skidding, which is
// likely why the full-speed turn was only covering about a quarter turn.
// Nothing else (Surprised's jolt, Curious's look-around) uses these, so
// tuning this won't change those moves.
const int SERVO_TURN_FWD = 1650;
const int SERVO_TURN_REV = 1350;

const unsigned long SPIN_360_MS = 1800;
// Happy step tuning: forward, then backward the same distance, then tank turn.
// Forward and backward use the same speed and time, so they cover equal distance.
const unsigned long HAPPY_FWD_MS  = 250;    // drive forward
const unsigned long HAPPY_BACK_MS = 250;    // drive backward (keep equal to FWD)
const unsigned long HAPPY_TURN_MS = 9000;   // tank turn - re-time this: see calibration steps
const unsigned long SPLASH_TOTAL_MS = 2600;
const unsigned long IDLE_HOLD_BEFORE_SLEEP_MS = 15000;
const unsigned long TOUCH_DEBOUNCE_MS = 50;
const int FACE_LOOP_FOREVER = -1;

const uint16_t NEON_CYAN         = 0x07FF;
const uint16_t NEON_CYAN_DIM     = 0x0410;
const uint16_t NEON_CYAN_GLOW    = 0x0208;
const uint16_t NEON_MAGENTA      = 0xF81F;
const uint16_t NEON_MAGENTA_DIM  = 0x8010;
const uint16_t NEON_MAGENTA_GLOW = 0x4008;

void setServo(Servo &s, int pulseUs) {
  s.writeMicroseconds(pulseUs);
}

void stopAllServos() {
  setServo(servoFL, SERVO_STOP);
  setServo(servoFR, SERVO_STOP);
  setServo(servoBL, SERVO_STOP);
  setServo(servoBR, SERVO_STOP);
}

void driveForward() {
  setServo(servoFL, SERVO_FWD_GENTLE); setServo(servoFR, SERVO_FWD_GENTLE);
  setServo(servoBL, SERVO_FWD_GENTLE); setServo(servoBR, SERVO_FWD_GENTLE);
}

void driveBackward() {
  setServo(servoFL, SERVO_REV_GENTLE); setServo(servoFR, SERVO_REV_GENTLE);
  setServo(servoBL, SERVO_REV_GENTLE); setServo(servoBR, SERVO_REV_GENTLE);
}

void driveBackwardFast() {
  setServo(servoFL, SERVO_REV_FAST); setServo(servoFR, SERVO_REV_FAST);
  setServo(servoBL, SERVO_REV_FAST); setServo(servoBR, SERVO_REV_FAST);
}

void creepForward() {
  setServo(servoFL, SERVO_TINY_FWD); setServo(servoFR, SERVO_TINY_FWD);
  setServo(servoBL, SERVO_TINY_FWD); setServo(servoBR, SERVO_TINY_FWD);
}

void creepBackward() {
  setServo(servoFL, SERVO_TINY_REV); setServo(servoFR, SERVO_TINY_REV);
  setServo(servoBL, SERVO_TINY_REV); setServo(servoBR, SERVO_TINY_REV);
}

void rotateCW() {
  setServo(servoFL, SERVO_FWD_FAST); setServo(servoBL, SERVO_FWD_FAST);
  setServo(servoFR, SERVO_REV_FAST); setServo(servoBR, SERVO_REV_FAST);
}

void rotateCCW() {
  setServo(servoFL, SERVO_REV_FAST); setServo(servoBL, SERVO_REV_FAST);
  setServo(servoFR, SERVO_FWD_FAST); setServo(servoBR, SERVO_FWD_FAST);
}

// Tank turn used by the Happy step. If the car drives STRAIGHT instead of
// turning, set SIDES_MIRRORED to true (right servos are mounted mirrored,
// so the turn needs the SAME pulse on both sides). If it turns the wrong
// way (counter-clockwise), swap SERVO_FWD_FAST and SERVO_REV_FAST below.
const bool SIDES_MIRRORED = true;

void tankTurnCW() {
  int left  = SERVO_TURN_FWD;
  int right = SIDES_MIRRORED ? SERVO_TURN_FWD : SERVO_TURN_REV;
  setServo(servoFL, left);  setServo(servoBL, left);
  setServo(servoFR, right); setServo(servoBR, right);
}

// Real straight driving for this car (respects mirrored servos).
// dir > 0 = forward, dir < 0 = backward. Same speed both ways.
void driveStraight(int dir) {
  int left  = dir > 0 ? SERVO_FWD_GENTLE : SERVO_REV_GENTLE;
  int right = SIDES_MIRRORED ? (dir > 0 ? SERVO_REV_GENTLE : SERVO_FWD_GENTLE) : left;
  setServo(servoFL, left);  setServo(servoBL, left);
  setServo(servoFR, right); setServo(servoBR, right);
}

enum Emotion {
  EMOTION_IDLE = 0,
  EMOTION_HAPPY,
  EMOTION_SURPRISED,
  EMOTION_CURIOUS,
  EMOTION_LOVE,
  EMOTION_SLEEPY,
  EMOTION_ANGRY,
  EMOTION_SAD,
  EMOTION_COUNT
};

enum MoveType {
  MOVE_NONE = 0,
  MOVE_SWAY,
  MOVE_JOLT,
  MOVE_LOOK_AROUND,
  MOVE_SPIN,
  MOVE_TWERK,
  MOVE_TWERK_SPIN,
  MOVE_ROCK,
  MOVE_ANGRY,
  MOVE_SAD
};

void drawFrame(Emotion e, int frameIndex);
void startFace(Emotion e, int loops, unsigned long loopMs);
void updateFace();
void moveFor(MoveType m, unsigned long elapsedMs);
void startPhasePlayback(int index);
void enterIdle();
void enterSleep();
void drawPlateText(const char *text, uint8_t textSize, int16_t topY);
void drawPlateBlank();
void drawPlateHeartbeat(int level);
void drawPlateSleepZZZ(int step);
int sleepZStep(unsigned long nowMs);
void drawPlateFlash(const char *text, uint8_t textSize, int16_t topY, unsigned long elapsedMs);
void printCenteredOLEDAt(Adafruit_SSD1306 &d, const char *text, const GFXfont *font, uint8_t textSize, int16_t x, int16_t topY);

struct FaceAnim {
  const char* name;
  const uint8_t* const* frames;
  const uint32_t* sizes;
  int count;
  float fps;
};

FaceAnim faces[EMOTION_COUNT] = {
  { "Idle",      cat_frames,       cat_frame_sizes,       CAT_FRAME_COUNT,       12.5f },
  { "Happy",     happy_frames,     happy_frame_sizes,     HAPPY_FRAME_COUNT,     9.0f  },
  { "Surprised", surprised_frames, surprised_frame_sizes, SURPRISED_FRAME_COUNT, 24.0f },
  { "Curious",   curious_frames,   curious_frame_sizes,   CURIOUS_FRAME_COUNT,   8.0f  },
  { "Love",      love_frames,      love_frame_sizes,      LOVE_FRAME_COUNT,      5.0f  },
  { "Sleepy",    sleepy_frames,    sleepy_frame_sizes,    SLEEPY_FRAME_COUNT,    3.0f  },
  { "Angry",     angry_frames,     angry_frame_sizes,     ANGRY_FRAME_COUNT,     10.0f },
  { "Sad",       sad_frames,       sad_frame_sizes,       SAD_FRAME_COUNT,       6.0f  },
};

enum PlateAnim { PLATE_STATIC, PLATE_ZZZ, PLATE_FLASH };

struct StartupPhase {
  Emotion emotion;
  int loops;
  unsigned long loopMs;
  MoveType move;
  const char *plateText;
  uint8_t plateTextSize;
  int16_t plateTopY;
  PlateAnim plateAnim; // PLATE_ZZZ: rising Z-Z-Z. PLATE_FLASH: plateText blinks. PLATE_STATIC: plateText, still.
};

// Shared by both the boot-time STARTUP run and touch-triggered reactions
// (STATE_REACT) - startPhasePlayback()/updatePhasePlayback() below just
// play whichever table activePhaseTable currently points at.

const StartupPhase startupPhases[] = {
  { EMOTION_SLEEPY,    1, 2400,        MOVE_SWAY,        "ZZZ...",   1, 10, PLATE_ZZZ   },
  { EMOTION_SURPRISED, 1, 0,           MOVE_JOLT,        "!!",       2, 8,  PLATE_FLASH },
  { EMOTION_CURIOUS,   1, 0,           MOVE_LOOK_AROUND, "MRRP?",    2, 8,  PLATE_STATIC},
  { EMOTION_HAPPY,     3, (HAPPY_FWD_MS + HAPPY_BACK_MS + HAPPY_TURN_MS) / 3, MOVE_TWERK_SPIN, "WEEE!", 2, 8, PLATE_STATIC },
  { EMOTION_HAPPY,     2, 0,           MOVE_TWERK,       "WEEE!",    2, 8,  PLATE_STATIC},
  { EMOTION_LOVE,      1, 0,           MOVE_ROCK,        "PURRR <3", 2, 8,  PLATE_STATIC},
};
const int STARTUP_PHASE_COUNT = sizeof(startupPhases) / sizeof(startupPhases[0]);

// Touch reactions (STATE_REACT) - a separate, smaller pool from the boot
// sequence above:
//   - Sleepy is left out: sleep only happens after Idle times out, not
//     as something touch can trigger.
//   - The Happy+360-spin combo (MOVE_TWERK_SPIN) is left out: that turn
//     is the startup's signature move, not a touch reaction. The plain
//     Happy twerk (MOVE_TWERK, no spin) is kept.
//   - Angry is added here only - it never appears in the wake-up story.
const StartupPhase touchReactions[] = {
  { EMOTION_SURPRISED, 1, 0, MOVE_JOLT,        "!!",       2, 8, PLATE_FLASH },
  { EMOTION_CURIOUS,   1, 0, MOVE_LOOK_AROUND, "MRRP?",    2, 8, PLATE_STATIC},
  { EMOTION_HAPPY,     2, 0, MOVE_TWERK,       "WEEE!",    2, 8, PLATE_STATIC},
  { EMOTION_LOVE,      1, 0, MOVE_ROCK,        "PURRR <3", 2, 8, PLATE_STATIC},
  { EMOTION_ANGRY,     1, 0, MOVE_ANGRY,       "HISS!",    2, 8, PLATE_STATIC},
  { EMOTION_SAD,       1, 0, MOVE_SAD,         "MEW...",   2, 8, PLATE_STATIC},
};
const int TOUCH_REACTION_COUNT = sizeof(touchReactions) / sizeof(touchReactions[0]);

const StartupPhase *activePhaseTable = startupPhases;
int activePhaseCount = STARTUP_PHASE_COUNT;

enum State { STATE_SPLASH, STATE_STARTUP, STATE_IDLE, STATE_SLEEP, STATE_REACT };
State state = STATE_SPLASH;

unsigned long splashStartTime = 0;
int splashLevel = -1;

int phaseIndex = 0;
unsigned long phaseStartTime = 0;

Emotion faceEmotion = EMOTION_IDLE;
int faceFrame = 0;
int faceLoopsLeft = FACE_LOOP_FOREVER;
unsigned long faceIntervalMs = 80;
unsigned long faceLastFrameTime = 0;
bool faceDone = true;

unsigned long idleStartTime = 0;
unsigned long lastTouchTime = 0;
bool touchArmed = true;

unsigned long lastPlateUpdate = 0;
const unsigned long PLATE_UPDATE_INTERVAL_MS = 40;

// Which touchReactions[] entry plays next on touch. Advances by one each
// touch and wraps with %, so touches never repeat the last reaction and
// cycle through every one of them before looping back to the start.
int reactCursor = 0;

bool jpegOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
  gfx->draw16bitRGBBitmap(x, y, bitmap, w, h);
  return true;
}

void drawFrame(Emotion e, int frameIndex) {
  const uint8_t *data = (const uint8_t *)pgm_read_ptr(&faces[e].frames[frameIndex]);
  uint32_t size = pgm_read_dword(&faces[e].sizes[frameIndex]);
  TJpgDec.drawJpg(0, 0, data, size);
}

void startFace(Emotion e, int loops, unsigned long loopMs) {
  faceEmotion = e;
  faceFrame = 0;
  faceLoopsLeft = loops;
  if (loopMs > 0 && faces[e].count > 0) {
    faceIntervalMs = loopMs / faces[e].count;
  } else {
    faceIntervalMs = (unsigned long)(1000.0f / faces[e].fps);
  }
  faceLastFrameTime = millis();
  faceDone = (faces[e].count == 0);
  if (!faceDone) drawFrame(e, 0);
}

void updateFace() {
  if (faceDone) return;
  unsigned long now = millis();
  if (now - faceLastFrameTime < faceIntervalMs) return;
  faceLastFrameTime = now;
  faceFrame++;

  if (faceFrame >= faces[faceEmotion].count) {
    if (faceLoopsLeft == FACE_LOOP_FOREVER) {
      faceFrame = 0;
    } else if (--faceLoopsLeft > 0) {
      faceFrame = 0;
    } else {
      faceDone = true;
      return;
    }
  }
  drawFrame(faceEmotion, faceFrame);
}

void drawNeonText(const char *text, const GFXfont *font, int16_t topY, uint16_t glow, uint16_t core, bool withGlow) {
  gfx->setFont(font);
  gfx->setTextSize(1);
  int16_t x1, y1;
  uint16_t w, h;
  gfx->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  int16_t x = (gfx->width() - (int16_t)w) / 2 - x1;
  int16_t y = topY - y1;

  if (withGlow) {
    gfx->setTextColor(glow);
    for (int dx = -2; dx <= 2; dx += 4) {
      for (int dy = -2; dy <= 2; dy += 4) {
        gfx->setCursor(x + dx, y + dy);
        gfx->print(text);
      }
    }
  }
  gfx->setTextColor(core);
  gfx->setCursor(x, y);
  gfx->print(text);
}

void drawNeonSplash(int level) {
  gfx->fillScreen(RGB565_BLACK);

  if (level == 0) {
    drawPlateBlank();
    return;
  }

  bool lit = (level == 2);
  int cx = gfx->width() / 2;
  int cy = gfx->height() / 2;
  uint16_t magenta = lit ? NEON_MAGENTA : NEON_MAGENTA_DIM;
  uint16_t cyan = lit ? NEON_CYAN : NEON_CYAN_DIM;

  if (lit) {
    gfx->drawCircle(cx, cy, 108, NEON_MAGENTA_GLOW);
    gfx->drawCircle(cx, cy, 115, NEON_MAGENTA_GLOW);
  }
  gfx->drawCircle(cx, cy, 111, magenta);
  gfx->drawCircle(cx, cy, 112, magenta);

  drawNeonText("YUMO",   &FreeSansBold24pt7b, 60,  NEON_CYAN_GLOW,    cyan,    lit);
  drawNeonText("BUILDS", &FreeSansBold18pt7b, 140, NEON_MAGENTA_GLOW, magenta, lit);

  drawPlateText("YUMO BUILDS", 1, 10);
}

int splashLevelAt(unsigned long t) {
  if (t < 250) return 0;
  if (t < 400) return 1;
  if (t < 480) return 2;
  if (t < 560) return 1;
  if (t < 700) return 2;
  if (t < 780) return 0;
  return 2;
}

void updateSplash() {
  unsigned long t = millis() - splashStartTime;
  int level = splashLevelAt(t);
  if (level != splashLevel) {
    splashLevel = level;
    drawNeonSplash(level);
  }
  if (t >= SPLASH_TOTAL_MS) {
    state = STATE_STARTUP;
    activePhaseTable = startupPhases;
    activePhaseCount = STARTUP_PHASE_COUNT;
    startPhasePlayback(0);
  }
}

void moveFor(MoveType m, unsigned long elapsedMs) {
  switch (m) {
    case MOVE_SWAY: {
      if (elapsedMs % 1000 < 500) {
        creepForward();
      } else {
        creepBackward();
      }
      break;
    }

    case MOVE_JOLT: {
      if (elapsedMs < 150) {
        driveBackwardFast();
      } else if (elapsedMs < 250) {
        rotateCW();
      } else {
        stopAllServos();
      }
      break;
    }

    case MOVE_LOOK_AROUND: {
      if (elapsedMs < 200) {
        rotateCW();
      } else if (elapsedMs < 400) {
        rotateCCW();
      } else if (elapsedMs < 600) {
        rotateCCW();
      } else if (elapsedMs < 800) {
        rotateCW();
      } else if (elapsedMs < 1100) {
        driveForward();
      } else if (elapsedMs < 1200) {
        stopAllServos();
      } else {
        driveBackward();
      }
      break;
    }

    case MOVE_SPIN:
      rotateCW();
      break;

    case MOVE_TWERK: {
      unsigned long t = elapsedMs % 500;
      if (t < 150) {
        driveForward();
      } else if (t < 250) {
        rotateCW();
      } else if (t < 350) {
        rotateCCW();
      } else {
        driveBackward();
      }
      break;
    }

    case MOVE_TWERK_SPIN: {
      // Happy: forward, then backward the same distance, then one full
      // clockwise tank turn, then stop.
      if (elapsedMs < HAPPY_FWD_MS) {
        driveStraight(+1);
      } else if (elapsedMs < HAPPY_FWD_MS + HAPPY_BACK_MS) {
        driveStraight(-1);
      } else if (elapsedMs < HAPPY_FWD_MS + HAPPY_BACK_MS + HAPPY_TURN_MS) {
        tankTurnCW();
      } else {
        stopAllServos();
      }
      break;
    }

    case MOVE_ROCK: {
      if (elapsedMs % 800 < 400) {
        driveForward();
      } else {
        driveBackward();
      }
      break;
    }

    case MOVE_ANGRY: {
      if (elapsedMs < 150) {
        driveBackwardFast();
      } else {
        unsigned long t = (elapsedMs - 150) % 200;
        if (t < 100) {
          rotateCW();
        } else {
          rotateCCW();
        }
      }
      break;
    }

    case MOVE_SAD: {
      // Retreats, turns away, then sits still and droops.
      if (elapsedMs < 500) {
        driveBackward();
      } else if (elapsedMs < 900) {
        rotateCCW();
      } else {
        stopAllServos();
      }
      break;
    }

    default:
      stopAllServos();
      break;
  }
}

void startPhasePlayback(int index) {
  phaseIndex = index;
  phaseStartTime = millis();
  const StartupPhase &p = activePhaseTable[index];
  startFace(p.emotion, p.loops, p.loopMs);
  if (p.plateAnim == PLATE_STATIC) {
    drawPlateText(p.plateText, p.plateTextSize, p.plateTopY);
  } else {
    lastPlateUpdate = 0; // ZZZ/FLASH are drawn every frame in updatePhasePlayback()
  }
  Serial.print("Playing: ");
  Serial.println(faces[p.emotion].name);
}

void updatePhasePlayback() {
  updateFace();
  const StartupPhase &p = activePhaseTable[phaseIndex];
  if (faceDone) {
    stopAllServos();
    // Only the boot-time STARTUP run chains to the next phase; a touch
    // reaction (STATE_REACT) always plays just the one phase, then Idle.
    if (state == STATE_STARTUP && phaseIndex + 1 < activePhaseCount) {
      startPhasePlayback(phaseIndex + 1);
    } else {
      enterIdle();
    }
  } else {
    unsigned long elapsed = millis() - phaseStartTime;
    moveFor(p.move, elapsed);
    if (p.plateAnim != PLATE_STATIC) {
      unsigned long now = millis();
      if (now - lastPlateUpdate >= PLATE_UPDATE_INTERVAL_MS) {
        lastPlateUpdate = now;
        if (p.plateAnim == PLATE_ZZZ) {
          drawPlateSleepZZZ(sleepZStep(elapsed));
        } else {
          drawPlateFlash(p.plateText, p.plateTextSize, p.plateTopY, elapsed);
        }
      }
    }
  }
}

void enterIdle() {
  state = STATE_IDLE;
  stopAllServos();
  startFace(EMOTION_IDLE, FACE_LOOP_FOREVER, 0);
  idleStartTime = millis();
  lastPlateUpdate = 0;
  Serial.println("Idle");
}

void enterSleep() {
  state = STATE_SLEEP;
  stopAllServos();
  startFace(EMOTION_SLEEPY, FACE_LOOP_FOREVER, 0);
  lastPlateUpdate = 0;
  Serial.println("Sleep");
}

void handleTouch() {
  static int touchCounter = 0;
  const int TOUCH_CONFIRM_THRESHOLD = 6;
  const int WAKE_CONFIRM_THRESHOLD  = 3;
  const int confirmThreshold = (state == STATE_SLEEP) ? WAKE_CONFIRM_THRESHOLD : TOUCH_CONFIRM_THRESHOLD;

  if (digitalRead(TOUCH_PIN) == HIGH) {
    touchCounter = min(touchCounter + 1, confirmThreshold);
  } else {
    touchCounter = max(touchCounter - 1, 0);
  }

  bool touched = (touchCounter >= confirmThreshold);
  unsigned long now = millis();

  if (touched && touchArmed && (now - lastTouchTime) > TOUCH_DEBOUNCE_MS) {
    lastTouchTime = now;
    touchArmed = false;

    if (state == STATE_SLEEP) {
      Serial.println("Touch: waking up");
      enterIdle();
    } else if (state == STATE_IDLE) {
      Serial.print("Touch: reaction ");
      Serial.println(reactCursor);
      state = STATE_REACT;
      activePhaseTable = touchReactions;
      activePhaseCount = TOUCH_REACTION_COUNT;
      startPhasePlayback(reactCursor);
      reactCursor = (reactCursor + 1) % TOUCH_REACTION_COUNT;
    }
  }
  if (!touched) touchArmed = true;
}

const int HEART_W = 20;
const int HEART_H = 18;
bool heartMask[HEART_H][HEART_W];

void buildHeartMask() {
  for (int y = 0; y < HEART_H; y++) {
    for (int x = 0; x < HEART_W; x++) {
      float nx = (x - HEART_W / 2.0f) / (HEART_W / 2.3f);
      float ny = -(y - HEART_H / 2.3f) / (HEART_H / 2.3f);
      float eq = powf(nx * nx + ny * ny - 1.0f, 3) - nx * nx * ny * ny * ny;
      heartMask[y][x] = (eq <= 0.0f);
    }
  }
}

const uint8_t BAYER4[4][4] = {
  { 0,  8,  2, 10 },
  { 12, 4, 14,  6 },
  { 3, 11,  1,  9 },
  { 15, 7, 13,  5 },
};

const unsigned long HEARTBEAT_PERIOD_MS = 1200;

int heartbeatLevel(unsigned long nowMs) {
  unsigned long t = nowMs % HEARTBEAT_PERIOD_MS;
  float phase = t / (float)HEARTBEAT_PERIOD_MS;
  float brightness = (1.0f - cosf(phase * 2.0f * (float)PI)) / 2.0f;
  return (int)(brightness * 16.0f);
}

void printCenteredOLEDAt(Adafruit_SSD1306 &d, const char *text, const GFXfont *font, uint8_t textSize, int16_t x, int16_t topY) {
  d.setFont(font);
  d.setTextSize(textSize);
  d.setTextColor(SSD1306_WHITE);
  int16_t x1, y1;
  uint16_t w, h;
  d.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  int16_t y = topY - y1;
  d.setCursor(x - x1, y);
  d.print(text);
}

void printCenteredOLED(Adafruit_SSD1306 &d, const char *text, const GFXfont *font, uint8_t textSize, int16_t topY) {
  d.setFont(font);
  d.setTextSize(textSize);
  d.setTextColor(SSD1306_WHITE);
  int16_t x1, y1;
  uint16_t w, h;
  d.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  int16_t x = (OLED_W - (int16_t)w) / 2 - x1;
  int16_t y = topY - y1;
  d.setCursor(x, y);
  d.print(text);
}

void drawPlateText(const char *text, uint8_t textSize, int16_t topY) {
  oled1.clearDisplay();
  printCenteredOLED(oled1, text, &FreeSansBold9pt7b, textSize, topY);
  oled1.display();

  oled2.clearDisplay();
  printCenteredOLED(oled2, text, &FreeSansBold9pt7b, textSize, topY);
  oled2.display();
}

void drawPlateBlank() {
  oled1.clearDisplay();
  oled1.display();
  oled2.clearDisplay();
  oled2.display();
}

void drawPlateHeartbeat(int level) {
  int ox = (OLED_W - HEART_W) / 2;
  int oy = (OLED_H - HEART_H) / 2;

  oled1.clearDisplay();
  oled2.clearDisplay();
  for (int y = 0; y < HEART_H; y++) {
    for (int x = 0; x < HEART_W; x++) {
      if (heartMask[y][x] && BAYER4[y % 4][x % 4] < level) {
        oled1.drawPixel(ox + x, oy + y, SSD1306_WHITE);
        oled2.drawPixel(ox + x, oy + y, SSD1306_WHITE);
      }
    }
  }
  oled1.display();
  oled2.display();
}

const unsigned long ZZZ_CYCLE_MS = 2400;
const unsigned long ZZZ_STEP_MS = ZZZ_CYCLE_MS / 4;

int sleepZStep(unsigned long nowMs) {
  return (nowMs % ZZZ_CYCLE_MS) / ZZZ_STEP_MS;
}

void drawPlateSleepZZZ(int step) {
  oled1.clearDisplay();
  oled2.clearDisplay();

  if (step >= 0) {
    printCenteredOLEDAt(oled1, "Z", &FreeSansBold9pt7b, 1, 6, 18);
    printCenteredOLEDAt(oled2, "Z", &FreeSansBold9pt7b, 1, 6, 18);
  }
  if (step >= 1) {
    printCenteredOLEDAt(oled1, "Z", &FreeSansBold9pt7b, 1, 42, 13);
    printCenteredOLEDAt(oled2, "Z", &FreeSansBold9pt7b, 1, 42, 13);
  }
  if (step >= 2) {
    printCenteredOLEDAt(oled1, "Z", &FreeSansBold9pt7b, 2, 84, 0);
    printCenteredOLEDAt(oled2, "Z", &FreeSansBold9pt7b, 2, 84, 0);
  }
  // step 3: brief blank pause before the cycle restarts

  oled1.display();
  oled2.display();
}

const unsigned long FLASH_INTERVAL_MS = 120;

void drawPlateFlash(const char *text, uint8_t textSize, int16_t topY, unsigned long elapsedMs) {
  bool on = ((elapsedMs / FLASH_INTERVAL_MS) % 2) == 0;
  oled1.clearDisplay();
  oled2.clearDisplay();
  if (on) {
    printCenteredOLED(oled1, text, &FreeSansBold9pt7b, textSize, topY);
    printCenteredOLED(oled2, text, &FreeSansBold9pt7b, textSize, topY);
  }
  oled1.display();
  oled2.display();
}


// ══════════════════════════════════════════════════════════════════════
// ADDITION: REMOTE JOYSTICK RECEIVER
// This section does NOT replace touch control or the existing animations.
// It only listens for joystick packets from the future remote ESP32.
// ══════════════════════════════════════════════════════════════════════

struct JoystickPacket {
  int16_t x;
  int16_t y;
  uint8_t button;
};

volatile JoystickPacket joystickPacket = {0, 0, 0};
volatile unsigned long joystickLastPacketTime = 0;

const int JOYSTICK_CENTER = 2048;
const int JOYSTICK_DEADZONE = 350;
const unsigned long JOYSTICK_TIMEOUT_MS = 300;

// ──────────────────────────────────────────────────────────────────────
// ADDITION: Automatic ESP-NOW pairing protocol (shared with Yumo_Remote.ino)
// The two boards must use IDENTICAL definitions for the packet below.
// ──────────────────────────────────────────────────────────────────────
#define YUMO_ESPNOW_CHANNEL 1                 // both boards use this Wi-Fi channel
#define YUMO_TX_POWER WIFI_POWER_8_5dBm       // ESP32-C3 Super Mini antennas dislike full power
const uint8_t YUMO_ID[4] = { 'Y', 'U', 'M', 'O' };

enum YumoPacketType : uint8_t {
  PACKET_PAIR_REQUEST = 1,   // remote -> broadcast : "is there a Cat Car?"
  PACKET_PAIR_ACK     = 2,   // car    -> remote    : "yes, I am here"
  PACKET_JOYSTICK     = 3,   // remote -> car       : joystick x/y/button
  PACKET_LINK_ACK     = 4    // car    -> remote    : heartbeat, link is alive
};

struct __attribute__((packed)) YumoPacket {
  uint8_t id[4];     // "YUMO"
  uint8_t type;      // YumoPacketType
  uint8_t button;    // 1 = pressed (transmitted only; no function assigned)
  int16_t x;         // 0..4095 raw ADC
  int16_t y;         // 0..4095 raw ADC
};
static_assert(sizeof(YumoPacket) == 10, "YumoPacket size mismatch between car and remote");

uint8_t remoteMac[6] = {0, 0, 0, 0, 0, 0};    // remembered after a valid pair request
volatile bool remotePaired = false;
bool remoteLinkReady = false;                 // true once esp_now_init() succeeded

volatile bool pendingPairAck = false;         // set in the callback, handled in loop()
uint8_t pendingPairMac[6] = {0, 0, 0, 0, 0, 0};
volatile bool pendingLinkAck = false;
unsigned long lastLinkAckTime = 0;

const unsigned long LINK_ACK_INTERVAL_MS = 100;   // heartbeat rate back to the remote
const unsigned long REMOTE_LINK_HOLD_MS  = 1500;  // a live remote can't be replaced by another

portMUX_TYPE yumoMux = portMUX_INITIALIZER_UNLOCKED;

// Runs in the Wi-Fi task: keep it short, no Serial, no esp_now_send/add_peer.
void onJoystickDataRecv(const esp_now_recv_info_t *recvInfo,
                        const uint8_t *incomingData,
                        int len) {
  if (len != (int)sizeof(YumoPacket)) return;

  YumoPacket packet;
  memcpy(&packet, incomingData, sizeof(packet));
  if (memcmp(packet.id, YUMO_ID, sizeof(YUMO_ID)) != 0) return;   // not one of ours

  const uint8_t *src = recvInfo->src_addr;

  if (packet.type == PACKET_PAIR_REQUEST) {
    portENTER_CRITICAL(&yumoMux);
    memcpy(pendingPairMac, src, 6);
    pendingPairAck = true;
    portEXIT_CRITICAL(&yumoMux);
    return;
  }

  if (packet.type == PACKET_JOYSTICK) {
    bool fromPairedRemote;
    portENTER_CRITICAL(&yumoMux);
    fromPairedRemote = remotePaired && (memcmp(src, remoteMac, 6) == 0);
    portEXIT_CRITICAL(&yumoMux);
    if (!fromPairedRemote) return;   // unknown sender: remote will time out and re-pair

    // (volatile struct cannot be assigned as a whole in C++, so field by field)
    joystickPacket.x = packet.x;
    joystickPacket.y = packet.y;
    joystickPacket.button = packet.button;
    unsigned long t = millis();
    joystickLastPacketTime = (t == 0) ? 1 : t;   // 0 is reserved for "never received"
    pendingLinkAck = true;
  }
}

bool yumoSend(const uint8_t *mac, uint8_t type) {
  YumoPacket p;
  memcpy(p.id, YUMO_ID, sizeof(YUMO_ID));
  p.type = type;
  p.button = 0;
  p.x = 0;
  p.y = 0;
  return esp_now_send(mac, (const uint8_t *)&p, sizeof(p)) == ESP_OK;
}

bool yumoAddPeer(const uint8_t *mac) {
  if (esp_now_is_peer_exist(mac)) return true;
  esp_now_peer_info_t peer;
  memset(&peer, 0, sizeof(peer));
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = YUMO_ESPNOW_CHANNEL;
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;
  return esp_now_add_peer(&peer) == ESP_OK;
}

void setupJoystickReceiver() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);   // keep the radio awake so packets are never missed
  WiFi.setTxPower(YUMO_TX_POWER);

  // Force the same Wi-Fi channel as the remote.
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(YUMO_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  esp_now_register_recv_cb(onJoystickDataRecv);
  remoteLinkReady = true;

  Serial.print("Car ESP32 MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.print("ESP-NOW channel: ");
  Serial.println(YUMO_ESPNOW_CHANNEL);
}

// ── Plate banner: "REMOTE FOUND" / "REMOTE CONNECTED" with a flashing frame ──
// kind 1 = car heard a pair request (diagnostic), kind 2 = remote is live.
// Only shown in IDLE/SLEEP, where the plate is redrawn constantly anyway, so
// nothing the startup/reaction animations drew is ever lost.
const unsigned long REMOTE_BANNER_MS = 2000;
const unsigned long REMOTE_BANNER_FLASH_MS = 120;
uint8_t remoteBannerKind = 0;          // 0 = none
bool remoteBannerActive = false;
unsigned long remoteBannerStart = 0;
long remoteBannerLastPhase = -1;
bool remoteConnectedShown = false;

void plateCenterDefaultFont(Adafruit_SSD1306 &d, const char *text, uint8_t size, int16_t y) {
  d.setFont(NULL);
  d.setTextSize(size);
  d.setTextColor(SSD1306_WHITE);
  int16_t x1, y1;
  uint16_t w, h;
  d.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  d.setCursor((OLED_W - (int16_t)w) / 2 - x1, y);
  d.print(text);
}

void drawPlateRemoteBanner(uint8_t kind, bool frameOn) {
  Adafruit_SSD1306 *plates[2] = { &oled1, &oled2 };
  for (int i = 0; i < 2; i++) {
    Adafruit_SSD1306 &d = *plates[i];
    d.clearDisplay();
    plateCenterDefaultFont(d, "REMOTE", 1, 4);
    plateCenterDefaultFont(d, (kind == 2) ? "CONNECTED" : "FOUND", 2, 13);
    if (frameOn) {
      d.drawRect(0, 0, OLED_W, OLED_H, SSD1306_WHITE);
      d.drawRect(2, 2, OLED_W - 4, OLED_H - 4, SSD1306_WHITE);
    }
    d.display();
  }
}

void updateRemoteBanner(unsigned long now) {
  if (remoteBannerKind == 0) return;

  bool plateFree = (state == STATE_IDLE || state == STATE_SLEEP);
  if (!plateFree) {
    // Waiting for startup/reaction to finish - or aborting if one interrupts us.
    if (remoteBannerActive) { remoteBannerActive = false; remoteBannerKind = 0; }
    return;
  }

  if (!remoteBannerActive) {
    remoteBannerActive = true;
    remoteBannerStart = now;
    remoteBannerLastPhase = -1;
  }

  unsigned long elapsed = now - remoteBannerStart;
  if (elapsed >= REMOTE_BANNER_MS) {
    remoteBannerActive = false;
    remoteBannerKind = 0;
    lastPlateUpdate = 0;     // let Idle/Sleep redraw its own plate right away
    return;
  }

  lastPlateUpdate = now;     // keeps the state machine from drawing over the banner
  long phase = elapsed / REMOTE_BANNER_FLASH_MS;
  if (phase != remoteBannerLastPhase) {
    remoteBannerLastPhase = phase;
    drawPlateRemoteBanner(remoteBannerKind, (phase % 2) == 0);
  }
}

// Called from loop(): does everything the receive callback must not do
// (add peer, send replies, print).
void handleRemoteLink() {
  if (!remoteLinkReady) return;
  unsigned long now = millis();

  if (pendingPairAck) {
    uint8_t mac[6];
    portENTER_CRITICAL(&yumoMux);
    memcpy(mac, pendingPairMac, 6);
    pendingPairAck = false;
    portEXIT_CRITICAL(&yumoMux);

    bool sameRemote = remotePaired && (memcmp(mac, remoteMac, 6) == 0);
    unsigned long lastPacket = joystickLastPacketTime;
    bool currentRemoteAlive = remotePaired && lastPacket != 0 &&
                              (now - lastPacket) <= REMOTE_LINK_HOLD_MS;

    // Re-pair the same remote any time; accept a different remote only if
    // the current one has gone quiet.
    if (!remotePaired || sameRemote || !currentRemoteAlive) {
      if (remotePaired && !sameRemote) esp_now_del_peer(remoteMac);

      if (yumoAddPeer(mac)) {
        portENTER_CRITICAL(&yumoMux);
        memcpy(remoteMac, mac, 6);
        remotePaired = true;
        portEXIT_CRITICAL(&yumoMux);

        yumoSend(mac, PACKET_PAIR_ACK);
        if (!remoteConnectedShown && remoteBannerKind == 0) remoteBannerKind = 1;   // "FOUND"

        Serial.print("Remote paired: ");
        for (int i = 0; i < 6; i++) {
          if (i) Serial.print(":");
          Serial.print(mac[i], HEX);
        }
        Serial.println();
      } else {
        Serial.println("Remote pairing: add_peer failed");
      }
    }
  }

  if (pendingLinkAck && remotePaired && (now - lastLinkAckTime) >= LINK_ACK_INTERVAL_MS) {
    pendingLinkAck = false;
    lastLinkAckTime = now;
    yumoSend(remoteMac, PACKET_LINK_ACK);
  }

  // Remote is "live" once joystick packets are actually flowing (so the remote
  // really received our pair ack). Announce it once per connection.
  unsigned long lastJoy = joystickLastPacketTime;
  bool linkLive = remotePaired && lastJoy != 0 && (now - lastJoy) <= REMOTE_LINK_HOLD_MS;
  if (linkLive && !remoteConnectedShown) {
    remoteConnectedShown = true;
    remoteBannerKind = 2;            // "CONNECTED" (replaces a "FOUND" if showing)
    remoteBannerActive = false;
  }
  if (!linkLive) remoteConnectedShown = false;

  updateRemoteBanner(now);
}

// Tank turn for the remote. tankTurnCW() (existing) already respects the
// mirrored servos; this adds only the matching counter-clockwise version.
// Set REMOTE_USE_TANK_TURN to false to use the original rotateCW()/rotateCCW().
const bool REMOTE_USE_TANK_TURN = true;

void remoteTankTurn(int dir) {
  if (dir > 0) {
    tankTurnCW();
    return;
  }
  int left  = SERVO_TURN_REV;
  int right = SIDES_MIRRORED ? SERVO_TURN_REV : SERVO_TURN_FWD;
  setServo(servoFL, left);  setServo(servoBL, left);
  setServo(servoFR, right); setServo(servoBR, right);
}

// ── Proportional (analog) remote driving ─────────────────────────────
// Push the stick a little = slow, push it all the way = fast.
// Forward/back and left/right are mixed, so a diagonal push drives in a curve.
// Set REMOTE_ANALOG_DRIVE to false to go back to the fixed-speed driving.
const bool  REMOTE_ANALOG_DRIVE  = true;
const int   REMOTE_DRIVE_MIN_US  = SERVO_TINY_FWD - SERVO_STOP;   // slowest pulse offset that still moves (60)
const int   REMOTE_DRIVE_MAX_US  = SERVO_FWD_FAST - SERVO_STOP;   // pulse offset at full stick (250)
const float REMOTE_TURN_SCALE    = 0.5f;   // how strong turning is vs driving (raise = sharper/faster turns)
const float REMOTE_SPEED_CURVE   = 1.5f;   // 1.0 = linear, higher = finer control at low speed
const int   REMOTE_FULL_MARGIN   = 100;    // full speed is reached this many ADC counts before the stick's end stop

// d = joystick offset from centre with the deadzone already applied (0 or |d| >= deadzone).
// Returns -1.0 .. +1.0.
float remoteAxisToSpeed(int d) {
  if (d == 0) return 0.0f;
  float span = (float)(JOYSTICK_CENTER - JOYSTICK_DEADZONE - REMOTE_FULL_MARGIN);
  float t = ((float)abs(d) - (float)JOYSTICK_DEADZONE) / span;
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;
  t = powf(t, REMOTE_SPEED_CURVE);
  return (d < 0) ? -t : t;
}

// v = -1..+1 forward speed for one side of the car. Right-side servos are
// mounted mirrored (SIDES_MIRRORED), exactly like driveStraight()/tankTurnCW().
int remoteSideToPulse(float v, bool rightSide) {
  if (fabsf(v) < 0.001f) return SERVO_STOP;
  int offset = REMOTE_DRIVE_MIN_US + (int)((REMOTE_DRIVE_MAX_US - REMOTE_DRIVE_MIN_US) * fabsf(v));
  bool forward = (v > 0.0f);
  if (rightSide && SIDES_MIRRORED) forward = !forward;
  return forward ? (SERVO_STOP + offset) : (SERVO_STOP - offset);
}

void remoteAnalogDrive(int dx, int dy) {
  float f = -remoteAxisToSpeed(dy);                        // stick forward (dy < 0) = forward
  float t = remoteAxisToSpeed(dx) * REMOTE_TURN_SCALE;     // stick right (dx > 0) = clockwise

  float l = f + t;
  float r = f - t;
  float m = fmaxf(fabsf(l), fabsf(r));
  if (m > 1.0f) { l /= m; r /= m; }

  int leftPulse  = remoteSideToPulse(l, false);
  int rightPulse = remoteSideToPulse(r, true);
  setServo(servoFL, leftPulse);  setServo(servoBL, leftPulse);
  setServo(servoFR, rightPulse); setServo(servoBR, rightPulse);
}

// Converts joystick Y into forward/backward.
// Converts joystick X into left/right.
// The joystick itself will be on the second ESP32.
void handleRemoteJoystick() {
  unsigned long lastPacket = joystickLastPacketTime;
  if (lastPacket == 0) return;

  // Safety: if the remote stops transmitting, stop the wheels.
  if (millis() - lastPacket > JOYSTICK_TIMEOUT_MS) {
    if (state == STATE_IDLE) {
      stopAllServos();
    }
    return;
  }

  int x = joystickPacket.x;
  int y = joystickPacket.y;

  int dx = x - JOYSTICK_CENTER;
  int dy = y - JOYSTICK_CENTER;

  // Deadzone around joystick centre.
  if (abs(dx) < JOYSTICK_DEADZONE) dx = 0;
  if (abs(dy) < JOYSTICK_DEADZONE) dy = 0;

  // Joystick is released.
  if (dx == 0 && dy == 0) {
    if (state == STATE_IDLE) {
      stopAllServos();
    }
    return;
  }

  // A moving joystick gives manual driving control.
  // It does not change the existing touch/reaction code.
  if (state == STATE_SLEEP) {
    enterIdle();
  }

  if (state != STATE_IDLE) return;

  // ADDED: driving counts as activity, so the cat doesn't fall asleep mid-drive.
  idleStartTime = millis();

  // ADDED: speed follows how far the stick is pushed.
  if (REMOTE_ANALOG_DRIVE) {
    remoteAnalogDrive(dx, dy);
    return;
  }

  // Simple tank-style control for the first version:
  // vertical = forward/backward
  // horizontal = left/right
  if (abs(dy) >= abs(dx)) {
    if (dy < 0) {
      driveStraight(+1);
    } else {
      driveStraight(-1);
    }
  } else {
    if (dx > 0) {
      if (REMOTE_USE_TANK_TURN) remoteTankTurn(+1); else rotateCW();
    } else {
      if (REMOTE_USE_TANK_TURN) remoteTankTurn(-1); else rotateCCW();
    }
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(TFT_BLK, OUTPUT);
  digitalWrite(TFT_BLK, HIGH);
  pinMode(TOUCH_PIN, INPUT);

  gfx->begin();
  gfx->fillScreen(RGB565_BLACK);
  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(jpegOutput);

  Wire.begin(I2C_SDA, I2C_SCL);
  oled1.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR_1);
  oled2.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR_2);
  buildHeartMask();

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  servoFL.setPeriodHertz(50);
  servoFR.setPeriodHertz(50);
  servoBL.setPeriodHertz(50);
  servoBR.setPeriodHertz(50);

  servoFL.attach(PIN_SERVO_FL, 1000, 2000);
  servoFR.attach(PIN_SERVO_FR, 1000, 2000);
  servoBL.attach(PIN_SERVO_BL, 1000, 2000);
  servoBR.attach(PIN_SERVO_BR, 1000, 2000);
  stopAllServos();

  setupJoystickReceiver();

  state = STATE_SPLASH;
  splashStartTime = millis();
}

void loop() {
  handleTouch();

  switch (state) {
    case STATE_SPLASH:
      updateSplash();
      break;

    case STATE_STARTUP:
    case STATE_REACT:
      updatePhasePlayback();
      break;

    case STATE_IDLE: {
      updateFace();
      unsigned long now = millis();
      if (now - lastPlateUpdate >= PLATE_UPDATE_INTERVAL_MS) {
        lastPlateUpdate = now;
        drawPlateHeartbeat(heartbeatLevel(now));
      }
      if (now - idleStartTime >= IDLE_HOLD_BEFORE_SLEEP_MS) {
        enterSleep();
      }
      break;
    }

    case STATE_SLEEP: {
      updateFace();
      unsigned long now = millis();
      if (now - lastPlateUpdate >= PLATE_UPDATE_INTERVAL_MS) {
        lastPlateUpdate = now;
        drawPlateSleepZZZ(sleepZStep(now));
      }
      break;
    }
  }

  // ADDITION: remote joystick is handled after the existing state machine,
  // so a joystick command can drive the car without replacing touch logic.
  handleRemoteLink();      // ADDED: pairing replies + link heartbeat
  handleRemoteJoystick();
}
