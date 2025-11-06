#ifndef WEB_HANDLERS_H
#define WEB_HANDLERS_H

#include <WebServer.h>
#include <WiFiManager.h>
#include <FS.h>
#include <LittleFS.h>
#include <deque>  // Add this include
#include <PubSubClient.h>  // Add this include
#include <FastLED.h> // <-- FIX 1: Added this to define CRGB
#include "config.h"  // Add this include

// External declarations from main file
extern WebServer server;
extern WiFiManager wm;
//extern Config config;
extern PubSubClient client;
extern bool manual_light_control;
extern bool external_light_is_on;
extern String current_light_mode;
extern String current_gcode_state;
extern int current_print_percentage;
extern int current_time_remaining;
extern int current_layer;
extern int current_stage;
extern float current_nozzle_temp;
extern float current_nozzle_target_temp;
extern float current_bed_temp;
extern float current_bed_target_temp;
extern String current_wifi_signal;
extern unsigned long finishTime;
extern const unsigned long FINISH_LIGHT_TIMEOUT;
extern File restoreFile;
extern bool restoreSuccess;

// External declarations from other modules
extern std::deque<String> mqtt_history;
// extern const int MAX_HISTORY_SIZE;
extern CRGB leds[];
// <-- FIX 2: Removed "extern const int MAX_LEDS;"
// This line was conflicting with the #define in config.h

// WiFiManager parameter declarations (from config.h)
extern WiFiManagerParameter custom_bbl_ip;
extern WiFiManagerParameter custom_bbl_serial;
extern WiFiManagerParameter custom_bbl_access_code;
extern WiFiManagerParameter custom_bbl_pin;
extern WiFiManagerParameter custom_bbl_invert;
extern WiFiManagerParameter custom_chamber_bright;
extern WiFiManagerParameter custom_chamber_finish_timeout;
extern WiFiManagerParameter custom_num_leds;
extern WiFiManagerParameter custom_led_order;
extern WiFiManagerParameter custom_idle_color;
extern WiFiManagerParameter custom_idle_bright;
extern WiFiManagerParameter custom_print_color;
extern WiFiManagerParameter custom_print_bright;
extern WiFiManagerParameter custom_pause_color;
extern WiFiManagerParameter custom_pause_bright;
extern WiFiManagerParameter custom_error_color;
extern WiFiManagerParameter custom_error_bright;
extern WiFiManagerParameter custom_finish_color;
extern WiFiManagerParameter custom_finish_bright;
extern WiFiManagerParameter custom_led_finish_timeout;
extern WiFiManagerParameter custom_ntp_server;
extern WiFiManagerParameter custom_timezone;

// Function declarations
void setupWebServer();
bool connectWiFi(bool forceReset);
void handleRoot();
void handleStatusJson();
void handleMqttJson();
void handleLightOn();
void handleLightOff();
void handleLightAuto();
void handleConfig();
void handleBackup();
void handleRestorePage();
void handleRestoreUpload();
void handleRestoreReboot();

#endif