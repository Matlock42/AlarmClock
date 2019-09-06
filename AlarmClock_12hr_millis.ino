/*
  :Project:Clock_Alarm
  :Author: Joel Cranmer
  :Date: 9/6/2019
  :Revision: 1.3.1
  :License: MIT License
*/
//************libraries**************//
#include <Wire.h>
#include "Adafruit_GFX.h"
#include <RTClib.h>
#include "Adafruit_LEDBackpack.h"
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
#include <avr/power.h>
#endif

//************Buttons*****************//
#define MENU_PB 6 // Button SET MENU'
#define PLUS_PB 4 // Button +
#define MINUS_PB 0 // Button -
#define LEDS 7 // LED strip
#define AlarmON 5 // Alarm Toggle switch
#define SNOOZE 8 // Snooze button

//************Constants*****************//
const int blinkDuration = 500;
const int blinkInterval = 500;
const int LEDInterval = 1000;     // every second
const int buttonInterval = 300;   // number of millisecond between button readings

const int AlarmStartAt = -1 * 60; // seconds to start alarm cycle from alarm time; (Negative is previous; positive is after)
const int AlarmEndAt = 4 * 60;    // seconds to end alarm cycle from alarm time;

//************Variables**************//
RTC_PCF8523 rtc;
Adafruit_7segment disp = Adafruit_7segment();
Adafruit_NeoPixel strip = Adafruit_NeoPixel(12, LEDS, NEO_GRB + NEO_KHZ800);

uint8_t hourupd;
uint8_t minupd;
uint8_t menu;
bool PM;
uint8_t alarmHour;
uint8_t alarmMin;
bool alarmPM;
bool AlarmLatch = false; // Used to hold LEDs on for longer

bool Blink_State = 0;
bool Menu_State = 0;
bool Plus_State = 0;
bool Minus_State = 0;

unsigned long currentMillis = 0;   // stores the value of millis() in each iteration of loop()
unsigned long prevBlinkMills = 0;  // stores last time LED was updated
unsigned long prevMenuMillis = 0;  // time when button press last checked
unsigned long prevPlusMillis = 0;
unsigned long prevMinusMillis = 0;
unsigned long prevLEDMillis = 0;  // time when LEDs last checked

void setup()
{
  disp.begin(0x70);
  // Set Brightness to low
  disp.setBrightness(0);
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'

  pinMode(MENU_PB, INPUT_PULLUP);
  pinMode(PLUS_PB, INPUT_PULLUP);
  pinMode(MINUS_PB, INPUT_PULLUP);
  pinMode(AlarmON, INPUT_PULLUP);
  pinMode(SNOOZE, INPUT_PULLUP);

  Serial.begin(57600);
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  menu = 0;
  alarmHour = 06;
  alarmMin = 00;

  // following line sets the RTC to the date & time this sketch was compiled
  //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  // Print JOEL
  disp.clear();
  disp.writeDigitRaw(0, 0x1E);
  disp.writeDigitNum(1, 0, false);
  disp.writeDigitNum(3, 0xE, false);
  disp.writeDigitRaw(4, 0x38);
  disp.writeDisplay();
  delay(1000);
}

void loop()
{
  currentMillis = millis();
  updateBlinkState();
  readMenuButton();
  readPlusButton();
  readMinusButton();
  UpdateLEDs();

  if (Menu_State)
  {
    if (digitalRead(AlarmON) == LOW && menu == 0)
    {
      menu = 6;
    }
    else
    {
      menu = menu + 1;
    }
  }
  
  // in which subroutine should we go?
  if (menu == 0)
  {
    DisplayDateTime(); // void DisplayDateTime
  }
  if (menu == 1)
  {
    DisplaySetHour();
  }
  if (menu == 2)
  {
    DisplaySetMinute();
  }
  if (menu == 3)
  {
    StoreAgg();
    menu = 0;
  }
  if (menu == 6)
  {
    DisplaySetAlarmHour();
  }
  if (menu == 7)
  {
    DisplaySetAlarmMinute();
  }
  if (menu == 8)
  {
    menu=0;
  }
}

void DisplayDateTime()
{
  uint8_t hr_24, hr_12;
  // We show the current time
  DateTime now = rtc.now();
  hr_24 = now.hour();
  // Convert to 12hr format
  if (hr_24 == 0 || hr_24 == 12) hr_12 = 12;
  else hr_12 = hr_24 % 12;
  // set AM/PM
  if (hr_24 >= 12) PM = true;
  else PM = false;

  uint16_t decimalTime = hr_12 * 100 + now.minute(); // 12:30 -> 1230

  // No BLINKING!!!
  disp.blinkRate(0);
  disp.print(decimalTime);

  uint8_t dots = 0;
  // src: https://learn.adafruit.com/adafruit-led-backpack/1-2-inch-7-segment-backpack
  // PM (bottom left)   - 0x08
  // Alarm (top left)   - 0x04
  // colon (:)          - 0x02
  // decimal (top right)- 0x10
  
  if (Blink_State)
  {
    dots = 0x02;
  }
  if (PM) dots += 0x08;
  if (digitalRead(AlarmON) == LOW) dots += 0x04;
  disp.writeDigitRaw(2, dots);
  
  disp.writeDisplay();

  // update memory
  hourupd = hr_12;
  minupd = now.minute();
}

