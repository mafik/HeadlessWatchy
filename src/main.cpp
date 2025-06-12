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

// Star Wars Imperial March melody
void playImperialMarch() {
  if (kDebug)
    printf("Playing Star Wars Imperial March (Extended)\n");

  // Proper Imperial March melody from euphonium arrangement - transposed up 2
  // octaves Note durations: 4 = quarter note, 8 = eighth note, 2 = half note, 1
  // = whole note
  int melody[] = {
      // Line 1: G3 G3 G3 Eb3 Bb3
      NOTE_G5, NOTE_G5, NOTE_G5, NOTE_DS5, NOTE_AS5,
      // Line 2: G3 Eb3 Bb3 G3
      NOTE_G5, NOTE_DS5, NOTE_AS5, NOTE_G5,
      // Line 3: D4 D4 D4 Eb4 Bb3
      NOTE_D6, NOTE_D6, NOTE_D6, NOTE_DS6, NOTE_AS5,
      // Line 4: Gb3 Eb3 Bb3 G3
      NOTE_FS5, NOTE_DS5, NOTE_AS5, NOTE_G5,
      // Line 5: G4 G3 G3 G4 Gb4 F4
      NOTE_G6, NOTE_G5, NOTE_G5, NOTE_G6, NOTE_FS6, NOTE_F6,
      // Line 6: E4 Eb4 E4 Ab3 Db4 C4 B3
      NOTE_E6, NOTE_DS6, NOTE_E6, NOTE_GS5, NOTE_CS6, NOTE_C6, NOTE_B5,
      // Line 7: Bb3 A3 Bb3 Eb3 Gb3 Eb3 Gb3
      NOTE_AS5, NOTE_A5, NOTE_AS5, NOTE_DS5, NOTE_FS5, NOTE_DS5, NOTE_FS5,
      // Line 8: Bb3 G3 Bb3 D4
      NOTE_AS5, NOTE_G5, NOTE_AS5, NOTE_D6,
      // Line 9: G4 G3 G3 G4 Gb4 F4
      NOTE_G6, NOTE_G5, NOTE_G5, NOTE_G6, NOTE_FS6, NOTE_F6,
      // Line 10: E4 Eb4 E4 Ab3 Db4 C4 B3
      NOTE_E6, NOTE_DS6, NOTE_E6, NOTE_GS5, NOTE_CS6, NOTE_C6, NOTE_B5,
      // Line 11: Bb3 A3 Bb3 Eb3 Gb3 Eb3 Bb3
      NOTE_AS5, NOTE_A5, NOTE_AS5, NOTE_DS5, NOTE_FS5, NOTE_DS5, NOTE_AS5,
      // Line 12: G3 Eb3 Bb3 G3
      NOTE_G5, NOTE_DS5, NOTE_AS5, NOTE_G5};

  int noteDurations[] = {// Line 1: Strong opening phrase
                         4, 4, 4, 8, 8,
                         // Line 2: Continuation
                         4, 8, 8, 2,
                         // Line 3: Higher phrase
                         4, 4, 4, 8, 8,
                         // Line 4: Resolution
                         4, 8, 8, 2,
                         // Line 5: Ascending dramatic part
                         4, 8, 8, 4, 8, 8,
                         // Line 6: Complex melodic line
                         8, 8, 8, 8, 4, 8, 8,
                         // Line 7: Rapid sequence
                         8, 8, 8, 8, 8, 8, 8,
                         // Line 8: Build up
                         8, 8, 8, 4,
                         // Line 9: Repeat of dramatic part
                         4, 8, 8, 4, 8, 8,
                         // Line 10: Complex melodic line repeat
                         8, 8, 8, 8, 4, 8, 8,
                         // Line 11: Final rapid sequence
                         8, 8, 8, 8, 8, 8, 8,
                         // Line 12: Final resolution
                         4, 8, 8, 2};

  int totalNotes = sizeof(melody) / sizeof(melody[0]);

  for (int thisNote = 0; thisNote < totalNotes; thisNote++) {
    // Calculate note duration: quarter note = 500ms, eighth note = 250ms, etc.
    int noteDuration = 2000 / noteDurations[thisNote];

    // Set PWM frequency to the note frequency
    analogWriteFrequency(melody[thisNote]);

    // Play the note using PWM value 32
    analogWrite(VIB_MOTOR_PIN, 32);
    delay(noteDuration);

    // Stop the note
    analogWrite(VIB_MOTOR_PIN, 0);

    // Pause between notes (30% of note duration)
    int pauseBetweenNotes = noteDuration * 0.30;
    delay(pauseBetweenNotes);
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

int zeroMillis = 50;
int oneMillis = zeroMillis * 3;
int separatorMillis = 120;

void vibZero() {
  digitalWrite(VIB_MOTOR_PIN, 64);
  delay(zeroMillis); // short vibration for dot
  digitalWrite(VIB_MOTOR_PIN, LOW);
  delay(separatorMillis); // pause between elements
}

void vibOne() {
  digitalWrite(VIB_MOTOR_PIN, HIGH);
  delay(oneMillis); // long vibration for dash
  digitalWrite(VIB_MOTOR_PIN, LOW);
  delay(separatorMillis); // pause between elements
}

void vibBinary(int value, int bits) {
  if (kDebug) {
    printf("vibBinary(%d, %d) ", value, bits);
    for (int i = 0; i < bits; ++i) {
      printf(((value >> (bits - i - 1)) & 1) ? "-" : ".");
    }
    printf("\n");
  }
  for (int i = 0; i < bits; ++i) {
    if ((value >> (bits - i - 1)) & 1) {
      vibOne();
    } else {
      vibZero();
    }
  }
}

void announceHour() {
  time_t t = DS3232RTC::get();
  struct tm *timeinfo = localtime(&t);

  if (kDebug) {
    printf("Announcing hour: %02d\n", timeinfo->tm_hour);
  }

  vibBinary(timeinfo->tm_hour, 3);
}

void announceMinutes() {
  time_t t = DS3232RTC::get();
  struct tm *timeinfo = localtime(&t);

  if (kDebug) {
    printf("Announcing minutes: %02d\n", timeinfo->tm_min);
  }

  vibBinary(timeinfo->tm_min, 6);
}

void announceTime() {
  time_t t = DS3232RTC::get();
  struct tm *timeinfo = localtime(&t);

  if (kDebug) {
    printf("Announcing time: %02d:%02d\n", timeinfo->tm_hour, timeinfo->tm_min);
  }

  vibBinary(timeinfo->tm_hour, 3);
  delay(separatorMillis * 3);
  vibBinary(timeinfo->tm_min, 6);
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
  analogWrite(VIB_MOTOR_PIN, 32);
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
      // BACK button pressed - announce time in morse code
      announceTime();
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
