#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFiManager.h> 
#include "FS.h" // File System
#include "LittleFS.h" // For ESP32 file system support
#include <FastLED.h> 
#include <driver/ledc.h> // <-- ADDED THIS INCLUDE FOR PWM FUNCTIONS

// --- Configuration Pin Defaults ---
const int DEFAULT_CHAMBER_LIGHT_PIN = 27;
#define FORCE_RESET_PIN 16 // GPIO pin to ground for factory reset
// #define PWM_CHANNEL 0 // No longer needed for V3 API
#define PWM_FREQ 5000 // 5kHz frequency for PWM
#define PWM_RESOLUTION 8 // 8-bit resolution (0-255)

// --- FASTLED CONSTANT (MUST BE A MACRO OR CONST EXPRESSION) ---
// If you need to change the GPIO pin for the WS212B, you MUST change this value AND recompile.
#define LED_DATA_PIN 4
const int LED_PIN_CONST = LED_DATA_PIN; // Used for status display in non-template code
// -----------------------------------------------------------------

const int DEFAULT_NUM_LEDS = 10;
#define MAX_LEDS 60 // Maximum number of LEDs supported by the global array

// --- JSON Configuration Structure and Defaults ---
struct Config {
  char bbl_ip[40];
  char bbl_serial[40];
  char bbl_access_code[50]; 
  int chamber_light_pin = DEFAULT_CHAMBER_LIGHT_PIN; // Configurable Light GPIO Pin
  bool invert_output = false; // Light output polarity
  int num_leds = DEFAULT_NUM_LEDS;
  
  // NEW: Chamber Light Brightness
  int chamber_pwm_brightness = 100; // 0-100%

  // NEW: LED Status Colors (stored as 0xRRGGBB)
  uint32_t led_color_idle = 0x000000; // Black (off)
  uint32_t led_color_print = 0xFFFFFF; // White
  uint32_t led_color_pause = 0xFFA500; // Orange
  uint32_t led_color_error = 0xFF0000; // Red

  // NEW: LED Status Brightness (0-255)
  int led_bright_idle = 0;     // Off
  int led_bright_print = 100;  // Medium
  int led_bright_pause = 100;  // Medium
  int led_bright_error = 150;  // Bright
};
Config config;

// --- Custom Parameter Names (used in HTML and storage) ---
char custom_ip_param[40] = "";
char custom_serial_param[40] = "";
char custom_code_param[50] = "";
char custom_invert_param[2] = "0"; // "1" for true, "0" for false
char custom_pin_param[5] = "27"; 
char custom_num_leds_param[5] = "10"; 
// NEW Params
char custom_chamber_bright_param[5] = "100";
char custom_idle_color_param[7] = "000000";
char custom_print_color_param[7] = "FFFFFF";
char custom_pause_color_param[7] = "FFA500";
char custom_error_color_param[7] = "FF0000";
char custom_idle_bright_param[5] = "0";
char custom_print_bright_param[5] = "100";
char custom_pause_bright_param[5] = "100";
char custom_error_bright_param[5] = "150";


// --- MQTT & JSON Globals ---
WiFiClient espClient;
PubSubClient client(espClient);
const size_t JSON_DOC_SIZE = 4096; 
const int mqtt_port = 1883; 
const char* mqtt_user = "bblp"; 
const char* clientID = "ESP32_BambuLight"; 
String mqtt_topic_status;

// --- Status & Web Server Globals ---
WebServer server(80);
String current_light_mode = "UNKNOWN"; // Bambu's internal light status
// NEW LED Globals
CRGB leds[MAX_LEDS]; // Array to hold LED color values
int current_print_percentage = 0;
bool current_error_state = false;
String current_gcode_state = "IDLE";

// --- Non-blocking Reconnect Timer ---
unsigned long lastReconnectAttempt = 0;
const unsigned long RECONNECT_INTERVAL = 5000; // 5 seconds

// --- Function Prototypes ---
void setup_mqtt_params();
void callback(char* topic, byte* payload, unsigned int length);
bool reconnect_mqtt(); 
bool loadConfig(); 
bool saveConfig(); 
void saveConfigCallback();
bool isValidGpioPin(int pin);
void handleRoot();
void update_leds(); 
void setup_chamber_light_pwm(int pin);

// -----------------------------------------------------

