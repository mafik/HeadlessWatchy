// Based on STARRY HORIZON by Dan Delany
// (https://github.com/dandelany/watchy-faces/)
// Modified to use morse code vibration instead of display
#include <Arduino.h>
#include <DS3232RTC.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <esp_pthread.h>
#include <thread>
#include <time.h>

#include "BLE.h"
#include "esp32-hal.h"

// pins
#define SDA 21
#define SCL 22
#define ADC_PIN 33
#define RTC_PIN GPIO_NUM_27
#define VIB_MOTOR_PIN 13
#define MENU_BTN_PIN 26
#define BACK_BTN_PIN 25
#define UP_BTN_PIN 32
#define DOWN_BTN_PIN 4
#define MENU_BTN_MASK GPIO_SEL_26
#define BACK_BTN_MASK GPIO_SEL_25
#define UP_BTN_MASK GPIO_SEL_32
#define DOWN_BTN_MASK GPIO_SEL_4
#define ACC_INT_MASK GPIO_SEL_14
#define BTN_PIN_MASK MENU_BTN_MASK | BACK_BTN_MASK | UP_BTN_MASK | DOWN_BTN_MASK

#include "config.h"

DS3232RTC RTC(false);

RTC_DATA_ATTR time_t timer_deadline;

// Star Wars Imperial March note frequencies (in Hz) - Transposed up 2 octaves
// for better vibration motor response (euphonium arrangement)
#define NOTE_G3 196
#define NOTE_GS3 208
#define NOTE_A3 220
#define NOTE_AS3 233
#define NOTE_B3 247
#define NOTE_C4 262
#define NOTE_CS4 277
#define NOTE_D4 294
#define NOTE_DS4 311
#define NOTE_E4 330
#define NOTE_F4 349
#define NOTE_FS4 370
#define NOTE_G4 392
#define NOTE_GS4 415
#define NOTE_A4 440
#define NOTE_AS4 466
#define NOTE_B4 494
#define NOTE_C5 523
#define NOTE_CS5 554
#define NOTE_D5 587
#define NOTE_DS5 622
#define NOTE_E5 659
#define NOTE_F5 698
#define NOTE_FS5 740
#define NOTE_G5 784
#define NOTE_GS5 831
#define NOTE_A5 880
#define NOTE_AS5 932
#define NOTE_B5 988
#define NOTE_C6 1047
#define NOTE_CS6 1109
#define NOTE_D6 1175
#define NOTE_DS6 1245
#define NOTE_E6 1319
#define NOTE_F6 1397
#define NOTE_FS6 1480
#define NOTE_G6 1568
#define NOTE_GS6 1661
#define NOTE_A6 1760

// Structure to hold note and duration information
struct MusicalNote {
  int frequency; // Note frequency in Hz (0 = rest)
  int duration;  // Duration: 1=whole, 2=half, 4=quarter, 8=eighth, 16=sixteenth
  bool dotted;   // Whether the note is dotted (1.5x duration)
};

// Play a single musical note with proper timing
void playNote(int frequency, int baseDuration, bool dotted = false) {
  // Base timing: quarter note = 565ms (106 BPM as marked in sheet music)
  int quarterNoteMs = 565;
  int noteDuration = quarterNoteMs * 4 / baseDuration;

  if (dotted) {
    noteDuration = noteDuration * 3 / 2; // Dotted notes are 1.5x longer
  }

  if (frequency > 0) {
    // Set PWM frequency and play the note
    analogWriteFrequency(frequency);
    analogWrite(VIB_MOTOR_PIN, 24);
    delay(noteDuration * 0.9); // Play note for 90% of duration

    // Stop the note
    analogWrite(VIB_MOTOR_PIN, 0);
    delay(noteDuration * 0.1); // 10% rest between notes
  } else {
    // Rest (frequency = 0)
    analogWrite(VIB_MOTOR_PIN, 0);
    delay(noteDuration);
  }
}

