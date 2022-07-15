#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "RTClib.h"
#include <LedMatrixDefinition.h>

///////////////
// Functions //
///////////////
void colorWipe(uint32_t color, int wait);
void button_ISR();
void updateFrame(uint8_t *message, uint8_t len, uint8_t row, uint32_t col);
// void printNumber(uint8_t *message, uint8_t len, uint8_t row, uint32_t col);

/////////////
// DEFINES //
/////////////
#define _ENABLE_SERIAL_
#define _ENABLE_LEDS_
#define _ENABLE_DS3231_
#define _ENABLE_INTERRUPTS_

#define FALSE 0
#define TRUE 1
#define SERIAL_SPEED      115200
#define REFRESH_TIME_STD  1000 // Refresh time for standard mode (Time\Temperature) [ms]
#define REFRESH_TIME_FAST 200  // Fast refresh time for time setting mode [ms]
#define MENU_TIME         5000 // [ms]
#define MENU_TIME_STD     MENU_TIME/REFRESH_TIME_STD
#define MENU_TIME_FAST    MENU_TIME/REFRESH_TIME_FAST
#define BUTTON_PIN 2
#define LED_PIN 12             // Which pin on the Arduino is connected to the NeoPixels?
#define LED_COUNT 125          // How many NeoPixels are attached to the Arduino?
#define RED     0x00FF0000
#define GREEN   0x0000FF00
#define BLUE    0x000000FF
#define WHITE   0x00FFFFFF

//////////////////////
// Global variables //
//////////////////////
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
enum states {S_TIME, S_TEMPERATURE, S_SET_HOUR, S_SET_MINUTE};
enum buttonStates {RELEASED, PRESSED};
uint8_t operatingMode = S_TIME;
uint8_t menuTimer = 0;
bool shortPressEvent = FALSE;
DateTime now; // Uninitialized :( 
unsigned long refreshTime = REFRESH_TIME_STD; // How often to refresh while in time mode.
uint8_t hour, minutes, temperature, minutes_range, dots = 0;

#ifdef _ENABLE_DS3231_
RTC_DS3231 rtc;
#endif // _ENABLE_DS3231_

#ifdef _ENABLE_INTERRUPTS_
const int buttonPin = BUTTON_PIN;
const int SHORT_PRESS_TIME = 500;
unsigned long timePressed, timeReleased = 0;
long pressDuration = 0;
#endif // _ENABLE_INTERRUPTS_


void setup() {
  #ifdef _ENABLE_SERIAL_
  Serial.begin(SERIAL_SPEED);
  #endif // _ENABLE_SERIAL_

  #ifdef _ENABLE_INTERRUPTS_
  pinMode(buttonPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(buttonPin), button_ISR, CHANGE);
  #endif // _ENABLE_INTERRUPTS_

  #ifdef _ENABLE_DS3231_
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1) delay(10);
  }
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, let's set the time!");
    rtc.adjust(DateTime(2017, 10, 30, 10, 25, 0));
  }
  #endif // _ENABLE_DS3231_

  #ifdef _ENABLE_LEDS_
  strip.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
  strip.setBrightness(255);  // Set BRIGHTNESS (max = 255)
  #endif // _ENABLE_LEDS_
}