void setup() {
  Serial.begin(115200);
  
  // --- Check for Factory Reset ---
  pinMode(FORCE_RESET_PIN, INPUT_PULLUP);
  Serial.println("Checking for factory reset...");
  Serial.println("Hold GPIO 16 to ground for 3 seconds for full factory reset...");
  delay(3000); // Wait 3 seconds to check the pin
  
  bool forceReset = (digitalRead(FORCE_RESET_PIN) == LOW);

  // 1. Initialize File System
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed. Running with defaults.");
  }

  // --- Handle Factory Reset Action ---
  if (forceReset) {
      Serial.println("Factory reset triggered!");
      Serial.println("Erasing /config.json...");
      LittleFS.remove("/config.json");
      // WiFi settings will be cleared later by the WiFiManager object
  }

  // 2. Load Configuration (if available)
  // This will fail and load defaults if we just reset
  if (!loadConfig()) {
    Serial.println("No saved configuration found. Using hardcoded defaults.");
    // Load hardcoded defaults for core values
    strcpy(config.bbl_ip, "192.168.1.100"); 
    strcpy(config.bbl_serial, "012345678900000"); 
    strcpy(config.bbl_access_code, "AABBCCDD");
    config.chamber_light_pin = DEFAULT_CHAMBER_LIGHT_PIN;
    // Other defaults are set in the struct definition
  }

  // 3. Initialize Light GPIO Pin (NOW AS PWM)
  Serial.print("Initializing Light Pin (PWM): ");
  Serial.println(config.chamber_light_pin);
  if (!isValidGpioPin(config.chamber_light_pin)) {
      Serial.println("ERROR: Configured light pin is invalid or unsafe! Using default (27).");
      config.chamber_light_pin = DEFAULT_CHAMBER_LIGHT_PIN;
  }
  setup_chamber_light_pwm(config.chamber_light_pin);


  // 4. Initialize LED Strip
  if (config.num_leds > 0 && config.num_leds <= MAX_LEDS && isValidGpioPin(LED_DATA_PIN)) {
      Serial.print("Initializing LED strip on Pin ");
      Serial.print(LED_DATA_PIN);
      Serial.print(" with ");
      Serial.print(config.num_leds);
      Serial.println(" LEDs.");
      
      FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(leds, config.num_leds).setCorrection(TypicalLEDStrip);
      // Brightness is now set dynamically in update_leds()
      FastLED.clear();
      FastLED.show();
  } else {
      Serial.println("WARNING: LED setup skipped due to invalid pin or 0/too many LEDs.");
      config.num_leds = 0; // Disable LED functionality if setup failed
  }
  
  // 5. Copy current config values to the custom parameter buffers for WiFiManager
  strcpy(custom_ip_param, config.bbl_ip);
  strcpy(custom_serial_param, config.bbl_serial);
  strcpy(custom_code_param, config.bbl_access_code);
  strcpy(custom_invert_param, config.invert_output ? "1" : "0");
  snprintf(custom_pin_param, 5, "%d", config.chamber_light_pin); 
  snprintf(custom_num_leds_param, 5, "%d", config.num_leds);
  // NEW
  snprintf(custom_chamber_bright_param, 5, "%d", config.chamber_pwm_brightness);
  snprintf(custom_idle_color_param, 7, "%06X", config.led_color_idle);
  snprintf(custom_print_color_param, 7, "%06X", config.led_color_print);
  snprintf(custom_pause_color_param, 7, "%06X", config.led_color_pause);
  snprintf(custom_error_color_param, 7, "%06X", config.led_color_error);
  snprintf(custom_idle_bright_param, 5, "%d", config.led_bright_idle);
  snprintf(custom_print_bright_param, 5, "%d", config.led_bright_print);
  snprintf(custom_pause_bright_param, 5, "%d", config.led_bright_pause);
  snprintf(custom_error_bright_param, 5, "%d", config.led_bright_error);


  // 6. Setup WiFiManager
  WiFiManager wm;

  // --- Handle Factory Reset Action (Part 2) ---
  if (forceReset) {
      Serial.println("Clearing saved Wi-Fi settings...");
      wm.resetSettings();
  }

  wm.setSaveConfigCallback(saveConfigCallback);

  // --- Add Custom Parameters for Config ---
  WiFiManagerParameter custom_bbl_ip("ip", "Bambu Printer IP", custom_ip_param, 40);
  WiFiManagerParameter custom_bbl_serial("serial", "Printer Serial", custom_serial_param, 40);
  WiFiManagerParameter custom_bbl_access_code("code", "Access Code (MQTT Pass)", custom_code_param, 50);
  
  // Chamber Light Params
  WiFiManagerParameter p_light_heading("<h2>External Light Settings</h2>");
  WiFiManagerParameter custom_bbl_pin("lightpin", "External Light GPIO Pin", custom_pin_param, 5, "type='number' min='0' max='39'");
  WiFiManagerParameter custom_bbl_invert("invert", "Invert Light Logic (1=Active Low)", custom_invert_param, 2, "type='checkbox' value='1'");
  WiFiManagerParameter custom_chamber_bright("chamber_bright", "External Light Brightness (0-100%)", custom_chamber_bright_param, 5, "type='number' min='0' max='100'");

  // LED Strip Params
  WiFiManagerParameter p_led_heading("<h2>LED Status Bar Settings</h2>");
  WiFiManagerParameter custom_num_leds("numleds", "Number of WS2812B LEDs (Max 60)", custom_num_leds_param, 5, "type='number' min='0' max='60'");
  WiFiManagerParameter p_led_info("<small><i>LED Data Pin is hardcoded to GPIO 4 for FastLED.</i></small>");
  
  WiFiManagerParameter p_led_idle_heading("<h3>Idle Status</h3>");
  WiFiManagerParameter custom_idle_color("idle_color", "Idle Color (RRGGBB)", custom_idle_color_param, 7, "placeholder='000000'");
  WiFiManagerParameter custom_idle_bright("idle_bright", "Idle Brightness (0-255)", custom_idle_bright_param, 5, "type='number' min='0' max='255'");
  
  WiFiManagerParameter p_led_print_heading("<h3>Printing Status</h3>");
  WiFiManagerParameter custom_print_color("print_color", "Print Color (RRGGBB)", custom_print_color_param, 7, "placeholder='FFFFFF'");
  WiFiManagerParameter custom_print_bright("print_bright", "Print Brightness (0-255)", custom_print_bright_param, 5, "type='number' min='0' max='255'");
  
  WiFiManagerParameter p_led_pause_heading("<h3>Paused Status</h3>");
  WiFiManagerParameter custom_pause_color("pause_color", "Pause Color (RRGGBB)", custom_pause_color_param, 7, "placeholder='FFA500'");
  WiFiManagerParameter custom_pause_bright("pause_bright", "Pause Brightness (0-255)", custom_pause_bright_param, 5, "type='number' min='0' max='255'");

  WiFiManagerParameter p_led_error_heading("<h3>Error Status</h3>");
  WiFiManagerParameter custom_error_color("error_color", "Error Color (RRGGBB)", custom_error_color_param, 7, "placeholder='FF0000'");
  WiFiManagerParameter custom_error_bright("error_bright", "Error Brightness (0-255)", custom_error_bright_param, 5, "type='number' min='0' max='255'");


  wm.addParameter(&custom_bbl_ip);
  wm.addParameter(&custom_bbl_serial);
  wm.addParameter(&custom_bbl_access_code);
  // Light
  wm.addParameter(&p_light_heading);
  wm.addParameter(&custom_bbl_pin); 
  wm.addParameter(&custom_bbl_invert);
  wm.addParameter(&custom_chamber_bright);
  // LEDs
  wm.addParameter(&p_led_heading);
  wm.addParameter(&custom_num_leds); 
  wm.addParameter(&p_led_info);
  wm.addParameter(&p_led_idle_heading);
  wm.addParameter(&custom_idle_color);
  wm.addParameter(&custom_idle_bright);
  wm.addParameter(&p_led_print_heading);
  wm.addParameter(&custom_print_color);
  wm.addParameter(&custom_print_bright);
  wm.addParameter(&p_led_pause_heading);
  wm.addParameter(&custom_pause_color);
  wm.addParameter(&custom_pause_bright);
  wm.addParameter(&p_led_error_heading);
  wm.addParameter(&custom_error_color);
  wm.addParameter(&custom_error_bright);

  
  Serial.println("Starting WiFiManager...");
  
  // 7. Connect to WiFi
  // If we triggered a reset, this will fail and launch the portal
  if (!wm.autoConnect("BambuLEDSetup", "password")) {
    Serial.println("Failed to connect and timed out. Restarting...");
    delay(3000);
    ESP.restart();
    delay(5000);
  }

  Serial.println("Connected to WiFi!");

  // 8. Update config struct from the custom parameters (in case they were changed in the portal)
  // This is now done in saveConfigCallback()
  
  // 9. Re-initialize pins in case they were changed
  int newLightPin = atoi(custom_pin_param);
  if (newLightPin != config.chamber_light_pin && isValidGpioPin(newLightPin)) {
      Serial.println("Light Pin has changed. Re-initializing PWM.");
      ledcDetach(config.chamber_light_pin); // V3 API CHANGE: ledcDetachPin -> ledcDetach
      config.chamber_light_pin = newLightPin;
      setup_chamber_light_pwm(config.chamber_light_pin); // Attach new pin
  }
  
  // 10. Setup MQTT and Callback
  setup_mqtt_params();
  client.setCallback(callback);
  
  // 11. Setup Web Server
  server.on("/", handleRoot);
  server.begin();
  Serial.print("Status page available at http://");
  Serial.println(WiFi.localIP());
}

