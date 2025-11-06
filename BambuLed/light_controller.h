#ifndef LIGHT_CONTROLLER_H
#define LIGHT_CONTROLLER_H

#include <Arduino.h>
#include <driver/ledc.h>
#include "config.h" // <-- ADDED

// External declarations from main file
extern bool external_light_is_on;
extern bool manual_light_control;

// Add extern for WiFiManager parameter
extern WiFiManagerParameter custom_bbl_pin;

// Function declarations
void initChamberLight();
void reinitHardwareIfNeeded();
void setChamberLightState(bool lightShouldBeOn);
void setupChamberLightPWM(int pin);

#endif