void loop() {

  /////////////////////////
  // TODO: LED INTENSITY //
  /////////////////////////
  
  strip.clear();
  // #define _SINGLE_LED_ONLY_
  #ifdef _SINGLE_LED_ONLY_
  strip.setPixelColor(0, WHITE);
  strip.show();
  delay(5000);
  strip.clear();
  // strip.setPixelColor(1, WHITE);
  strip.fill(WHITE,0,LED_COUNT); // Turn on all leds.
  strip.show();
  delay(4000);
  #endif // _SINGLE_LED_ONLY_

  refreshTime = REFRESH_TIME_STD;
  #ifndef _SINGLE_LED_ONLY_
  switch (operatingMode){
    case S_TIME:
      now = rtc.now(); // Get time
      hour = (now.hour()>12 ? (now.hour()-12) : (now.hour())); // Convert 24-h format into 12-h format
      Serial.print("Time mode: ");
      Serial.print(hour, DEC);
      Serial.print(':');
      Serial.println(now.minute(), DEC);
      // Set hours
      if (hour != 1){
        updateFrame(HOURS[0], sizeof(HOURS[0]), NO_ROW, WHITE); // Unless it is 1 print "Sono le ore"
      }
      updateFrame(HOURS[hour], sizeof(HOURS[hour]), NO_ROW, WHITE);
      // Set minutes (words)
      minutes_range = (uint8_t)floor(now.minute()/5);
      updateFrame(MINUTES[minutes_range], sizeof(MINUTES[minutes_range]), NO_ROW, WHITE);
      // Set minutes (dots)
      dots = now.minute() - minutes_range * 5;
      updateFrame(DOTS[dots], sizeof(DOTS[dots]), NO_ROW, WHITE);

      break;

    case S_TEMPERATURE:
      temperature = (uint8_t)(rtc.getTemperature()+0.5); // Get temperature (and convert in integer)
      Serial.print("Temperature mode: ");
      Serial.println(temperature);
      updateFrame(NUMBERS[temperature/10], sizeof(NUMBERS[temperature/10]), 6, RED);
      updateFrame(NUMBERS[temperature-(temperature/10)*10], sizeof(NUMBERS[temperature-(temperature/10)*10]), 0, RED);
      menuTimer++; // Back to time mode once timer expires
      if (menuTimer == MENU_TIME_STD){
        operatingMode = S_TIME;
      }
      break;

    case S_SET_HOUR:
      Serial.print("Setting hour: ");
      Serial.println(hour, DEC);
      refreshTime = REFRESH_TIME_FAST;
      if (shortPressEvent == TRUE){
        shortPressEvent = FALSE; // Clear event
        hour = (hour < 23 ? (hour+1) : (0)); // Increment hours
        menuTimer = 0; // Restart timer
      }
      updateFrame(ORE, sizeof(ORE), NO_ROW, BLUE);
      updateFrame(NUMBERS[hour/10], sizeof(NUMBERS[hour/10]), 6, GREEN);
      updateFrame(NUMBERS[hour-(hour/10)*10], sizeof(NUMBERS[hour-(hour/10)*10]), 0, GREEN);
      menuTimer++; // Moving to minutes set mode once timer expires
      if (menuTimer == MENU_TIME_FAST){
        operatingMode = S_SET_MINUTE;
        menuTimer = 0;
        now = rtc.now(); // Get time
        minutes = now.minute();
      }
      break;

    case S_SET_MINUTE:
      Serial.print("Setting minutes: ");
      Serial.println(minutes, DEC);
      refreshTime = REFRESH_TIME_FAST;
      if (shortPressEvent == TRUE){
        shortPressEvent = FALSE; // Clear event
        minutes = (minutes < 59 ? (minutes+1) : (0)); // Increment minutes
        menuTimer = 0; // Restart timer
      }
      updateFrame(MINUTI, sizeof(MINUTI), NO_ROW, BLUE);
      updateFrame(NUMBERS[minutes/10], sizeof(NUMBERS[minutes/10]), 6, GREEN);
      updateFrame(NUMBERS[minutes-(minutes/10)*10], sizeof(NUMBERS[minutes-(minutes/10)*10]), 0, GREEN);
      menuTimer++; // Back to time mode once timer expires
      if (menuTimer == MENU_TIME_FAST){
        rtc.adjust(DateTime(2017, 10, 30, hour, minutes, 0));
        operatingMode = S_TIME;
      }
      break;

    default:
      break;
  }
  #endif // _SINGLE_LED_ONLY_
  
  strip.show();
  delay(refreshTime);
}


void updateFrame(uint8_t *message, uint8_t len, uint8_t row, uint32_t col){
  for(int i=0; i<len; i++) { 
    // strip.setPixelColor(message[i], col);
    strip.setPixelColor(message[i] + row * 11, col);  
  }
}


#ifdef _ENABLE_INTERRUPTS_
void button_ISR() {
  if(digitalRead(buttonPin) == PRESSED){ // On press...
    timePressed = millis(); // No action on press. Simply record the time.
  } else { // On release...
    timeReleased = millis();
    // LONG PRESS - Only effective while in TIME mode
    if((timeReleased - timePressed) > SHORT_PRESS_TIME){
      Serial.println("Long press.");
      if (operatingMode == S_TIME){
        operatingMode = S_SET_HOUR;
        menuTimer = 0;
      }
      Serial.println(operatingMode);
    } else {
    // SHORT PRESS
      Serial.println("Short press.");
      if((operatingMode == S_TIME) || (operatingMode == S_TEMPERATURE)){
        operatingMode = S_TEMPERATURE;
        menuTimer = 0;
      } else {
        shortPressEvent = TRUE;
      }
      Serial.println(operatingMode);
    }
  }
  return;
}
#endif // _ENABLE_INTERRUPTS