void loop() {
  // Handle web client requests
  server.handleClient();
  
  if (!client.connected()) {
    // Non-blocking reconnect attempt
    if (millis() - lastReconnectAttempt > RECONNECT_INTERVAL) {
      lastReconnectAttempt = millis();
      if (reconnect_mqtt()) {
        lastReconnectAttempt = 0; // Successful connection
      }
    }
  } else {
    client.loop();
  }
}

// -----------------------------------------------------
// --- Validation & Web Status Functions ---

/**
 * @brief Checks if a GPIO pin is generally safe for digital output on an ESP32.
 */
bool isValidGpioPin(int pin) {
    if (pin < 0 || pin > 39) return false;
    // Reserved/Flash pins (6-11)
    if (pin >= 6 && pin <= 11) return false;
    // Input-only pins (34, 35, 36, 39)
    if (pin >= 34) return false;
    return true; 
}


/**
 * @brief Serves a simple status page on the device's IP.
 */
void handleRoot() {
  // Read the current PWM duty cycle from the channel
  int pwm_duty = ledcRead(config.chamber_light_pin); // V3 API CHANGE: Read from pin, not channel
  bool is_on = (config.invert_output) ? (pwm_duty < 255) : (pwm_duty > 0);
  
  String led_status_str;
  if (config.num_leds == 0) {
      led_status_str = "Disabled";
  } else if (current_error_state) {
      led_status_str = "Error (Red)";
  } else if (current_gcode_state == "PAUSED") {
      led_status_str = "Paused (Orange)";
  } else if (current_print_percentage > 0) {
      led_status_str = "Printing Progress (" + String(current_print_percentage) + "%)";
  } else {
      led_status_str = "Idle/Off (No Light)";
  }

  // Pre-calculate the LED status class
  String led_status_class;
  if (current_error_state) {
      led_status_class = "error";
  } else if (current_gcode_state == "PAUSED") {
      led_status_class = "warning"; // Use warning color for Orange
  } else if (current_print_percentage > 0) {
      led_status_class = "warning";
  } else {
      led_status_class = "light-on"; // Use light-on for idle (off)
  }

  // ---
  // Build the HTML string by robustly appending (+=) each part.
  // ---
  String html = String("<!DOCTYPE html><html><head>");
  html += String("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
  html += String("<title>Bambu Light Status</title><style>");
  html += String("body { font-family: Arial, sans-serif; margin: 20px; }");
  html += String(".status { padding: 10px; margin-bottom: 10px; border-radius: 5px; }");
  html += String(".connected { background-color: #d4edda; color: #155724; border: 1px solid #c3e6cb; }");
  html += String(".disconnected { background-color: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }");
  html += String(".warning { background-color: #fff3cd; color: #856404; border: 1px solid #ffeeba; }");
  html += String(".light-on { background-color: #cce5ff; color: #004085; border: 1px solid #b8daff; }");
  html += String(".error { background-color: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; font-weight: bold; }");
  html += String("button { background-color: #007bff; color: white; padding: 10px 15px; border: none; border-radius: 5px; cursor: pointer; }");
  html += String("</style></head><body><h1>Bambu Chamber Light Controller</h1>");

  // --- WiFi Status Block ---
  html += String("<div class=\"status ");
  html += (WiFi.status() == WL_CONNECTED ? "connected" : "disconnected");
  html += String("\"><strong>WiFi Status:</strong> ");
  String wifi_status = (WiFi.status() == WL_CONNECTED ? "CONNECTED (" + WiFi.localIP().toString() + ")" : "DISCONNECTED");
  html += wifi_status;
  html += String("</div>");
  
  // --- MQTT Status Block ---
  html += String("<div class=\"status ");
  html += (client.connected() ? "connected" : "disconnected");
  html += String("\"><strong>MQTT Status:</strong> ");
  html += (client.connected() ? "CONNECTED" : "DISCONNECTED");
  html += String("</div>");

  // --- Printer Status Block ---
  html += String("<h2>Printer Status</h2>");
  html += String("<p><strong>GCODE State:</strong> ") + current_gcode_state + String("</p>");
  html += String("<p><strong>Print Percentage:</strong> ") + String(current_print_percentage) + String(" %</p>");
  
  // --- External Outputs Block ---
  html += String("<h2>External Outputs</h2>");
  
  // Chamber Light Block
  html += String("<div class=\"status ");
  html += (is_on ? "light-on" : "disconnected");
  html += String("\"><strong>External Light (Pin ") + String(config.chamber_light_pin) + String("):</strong> ");
  html += (is_on ? "ON" : "OFF");
  html += String(" (") + String(config.chamber_pwm_brightness) + "%)";
  html += String("<small>(Logic: ") + (config.invert_output ? "Active LOW" : "Active HIGH");
  html += String(" | Bambu Light Mode: ") + current_light_mode + String("</small></div>");

  // LED Status Bar Block
  html += String("<div class=\"status ");
  html += led_status_class;
  html += String("\"><strong>LED Status Bar (Pin ") + String(LED_PIN_CONST) + String(" / ") + String(config.num_leds) + String(" LEDs):</strong> ");
  html += led_status_str;
  html += String("<small>Data Pin is hardcoded to GPIO ") + String(LED_PIN_CONST) + String(" for FastLED compatibility.</small></div>");
  
  // --- Footer ---
  html += String("<p><a href=\"/config\"><button>Change Wi-Fi / Printer Configuration</button></a></p>");
  html += String("</body></html>");
  
  server.send(200, "text/html", html);
}

// -----------------------------------------------------
// --- LED Control Function (REWRITTEN) ---

/**
 * @brief Updates the WS2812B strip based on configured colors and brightness.
 */
void update_leds() {
  // If LEDs are configured to be disabled (num_leds == 0), exit immediately
  if (config.num_leds == 0) {
    FastLED.clear();
    FastLED.show(); // Ensure they are off if just disabled
    return;
  }

  if (current_error_state) {
    // Red for Error/Stop/Failed
    FastLED.setBrightness(config.led_bright_error);
    fill_solid(leds, config.num_leds, CRGB(config.led_color_error));
  }
  else if (current_gcode_state == "PAUSED") {
    // Orange for Paused
    FastLED.setBrightness(config.led_bright_pause);
    fill_solid(leds, config.num_leds, CRGB(config.led_color_pause));
  }
  else if (current_print_percentage > 0 && current_gcode_state != "IDLE" && current_gcode_state != "FINISH") {
    // Printing (Progress Bar)
    FastLED.setBrightness(config.led_bright_print);
    
    // Calculate how many LEDs should be lit
    int leds_to_light = map(current_print_percentage, 1, 100, 1, config.num_leds);
    if (leds_to_light > config.num_leds) leds_to_light = config.num_leds;

    // Fill the "on" LEDs
    fill_solid(leds, leds_to_light, CRGB(config.led_color_print));
    // Fill the "off" LEDs
    fill_solid(leds + leds_to_light, config.num_leds - leds_to_light, CRGB::Black);
  } 
  else {
    // Idle, finished, or off state
    FastLED.setBrightness(config.led_bright_idle);
    fill_solid(leds, config.num_leds, CRGB(config.led_color_idle));
  }
  
  FastLED.show();
}

// -----------------------------------------------------
// --- Configuration Functions ---

/**
 * @brief Sets up the chamber light pin as a PWM output.
 */
void setup_chamber_light_pwm(int pin) {
    // V3 API CHANGE: ledcSetup and ledcAttachPin are combined.
    // This now configures and attaches the pin automatically.
    ledcAttach(pin, PWM_FREQ, PWM_RESOLUTION);
    
    // Set initial state (OFF)
    int off_value = config.invert_output ? 255 : 0;
    ledcWrite(pin, off_value); // V3 API CHANGE: Write to pin, not channel
    Serial.printf("PWM enabled on GPIO %d. OFF value: %d\n", pin, off_value);
}


/**
 * @brief Saves the current configuration struct to a JSON file in LittleFS.
 */
bool saveConfig() {
  Serial.println("Saving configuration to LittleFS...");
  DynamicJsonDocument doc(2048); // Increased size for new params
  doc["bbl_ip"] = config.bbl_ip;
  doc["bbl_serial"] = config.bbl_serial;
  doc["bbl_access_code"] = config.bbl_access_code;
  doc["invert_output"] = config.invert_output;
  doc["chamber_light_pin"] = config.chamber_light_pin; 
  doc["num_leds"] = config.num_leds;
  
  // NEW
  doc["chamber_pwm_brightness"] = config.chamber_pwm_brightness;
  doc["led_color_idle"] = config.led_color_idle;
  doc["led_color_print"] = config.led_color_print;
  doc["led_color_pause"] = config.led_color_pause;
  doc["led_color_error"] = config.led_color_error;
  doc["led_bright_idle"] = config.led_bright_idle;
  doc["led_bright_print"] = config.led_bright_print;
  doc["led_bright_pause"] = config.led_bright_pause;
  doc["led_bright_error"] = config.led_bright_error;

  File configFile = LittleFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return false;
  }
  
  if (serializeJson(doc, configFile) == 0) {
      Serial.println("Failed to write to file");
      configFile.close();
      return false;
  }
  configFile.close();
  Serial.println("Configuration saved successfully.");
  return true;
}