// Star Wars Imperial March melody - following the exact sheet music timing
void playImperialMarch() {
  if (kDebug)
    printf("Playing Star Wars Imperial March (Sheet Music Accurate)\n");

  // Imperial March melody following the exact sheet music durations
  // Tempo: Quarter note = 106 BPM as marked on the sheet
  MusicalNote melody[] = {
      // Measure 1: G G G Eb-Bb (quarter, quarter, quarter, eighth+eighth,
      // quarter)
      {NOTE_G5, 4, true},
      {NOTE_G5, 4, true},
      {NOTE_G5, 4, true},
      {NOTE_DS5, 8, true},
      {0, 16, false},
      {NOTE_AS5, 16, false},

      // Measure 2: G Eb-Bb G (eighth+eighth, quarter, half)
      {NOTE_G5, 4, true},
      {NOTE_DS5, 8, true},
      {0, 16, false},
      {NOTE_AS5, 16, false},
      {NOTE_G5, 4, false},
      {0, 4, false}, // rest

      // Measure 3: D D D Eb-Bb (quarter, quarter, quarter, eighth+eighth,
      // quarter)
      {NOTE_D6, 4, true},
      {NOTE_D6, 4, true},
      {NOTE_D6, 4, true},
      {NOTE_DS6, 8, true},
      {0, 16, false},
      {NOTE_AS5, 16, false},

      // Measure 4: Gb Eb-Bb G (eighth+eighth, quarter, half)
      {NOTE_FS5, 4, true},
      {NOTE_DS5, 8, true},
      {0, 16, false},
      {NOTE_AS5, 16, false},
      {NOTE_G5, 4, false},
      {0, 4, false}, // rest

      // Measure 5: G G-G G Gb-F (sixteenth+sixteenth+eighth, quarter,
      // eighth+eighth)
      {NOTE_G6, 4, true},
      {NOTE_G5, 8, true},
      {NOTE_G5, 16, false},
      {NOTE_G6, 4, true},
      {NOTE_FS6, 8, true},
      {0, 16, false},
      {NOTE_F6, 16, false},

      // Measure 6: E Eb-E rest Ab Db-C-B (eighth+eighth+eighth, eighth rest,
      // eighth, eighth+eighth+eighth)
      {NOTE_E6, 16, false},
      {NOTE_DS6, 16, false},
      {NOTE_E6, 8, true},
      {0, 8, false}, // rest
      {NOTE_GS5, 8, true},
      {NOTE_CS6, 4, true},
      {NOTE_C6, 8, true},
      {0, 16, false},
      {NOTE_B5, 16, false},

      // Measure 7: Bb A-Bb rest Eb Gb-Eb-Gb (eighth+eighth+eighth, eighth rest,
      // eighth, eighth+eighth+eighth)
      {NOTE_AS5, 16, false},
      {NOTE_A5, 16, false},
      {NOTE_AS5, 8, true},
      {0, 8, false}, // rest
      {NOTE_DS5, 8, true},
      {NOTE_FS5, 4, true},
      {NOTE_DS5, 8, true},
      {0, 16, false},
      {NOTE_FS5, 16, false},

      // Measure 8: Bb G-Bb D (eighth+eighth+eighth, quarter)
      {NOTE_AS5, 4, true},
      {NOTE_G5, 8, true},
      {0, 16, false},
      {NOTE_AS5, 16, false},
      {NOTE_D6, 4, false},
      {0, 4, false},

      // Measure 9: G G-G G Gb-F (repeat of measure 5)
      {NOTE_G6, 4, true},
      {NOTE_G5, 8, true},
      {0, 16, false},
      {NOTE_G5, 16, false},
      {NOTE_G6, 4, true},
      {NOTE_FS6, 8, true},
      {0, 16, false},
      {NOTE_F6, 16, false},

      // Measure 10: E Eb-E rest Ab Db-C-B (repeat of measure 6)
      {NOTE_E6, 16, false},
      {NOTE_DS6, 16, false},
      {NOTE_E6, 8, true},
      {0, 8, false}, // rest
      {NOTE_GS5, 8, true},
      {NOTE_CS6, 4, true},
      {NOTE_C6, 8, true},
      {0, 16, false},
      {NOTE_B5, 16, false},

      // Measure 11: Bb A-Bb rest Eb Gb-Eb-Bb (similar to measure 7 but ending
      // different)
      {NOTE_AS5, 16, false},
      {NOTE_A5, 16, false},
      {NOTE_AS5, 8, true},
      {0, 8, false}, // rest
      {NOTE_DS5, 8, true},
      {NOTE_FS5, 4, true},
      {NOTE_DS5, 8, true},
      {0, 16, false},
      {NOTE_AS5, 16, false},

      // Measure 12: G Eb-Bb G (final resolution)
      {NOTE_G5, 4, true},
      {NOTE_DS5, 8, true},
      {0, 16, false},
      {NOTE_AS5, 16, false},
      {NOTE_G5, 4, false}};

  int totalNotes = sizeof(melody) / sizeof(melody[0]);

  for (int i = 0; i < totalNotes; i++) {
    playNote(melody[i].frequency, melody[i].duration, melody[i].dotted);
  }

  // Reset frequency to default
  analogWriteFrequency(1000);

  if (kDebug)
    printf("Imperial March complete\n");
}

