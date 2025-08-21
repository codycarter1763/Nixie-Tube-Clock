/******************************************
 * Nixie Tube Clock Arduino Code
 * Author: Cody Carter
 * Date: August 2025
 * Version: 1.2.1
 * 
 * This firmware handles all of the nixie tube display and poison prevention
 * logic, real time clock control, IR remote decoding, and RGB presets.
 ******************************************/

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <IRremote.hpp>
#include "RTClib.h"
#include <EEPROM.h>

// Shift register, IR input, and colon digit pins
#define Shift_Data 4
#define Shift_CLK 5
#define Shift_Latch 6
#define Colon_Digit 10
#define IR_Pin 9

// Decoded hex values with IR remote and reciever
#define Btn_1 0XBA45FF00
#define Btn_2 0XB946FF00
#define Btn_3 0XB847FF00
#define Btn_4 0XBB44FF00
#define Btn_5 0XBF40FF00
#define Btn_6 0XBC43FF00
#define Btn_7 0XF807FF00
#define Btn_8 0XEA15FF00
#define Btn_9 0XF609FF00
#define Btn_asterisk 0XE916FF00
#define Btn_0 0XE619FF00
#define Btn_pnd 0XF20DFF00
#define Btn_UpArrow 0XE718FF00
#define Btn_DownArrow 0XAD52FF00
#define Btn_LeftArrow 0XF708FF00
#define Btn_RightArrow 0XA55AFF00
#define Btn_OK 0XE31CFF00

// Preset name to number value mapping
#define Preset_Off 0
#define Preset_Red 1
#define Preset_Blue 2
#define Preset_Green 3
#define Preset_RGB_yellow_orange 4
#define Preset_Cyan 5
#define Preset_Magenta 6
#define Preset_LSU 7
#define Preset_Fire 8
#define Preset_Rainbow 9

// GAmma correction
#define GAMMA 2.2

int RGB_Brightness = 1000; // Default brightness
int currentPreset = 0; // Current RGB preset
uint8_t activeEffect = 0; // Current RGB preset being displayed
bool longPoison = false;

uint32_t code;

DateTime now;
// EEPROM addresses to store variables in flash
const int EEPROM_Brightness_Address = 0;
const int EEPROM_Preset_Address = 1;
const int EEPROM_DaylightSavings_Address = 2;

RTC_DS3231 rtc; // Real-time-clock instance

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(); // default I2C address 0x40 for PCA RGB driver

// Abstraction of RGB LED channels
uint8_t rgbChannels[4][3] = {
  {0, 1, 2},   // LED 1
  {3, 4, 5},   // LED 2
  {6, 7, 8},   // LED 3
  {9, 10, 11}  // LED 4
};

uint16_t gammaLUT[256];  

void initGammaTable(uint16_t maxPWM) {
  for (int i = 0; i < 256; i++) {
    gammaLUT[i] = pow((float)i / 255.0, GAMMA) * maxPWM;
  }
}

// Convert 0–255 RGB to 0–4095 PWM
void rgbToPwm(uint8_t r, uint8_t g, uint8_t b, uint16_t &pr, uint16_t &pg, uint16_t &pb) {
    pr = map(r, 0, 255, 0, 4095);
    pg = map(g, 0, 255, 0, 4095);
    pb = map(b, 0, 255, 0, 4095);
}

// Logic to display digits on nixie tubes using shift registers and BCD IC
void displayDigit(uint8_t digits[4]) {
  uint8_t data[2];

  for (uint8_t i = 0; i < 2; i++) {
    uint8_t digit1 = digits[i * 2];
    uint8_t digit2 = digits[i * 2 + 1];

    if (digit1 > 9)
      digit1 = 0xF;
    if (digit2 > 9)
      digit2 = 0xF;

    data[i] = (digit2 << 4) | (digit1 & 0x0F);
  }

  digitalWrite(Shift_Latch, LOW);
  for (int i = 2 - 1; i >= 0; i--) {
    shiftOut(Shift_Data, Shift_CLK, MSBFIRST, data[i]);
  }
  digitalWrite(Shift_Latch, HIGH);
}

