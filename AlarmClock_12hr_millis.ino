/*
  :Project:Clock_Alarm
  :Author: Joel Cranmer
  :Date: 11/8/2018
  :Revision: 1.1
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
#define MENU_PB 7 // Button SET MENU'
#define PLUS_PB 4 // Button +
#define MINUS_PB 1 // Button -
#define LEDS 6 // LED strip

//************Constants*****************//
const int blinkDuration = 500;
const int blinkInterval = 500;
const int LEDInterval = 1000; // every second

const int buttonInterval = 300; // number of millisecs between button readings

//************Variables**************//
RTC_PCF8523 rtc;
Adafruit_7segment disp = Adafruit_7segment();
Adafruit_NeoPixel strip = Adafruit_NeoPixel(8, LEDS, NEO_GRB + NEO_KHZ800);

uint8_t hourupd;
uint8_t minupd;
uint8_t menu;
bool PM;
uint16_t LEDtimer = 0;
uint8_t alarmHour;
uint8_t alarmMin;
bool alarmPM;

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
  // Set Brighness to low
  disp.setBrightness(0);
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'

  pinMode(MENU_PB, INPUT_PULLUP);
  pinMode(PLUS_PB, INPUT_PULLUP);
  pinMode(MINUS_PB, INPUT_PULLUP);

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
    // Comment out for debug
    menu = menu + 1;
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
}

void DisplayDateTime ()
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
  if (Blink_State)
  {
    if (PM) disp.writeDigitRaw(2, 0x0A);
    else disp.drawColon(true);
  }
  else
  {
    if (PM) disp.writeDigitRaw(2, 0x08);
  }
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
    UpMin();
  }
  if (Minus_State)
  {
    DownMin();
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

void UpMin()
{
  if (minupd == 59)
  {
    minupd = 0;
  }
  else
  {
    minupd = minupd + 1;
  }
}

void DownMin()
{
  if (minupd == 0)
  {
    minupd = 59;
  }
  else
  {
    minupd = minupd - 1;
  }
}

void UpdateLEDs()
{
  if (currentMillis - prevLEDMillis >= LEDInterval)
  {
    int AlarmTime = (alarmHour * 100) + alarmMin;
    DateTime now = rtc.now();
    int time = (now.hour() * 100) + now.minute();
    if (time >= (AlarmTime - 2) && time <= (AlarmTime + 3))
    {
      Sunrise(LEDtimer);
      // go for 6 minutes
      LEDtimer = LEDtimer + 1;
      if (LEDtimer > 360) LEDtimer = 0;
    }
    else {
      ClearLEDs();
      LEDtimer = 0;
    }
    prevLEDMillis = prevLEDMillis + LEDInterval;
  }
}

void Sunrise( uint16_t t )
{
  uint32_t black = strip.Color(0, 0, 0);
  uint32_t pink = strip.Color(32, 10, 16);
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
  else if (t > 100 && t <= 200) // pink -> yellow
  {
    color = InterpolateColors((t - 100), pink, yellow);
  }
  else // yellow -> white
  {
    color = InterpolateColors((t - 200), yellow, white);
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
