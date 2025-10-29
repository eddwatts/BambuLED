#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFiManager.h> 
#include "FS.h" // File System
#include "LittleFS.h" // For ESP32 file system support
#include <FastLED.h> 
#include <driver/ledc.h> // For PWM functions
#include <ESPmDNS.h> // For OTA
#include <ArduinoOTA.h> // For OTA updates

// --- Configuration Pin Defaults ---
const int DEFAULT_CHAMBER_LIGHT_PIN = 27;
#define FORCE_RESET_PIN 16 // GPIO pin to ground for factory reset
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
  
  // Chamber Light Brightness
  int chamber_pwm_brightness = 100; // 0-100%

  // LED Status Colors (stored as 0xRRGGBB)
  uint32_t led_color_idle = 0x000000; // Black (off)
  uint32_t led_color_print = 0xFFFFFF; // White
  uint32_t led_color_pause = 0xFFA500; // Orange
  uint32_t led_color_error = 0xFF0000; // Red

  // LED Status Brightness (0-255)
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
// LED Globals
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
void setup_ota(); // New function for OTA

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
  }

  // 2. Load Configuration
  if (!loadConfig()) {
    Serial.println("No saved configuration found. Using hardcoded defaults.");
    // Load hardcoded defaults for core values
    strcpy(config.bbl_ip, "192.168.1.100"); 
    strcpy(config.bbl_serial, "012345678900000"); 
    strcpy(config.bbl_access_code, "AABBCCDD");
    config.chamber_light_pin = DEFAULT_CHAMBER_LIGHT_PIN;
  }

  // 3. Initialize Light GPIO Pin (PWM)
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
      FastLED.clear();
      FastLED.show();
  } else {
      Serial.println("WARNING: LED setup skipped due to invalid pin or 0/too many LEDs.");
      config.num_leds = 0; // Disable LED functionality
  }
  
  // 5. Copy current config values to the custom parameter buffers for WiFiManager
  strcpy(custom_ip_param, config.bbl_ip);
  strcpy(custom_serial_param, config.bbl_serial);
  strcpy(custom_code_param, config.bbl_access_code);
  strcpy(custom_invert_param, config.invert_output ? "1" : "0");
  snprintf(custom_pin_param, 5, "%d", config.chamber_light_pin); 
  snprintf(custom_num_leds_param, 5, "%d", config.num_leds);
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

  if (forceReset) {
      Serial.println("Clearing saved Wi-Fi settings...");
      wm.resetSettings();
  }

  wm.setSaveConfigCallback(saveConfigCallback);

  // --- Add Custom Parameters for Config ---
  WiFiManagerParameter custom_bbl_ip("ip", "Bambu Printer IP", custom_ip_param, 40);
  WiFiManagerParameter custom_bbl_serial("serial", "Printer Serial", custom_serial_param, 40);
  WiFiManagerParameter custom_bbl_access_code("code", "Access Code (MQTT Pass)", custom_code_param, 50);
  
  WiFiManagerParameter p_light_heading("<h2>External Light Settings</h2>");
  WiFiManagerParameter custom_bbl_pin("lightpin", "External Light GPIO Pin", custom_pin_param, 5, "type='number' min='0' max='39'");
  WiFiManagerParameter custom_bbl_invert("invert", "Invert Light Logic (1=Active Low)", custom_invert_param, 2, "type='checkbox' value='1'");
  WiFiManagerParameter custom_chamber_bright("chamber_bright", "External Light Brightness (0-100%)", custom_chamber_bright_param, 5, "type='number' min='0' max='100'");

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
  wm.addParameter(&p_light_heading);
  wm.addParameter(&custom_bbl_pin); 
  wm.addParameter(&custom_bbl_invert);
  wm.addParameter(&custom_chamber_bright);
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
  if (!wm.autoConnect("BambuLightSetup", "password")) {
    Serial.println("Failed to connect and timed out. Restarting...");
    delay(3000);
    ESP.restart();
    delay(5000);
  }

  Serial.println("Connected to WiFi!");

  // 8. Re-initialize pins in case they were changed in the portal
  int newLightPin = atoi(custom_pin_param);
  if (newLightPin != config.chamber_light_pin && isValidGpioPin(newLightPin)) {
      Serial.println("Light Pin has changed. Re-initializing PWM.");
      ledcDetach(config.chamber_light_pin); 
      config.chamber_light_pin = newLightPin;
      setup_chamber_light_pwm(config.chamber_light_pin);
  }
  
  // 9. Setup OTA (Over-the-Air) Updates
  setup_ota();

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
  // OTA must be handled in the main loop
  ArduinoOTA.handle(); 
  
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
// --- OTA Setup Function ---

/**
 * @brief Initializes the ArduinoOTA service with LED feedback.
 */