// Displays current month and day for 5 seconds
unsigned long dateDisplayStart = 0;
const unsigned long dateDisplayDuration = 5000;
bool showingDate = false;
uint8_t Date_Digits[4];
void Display_Date() {
  if (millis() - dateDisplayStart >= dateDisplayDuration)
    showingDate = false;
  displayDigit(Date_Digits);
}

// Adjustment for Daylight Savings 
bool isDST = false;
void Daylight_Savings() {
  DateTime now = rtc.now();
  
  if (!isDST) {
    // Enter DST: add 1 hour
    rtc.adjust(DateTime(now.year(), now.month(), now.day(), now.hour() + 1, now.minute(), now.second()));
    isDST = true;
  } else {
    // Exit DST: subtract 1 hour
    rtc.adjust(DateTime(now.year(), now.month(), now.day(), now.hour() - 1, now.minute(), now.second()));
    isDST = false;
  }

  // Save DST state to EEPROM to remember after power down
  EEPROM.put(EEPROM_DaylightSavings_Address, isDST);
}

// Non-blocking slot machine style nixie tube poison prevention
unsigned long lastPoisonCycle = 0;      
unsigned long poisonInterval = 14400000; // 4 hours in ms
unsigned long stepInterval = 200;    // ms between digit updates
bool poisonPrevention = false;

uint8_t currentDigit[4]; // current digits being displayed
bool digitStopped[4]; // true when tube has stopped
unsigned long lastStepTime = 0;

int stopOrder[4] = {1, 0, 3, 2};  // rightmost first
int stopIndex = 0;                 // index in stopOrder
uint8_t targetDigit[4]; // Target digits to stop at

int fullSpinsBeforeStop = 2; // number of full rotations before stopping
int spinCounter[4] = {0, 0, 0, 0};
void Nixie_Poisoning_Prevention() {
    unsigned long poisonNow = millis();

    // Trigger poison prevention every 4 hours or at 3AM
    if ((!poisonPrevention && poisonNow - lastPoisonCycle >= poisonInterval) || longPoison) {
        
        // For longer cycle at 3AM
        if (longPoison) {
          stepInterval = 1000;
          fullSpinsBeforeStop = 1;
        }

        // For shorter cycle every 4 hours
        else {
          stepInterval = 200;
          fullSpinsBeforeStop = 2;
        }

        poisonPrevention = true;
        lastStepTime = poisonNow;
        lastPoisonCycle = poisonNow;

        // Get current time to use as final target
        DateTime nowTime = rtc.now();
        int hour24 = nowTime.hour();
        int hour12 = hour24 % 12;
        if (hour12 == 0) hour12 = 12;

        targetDigit[0] = nowTime.minute() / 10;
        targetDigit[1] = nowTime.minute() % 10;
        targetDigit[2] = hour12 / 10;
        targetDigit[3] = hour12 % 10;

        // Initialize current digits and stop flags
        for (int i = 0; i < 4; i++) {
            currentDigit[i] = 0;  // start spinning from 0
            digitStopped[i] = false;
            spinCounter[i] = 0;   // reset full spin counter
        }
        stopIndex = 0;
    }

    if (!poisonPrevention) return;

    // Update digits if step interval elapsed
    if (poisonNow - lastStepTime >= stepInterval) {
        lastStepTime = poisonNow;

        // Spin digits that haven't stopped yet
        for (int i = 0; i < 4; i++) {
            if (!digitStopped[i]) {
                currentDigit[i]++;
                if (currentDigit[i] > 9) {
                    currentDigit[i] = 0;
                    spinCounter[i]++; // one full rotation completed
                }
            }
        }

        // Stop digits one by one in stopOrder at targetDigit after enough spins
        if (stopIndex < 4) {
            int idx = stopOrder[stopIndex];
            if (spinCounter[idx] >= fullSpinsBeforeStop && currentDigit[idx] == targetDigit[idx]) {
                digitStopped[idx] = true;
                stopIndex++;  // move to next digit
            }
        }

        // Display the current digits
        displayDigit(currentDigit);

        // Finish cycle if all digits stopped
        bool allStopped = true;
        for (int i = 0; i < 4; i++) {
            if (!digitStopped[i]) allStopped = false;
        }
        if (allStopped) {
            poisonPrevention = false; // done
        }
    }
}

