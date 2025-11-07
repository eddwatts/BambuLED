#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <FastLED.h>
#include <Arduino.h>
#include "config.h"  // Add this include

// LED Constants
// #define LED_DATA_PIN 4
// const int LED_PIN_CONST = LED_DATA_PIN;
// #define MAX_LEDS 60

// External declarations from main file
// extern Config config;
extern String current_gcode_state;
extern int current_print_percentage;
extern bool current_error_state;
extern unsigned long finishTime;
extern const unsigned long FINISH_LIGHT_TIMEOUT;
extern unsigned long lastAnimationUpdate;

// Add external declarations needed for handleFinishTimers
extern bool external_light_is_on;
extern bool manual_light_control;

// LED array declaration
extern CRGB leds[MAX_LEDS];

// Function declarations
void initLEDStrip();
void updateLEDs();
void handleFinishTimers();

#endif