void doWiFiUpdate() {
  WiFi.begin(kWiFiSSID, kWiFiPass);
  if (WiFi.waitForConnectResult() == WL_CONNECTED) {
    configTzTime(kTZ, "pool.ntp.org", "time.nist.gov");
    if (kDebug)
      printf("Waiting for NTP time sync: ");
    time_t nowSecs = time(nullptr);
    while (nowSecs < 8 * 3600 * 2) {
      delay(500);
      if (kDebug)
        printf(".");
      yield();
      nowSecs = time(nullptr);
    }
    if (kDebug) {
      auto *timeinfo = localtime(&nowSecs);
      printf("\nLocal time: %s\n", asctime(timeinfo));
    }
    RTC.set(nowSecs);
  }
  WiFi.mode(WIFI_OFF);
  btStop();
}

enum VibrationSpeed {
  NORMAL_SPEED = 1,
  SLOW_SPEED = 2
};

int zeroMillis = 50;
int oneMillis = zeroMillis * 3;
int separatorMillis = 150;

void vibZero(VibrationSpeed speed = NORMAL_SPEED) {
  digitalWrite(VIB_MOTOR_PIN, 64);
  delay(zeroMillis * speed); // short vibration for dot
  digitalWrite(VIB_MOTOR_PIN, LOW);
  delay(separatorMillis * speed); // pause between elements
}

void vibOne(VibrationSpeed speed = NORMAL_SPEED) {
  digitalWrite(VIB_MOTOR_PIN, HIGH);
  delay(oneMillis * speed); // long vibration for dash
  digitalWrite(VIB_MOTOR_PIN, LOW);
  delay(separatorMillis * speed); // pause between elements
}

void vibBinary(int value, int bits, VibrationSpeed speed = NORMAL_SPEED) {
  if (kDebug) {
    printf("vibBinary(%d, %d, %s) ", value, bits, speed == SLOW_SPEED ? "slow" : "normal");
    for (int i = 0; i < bits; ++i) {
      printf(((value >> (bits - i - 1)) & 1) ? "-" : ".");
    }
    printf("\n");
  }
  for (int i = 0; i < bits; ++i) {
    if ((value >> (bits - i - 1)) & 1) {
      vibOne(speed);
    } else {
      vibZero(speed);
    }
  }
}

void vibMinutesQuarterHour(int minutes, VibrationSpeed speed = NORMAL_SPEED) {
  // Split minutes into quarter-hour (0-3) and offset within quarter (0-14)
  int quarter = minutes / 15;        // 0, 1, 2, or 3
  int offset = minutes % 15;         // 0-14 minutes within the quarter
  
  if (kDebug) {
    printf("vibMinutesQuarterHour(%d, %s) quarter=%d offset=%d ", 
           minutes, speed == SLOW_SPEED ? "slow" : "normal", quarter, offset);
    // Show quarter pattern (2 bits)
    for (int i = 0; i < 2; ++i) {
      printf(((quarter >> (1 - i)) & 1) ? "-" : ".");
    }
    printf(" ");
    // Show offset pattern (4 bits)
    for (int i = 0; i < 4; ++i) {
      printf(((offset >> (3 - i)) & 1) ? "-" : ".");
    }
    printf("\n");
  }
  
  // Vibrate quarter-hour (2 bits)
  for (int i = 0; i < 2; ++i) {
    if ((quarter >> (1 - i)) & 1) {
      vibOne(speed);
    } else {
      vibZero(speed);
    }
  }
  
  // Longer pause between quarter and offset
  delay(separatorMillis * speed * 3);
  
  // Vibrate offset within quarter (4 bits)
  for (int i = 0; i < 4; ++i) {
    if ((offset >> (3 - i)) & 1) {
      vibOne(speed);
    } else {
      vibZero(speed);
    }
  }
}