void setup() {
  Serial.begin(9600);
  pinMode(Shift_Data, OUTPUT);
  pinMode(Shift_CLK, OUTPUT);
  pinMode(Shift_Latch, OUTPUT);

  pinMode(Colon_Digit, OUTPUT);

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  // Begin PWM for PCA RGB chip
  pwm.begin();
  pwm.setPWMFreq(1000);
  initGammaTable(RGB_Brightness);

  // Load stored EEPROM values from particular address in flash
  EEPROM.get(EEPROM_Brightness_Address, RGB_Brightness);
  EEPROM.get(EEPROM_Preset_Address, currentPreset);
  EEPROM.get(EEPROM_DaylightSavings_Address, isDST);

  // Begin IR reciever for remote input
  IrReceiver.begin(IR_Pin, ENABLE_LED_FEEDBACK);

  // Only uncomment below to set current time on real-time-clock
  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
}

// Sets all LEDs red
void RGB_red() {
  for (uint8_t led = 0; led < 4; led++) {
    pwm.setPWM(rgbChannels[led][0], 0, RGB_Brightness); 
    pwm.setPWM(rgbChannels[led][1], 0, 0);              
    pwm.setPWM(rgbChannels[led][2], 0, 0);             
  }
}

// Sets all LEDs green
void RGB_green() {
  for (uint8_t led = 0; led < 4; led++) {
    pwm.setPWM(rgbChannels[led][0], 0, 0);
    pwm.setPWM(rgbChannels[led][1], 0, RGB_Brightness);           
    pwm.setPWM(rgbChannels[led][2], 0, 0);             
  }
}

// Sets all LEDs blue
void RGB_blue() {
  for (uint8_t led = 0; led < 4; led++) {
    pwm.setPWM(rgbChannels[led][0], 0, 0); 
    pwm.setPWM(rgbChannels[led][1], 0, 0);             
    pwm.setPWM(rgbChannels[led][2], 0, RGB_Brightness);              
  }
}
unsigned long lastWaveUpdate = 0;
unsigned long waveDelay = 10; // Animation speed
float waveStep = 0.0;         // phase step for wave

void RGB_yellow_orange() {
    unsigned long now = millis();
    if (now - lastWaveUpdate < waveDelay)
        return;

    lastWaveUpdate = now;

    for (uint8_t led = 0; led < 4; led++) {
        float phase = (waveStep * 0.05) + (led * 1.0); // phase offset per LED
        float raw = (sin(phase) * 0.5 + 0.5);          // normalized 0–1 intensity

        uint8_t r8, g8, b8;

        if (led % 2 == 0) {
            // Yellow-ish LED (can adjust if needed)
            r8 = 255 * raw;
            g8 = 220 * raw;
            b8 = 0;
        } else {
            // Orange LED
            r8 = 255 * raw;
            g8 = 20 * raw;
            b8 = 0;
        }

        // Map directly to PWM with linear scaling (no gamma)
        uint16_t r_pwm = map(r8, 0, 255, 0, RGB_Brightness);
        uint16_t g_pwm = map(g8, 0, 255, 0, RGB_Brightness);
        uint16_t b_pwm = map(b8, 0, 255, 0, RGB_Brightness);

        pwm.setPWM(rgbChannels[led][0], 0, r_pwm);
        pwm.setPWM(rgbChannels[led][1], 0, g_pwm);
        pwm.setPWM(rgbChannels[led][2], 0, b_pwm);
    }

    waveStep += 0.3;
}
// Sets all LEDs cyan
void RGB_cyan() {
  uint16_t r_pwm = gammaLUT[0];   // red off
  uint16_t g_pwm = gammaLUT[255]; // max green
  uint16_t b_pwm = gammaLUT[255]; // max blue

  for (uint8_t led = 0; led < 4; led++) {
    pwm.setPWM(rgbChannels[led][0], 0, r_pwm); 
    pwm.setPWM(rgbChannels[led][1], 0, g_pwm);             
    pwm.setPWM(rgbChannels[led][2], 0, b_pwm);              
  }
}