/**
 * @brief Loads the configuration from a JSON file in LittleFS.
 */
bool loadConfig() {
  File configFile = LittleFS.open("/config.json", "r");
  if (!configFile) {
    return false;
  }

  size_t size = configFile.size();
  if (size > 2048) { // Increased size
    Serial.println("Config file size is too large");
    configFile.close();
    return false;
  }

  DynamicJsonDocument doc(2048); // Increased size
  DeserializationError error = deserializeJson(doc, configFile);
  configFile.close();

  if (error) {
    Serial.print("Failed to parse config file: ");
    Serial.println(error.c_str());
    return false;
  }
  
  // Copy loaded values into the config struct
  strlcpy(config.bbl_ip, doc["bbl_ip"] | "192.168.1.100", sizeof(config.bbl_ip));
  strlcpy(config.bbl_serial, doc["bbl_serial"] | "012345678900000", sizeof(config.bbl_serial));
  strlcpy(config.bbl_access_code, doc["bbl_access_code"] | "AABBCCDD", sizeof(config.bbl_access_code));
  config.invert_output = doc["invert_output"] | false;
  config.chamber_light_pin = doc["chamber_light_pin"] | DEFAULT_CHAMBER_LIGHT_PIN; 
  config.num_leds = doc["num_leds"] | DEFAULT_NUM_LEDS;
  
  // NEW
  config.chamber_pwm_brightness = doc["chamber_pwm_brightness"] | 100;
  config.led_color_idle = doc["led_color_idle"] | 0x000000;
  config.led_color_print = doc["led_color_print"] | 0xFFFFFF;
  config.led_color_pause = doc["led_color_pause"] | 0xFFA500;
  config.led_color_error = doc["led_color_error"] | 0xFF0000;
  config.led_bright_idle = doc["led_bright_idle"] | 0;
  config.led_bright_print = doc["led_bright_print"] | 100;
  config.led_bright_pause = doc["led_bright_pause"] | 100;
  config.led_bright_error = doc["led_bright_error"] | 150;

  Serial.println("Configuration loaded successfully.");
  return true;
}