void setup_ota() {
  // Set a hostname for the device
  ArduinoOTA.setHostname("bambu-light-controller");
  
  // (Optional) You can set a password for updates
  // ArduinoOTA.setPassword("your_ota_password");

  ArduinoOTA
    .onStart([]() {
      Serial.println("OTA Start");
      // Use the "Error" brightness and color for visibility
      FastLED.setBrightness(config.led_bright_error); 
      fill_solid(leds, config.num_leds, CRGB::Blue);
      FastLED.show();
    })
    .onEnd([]() {
      Serial.println("\nOTA End");
      fill_solid(leds, config.num_leds, CRGB::Green);
      FastLED.show();
      delay(1000); // Show success
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
      
      // Update LED progress bar
      int leds_to_light = map(progress, 0, total, 0, config.num_leds);
      fill_solid(leds, leds_to_light, CRGB::Blue);
      fill_solid(leds + leds_to_light, config.num_leds - leds_to_light, CRGB::Black);
      FastLED.show();
    })
    .onError([](ota_error_t error) {
      Serial.printf("OTA Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
      
      fill_solid(leds, config.num_leds, CRGB::Red); // Show Red on error
      FastLED.show();
      delay(2000); // Show error
    });

  ArduinoOTA.begin();
  Serial.println("OTA Ready. Hostname: bambu-light-controller");
}

// -----------------------------------------------------
// --- Validation & Web Status Functions ---

bool isValidGpioPin(int pin) {
    if (pin < 0 || pin > 39) return false;
    if (pin >= 6 && pin <= 11) return false; // Reserved/Flash
    if (pin >= 34) return false; // Input-only
    return true; 
}


void handleRoot() {
  int pwm_duty = ledcRead(config.chamber_light_pin);
  bool is_on = (config.invert_output) ? (pwm_duty < 255) : (pwm_duty > 0);
  
  String led_status_str;
  if (config.num_leds == 0) led_status_str = "Disabled";
  else if (current_error_state) led_status_str = "Error (Red)";
  else if (current_gcode_state == "PAUSED") led_status_str = "Paused (Orange)";
  else if (current_print_percentage > 0) led_status_str = "Printing Progress (" + String(current_print_percentage) + "%)";
  else led_status_str = "Idle/Off (No Light)";

  String led_status_class;
  if (current_error_state) led_status_class = "error";
  else if (current_gcode_state == "PAUSED") led_status_class = "warning";
  else if (current_print_percentage > 0) led_status_class = "warning";
  else led_status_class = "light-on"; 

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

  html += String("<div class=\"status ");
  html += (WiFi.status() == WL_CONNECTED ? "connected" : "disconnected");
  html += String("\"><strong>WiFi Status:</strong> ");
  String wifi_status = (WiFi.status() == WL_CONNECTED ? "CONNECTED (" + WiFi.localIP().toString() + ")" : "DISCONNECTED");
  html += wifi_status;
  html += String("</div>");
  
  html += String("<div class=\"status ");
  html += (client.connected() ? "connected" : "disconnected");
  html += String("\"><strong>MQTT Status:</strong> ");
  html += (client.connected() ? "CONNECTED" : "DISCONNECTED");
  html += String("</div>");

  html += String("<h2>Printer Status</h2>");
  html += String("<p><strong>GCODE State:</strong> ") + current_gcode_state + String("</p>");
  html += String("<p><strong>Print Percentage:</strong> ") + String(current_print_percentage) + String(" %</p>");
  
  html += String("<h2>External Outputs</h2>");
  
  html += String("<div class=\"status ");
  html += (is_on ? "light-on" : "disconnected");
  html += String("\"><strong>External Light (Pin ") + String(config.chamber_light_pin) + String("):</strong> ");
  html += (is_on ? "ON" : "OFF");
  html += String(" (") + String(config.chamber_pwm_brightness) + "%)";
  html += String("<small>(Logic: ") + (config.invert_output ? "Active LOW" : "Active HIGH");
  html += String(" | Bambu Light Mode: ") + current_light_mode + String("</small></div>");

  html += String("<div class=\"status ");
  html += led_status_class;
  html += String("\"><strong>LED Status Bar (Pin ") + String(LED_PIN_CONST) + String(" / ") + String(config.num_leds) + String(" LEDs):</strong> ");
  html += led_status_str;
  html += String("<small>Data Pin is hardcoded to GPIO ") + String(LED_PIN_CONST) + String(" for FastLED compatibility.</small></div>");
  
  html += String("<p><a href=\"/config\"><button>Change Wi-Fi / Printer Configuration</button></a></p>");
  html += String("</body></html>");
  
  server.send(200, "text/html", html);
}

// -----------------------------------------------------
// --- LED Control Function ---

void update_leds() {
  if (config.num_leds == 0) {
    FastLED.clear();
    FastLED.show();
    return;
  }

  if (current_error_state) {
    FastLED.setBrightness(config.led_bright_error);
    fill_solid(leds, config.num_leds, CRGB(config.led_color_error));
  }
  else if (current_gcode_state == "PAUSED") {
    FastLED.setBrightness(config.led_bright_pause);
    fill_solid(leds, config.num_leds, CRGB(config.led_color_pause));
  }
  else if (current_print_percentage > 0 && current_gcode_state != "IDLE" && current_gcode_state != "FINISH") {
    FastLED.setBrightness(config.led_bright_print);
    int leds_to_light = map(current_print_percentage, 1, 100, 1, config.num_leds);
    if (leds_to_light > config.num_leds) leds_to_light = config.num_leds;
    fill_solid(leds, leds_to_light, CRGB(config.led_color_print));
    fill_solid(leds + leds_to_light, config.num_leds - leds_to_light, CRGB::Black);
  } 
  else {
    FastLED.setBrightness(config.led_bright_idle);
    fill_solid(leds, config.num_leds, CRGB(config.led_color_idle));
  }
  
  FastLED.show();
}

// -----------------------------------------------------
// --- Configuration Functions ---

void setup_chamber_light_pwm(int pin) {
    ledcAttach(pin, PWM_FREQ, PWM_RESOLUTION);
    int off_value = config.invert_output ? 255 : 0;
    ledcWrite(pin, off_value);
    Serial.printf("PWM enabled on GPIO %d. OFF value: %d\n", pin, off_value);
}


bool saveConfig() {
  Serial.println("Saving configuration to LittleFS...");
  DynamicJsonDocument doc(2048);
  doc["bbl_ip"] = config.bbl_ip;
  doc["bbl_serial"] = config.bbl_serial;
  doc["bbl_access_code"] = config.bbl_access_code;
  doc["invert_output"] = config.invert_output;
  doc["chamber_light_pin"] = config.chamber_light_pin; 
  doc["num_leds"] = config.num_leds;
  
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

bool loadConfig() {
  File configFile = LittleFS.open("/config.json", "r");
  if (!configFile) {
    return false;
  }

  size_t size = configFile.size();
  if (size > 2048) {
    Serial.println("Config file size is too large");
    configFile.close();
    return false;
  }

  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, configFile);
  configFile.close();

  if (error) {
    Serial.print("Failed to parse config file: ");
    Serial.println(error.c_str());
    return false;
  }
  
  strlcpy(config.bbl_ip, doc["bbl_ip"] | "192.168.1.100", sizeof(config.bbl_ip));
  strlcpy(config.bbl_serial, doc["bbl_serial"] | "012345678900000", sizeof(config.bbl_serial));
  strlcpy(config.bbl_access_code, doc["bbl_access_code"] | "AABBCCDD", sizeof(config.bbl_access_code));
  config.invert_output = doc["invert_output"] | false;
  config.chamber_light_pin = doc["chamber_light_pin"] | DEFAULT_CHAMBER_LIGHT_PIN; 
  config.num_leds = doc["num_leds"] | DEFAULT_NUM_LEDS;
  
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

void saveConfigCallback() {
  Serial.println("WiFiManager signaled configuration save.");
  
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

  config.chamber_pwm_brightness = atoi(custom_chamber_bright_param);
  config.led_color_idle = strtoul(custom_idle_color_param, NULL, 16);
  config.led_color_print = strtoul(custom_print_color_param, NULL, 16);
  config.led_color_pause = strtoul(custom_pause_color_param, NULL, 16);
  config.led_color_error = strtoul(custom_error_color_param, NULL, 16);
  
  config.led_bright_idle = atoi(custom_idle_bright_param);
  config.led_bright_print = atoi(custom_print_bright_param);
  config.led_bright_pause = atoi(custom_pause_bright_param);
  config.led_bright_error = atoi(custom_error_bright_param);

  saveConfig();
}

// -----------------------------------------------------
// --- MQTT Functions ---

void setup_mqtt_params() {
    client.setServer(config.bbl_ip, mqtt_port);
    mqtt_topic_status = "device/" + String(config.bbl_serial) + "/report";
    Serial.print("MQTT Server: "); Serial.println(config.bbl_ip);
    Serial.print("Subscription Topic: "); Serial.println(mqtt_topic_status);
}

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

void callback(char* topic, byte* payload, unsigned int length) {
  
  char messageBuffer[length + 1];
  memcpy(messageBuffer, payload, length);
  messageBuffer[length] = '\0'; 

  DynamicJsonDocument doc(JSON_DOC_SIZE); 
  DeserializationError error = deserializeJson(doc, messageBuffer);

  if (error) {
    return;
  }

  // --- 1. Chamber Light Control (PWM) ---
  const char* chamberLightMode = doc["report"]["system"]["chamber_light"]["led_mode"] | "off";
  current_light_mode = chamberLightMode; 

  bool lightShouldBeOn = (strcmp(chamberLightMode, "on") == 0 || strcmp(chamberLightMode, "flashing") == 0);
  
  int pwm_value = 0; // Default to OFF
  if (lightShouldBeOn) {
    pwm_value = map(config.chamber_pwm_brightness, 0, 100, 0, 255);
  }

  int output_pwm = (config.invert_output) ? (255 - pwm_value) : pwm_value;
  ledcWrite(config.chamber_light_pin, output_pwm);


  // --- 2. LED Status Bar Control ---
  current_print_percentage = doc["report"]["print"]["print_percentage"] | 0;
  const char* gcodeState = doc["report"]["print"]["gcode_state"] | "IDLE";
  current_gcode_state = gcodeState;

  current_error_state = (strcmp(gcodeState, "FAILED") == 0 || 
                         strcmp(gcodeState, "STOP") == 0); 
  
  update_leds();
}