// Sets all LEDs magenta
void RGB_magenta() {
  uint16_t r_pwm = gammaLUT[255]; // max red
  uint16_t b_pwm = gammaLUT[255]; // max blue
  uint16_t g_pwm = gammaLUT[0];   // green off


  for (uint8_t led = 0; led < 4; led++) {
    pwm.setPWM(rgbChannels[led][0], 0, r_pwm); 
    pwm.setPWM(rgbChannels[led][1], 0, g_pwm);             
    pwm.setPWM(rgbChannels[led][2], 0, b_pwm);              
  }
}

// Sets all LEDs purple and yellow and creates a fade wave across LEDs
unsigned long lastLSUUpdate = 0;
unsigned long lsuDelay = 1; // Animation speed
float lsuStep = 0.0; // phase step for wave
void LSU() {
  unsigned long now = millis();
  if (now - lastLSUUpdate < lsuDelay)
    return;

  lastLSUUpdate = now;

  for (uint8_t led = 0; led < 4; led++) {
    float phase = (lsuStep * 0.02) + (led * 1.0);
    float raw = (sin(phase) * 0.5 + 0.5); // normalized 0–1
    uint16_t r, g, b;

    if (led % 2 == 0) {
      // Yellow
      r = raw * RGB_Brightness;
      g = raw * RGB_Brightness;
      b = 0;
    } else {
      // Purple
      r = raw * RGB_Brightness;
      g = 0;
      b = raw * RGB_Brightness;
    }
    
    uint8_t r8 = constrain((int)r, 0, 255);
    uint8_t g8 = constrain((int)g, 0, 255);
    uint8_t b8 = constrain((int)b, 0, 255);

    uint16_t r_pwm = gammaLUT[r8];
    uint16_t g_pwm = gammaLUT[g8];
    uint16_t b_pwm = gammaLUT[b8];

    // Display to RGB
    pwm.setPWM(rgbChannels[led][0], 0, r_pwm); 
    pwm.setPWM(rgbChannels[led][1], 0, g_pwm);             
    pwm.setPWM(rgbChannels[led][2], 0, b_pwm);   
  }
  lsuStep += 0.3;
}

  
// Fire effect: base orange with yellow flicker highlights
void Fire() {
    static unsigned long lastFireUpdate = 0;
    static unsigned long fireInterval = 50;

    unsigned long now = millis();
    if (now - lastFireUpdate < fireInterval) return;
    lastFireUpdate = now;

    fireInterval = random(30, 150); // random flicker speed

    for (int i = 0; i < 4; i++) { // 4 RGB LEDs
        // Base orange (darker)
        uint8_t baseR = 200;
        uint8_t baseG = 60;
        uint8_t baseB = 0;

        // Convert to PWM
        uint16_t r_pwm, g_pwm, b_pwm;
        rgbToPwm(baseR, baseG, baseB, r_pwm, g_pwm, b_pwm);

        // Apply gamma correction
        r_pwm = gammaLUT[ map(r_pwm, 0, 4095, 0, 255) ];
        g_pwm = gammaLUT[ map(g_pwm, 0, 4095, 0, 255) ];
        b_pwm = gammaLUT[ map(b_pwm, 0, 4095, 0, 255) ];

        // Flicker highlights (yellowish)
        int r_flicker = constrain(r_pwm + random(0, 55), 0, 4095);  // R brighter
        int g_flicker = constrain(g_pwm + random(0, 40), 0, 4095);  // G brighter
        int b_flicker = constrain(b_pwm + random(0, 20), 0, 4095);  // B subtle

        // Send to PWM
        pwm.setPWM(rgbChannels[i][0], 0, r_flicker);
        pwm.setPWM(rgbChannels[i][1], 0, g_flicker);
        pwm.setPWM(rgbChannels[i][2], 0, b_flicker);
    }
}

