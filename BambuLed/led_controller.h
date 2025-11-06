#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <FastLED.h>
#include <Arduino.h>
#include "config.h"  // Add this include

// External declarations from main file
// extern Config config;
extern String current_gcode_state;
extern int current_print_percentage;
extern bool current_error_state;
extern unsigned long finishTime;
extern const unsigned long FINISH_LIGHT_TIMEOUT;

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