/**
 * @brief Callback fired when parameters are successfully saved via the portal.
 */
void saveConfigCallback() {
  Serial.println("WiFiManager signaled configuration save.");
  
  // 1. Copy new values from custom parameter buffers 
  strcpy(config.bbl_ip, custom_ip_param);
  strcpy(config.bbl_serial, custom_serial_param);
  strcpy(config.bbl_access_code, custom_code_param);
  config.invert_output = (strcmp(custom_invert_param, "1") == 0);
  
  int tempLightPin = atoi(custom_pin_param);
  if (isValidGpioPin(tempLightPin)) {
      config.chamber_light_pin = tempLightPin; 
  } else {
      Serial.print("ERROR: Light Pin ");
      Serial.print(tempLightPin);
      Serial.println(" is invalid or unsafe. Retaining previous pin.");
  }
  
  int tempNumLeds = atoi(custom_num_leds_param);
  if (tempNumLeds <= MAX_LEDS) {
      config.num_leds = tempNumLeds;
  } else {
      Serial.println("WARNING: Invalid LED count entered. Disabling LEDs.");
      config.num_leds = 0; 
  }

  // NEW
  config.chamber_pwm_brightness = atoi(custom_chamber_bright_param);
  // Convert hex strings (RRGGBB) to uint32_t (0xRRGGBB)
  config.led_color_idle = strtoul(custom_idle_color_param, NULL, 16);
  config.led_color_print = strtoul(custom_print_color_param, NULL, 16);
  config.led_color_pause = strtoul(custom_pause_color_param, NULL, 16);
  config.led_color_error = strtoul(custom_error_color_param, NULL, 16);
  
  config.led_bright_idle = atoi(custom_idle_bright_param);
  config.led_bright_print = atoi(custom_print_bright_param);
  config.led_bright_pause = atoi(custom_pause_bright_param);
  config.led_bright_error = atoi(custom_error_bright_param);

  // 2. Save the config
  saveConfig();
}