// Rainbow effect using PCA9685 PWM
unsigned long rainbowWait = 20;       // speed of animation in ms (frame delay)
unsigned long lastRainbowStep = 0;
float rainbowStep = 0.0;              // step for color cycling
void Rainbow() {
  unsigned long now = millis();
  if (now - lastRainbowStep >= rainbowWait) {
    lastRainbowStep = now;

    for (int i = 0; i < 4; i++) { // 4 RGB LEDs
      float phase = (rainbowStep * 0.02) + (i * 1.0); // smaller multiplier = slower, smoother

      int r = (sin(phase) * 127 + 128);
      int g = (sin(phase + 2.0) * 127 + 128);
      int b = (sin(phase + 4.0) * 127 + 128);

      // Apply brightness scaling (0-255)
      uint8_t r8 = map(constrain(r, 0, 255), 0, 255, 0, min(RGB_Brightness, 255));
      uint8_t g8 = map(constrain(g, 0, 255), 0, 255, 0, min(RGB_Brightness, 255));
      uint8_t b8 = map(constrain(b, 0, 255), 0, 255, 0, min(RGB_Brightness, 255));

      // Apply gamma correction
      uint16_t pr = gammaLUT[r8];
      uint16_t pg = gammaLUT[g8];
      uint16_t pb = gammaLUT[b8];

      // Send to PWM
      pwm.setPWM(rgbChannels[i][0], 0, pr);
      pwm.setPWM(rgbChannels[i][1], 0, pg);
      pwm.setPWM(rgbChannels[i][2], 0, pb);
    }

    // Increment rainbowStep by a small float for smooth flow
    rainbowStep += 1.0;
    if (rainbowStep >= 65536.0) {
      rainbowStep = 0.0; // loop back seamlessly
    }
  }
}

// Colon fade in and out logic using PWM
unsigned long lastColonUpdate = 0;
int colonBrightness = 0;    // 0–255
int colonStep = 5;          // change per update
const unsigned long colonInterval = 30; // ms between updates
void colonFade() {
  unsigned long now = millis();
  if (now - lastColonUpdate >= colonInterval) {
    lastColonUpdate = now;
    colonBrightness += colonStep;

    if (colonBrightness >= 255) {
      colonStep = -colonStep;
    }

    if (colonBrightness <= 0) {
      colonStep = -colonStep;
    }

    analogWrite(Colon_Digit, colonBrightness);
  }
}

