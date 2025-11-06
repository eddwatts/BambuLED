#include "ota_handler.h"
#include <ArduinoOTA.h>
#include "config.h"
#include "led_controller.h"

void setupOTA() {
  ArduinoOTA.setHostname("bambu-light-controller");

  ArduinoOTA
    .onStart([]() {
      Serial.println("OTA Start");
      if(config.num_leds > 0) {
        FastLED.setBrightness(config.led_bright_error);
        fill_solid(leds, config.num_leds, CRGB::Blue);
        FastLED.show();
      }
    })
    .onEnd([]() {
      Serial.println("\nOTA End");
       if(config.num_leds > 0) {
        fill_solid(leds, config.num_leds, CRGB::Green);
        FastLED.show();
        delay(1000);
      }
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
      if(config.num_leds > 0) {
        int leds_to_light = map(progress, 0, total, 0, config.num_leds);
        fill_solid(leds, leds_to_light, CRGB::Blue);
        fill_solid(leds + leds_to_light, config.num_leds - leds_to_light, CRGB::Black);
        FastLED.show();
      }
    })
    .onError([](ota_error_t error) {
      Serial.printf("OTA Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");

      if(config.num_leds > 0) {
        fill_solid(leds, config.num_leds, CRGB::Red);
        FastLED.show();
        delay(2000);
      }
    });

  ArduinoOTA.begin();
}