void announceHour() {
  time_t t = DS3232RTC::get();
  struct tm *timeinfo = localtime(&t);

  if (kDebug) {
    printf("Announcing hour: %02d\n", timeinfo->tm_hour);
  }

  vibBinary(timeinfo->tm_hour, 3, SLOW_SPEED);
}

void announceMinutes() {
  time_t t = DS3232RTC::get();
  struct tm *timeinfo = localtime(&t);

  if (kDebug) {
    printf("Announcing minutes: %02d\n", timeinfo->tm_min);
  }

  vibMinutesQuarterHour(timeinfo->tm_min);
}

void announceTime() {
  time_t t = DS3232RTC::get();
  struct tm *timeinfo = localtime(&t);

  if (kDebug) {
    printf("Announcing time: %02d:%02d\n", timeinfo->tm_hour, timeinfo->tm_min);
  }

  vibBinary(timeinfo->tm_hour, 3);
  delay(separatorMillis * 3);
  vibMinutesQuarterHour(timeinfo->tm_min);
}

// two short vibrations with decreasing speed
void vibBad() {
  for (int i = 0; i < 2; ++i) {
    if (i)
      delay(100);
    analogWrite(VIB_MOTOR_PIN, 255);
    delay(50);
    analogWrite(VIB_MOTOR_PIN, 128);
    delay(50);
    analogWrite(VIB_MOTOR_PIN, 96);
    delay(50);
    analogWrite(VIB_MOTOR_PIN, 64);
    delay(50);
    analogWrite(VIB_MOTOR_PIN, 32);
    delay(50);
    analogWrite(VIB_MOTOR_PIN, 0);
  }
  digitalWrite(VIB_MOTOR_PIN, LOW);
}

// one subtle vibration
void vibTick() {
  analogWrite(VIB_MOTOR_PIN, 24);
  delay(50);
  analogWrite(VIB_MOTOR_PIN, 0);
}

// one rising vibration
void vibGood() {
  static const uint8_t kSteps[] = {64, 64, 96, 128, 255};
  for (int i = 0; i < sizeof(kSteps); ++i) {
    analogWrite(VIB_MOTOR_PIN, kSteps[i]);
    delay(100);
  }
  analogWrite(VIB_MOTOR_PIN, 0);
  digitalWrite(VIB_MOTOR_PIN, LOW);
}

void bluetoothApp() {
  pinMode(MENU_BTN_PIN, INPUT);
  BLE ble{};
  auto start = millis();
  while (true) {
    auto now = millis();
    if (now - start > 30000) {
      vibBad();
      break;
    }
    if (digitalRead(MENU_BTN_PIN)) {
      vibZero();
      break;
    }
    if (ble.notified()) {
      vibGood();
      break;
    }
    delay(10);
  }
}

void startTimer() {
  RTC.setAlarm(DS3232RTC::ALM1_EVERY_SECOND, 0, 0, 0, 0);
  RTC.alarmInterrupt(DS3232RTC::ALARM_1, true); // enable alarm interrupt
}

void stopTimer() {
  timer_deadline = 0;
  RTC.alarmInterrupt(DS3232RTC::ALARM_1, false);
}

void debounce(uint64_t button_pin) {
  while (digitalRead(button_pin) == HIGH)
    delay(10);
}