void loop() {
  now = rtc.now(); // Fetches current date and time from real-time-clock

  // Converts 24 hour to 12 hour time
  int hour24 = now.hour();
  int hour12 = hour24 % 12;
  if (hour12 == 0)
    hour12 = 12;
  
  uint8_t digits[4] = {
    (uint8_t)(now.minute() / 10),
    (uint8_t)(now.minute() % 10),
    (uint8_t)(hour12 / 10),
    (uint8_t)(hour12 % 10)
  };

  uint8_t digitsToShow[4];
  if (showingDate) {
    Display_Date();  // handles timeout
  } 
  else if (poisonPrevention) {
    for (int i = 0; i < 4; i++)
        digitsToShow[i] = currentDigit[i]; // slot-machine digits
  }
  else {
    for (int i = 0; i < 4; i++)
      digitsToShow[i] = digits[i];      // normal time
  }

  if (showingDate) 
    displayDigit(Date_Digits);
  else 
    displayDigit(digitsToShow);

  colonFade(); // Fades colon in and out
  
  Nixie_Poisoning_Prevention(); // Non-blocking nixie tube poisoning prevention

  longPoison = false;
  if (now.hour() == 3 && now.minute() == 0) {
    longPoison = true;
    Nixie_Poisoning_Prevention(); // Longer Nixie tube poisoning prevention at 3AM
  }

  // Sets RGB preset assigned on IR remote to display 
  if (currentPreset != activeEffect) {
    activeEffect = currentPreset;
  }

  // For animated presets to run properly
    switch(currentPreset) {
      case Preset_Off:
        break;

      case Preset_Red:
        RGB_red();
        break;

      case Preset_Green:
        RGB_green();
        break;

      case Preset_Blue:
        RGB_blue();
        break;

      case Preset_RGB_yellow_orange:
        RGB_yellow_orange();
        break;

      case Preset_Cyan:
        RGB_cyan();
        break;

      case Preset_Magenta:
        RGB_magenta();
        break;

      case Preset_LSU:
        LSU();
        break;

      case Preset_Fire:
        Fire();
        break;

      case Preset_Rainbow:
        Rainbow();
        break;
      }

  // Decodes HEX values of recieved IR data into various clock functions
  if (IrReceiver.decode()) {
    code = IrReceiver.decodedIRData.decodedRawData;

    Serial.println(code, HEX);
    IrReceiver.printIRResultShort(&Serial);
    IrReceiver.printIRSendUsage(&Serial);

    // --- Handle presets, DST, date display ---
      switch (code) {
        case Btn_UpArrow:
          RGB_Brightness += 200;
          RGB_Brightness = constrain(RGB_Brightness, 100, 4095);
          EEPROM.put(EEPROM_Brightness_Address, RGB_Brightness);
          break;

        case Btn_DownArrow:
          RGB_Brightness -= 200;
          RGB_Brightness = constrain(RGB_Brightness, 100, 4095);
          EEPROM.put(EEPROM_Brightness_Address, RGB_Brightness);
          break;

        case Btn_pnd:
            Date_Digits[0] = now.day() / 10;
            Date_Digits[1] = now.day() % 10;
            Date_Digits[2] = now.month() / 10;
            Date_Digits[3] = now.month() % 10;
            dateDisplayStart = millis();
            showingDate = true;
            break;

        case Btn_asterisk:
            Daylight_Savings();
            break;

        case Btn_0:
          currentPreset = Preset_Off;
          EEPROM.put(EEPROM_Preset_Address, currentPreset);

          for (uint8_t led = 0; led < 4; led++) {
            pwm.setPWM(rgbChannels[led][0], 0, 0);
            pwm.setPWM(rgbChannels[led][1], 0, 0);
            pwm.setPWM(rgbChannels[led][2], 0, 0);
          }
          break;
        case Btn_1:
            currentPreset = Preset_Red;
            EEPROM.put(EEPROM_Preset_Address, currentPreset);
            break;
        case Btn_2:
            currentPreset = Preset_Blue;
            EEPROM.put(EEPROM_Preset_Address, currentPreset);
            break;
        case Btn_3:
            currentPreset = Preset_Green;
            EEPROM.put(EEPROM_Preset_Address, currentPreset);
            break;
        case Btn_4:
            currentPreset = Preset_RGB_yellow_orange;
            EEPROM.put(EEPROM_Preset_Address, currentPreset);
            break;
        case Btn_5:
            currentPreset = Preset_Cyan;
            EEPROM.put(EEPROM_Preset_Address, currentPreset);
            break;
        case Btn_6:
            currentPreset = Preset_Magenta;
            EEPROM.put(EEPROM_Preset_Address, currentPreset);
            break;
        case Btn_7:
            currentPreset = Preset_LSU;
            EEPROM.put(EEPROM_Preset_Address, currentPreset);
            break;
        case Btn_8:
            currentPreset = Preset_Fire;
            EEPROM.put(EEPROM_Preset_Address, currentPreset);
            break;
        case Btn_9:
            currentPreset = Preset_Rainbow;
            EEPROM.put(EEPROM_Preset_Address, currentPreset);
            break;
    }
    IrReceiver.resume(); // ready for next IR signal
  }
}