// *********************
// Time Setting
// *********************

void DisplaySetHour()
{
  // Setting the hour
  if (Plus_State)
  {
    UpHour();
  }
  if (Minus_State)
  {
    DownHour();
  }
  // clear the min
  disp.clear();
  disp.drawColon(true);
  if (PM) disp.writeDigitRaw(2, 0x0A);
  disp.blinkRate(0);
  // show only hour (not min)
  disp.writeDigitNum(0, (hourupd / 10) % 10, false);
  disp.writeDigitNum(1, hourupd % 10, false);
  disp.writeDisplay();
}

void DisplaySetMinute()
{
  // Setting the minutes
  if (Plus_State)
  {
    minupd = UpMin(minupd);
  }
  if (Minus_State)
  {
    minupd = DownMin(minupd);
  }
  // clear the hour
  disp.clear();
  disp.drawColon(true);
  disp.blinkRate(0);
  // show only min (not hour)
  disp.writeDigitNum(3, (minupd / 10) % 10, false);
  disp.writeDigitNum(4, minupd % 10, false);
  disp.writeDisplay();
}

void StoreAgg()
{
  // Variable saving
  DateTime now = rtc.now();
  if (PM) hourupd = hourupd + 12;
  if (hourupd > 23) hourupd = 0;
  rtc.adjust(DateTime(now.year(), now.month(), now.day(), hourupd, minupd, 0));
  DisplayDateTime();
}

// *********************
// Alarm Setting
// *********************

void DisplaySetAlarmHour()
{
  // Setting the hour
  if (Plus_State)
  {
    UpAlarmHour();
  }
  if (Minus_State)
  {
    DownAlarmHour();
  }
  // clear the min
  disp.clear();
  uint8_t dots = 0x02 + 0x04;
  if (alarmPM) dots += 0x08;
  disp.writeDigitRaw(2, dots);
  disp.blinkRate(0);
  // show only hour (not min)
  disp.writeDigitNum(0, (alarmHour / 10) % 10, false);
  disp.writeDigitNum(1, alarmHour % 10, false);
  disp.writeDisplay();
}

void DisplaySetAlarmMinute()
{
  // Setting the minutes
  if (Plus_State)
  {
    alarmMin = UpMin(alarmMin);
  }
  if (Minus_State)
  {
    alarmMin = DownMin(alarmMin);
  }
  // clear the hour
  disp.clear();
  uint8_t dots = 0x02 + 0x04;
  disp.writeDigitRaw(2, dots);
  disp.blinkRate(0);
  // show only min (not hour)
  disp.writeDigitNum(3, (alarmMin / 10) % 10, false);
  disp.writeDigitNum(4, alarmMin % 10, false);
  disp.writeDisplay();
}

void readMenuButton()
{
  if (millis() - prevMenuMillis >= buttonInterval)
  {
    if (digitalRead(MENU_PB) == LOW)
    {
      Menu_State =  HIGH;
      prevMenuMillis = currentMillis;
    }
  }
  else
  {
    Menu_State = LOW;
  }
}

void readPlusButton()
{
  if (millis() - prevPlusMillis >= buttonInterval)
  {
    if (digitalRead(PLUS_PB) == LOW)
    {
      Plus_State = HIGH;
      prevPlusMillis = currentMillis;
    }
  }
  else
  {
    Plus_State = LOW;
  }
}

void readMinusButton()
{
  if (millis() - prevMinusMillis >= buttonInterval)
  {
    if (digitalRead(MINUS_PB) == LOW)
    {
      Minus_State = HIGH;
      prevMinusMillis = currentMillis;
    }
  }
  else
  {
    Minus_State = LOW;
  }
}

void updateBlinkState()
{
  if (Blink_State == 0)
  {
    if (currentMillis - prevBlinkMills >= blinkInterval) {
      Blink_State = 1;
      prevBlinkMills += blinkInterval;
    }
  }
  else
  {
    if (currentMillis - prevBlinkMills >= blinkDuration) {
      Blink_State = LOW;
      prevBlinkMills += blinkDuration;
    }
  }
}

