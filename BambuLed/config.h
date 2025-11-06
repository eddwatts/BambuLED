#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>

// Configuration constants
const int DEFAULT_CHAMBER_LIGHT_PIN = 14;
#define FORCE_RESET_PIN 16
#define PWM_FREQ 5000
#define PWM_RESOLUTION 8
#define LED_DATA_PIN 4
#define MAX_LEDS 60  // Changed from const int to #define

const int DEFAULT_NUM_LEDS = 10;

// Configuration structure
struct Config {
  char bbl_ip[40];
  char bbl_serial[40];
  char bbl_access_code[50];
  int chamber_light_pin = DEFAULT_CHAMBER_LIGHT_PIN;
  bool invert_output = false;
  int num_leds = DEFAULT_NUM_LEDS;
  int chamber_pwm_brightness = 100;
  bool chamber_light_finish_timeout = true;
  
  uint32_t led_color_idle = 0x000000;
  uint32_t led_color_print = 0xFFFFFF;
  uint32_t led_color_pause = 0xFFA500;
  uint32_t led_color_error = 0xFF0000;
  uint32_t led_color_finish = 0x00FF00;
  
  int led_bright_idle = 0;
  int led_bright_print = 100;
  int led_bright_pause = 100;
  int led_bright_error = 150;
  int led_bright_finish = 100;
  
  bool led_finish_timeout = true;
  char ntp_server[60];
  char timezone[50];
  char led_color_order[4];
};

extern Config config;

// Add JSON document size constant
const size_t JSON_DOC_SIZE = 4096;

// WiFiManager parameter declarations (needed across files)
extern WiFiManagerParameter custom_bbl_ip;
// ... (keep all the WiFiManager parameter declarations the same)
// Function declarations
bool checkFactoryReset();
bool initFileSystem();
void performFactoryReset();
bool loadConfig();
bool saveConfig();
void setupDefaultConfig();
void applyConfigFixes();
void printConfig();
void setupWiFiManagerParams();
void saveConfigCallback();
bool isValidGpioPin(int pin);
void configureTime();
String getTimestamp();
String getTimezoneDropdown(String selectedTz);
String getLedOrderDropdown(String selectedOrder);

#endif