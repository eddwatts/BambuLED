#include "light_controller.h"
#include "config.h"

// External declarations (from main)
extern bool external_light_is_on;
extern bool manual_light_control;

// External declarations (from config)
extern WiFiManagerParameter custom_bbl_pin;

void initChamberLight() {
  Serial.print("Initializing Light Pin (PWM): ");
  Serial.println(config.chamber_light_pin);
  if (!isValidGpioPin(config.chamber_light_pin)) {
      Serial.printf("ERROR: Configured light pin (%d) is invalid! Using default (%d).\n", config.chamber_light_pin, DEFAULT_CHAMBER_LIGHT_PIN);
      config.chamber_light_pin = DEFAULT_CHAMBER_LIGHT_PIN;
  }
  setupChamberLightPWM(config.chamber_light_pin);
  Serial.println("PWM Light Pin OK.");
}

void reinitHardwareIfNeeded() {
  int newLightPin = atoi(custom_bbl_pin.getValue());
  if (newLightPin != config.chamber_light_pin && isValidGpioPin(newLightPin)) {
      Serial.println("Light Pin has changed after portal. Re-initializing PWM.");
      ledcDetach(config.chamber_light_pin);
      config.chamber_light_pin = newLightPin;
      setupChamberLightPWM(config.chamber_light_pin);
  }
}

void setChamberLightState(bool lightShouldBeOn) {
  int pwm_value = 0;
  if (lightShouldBeOn) {
    pwm_value = map(config.chamber_pwm_brightness, 0, 100, 0, 255);
  }
  int output_pwm = (config.invert_output) ? (255 - pwm_value) : pwm_value;
  output_pwm = constrain(output_pwm, 0, 255);
  ledcWrite(config.chamber_light_pin, output_pwm);
  external_light_is_on = lightShouldBeOn;
}

void setupChamberLightPWM(int pin) {
    ledcAttach(pin, PWM_FREQ, PWM_RESOLUTION);

    int off_value = config.invert_output ? 255 : 0;
    ledcWrite(pin, off_value);
    external_light_is_on = false;
    Serial.printf("PWM enabled on GPIO %d. OFF value: %d\n", pin, off_value);
}