void setup() {
  if (kDebug)
    Serial.begin(9600);

  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause(); // get wake up reason

  pinMode(VIB_MOTOR_PIN, OUTPUT);
  analogWriteResolution(8);
  analogWriteFrequency(1000);

  // Start vibration as early as possible to improve button responsiveness
  uint64_t wakeup_reason_ext1;
  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {
    wakeup_reason_ext1 = esp_sleep_get_ext1_wakeup_status();
    if (wakeup_reason_ext1 & (BACK_BTN_MASK | UP_BTN_MASK)) {
      // Time button uses vibrations - don't do the regular vibration pattern in
      // this case
    } else {
      digitalWrite(VIB_MOTOR_PIN, HIGH);
      delay(50);
      digitalWrite(VIB_MOTOR_PIN, LOW);
    }
  }

  Wire.begin(SDA, SCL); // init i2c

  setenv("TZ", kTZ, 1);
  tzset();

  // Run threads on the other core (0)
  auto cfg = esp_pthread_get_default_config();
  cfg.pin_to_core = 0;
  esp_pthread_set_cfg(&cfg);

  switch (wakeup_reason) {
  case ESP_SLEEP_WAKEUP_EXT0: // RTC Alarm
    if (kDebug)
      printf("OnRTC (core %d)\n", xPortGetCoreID());
    if (RTC.alarm(DS3232RTC::ALARM_1)) {
      if (RTC.get() >= timer_deadline) {
        vibGood();
        stopTimer();
      } else {
        int level = timer_deadline - RTC.get() + 1;
        level *= 100;
        if (level > 15000) {
          level = 15000;
        }
        analogWriteFrequency(level);
        vibTick();
        analogWriteFrequency(1000);
      }
    }
    if (RTC.alarm(DS3232RTC::ALARM_2)) {
      announceHour();
      
      // Check if it's 3am for daily WiFi sync
      time_t t = DS3232RTC::get();
      struct tm *timeinfo = localtime(&t);
      if (timeinfo->tm_hour == 3) {
        if (kDebug)
          printf("3am WiFi sync triggered\n");
        doWiFiUpdate();
      }
    }
    break;
  case ESP_SLEEP_WAKEUP_EXT1: // button Press
    if (kDebug)
      printf("OnButtonPress (core %d)\n", xPortGetCoreID());
    if (wakeup_reason_ext1 & MENU_BTN_MASK) {
      if (timer_deadline) {
        stopTimer();
        debounce(MENU_BTN_PIN);
      } else {
        bluetoothApp();
      }
    } else if (wakeup_reason_ext1 & DOWN_BTN_MASK) {
      if (timer_deadline == 0) {
        timer_deadline = RTC.get();
      }
      timer_deadline += 60;
      startTimer();
      debounce(DOWN_BTN_PIN);
    } else if (wakeup_reason_ext1 & BACK_BTN_MASK) {
      // BACK button pressed - announce hour only
      announceHour();
    } else if (wakeup_reason_ext1 & UP_BTN_MASK) {
      if (timer_deadline) {
        timer_deadline += 300;
      } else {
        announceMinutes();
      }
      debounce(UP_BTN_PIN);
    }
    break;
  default: // reset
    if (kDebug)
      printf("OnReset (core %d)\n", xPortGetCoreID());
    doWiFiUpdate();

    // https://github.com/JChristensen/DS3232RTC
    RTC.squareWave(DS3232RTC::SQWAVE_NONE); // disable square wave output
    RTC.setAlarm(DS3232RTC::ALM2_MATCH_MINUTES, 0, 0, 0,
                 0); // alarm wakes up Watchy every hour
    RTC.alarmInterrupt(DS3232RTC::ALARM_2, true); // enable alarm interrupt

    // Play Star Wars Imperial March on boot
    playImperialMarch();
    break;
  }

  // Deep sleep until RTC alarm or button press
  esp_sleep_enable_ext0_wakeup(RTC_PIN, 0);
  esp_sleep_enable_ext1_wakeup(BTN_PIN_MASK, ESP_EXT1_WAKEUP_ANY_HIGH);
  esp_deep_sleep_start();
}

void loop() {
  // this should never run, Watchy deep sleeps after init();
}
