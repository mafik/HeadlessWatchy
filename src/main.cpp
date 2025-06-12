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

// Morse code patterns (1 = dot, 2 = dash)
const uint8_t morseDigits[][5] = {
  {2, 2, 2, 2, 2}, // 0: -----
  {1, 2, 2, 2, 2}, // 1: .----
  {1, 1, 2, 2, 2}, // 2: ..---
  {1, 1, 1, 2, 2}, // 3: ...--
  {1, 1, 1, 1, 2}, // 4: ....-
  {1, 1, 1, 1, 1}, // 5: .....
  {2, 1, 1, 1, 1}, // 6: -....
  {2, 2, 1, 1, 1}, // 7: --...
  {2, 2, 2, 1, 1}, // 8: ---..
  {2, 2, 2, 2, 1}  // 9: ----.
};

const uint8_t morsePadding = 0; // padding between characters

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

// Morse code vibration functions
void vibDot() {
  digitalWrite(VIB_MOTOR_PIN, HIGH);
  delay(50); // short vibration for dot
  digitalWrite(VIB_MOTOR_PIN, LOW);
  delay(50); // pause between elements
}

void vibDash() {
  digitalWrite(VIB_MOTOR_PIN, HIGH);
  delay(150); // long vibration for dash
  digitalWrite(VIB_MOTOR_PIN, LOW);
  delay(50); // pause between elements
}

void vibPause() {
  delay(100); // longer pause between digits
}

void vibLongPause() {
  delay(200); // pause between hours and minutes
}

void vibDigit(uint8_t digit) {
  if (digit > 9) return;

  if (kDebug) {
    printf("Digit: %hhd\n", digit);
  }

  for (int i = 0; i < 5; i++) {
    if (morseDigits[digit][i] == 1) {
      vibDot();
    } else if (morseDigits[digit][i] == 2) {
      vibDash();
    }
  }
  vibPause();
}

void announceHour() {
  time_t t = DS3232RTC::get();
  struct tm *timeinfo = localtime(&t);

  if (kDebug) {
    printf("Announcing hour: %02d\n", timeinfo->tm_hour);
  }

  if (timeinfo->tm_hour >= 10) {
    vibDigit(timeinfo->tm_hour / 10);
  }
  vibDigit(timeinfo->tm_hour % 10);
}

void announceTime() {
  time_t t = DS3232RTC::get();
  struct tm *timeinfo = localtime(&t);

  if (kDebug) {
    printf("Announcing time: %02d:%02d\n", timeinfo->tm_hour, timeinfo->tm_min);
  }

  // Announce hours
  if (timeinfo->tm_hour >= 10) {
    vibDigit(timeinfo->tm_hour / 10);
  }
  vibDigit(timeinfo->tm_hour % 10);

  vibLongPause();

  // Announce minutes
  if (timeinfo->tm_min >= 10) {
    vibDigit(timeinfo->tm_min / 10);
  }
  vibDigit(timeinfo->tm_min % 10);
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
      vibDot();
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

  // Start vibration as early as possible to improve button responsiveness
  uint64_t wakeup_reason_ext1;
  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {
    wakeup_reason_ext1 = esp_sleep_get_ext1_wakeup_status();
    if (wakeup_reason_ext1 & BACK_BTN_MASK) {
      // Time button uses vibrations - don't do the regular vibration pattern in this case
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
        vibTick();
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
        debounce(UP_BTN_PIN);
      }
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