// -----------------------------------------------------
// --- MQTT Functions ---

/**
 * @brief Sets up MQTT parameters using the configured values.
 */
void setup_mqtt_params() {
    client.setServer(config.bbl_ip, mqtt_port);
    // Construct the subscription topic using the configured serial number
    mqtt_topic_status = "device/" + String(config.bbl_serial) + "/report";
    Serial.print("MQTT Server: "); Serial.println(config.bbl_ip);
    Serial.print("Subscription Topic: "); Serial.println(mqtt_topic_status);
}

/**
 * @brief Handles the MQTT reconnection logic and subscription (non-blocking).
 * @return true if connected, false otherwise.
 */
bool reconnect_mqtt() {
  Serial.print("Attempting MQTT connection...");
  
  if (client.connect(clientID, mqtt_user, config.bbl_access_code)) {
    Serial.println("connected");
    client.subscribe(mqtt_topic_status.c_str());
    Serial.print("Subscribed to: ");
    Serial.println(mqtt_topic_status);
    return true;
  } else {
    Serial.print("failed, rc=");
    Serial.print(client.state());
    Serial.println("");
    return false;
  }
}

/**
 * @brief The function called when an MQTT message is received.
 * It parses the JSON payload and controls the external outputs.
 */
void callback(char* topic, byte* payload, unsigned int length) {
  
  char messageBuffer[length + 1];
  memcpy(messageBuffer, payload, length);
  messageBuffer[length] = '\0'; 

  DynamicJsonDocument doc(JSON_DOC_SIZE); 
  DeserializationError error = deserializeJson(doc, messageBuffer);

  if (error) {
    return;
  }

  // --- 1. Chamber Light Control (NOW WITH PWM) ---
  const char* chamberLightMode = doc["report"]["system"]["chamber_light"]["led_mode"] | "off";
  current_light_mode = chamberLightMode; // Update global status

  bool lightShouldBeOn = (strcmp(chamberLightMode, "on") == 0 || strcmp(chamberLightMode, "flashing") == 0);
  
  int pwm_value = 0; // Default to OFF
  if (lightShouldBeOn) {
    // Map 0-100% brightness config to 0-255 PWM value
    pwm_value = map(config.chamber_pwm_brightness, 0, 100, 0, 255);
  }

  // Apply inversion logic
  int output_pwm = (config.invert_output) ? (255 - pwm_value) : pwm_value;
  
  // Write the final value (0-255) to the PWM channel
  ledcWrite(config.chamber_light_pin, output_pwm); // V3 API CHANGE: Write to pin, not channel


  // --- 2. LED Status Bar Control ---
  current_print_percentage = doc["report"]["print"]["print_percentage"] | 0;
  const char* gcodeState = doc["report"]["print"]["gcode_state"] | "IDLE";
  current_gcode_state = gcodeState;

  // Error condition check
  // "PAUSED" is now handled separately by update_leds()
  current_error_state = (strcmp(gcodeState, "FAILED") == 0 || 
                         strcmp(gcodeState, "STOP") == 0); 
  
  // Update the LED status bar
  update_leds();
}

