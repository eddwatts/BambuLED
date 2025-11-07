#include "led_controller.h"
#include "config.h"
#include "light_controller.h" 
#include <math.h> // Include for sinf() and PI

// LED array definition
CRGB leds[MAX_LEDS];

void initLEDStrip() {
  // --- FIX ---
  // Removed the "isValidGpioPin(LED_DATA_PIN)" check.
  // This check was designed for the *external* light pin and was incorrectly
  // flagging the (valid) LED_DATA_PIN as a conflict, causing this function
  // to take the 'else' path and set config.num_leds to 0.
  if (config.num_leds > 0 && config.num_leds <= MAX_LEDS) {
  // --- END FIX ---
  
      Serial.print("Initializing LED strip on Pin ");
      Serial.print(LED_DATA_PIN);
      Serial.print(" with ");
      Serial.print(config.num_leds);
      Serial.println(" LEDs.");
      
      yield();

      Serial.print("Setting LED Color Order to: ");
      Serial.println(config.led_color_order);
      if (strcmp(config.led_color_order, "RGB") == 0) {
          FastLED.addLeds<WS2812B, LED_DATA_PIN, RGB>(leds, config.num_leds).setCorrection(TypicalLEDStrip);
      } else if (strcmp(config.led_color_order, "BRG") == 0) {
          FastLED.addLeds<WS2812B, LED_DATA_PIN, BRG>(leds, config.num_leds).setCorrection(TypicalLEDStrip);
      } else if (strcmp(config.led_color_order, "GBR") == 0) {
          FastLED.addLeds<WS2812B, LED_DATA_PIN, GBR>(leds, config.num_leds).setCorrection(TypicalLEDStrip);
      } else if (strcmp(config.led_color_order, "RBG") == 0) {
          FastLED.addLeds<WS2812B, LED_DATA_PIN, RBG>(leds, config.num_leds).setCorrection(TypicalLEDStrip);
      } else if (strcmp(config.led_color_order, "BGR") == 0) {
          FastLED.addLeds<WS2812B, LED_DATA_PIN, BGR>(leds, config.num_leds).setCorrection(TypicalLEDStrip);
      } else {
          if (strcmp(config.led_color_order, "GRB") != 0) {
              Serial.println("Unknown order, defaulting to GRB.");
          }
          FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(leds, config.num_leds).setCorrection(TypicalLEDStrip);
      }

      FastLED.clear();
      FastLED.show();
      Serial.println("FastLED OK.");
  } else {
      Serial.println("WARNING: LED setup skipped due to 0 or too many LEDs.");
      config.num_leds = 0; // This is now correct, it only runs if num_leds is 0 or > MAX_LEDS
  }
}

void updateLEDs() {
  if (config.num_leds <= 0 || config.num_leds > MAX_LEDS) {
     if(FastLED.getBrightness() != 0 || leds[0] != CRGB::Black) {
        FastLED.clear();
        FastLED.show();
     }
    return;
  }
  
  // Only update animations every 16ms (around 60fps)
  if (millis() - lastAnimationUpdate < 16) {
    return;
  }
  lastAnimationUpdate = millis();


  // --- Animation Logic from Suggestion 1 ---

  if (current_gcode_state == "PAUSED") {
    
    // Calculate brightness using a sine wave for a "breathing" effect
    // millis() / 2000.0f -> One full cycle every 2000ms (2 seconds)
    // sinf() -> returns a value between -1.0 and 1.0
    // (sinf(...) + 1.0) / 2.0 -> maps it to 0.0 - 1.0
    float breath = (sinf(millis() / 2000.0f * 2.0f * PI) + 1.0f) / 2.0f;
    
    // We want it to pulse from a minimum brightness (e.g., 20% of max) up to full
    // So we scale the 0.0-1.0 value to be 0.2-1.0
    float pulse_scale = 0.2f + (breath * 0.8f); 
    
    int new_brightness = (int)(config.led_bright_pause * pulse_scale);
    
    FastLED.setBrightness(new_brightness);
    fill_solid(leds, config.num_leds, CRGB(config.led_color_pause));
  }
  else if (current_error_state) {
    // --- Example: Blinking Error ---
    // % 1000 > 500 means "on for 500ms, off for 500ms"
    bool is_on = (millis() % 1000) > 500; 
    if (is_on) {
      FastLED.setBrightness(config.led_bright_error);
      fill_solid(leds, config.num_leds, CRGB(config.led_color_error));
    } else {
      FastLED.setBrightness(0);
      fill_solid(leds, config.num_leds, CRGB::Black);
    }
  }
  // --- End Animation Logic ---

  else if (current_gcode_state == "FINISH") {
    bool show_finish_light = false;
    if (config.led_finish_timeout) {
      if (finishTime > 0 && (millis() - finishTime < FINISH_LIGHT_TIMEOUT)) {
        show_finish_light = true;
      }
    } else {
      show_finish_light = true;
    }

    if (show_finish_light) {
      FastLED.setBrightness(config.led_bright_finish);
      fill_solid(leds, config.num_leds, CRGB(config.led_color_finish));
    } else {
      FastLED.setBrightness(config.led_bright_idle);
      fill_solid(leds, config.num_leds, CRGB(config.led_color_idle));
    }
  }
  else if (current_print_percentage > 0 && current_gcode_state != "IDLE") {
    FastLED.setBrightness(config.led_bright_print);
    int leds_to_light = map(current_print_percentage, 1, 100, 1, config.num_leds);
    leds_to_light = constrain(leds_to_light, 0, config.num_leds);

    CRGB targetColor = CRGB(config.led_color_print);
    // This check prevents re-writing the same data, which can be slow
    if (leds_to_light > 0 && leds[leds_to_light - 1] != targetColor) {
        fill_solid(leds, leds_to_light, targetColor);
        fill_solid(leds + leds_to_light, config.num_leds - leds_to_light, CRGB::Black);
    } else if (leds_to_light == 0 && leds[0] != CRGB::Black) {
        fill_solid(leds, config.num_leds, CRGB::Black);
    }
  }
  else {
    CRGB targetIdleColor = CRGB(config.led_color_idle);
    // This check prevents re-writing the same data, which can be slow
     if (FastLED.getBrightness() != config.led_bright_idle || leds[0] != targetIdleColor ) {
        FastLED.setBrightness(config.led_bright_idle);
        fill_solid(leds, config.num_leds, targetIdleColor);
     }
  }

  FastLED.show();
}

void handleFinishTimers() {
  if (config.chamber_light_finish_timeout && finishTime > 0 &&
      (millis() - finishTime > FINISH_LIGHT_TIMEOUT)) {
      
      if (external_light_is_on) {
          if (!manual_light_control) {
              Serial.println("External Light: Finish timeout reached. Turning OFF via loop timer.");
              setChamberLightState(false);
          }
          finishTime = 0; // Reset timer only after action
      } else {
          finishTime = 0; // Timer expired, just reset it
      }
  }

  if (config.led_finish_timeout && current_gcode_state == "FINISH" &&
      finishTime > 0 && (millis() - finishTime > FINISH_LIGHT_TIMEOUT)) {

      // Check if LEDs are *already* idle
      bool already_idle = (FastLED.getBrightness() == config.led_bright_idle &&
                           leds[0].r == (config.led_color_idle >> 16 & 0xFF) &&
                           leds[0].g == (config.led_color_idle >> 8 & 0xFF) &&
                           leds[0].b == (config.led_color_idle & 0xFF) );

      if (!already_idle) {
          // Force an update to set LEDs to idle state
          updateLEDs(); 
      }
  }
}