void UpHour()
{
  if (hourupd == 11) PM = !PM; // switch AM/PM
  if (hourupd == 12)
  {
    hourupd = 1;
  }
  else
  {
    hourupd = hourupd + 1;
  }
}

void DownHour()
{
  if (hourupd == 12) PM = !PM;
  if (hourupd == 1)
  {
    hourupd = 12;
  }
  else
  {
    hourupd = hourupd - 1;
  }
}

uint8_t UpMin(uint8_t min)
{
  if (min == 59)
  {
    min = 0;
  }
  else
  {
    min = min + 1;
  }
  return min;
}

uint8_t DownMin(uint8_t min)
{
  if (min == 0)
  {
    min = 59;
  }
  else
  {
    min = min - 1;
  }
  return min;
}

void UpAlarmHour()
{
  if (alarmHour == 11) alarmPM = !alarmPM; // switch AM/PM
  if (alarmHour == 12)
  {
    alarmHour = 1;
  }
  else
  {
    alarmHour = alarmHour + 1;
  }
}

void DownAlarmHour()
{
  if (alarmHour == 12) alarmPM = !alarmPM;
  if (alarmHour == 1)
  {
    alarmHour = 12;
  }
  else
  {
    alarmHour = alarmHour - 1;
  }
}

void UpdateLEDs()
{
  if (currentMillis - prevLEDMillis >= LEDInterval)
  {
    int AlarmTime = (alarmHour * 60 * 60) + alarmMin * 60;
    if (alarmPM)
    {  AlarmTime = AlarmTime + (12 * 60 * 60); }
    DateTime now = rtc.now();
    int curTime = (now.hour() * 60 * 60) + (now.minute() * 60) + now.second();
    int diff = curTime - AlarmTime;
    if (diff > AlarmStartAt && diff < AlarmEndAt )
    {
      Sunrise(diff-AlarmStartAt);
      AlarmLatch = true; // force "Snooze" to be pressed.
    }
    else // Stop updating the LEDs
    {
      //if(AlarmLatch == false) // when "Snooze" is pressed
      //{
        ClearLEDs();
      //}
    }
    prevLEDMillis = prevLEDMillis + LEDInterval;
  }
}

void Sunrise( uint16_t t )
{
  uint32_t black = strip.Color(0, 0, 0);
  uint32_t pink = strip.Color(32, 10, 16);
  uint32_t orange = strip.Color(127, 48, 10);
  uint32_t yellow = strip.Color(127, 127, 16);
  uint32_t white = strip.Color(191, 191, 98);
  uint32_t color;

  // clear all previous colors
  for (uint16_t i = 0; i < strip.numPixels(); i++)
  {
    strip.setPixelColor(i, 0);
  }

  if (t <= 100) // black -> pink
  {
    color = InterpolateColors( t, black, pink);
  }
  else if (t > 100 && t <= 200) // pink -> orange
  {
    color = InterpolateColors((t - 100), pink, orange);
  }
  else // orange -> yellow
  {
    color = InterpolateColors((t - 200), orange, yellow);
  }

  // Set all pixels to color
  for (uint16_t i = 0; i < strip.numPixels(); i++)
  {
    strip.setPixelColor(i, color);
    //if(t<150) i++; // every other pixel
    //if(t<70) i++; // every 3rd pixel
  }
  // write the colors
  strip.show();
}

void ClearLEDs()
{
  for (uint16_t i = 0; i < strip.numPixels(); i++)
  {
    strip.setPixelColor(i, 0);
  }
  // write the colors
  strip.show();
}

/// <summary>
/// Provides linear interpolation between 2 colors
/// </summary>
/// <param name="x">Percentage between the two colors. 0 to 100.</param>
/// <param name="color1">First Color</param>
/// <param name="color2">Second Color</param>
/// <returns>Color somewhere between the two colors</returns>
uint32_t InterpolateColors(uint8_t x, uint32_t color1, uint32_t color2)
{
  uint16_t  r1, g1, b1;
  uint16_t  r2, g2, b2;
  uint8_t  r, g, b;

  // Decompose each component from the colors into separate R, G, B
  r1 = (uint16_t)((color1 >> 16) & 0xff);
  g1 = (uint16_t)((color1 >> 8) & 0xff);
  b1 = (uint16_t)(color1 & 0xff);

  r2 = (uint16_t)((color2 >> 16) & 0xff);
  g2 = (uint16_t)((color2 >> 8) & 0xff);
  b2 = (uint16_t)(color2 & 0xff);

  r = (uint8_t)((r1 * (100 - x) + r2 * (x)) / 100);
  g = (uint8_t)((g1 * (100 - x) + g2 * (x)) / 100);
  b = (uint8_t)((b1 * (100 - x) + b2 * (x)) / 100);

  return strip.Color((byte) r, (byte) g, (byte) b);
}
