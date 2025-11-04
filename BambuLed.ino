#include <WiFi.h>
#include <WiFiClientSecure.h> // <-- ADD THIS
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include "FS.h" // File System
#include "LittleFS.h" // For ESP32 file system support
#include <FastLED.h>
#include <driver/ledc.h> // For PWM functions
#include <ESPmDNS.h> // <-- RE-ENABLED FOR OTA
#include <ArduinoOTA.h> // <-- RE-ENABLED FOR OTA
#include <deque> // <-- NEW: For MQTT history
#include <time.h> // <-- NEW: For NTP Time

// --- Configuration Pin Defaults ---
const int DEFAULT_CHAMBER_LIGHT_PIN = 14; // <-- YOUR FIX
#define FORCE_RESET_PIN 16 // GPIO pin to ground for factory reset
#define PWM_FREQ 5000 // 5kHz frequency for PWM
#define PWM_RESOLUTION 8 // 8-bit resolution (0-255)
// #define PWM_CHANNEL 0 // <-- REMOVED - Using v3 API

// --- FASTLED CONSTANT (MUST BE A MACRO OR CONST EXPRESSION) ---
// If you need to change the GPIO pin for the WS212B, you MUST change this value AND recompile.
#define LED_DATA_PIN 4
const int LED_PIN_CONST = LED_DATA_PIN; // Used for status display in non-template code
// -----------------------------------------------------------------

const int DEFAULT_NUM_LEDS = 18;
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
  // Chamber light finish timeout
  bool chamber_light_finish_timeout = true;

  // LED Status Colors (stored as 0xRRGGBB)
  uint32_t led_color_idle = 0x000000; // Black (off)
  uint32_t led_color_print = 0xFFFFFF; // White
  uint32_t led_color_pause = 0xFFA500; // Orange
  uint32_t led_color_error = 0xFF0000; // Red
  uint32_t led_color_finish = 0x00FF00; // Green

  // LED Status Brightness (0-255)
  int led_bright_idle = 0;     // Off
  int led_bright_print = 100;  // Medium
  int led_bright_pause = 100;  // Medium
  int led_bright_error = 150;  // Bright
  int led_bright_finish = 100; // Medium

  // LED Finish timeout toggle
  bool led_finish_timeout = true; // Enable 2-min timeout by default
  
  // --- NEW: Time settings ---
  char ntp_server[60];
  char timezone[50];
};
Config config;

// --- Custom Parameter Names (used in HTML and storage) ---
// Note: These need to be accessible globally for the re-config handler
WiFiManagerParameter custom_bbl_ip("ip", "Bambu Printer IP", config.bbl_ip, 40);
WiFiManagerParameter custom_bbl_serial("serial", "Printer Serial", config.bbl_serial, 40);
WiFiManagerParameter custom_bbl_access_code("code", "Access Code (MQTT Pass)", config.bbl_access_code, 50);
WiFiManagerParameter custom_bbl_pin("lightpin", "External Light GPIO Pin", "", 5, "type='number' min='0' max='39'"); // Value set later
WiFiManagerParameter custom_bbl_invert("invert", "Invert Light Logic (1=Active Low)", "", 2, "type='checkbox' value='1'"); // Value set later
WiFiManagerParameter custom_chamber_bright("chamber_bright", "External Light Brightness (0-100%)", "", 5, "type='number' min='0' max='100'"); // Value set later
WiFiManagerParameter custom_chamber_finish_timeout("chamber_timeout", "Enable 2-Min Finish Timeout (Light OFF)", "", 2, "type='checkbox' value='1'"); // Value set later
WiFiManagerParameter custom_num_leds("numleds", "Number of WS2812B LEDs (Max 60)", "", 5, "type='number' min='0' max='60'"); // Value set later
WiFiManagerParameter custom_idle_color("idle_color", "Idle Color (RRGGBB)", "", 7, "placeholder='000000'"); // Value set later
WiFiManagerParameter custom_idle_bright("idle_bright", "Idle Brightness (0-255)", "", 5, "type='number' min='0' max='255'"); // Value set later
WiFiManagerParameter custom_print_color("print_color", "Print Color (RRGGBB)", "", 7, "placeholder='FFFFFF'"); // Value set later
WiFiManagerParameter custom_print_bright("print_bright", "Print Brightness (0-255)", "", 5, "type='number' min='0' max='255'"); // Value set later
WiFiManagerParameter custom_pause_color("pause_color", "Pause Color (RRGGBB)", "", 7, "placeholder='FFA500'"); // Value set later
WiFiManagerParameter custom_pause_bright("pause_bright", "Pause Brightness (0-255)", "", 5, "type='number' min='0' max='255'"); // Value set later
WiFiManagerParameter custom_error_color("error_color", "Error Color (RRGGBB)", "", 7, "placeholder='FF0000'"); // Value set later
WiFiManagerParameter custom_error_bright("error_bright", "Error Brightness (0-255)", "", 5, "type='number' min='0' max='255'"); // Value set later
WiFiManagerParameter custom_finish_color("finish_color", "Finish Color (RRGGBB)", "", 7, "placeholder='00FF00'"); // Value set later
WiFiManagerParameter custom_finish_bright("finish_bright", "Finish Brightness (0-255)", "", 5, "type='number' min='0' max='255'"); // Value set later
WiFiManagerParameter custom_led_finish_timeout("led_finish_timeout", "Enable 2-Min Finish Timeout (LEDs)", "", 2, "type='checkbox' value='1'"); // Value set later
WiFiManagerParameter custom_ntp_server("ntp", "NTP Server", config.ntp_server, 60); // <-- NEW
WiFiManagerParameter custom_timezone("tz", "Timezone (TZ String)", config.timezone, 50); // <-- NEW


// Buffers to hold the actual values for the parameters (needed because getValue() returns const char*)
char custom_pin_param_buffer[5] = "14"; // <-- YOUR FIX
char custom_invert_param_buffer[2] = "0";
char custom_chamber_bright_param_buffer[5] = "100";
char custom_chamber_finish_timeout_param_buffer[2] = "1";
char custom_num_leds_param_buffer[5] = "10";
char custom_idle_color_param_buffer[7] = "000000";
char custom_idle_bright_param_buffer[5] = "0";
char custom_print_color_param_buffer[7] = "FFFFFF";
char custom_print_bright_param_buffer[5] = "100";
char custom_pause_color_param_buffer[7] = "FFA500";
char custom_pause_bright_param_buffer[5] = "100";
char custom_error_color_param_buffer[7] = "FF0000";
char custom_error_bright_param_buffer[5] = "150";
char custom_finish_color_param_buffer[7] = "00FF00";
char custom_finish_bright_param_buffer[5] = "100";
char custom_led_finish_timeout_param_buffer[2] = "1";
char custom_ntp_buffer[60] = "pool.ntp.org"; // <-- NEW
char custom_tz_buffer[50] = "GMT0BST,M3.5.0/1,M10.5.0"; // <-- NEW (London w/ DST)


// --- MQTT & JSON Globals ---
WiFiClientSecure espClient; // <-- CHANGE THIS
PubSubClient client(espClient);
const size_t JSON_DOC_SIZE = 4096;
const int mqtt_port = 8883; // <-- CHANGE THIS
const char* mqtt_user = "bblp";
// const char* clientID = "ESP32_BambuLight"; // <-- REMOVED: Now generated dynamically
String mqtt_topic_status;

// --- Status & Web Server Globals ---
WebServer server(80);
String current_light_mode = "UNKNOWN"; // Bambu's internal light status
bool manual_light_control = false; // Flag for manual override
bool external_light_is_on = false; // <-- NEW: Software state tracking
// LED Globals
CRGB leds[MAX_LEDS]; // Array to hold LED color values
int current_print_percentage = 0;
bool current_error_state = false;
String current_gcode_state = "IDLE";
float current_bed_temp = 0.0;
float current_nozzle_temp = 0.0;
float current_bed_target_temp = 0.0;
float current_nozzle_target_temp = 0.0;
int current_time_remaining = 0;
int current_layer = 0; // <-- NEW
String current_wifi_signal = "N/A";
std::deque<String> mqtt_history; // <-- NEW: Replaces last_mqtt_json
const int MAX_HISTORY_SIZE = 100; // <-- NEW: Max history to keep
File restoreFile;
bool restoreSuccess = false;

// --- Non-blocking Reconnect Timer ---
unsigned long lastReconnectAttempt = 0;
const unsigned long RECONNECT_INTERVAL = 5000; // 5 seconds

// --- Print Finish Timer ---
unsigned long finishTime = 0; // Timestamp (ms) when print finished
const unsigned long FINISH_LIGHT_TIMEOUT = 120000; // 2 minutes in ms

// --- Function Prototypes ---
void setup_mqtt_params();
void callback(char* topic, byte* payload, unsigned int length);
bool reconnect_mqtt();
bool loadConfig();
bool saveConfig();
void saveConfigCallback();
bool isValidGpioPin(int pin);
String getTimestamp(); // <-- NEW
String getTimezoneDropdown(String selectedTz); // <-- NEW
void handleRoot();
void handleStatusJson(); // <-- NEW
void update_leds();
void setup_chamber_light_pwm(int pin);
void setup_ota(); // <-- RE-ENABLED
void set_chamber_light_state(bool lightShouldBeOn);
void handleLightOn();
void handleLightOff();
void handleLightAuto();
void handleConfig(); // Handler for /config path
void handleMqttJson();
void handleBackup();
void handleRestorePage();
void handleRestoreUpload();
void handleRestoreReboot();
void parseFullReport(JsonObject doc);
void parseDeltaUpdate(JsonArray arr);
void updatePrinterState(String gcodeState, int printPercentage, String chamberLightMode, float bedTemp, float nozzleTemp, String wifiSignal, float bedTargetTemp, float nozzleTargetTemp, int timeRemaining, int layerNum); // <-- UPDATED

// -----------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(100); // <-- NEW: Short delay to let power stabilize
  Serial.println("\n\nBooting Bambu Light Controller...");
  yield(); // Pat the watchdog

  // --- Check for Factory Reset ---
  pinMode(FORCE_RESET_PIN, INPUT_PULLUP);
  // Read the pin state immediately
  bool forceReset = (digitalRead(FORCE_RESET_PIN) == LOW); 
  if(forceReset) {
    Serial.println("!!! Factory reset pin (GPIO 16) is LOW. Wiping config. !!!");
  } else {
    Serial.println("Factory reset pin is HIGH. Booting normally.");
  }
  yield();

  // 1. Initialize File System
  Serial.println("Mounting LittleFS...");
  if (!LittleFS.begin(true)) {
    Serial.println("LITTLEFS MOUNT FAILED! Check partition scheme. Restarting...");
    delay(2000);
    ESP.restart(); // This is a fatal error, don't continue
  }
  Serial.println("LittleFS mounted.");
  yield();

  // --- NEW: Check for PSRAM ---
  if(psramFound()){
    Serial.printf("PSRAM found! Total: %u, Free: %u\n", ESP.getPsramSize(), ESP.getFreePsram());
  } else {
    Serial.println("No PSRAM found. MQTT history will be stored in internal RAM.");
  }
  // --- END NEW ---

  // --- NEW: Add initial boot message to log ---
  mqtt_history.push_back("[--:--:--] System Booted. Initializing...");
  // --- END NEW ---

  // --- Handle Factory Reset Action ---
  if (forceReset) {
      Serial.println("Factory reset triggered!");
      Serial.println("Erasing /config.json...");
      if (LittleFS.remove("/config.json")) {
        Serial.println("Config file erased.");
      } else {
        Serial.println("Config file not found or erase failed.");
      }
  }

  // 2. Load Configuration
  Serial.println("Loading configuration...");
  if (!loadConfig()) {
    Serial.println("No saved configuration found. Using hardcoded defaults.");
    // Load hardcoded defaults for core values
    strcpy(config.bbl_ip, "192.168.1.100");
    strcpy(config.bbl_serial, "012345678900000");
    strcpy(config.bbl_access_code, "AABBCCDD");
    config.chamber_light_pin = DEFAULT_CHAMBER_LIGHT_PIN;
    strcpy(config.ntp_server, "pool.ntp.org"); // <-- NEW
    strcpy(config.timezone, "GMT0BST,M3.5.0/1,M10.5.0"); // <-- NEW (London)
  }
  Serial.println("Configuration loaded.");
  yield();

  // --- NEW: Add temporary fix for invalid GPIO 25 ---
  if (config.chamber_light_pin == 25) {
    Serial.println("!!! WARNING: Invalid GPIO 25 detected in saved config.");
    Serial.println("!!! This pin conflicts with WiFi hardware.");
    Serial.println("!!! Temporarily reverting to default pin 14 to allow boot.");
    Serial.println("!!! Please use the /config page to save a valid pin (e.g., 14, 26, 12, 13, etc).");
    config.chamber_light_pin = DEFAULT_CHAMBER_LIGHT_PIN; // Force to 14 in memory
  }
  // --- END TEMPORARY FIX ---

  // --- Add debug prints for loaded config ---
  Serial.println("--- Loaded Config Values ---");
  Serial.print("Printer IP: ");
  Serial.println(config.bbl_ip);
  Serial.print("Printer Serial: ");
  Serial.println(config.bbl_serial);
  Serial.print("NTP Server: "); // <-- NEW
  Serial.println(config.ntp_server); // <-- NEW
  Serial.print("Timezone: "); // <-- NEW
  Serial.println(config.timezone); // <-- NEW
  Serial.println("------------------------------");
  yield();

  // 3. Initialize Light GPIO Pin (PWM)
  Serial.print("Initializing Light Pin (PWM): ");
  Serial.println(config.chamber_light_pin);
  if (!isValidGpioPin(config.chamber_light_pin)) {
      Serial.printf("ERROR: Configured light pin (%d) is invalid! Using default (%d).\n", config.chamber_light_pin, DEFAULT_CHAMBER_LIGHT_PIN);
      config.chamber_light_pin = DEFAULT_CHAMBER_LIGHT_PIN;
  }
  setup_chamber_light_pwm(config.chamber_light_pin);
  Serial.println("PWM Light Pin OK.");
  yield();


  // 4. Initialize LED Strip
  if (config.num_leds > 0 && config.num_leds <= MAX_LEDS && isValidGpioPin(LED_DATA_PIN)) {
      Serial.print("Initializing LED strip on Pin ");
      Serial.print(LED_DATA_PIN);
      Serial.print(" with ");
      Serial.print(config.num_leds);
      Serial.println(" LEDs.");
      
      yield(); // <-- NEW: Pat watchdog before FastLED init
      FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(leds, config.num_leds).setCorrection(TypicalLEDStrip);
      FastLED.clear();
      FastLED.show();
      Serial.println("FastLED OK.");
  } else {
      Serial.println("WARNING: LED setup skipped due to invalid pin or 0/too many LEDs.");
      config.num_leds = 0; // Disable LED functionality
  }
  yield();
  
  // 5. Copy current config values to the custom parameter buffers for WiFiManager
  Serial.println("Setting up WiFiManager parameters...");
  snprintf(custom_pin_param_buffer, 5, "%d", config.chamber_light_pin);
  strcpy(custom_invert_param_buffer, config.invert_output ? "1" : "0");
  snprintf(custom_chamber_bright_param_buffer, 5, "%d", config.chamber_pwm_brightness);
  strcpy(custom_chamber_finish_timeout_param_buffer, config.chamber_light_finish_timeout ? "1" : "0");
  snprintf(custom_num_leds_param_buffer, 5, "%d", config.num_leds);
  snprintf(custom_idle_color_param_buffer, 7, "%06X", config.led_color_idle);
  snprintf(custom_idle_bright_param_buffer, 5, "%d", config.led_bright_idle);
  snprintf(custom_print_color_param_buffer, 7, "%06X", config.led_color_print);
  snprintf(custom_print_bright_param_buffer, 5, "%d", config.led_bright_print);
  snprintf(custom_pause_color_param_buffer, 7, "%06X", config.led_color_pause);
  snprintf(custom_pause_bright_param_buffer, 5, "%d", config.led_bright_pause);
  snprintf(custom_error_color_param_buffer, 7, "%06X", config.led_color_error);
  snprintf(custom_error_bright_param_buffer, 5, "%d", config.led_bright_error);
  snprintf(custom_finish_color_param_buffer, 7, "%06X", config.led_color_finish);
  snprintf(custom_finish_bright_param_buffer, 5, "%d", config.led_bright_finish);
  strcpy(custom_led_finish_timeout_param_buffer, config.led_finish_timeout ? "1" : "0");
  snprintf(custom_ntp_buffer, 60, "%s", config.ntp_server); // <-- NEW
  snprintf(custom_tz_buffer, 50, "%s", config.timezone); // <-- NEW

  custom_bbl_pin.setValue(custom_pin_param_buffer, 5);
  custom_bbl_invert.setValue(custom_invert_param_buffer, 2);
  custom_chamber_bright.setValue(custom_chamber_bright_param_buffer, 5);
  custom_chamber_finish_timeout.setValue(custom_chamber_finish_timeout_param_buffer, 2);
  custom_num_leds.setValue(custom_num_leds_param_buffer, 5);
  custom_idle_color.setValue(custom_idle_color_param_buffer, 7);
  custom_idle_bright.setValue(custom_idle_bright_param_buffer, 5);
  custom_print_color.setValue(custom_print_color_param_buffer, 7);
  custom_print_bright.setValue(custom_print_bright_param_buffer, 5);
  custom_pause_color.setValue(custom_pause_color_param_buffer, 7);
  custom_pause_bright.setValue(custom_pause_bright_param_buffer, 5);
  custom_error_color.setValue(custom_error_color_param_buffer, 7);
  custom_error_bright.setValue(custom_error_bright_param_buffer, 5);
  custom_finish_color.setValue(custom_finish_color_param_buffer, 7);
  custom_finish_bright.setValue(custom_finish_bright_param_buffer, 5);
  custom_led_finish_timeout.setValue(custom_led_finish_timeout_param_buffer, 2);
  custom_ntp_server.setValue(custom_ntp_buffer, 60); // <-- NEW
  custom_timezone.setValue(custom_tz_buffer, 50); // <-- NEW
  Serial.println("WiFiManager parameters set.");
  yield();

  // --- NEW: Set lower WiFi power to help prevent brownout ---
  // Must be called BEFORE WiFi.begin() or wm.autoConnect()
  Serial.println("Setting WiFi Tx Power to 11dBm to reduce current spike...");
  WiFi.setTxPower(WIFI_POWER_11dBm);


  // 6. Setup WiFiManager
  WiFiManager wm;

  if (forceReset) {
      Serial.println("Clearing saved Wi-Fi settings...");
      wm.resetSettings();
  }

  wm.setSaveConfigCallback(saveConfigCallback);

  // --- Add Custom Parameters for Config ---
  WiFiManagerParameter p_time_heading("<h2>Time Settings</h2>"); // <-- NEW
  WiFiManagerParameter p_light_heading("<h2>External Light Settings</h2>");
  WiFiManagerParameter p_led_heading("<h2>LED Status Bar Settings</h2>");
  WiFiManagerParameter p_led_info("<small><i>LED Data Pin is hardcoded to GPIO 4 for FastLED.</i></small>");
  WiFiManagerParameter p_led_idle_heading("<h3>Idle Status</h3>");
  WiFiManagerParameter p_led_print_heading("<h3>Printing Status</h3>");
  WiFiManagerParameter p_led_pause_heading("<h3>Paused Status</h3>");
  WiFiManagerParameter p_led_error_heading("<h3>Error Status</h3>");
  WiFiManagerParameter p_led_finish_heading("<h3>Finish Status</h3>");

  wm.addParameter(&custom_bbl_ip);
  wm.addParameter(&custom_bbl_serial);
  wm.addParameter(&custom_bbl_access_code);
  wm.addParameter(&p_time_heading); // <-- NEW
  wm.addParameter(&custom_ntp_server); // <-- NEW
  wm.addParameter(&custom_timezone); // <-- NEW
  wm.addParameter(&p_light_heading);
  wm.addParameter(&custom_bbl_pin);
  wm.addParameter(&custom_bbl_invert);
  wm.addParameter(&custom_chamber_bright);
  wm.addParameter(&custom_chamber_finish_timeout);
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
  wm.addParameter(&p_led_finish_heading);
  wm.addParameter(&custom_finish_color);
  wm.addParameter(&custom_finish_bright);
  wm.addParameter(&custom_led_finish_timeout);


  Serial.println("Starting WiFiManager...");
  yield(); // Pat the dog before this blocking call
  
  // 7. Connect to WiFi
  wm.setConfigPortalTimeout(180); // 3 minutes

  if (!wm.autoConnect("BambuLightSetup", "password")) {
    Serial.println("Failed to connect via portal and timed out. Restarting...");
    delay(3000);
    ESP.restart();
    delay(5000);
  }

  Serial.println("Connected to WiFi!");
  Serial.print("Connected to SSID: ");
  Serial.println(WiFi.SSID());
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  yield();

  // --- NEW: Configure Time ---
  Serial.println("Configuring time...");
  configTime(0, 0, config.ntp_server); // First arg is useless, TZ string handles everything
  setenv("TZ", config.timezone, 1);
  tzset();
  Serial.printf("Timezone set to: %s\n", config.timezone);
  // --- END NEW ---

  // 8. Re-initialize pins in case they were changed in the portal
  int newLightPin = atoi(custom_bbl_pin.getValue());
  if (newLightPin != config.chamber_light_pin && isValidGpioPin(newLightPin)) {
      Serial.println("Light Pin has changed after portal. Re-initializing PWM.");
      ledcDetach(config.chamber_light_pin);
      config.chamber_light_pin = newLightPin; // Update config struct as well
      setup_chamber_light_pwm(config.chamber_light_pin);
  }

  // 9. Setup OTA (Over-the-Air) Updates
  Serial.println("Setting up OTA...");
  setup_ota(); // <-- RE-ENABLED
  Serial.println("OTA OK.");
  yield();

  // 10. Setup MQTT and Callback
  Serial.println("Setting up MQTT...");
  setup_mqtt_params(); // Use the potentially updated config.bbl_ip / config.bbl_serial
  client.setCallback(callback);
  Serial.println("MQTT OK.");
  yield();

  // 11. Setup Web Server
  Serial.println("Setting up Web Server...");
  server.on("/", handleRoot);
  server.on("/status.json", handleStatusJson); // <-- NEW: Add handler for live data
  server.on("/light/on", handleLightOn);
  server.on("/light/off", handleLightOff);
  server.on("/light/auto", handleLightAuto);
  server.on("/config", handleConfig); // Add handler for config page
  server.on("/mqtt", handleMqttJson);
  server.on("/backup", HTTP_GET, handleBackup);
  server.on("/restore", HTTP_GET, handleRestorePage);
  server.on("/restore", HTTP_POST, handleRestoreReboot);
  server.onFileUpload(handleRestoreUpload);
  server.begin();
  Serial.print("Status page available at http://");
  Serial.println(WiFi.localIP());
  Serial.println("--- Setup Complete ---");
}

void loop() {
  // OTA must be handled in the main loop
  ArduinoOTA.handle(); // <-- RE-ENABLED

  // Handle web client requests
  server.handleClient();

  // --- Non-blocking MQTT reconnect ---
  if (!client.connected()) {
    if (millis() - lastReconnectAttempt > RECONNECT_INTERVAL) {
      lastReconnectAttempt = millis();
      if (reconnect_mqtt()) {
        lastReconnectAttempt = 0; // Successful connection
      }
    }
  } else {
    client.loop();
  }

  // --- Non-blocking Timer Logic for External Light ---
  // This timer is the "master override" and must run regardless of gcode_state
  if (config.chamber_light_finish_timeout && finishTime > 0 &&
      (millis() - finishTime > FINISH_LIGHT_TIMEOUT)) {
      
      if (external_light_is_on) { // If the light is on (and not in manual mode)
          if (!manual_light_control) {
              Serial.println("External Light: Finish timeout reached. Turning OFF via loop timer.");
              set_chamber_light_state(false); // Turn light OFF
          }
          finishTime = 0; // Clear the timer
      } else {
          finishTime = 0; // Timer expired, but light is already off, just clear the timer
      }
  }

  // --- Non-blocking Timer Logic for LEDs ---
  if (config.led_finish_timeout && current_gcode_state == "FINISH" &&
      finishTime > 0 && (millis() - finishTime > FINISH_LIGHT_TIMEOUT)) {

      // Check if LEDs are already idle to avoid unnecessary updates
      // Compare brightness and the color of the first LED
      bool already_idle = (FastLED.getBrightness() == config.led_bright_idle &&
                           leds[0].r == (config.led_color_idle >> 16 & 0xFF) &&
                           leds[0].g == (config.led_color_idle >> 8 & 0xFF) &&
                           leds[0].b == (config.led_color_idle & 0xFF) );

      if (!already_idle) {
          // Serial.println("LED Strip: Finish timeout reached. Reverting to IDLE."); // Too noisy
          update_leds(); // This will now fall through to the IDLE state
      }
  }
}

// -----------------------------------------------------
// --- OTA Setup Function ---

// --- RE-ENABLED OTA FUNCTION ---
void setup_ota() {
  ArduinoOTA.setHostname("bambu-light-controller");

  ArduinoOTA
    .onStart([]() {
      Serial.println("OTA Start");
      if(config.num_leds > 0) { // Only update LEDs if enabled
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
  // Serial.println("OTA Ready. Hostname: bambu-light-controller"); // Already printed
}
// --- END RE-ENABLED OTA FUNCTION ---


// -----------------------------------------------------
// --- Validation & Web Status Functions ---

bool isValidGpioPin(int pin) {
    if (pin < 0 || pin > 39) return false;
    if (pin >= 6 && pin <= 11) return false; // Reserved/Flash
    if (pin >= 34) return false; // Input-only
    return true;
}

// --- NEW: Get formatted timestamp ---
String getTimestamp() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    return "[--:--:--]";
  }
  char buffer[30];
  strftime(buffer, sizeof(buffer), "[%Y-%m-%d %H:%M:%S]", &timeinfo);
  return String(buffer);
}


// --- UPDATED: handleRoot() now serves a static page, JS does the work ---
void handleRoot() {
  // This function just serves the static HTML page.
  // The live data will be fetched by JavaScript.

  String html = String("<!DOCTYPE html><html><head>");
  html += String("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
  html += String("<title>Bambu Light Status</title><style>");
  html += String("body { font-family: Arial, sans-serif; margin: 20px; background-color: #1a1a1b; color: #e0e0e0; }");
  html += String("h1, h2 { color: #ffffff; }");
  html += String("a { color: #58a6ff; }");
  html += String(".status { padding: 10px; margin-bottom: 10px; border-radius: 5px; transition: background-color 0.5s, border-color 0.5s; background-color: #2c2c2e; border: 1px solid #444; }"); // Added transition
  html += String(".connected { background-color: #1a3a24; color: #8cda9b; border: 1px solid #336d3f; }");
  html += String(".disconnected { background-color: #401f22; color: #f0989f; border: 1px solid #7c333a; }");
  html += String(".warning { background-color: #423821; color: #f0d061; border: 1px solid #7e6c33; }");
  html += String(".light-on { background-color: #1c314a; color: #9cc2ef; border: 1px solid #335d88; }");
  html += String(".error { background-color: #401f22; color: #f0989f; border: 1px solid #7c333a; font-weight: bold; }");
  html += String("button { background-color: #378cf0; color: #ffffff; padding: 10px 15px; border: none; border-radius: 5px; cursor: pointer; margin-right: 5px; margin-bottom: 5px; }");
  html += String("button.off { background-color: #6c757d; } button.auto { background-color: #28a745; }");
  html += String("span.data { font-weight: normal; }"); // Class for the data part
  html += String("</style></head><body><h1>Bambu Chamber Light Controller</h1>");

  // --- WiFi Status (Static on load) ---
  html += String("<div class=\"status ");
  html += (WiFi.status() == WL_CONNECTED ? "connected" : "disconnected");
  html += String("\"><strong>WiFi Status:</strong> ");
  String wifi_status = (WiFi.status() == WL_CONNECTED ? "CONNECTED (" + WiFi.SSID() + " / " + WiFi.localIP().toString() + ")" : "DISCONNECTED");
  html += wifi_status;
  html += String("</div>");

  // --- MQTT Status (Live) ---
  html += String("<div id=\"mqtt-status-div\" class=\"status disconnected\">"); // Start as disconnected
  html += String("<strong>MQTT Status:</strong> <span id=\"mqtt-status\" class=\"data\">DISCONNECTED</span>");
  html += String(" | <a href=\"/mqtt\" target=\"_blank\">View MQTT History</a>"); // <-- UPDATED TEXT
  html += String("</div>");

  // --- Printer Status (Live) ---
  html += String("<h2>Printer Status</h2>");
  html += String("<p><strong>GCODE State:</strong> <span id=\"gcode-state\" class=\"data\">N/A</span></p>");
  html += String("<p><strong>Print Percentage:</strong> <span id=\"print-percent\" class=\"data\">0 %</span></p>");
  html += String("<p><strong>Current Layer:</strong> <span id=\"layer-num\" class=\"data\">0</span></p>"); // <-- NEW
  html += String("<p><strong>Time Remaining:</strong> <span id=\"print-time\" class=\"data\">--:--:--</span></p>");
  html += String("<p><strong>Nozzle Temp:</strong> <span id=\"nozzle-temp\" class=\"data\">0.0 / 0.0 &deg;C</span></p>");
  html += String("<p><strong>Bed Temp:</strong> <span id=\"bed-temp\" class=\"data\">0.0 / 0.0 &deg;C</span></p>");
  html += String("<p><strong>Printer WiFi Signal:</strong> <span id=\"wifi-signal\" class=\"data\">N/A</span></p>");

  // --- External Outputs (Live) ---
  html += String("<h2>External Outputs</h2>");
  html += String("<div id=\"light-status-div\" class=\"status disconnected\">");
  html += String("<strong>External Light (Pin ") + String(config.chamber_light_pin) + String("):</strong> ");
  html += String("<span id=\"light-status\" class=\"data\">N/A</span>");
  html += String("<br><small><strong>Control Mode:</strong> <span id=\"light-mode\" class=\"data\">N/A</span>");
  html += String(" | (Logic: ") + (config.invert_output ? "Active LOW" : "Active HIGH");
  html += String(" | Bambu Light Mode: <span id=\"bambu-light-mode\" class=\"data\">N/A</span>)</small></div>");

  // --- LED Status (Live) ---
  html += String("<div id=\"led-status-div\" class=\"status disconnected\">");
  html += String("<strong>LED Status Bar (Pin ") + String(LED_PIN_CONST) + String(" / ") + String(config.num_leds) + String(" LEDs):</strong> ");
  html += String("<span id=\"led-status\" class=\"data\">N/A</span>");
  
  // --- NEW: Live Virtual LED Bar ---
  html += String("<div id='virtual-bar-container' style='margin-top: 10px;'>");
  html += String("<div id='virtual-bar' style='display: flex; width: 100%; height: 20px; background: #222; border-radius: 5px; overflow: hidden; border: 1px solid #444;'>");
  // Add virtual LEDs based on config
  for(int i=0; i < config.num_leds && i < MAX_LEDS; i++) {
    html += "<div class='vled' style='flex-grow: 1; height: 100%; transition: background-color 0.5s, opacity 0.5s;'></div>";
  }
  html += String("</div></div>"); // end virtual-bar and container
  // --- END NEW ---

  html += String("<br><small>Data Pin is hardcoded to GPIO ") + String(LED_PIN_CONST) + String(" for FastLED compatibility.</small></div>");

  // --- Manual Control (Static) ---
  html += String("<h2>Manual Control</h2>");
  html += String("<p><a href=\"/light/on\"><button>Turn Light ON</button></a>");
  html += String("<a href=\"/light/off\"><button class=\"off\">Turn Light OFF</button></a>");
  html += String("<a href=\"/light/auto\"><button class=\"auto\">Set to AUTO</button></a></p>");

  html += String("<hr><p><a href=\"/config\"><button>Change Device Settings</button></a></p>");
  
  // --- NEW: JavaScript for Live Updates ---
  html += String("<script>");
  html += String("function formatTime(s) {");
  html += String("if (s <= 0) return '--:--:--';");
  html += String("let h = Math.floor(s / 3600); s %= 3600;");
  html += String("let m = Math.floor(s / 60); s %= 60;");
  html += String("let m_str = m < 10 ? '0' + m : m;"); // leading zero for minutes
  html += String("let s_str = s < 10 ? '0' + s : s;"); // leading zero for seconds
  html += String("return h > 0 ? h + ':' + m_str + ':' + s_str : m_str + ':' + s_str;"); // Show HH:MM:SS or MM:SS
  html += String("}");
  
  html += String("async function updateStatus() {");
  html += String("try {");
  html += String("const response = await fetch('/status.json');");
  html += String("if (!response.ok) return;");
  html += String("const data = await response.json();");
  
  // MQTT Status
  html += String("document.getElementById('mqtt-status').innerText = data.mqtt_connected ? 'CONNECTED' : 'DISCONNECTED';");
  html += String("document.getElementById('mqtt-status-div').className = 'status ' + (data.mqtt_connected ? 'connected' : 'disconnected');");
  
  // Printer Status
  html += String("document.getElementById('gcode-state').innerText = data.gcode_state;");
  html += String("document.getElementById('print-percent').innerText = data.print_percentage + ' %';");
  html += String("document.getElementById('layer-num').innerText = data.layer_num;"); // <-- NEW
  html += String("document.getElementById('print-time').innerText = formatTime(data.time_remaining);");
  html += String("document.getElementById('nozzle-temp').innerHTML = data.nozzle_temp.toFixed(1) + ' / ' + data.nozzle_target_temp.toFixed(1) + ' &deg;C';");
  html += String("document.getElementById('bed-temp').innerHTML = data.bed_temp.toFixed(1) + ' / ' + data.bed_target_temp.toFixed(1) + ' &deg;C';");
  html += String("document.getElementById('wifi-signal').innerText = data.wifi_signal;");
  
  // Light Status
  html += String("document.getElementById('light-status').innerText = data.light_is_on ? ('ON (' + data.chamber_bright + '%)') : 'OFF';");
  html += String("document.getElementById('light-status-div').className = 'status ' + (data.light_is_on ? 'light-on' : 'disconnected');");
  html += String("document.getElementById('light-mode').innerText = data.manual_control ? 'MANUAL' : ('AUTO' + data.light_mode_extra);");
  html += String("document.getElementById('bambu-light-mode').innerText = data.bambu_light_mode;");
  
  // LED Status
  html += String("document.getElementById('led-status').innerText = data.led_status_str;");
  html += String("document.getElementById('led-status-div').className = 'status ' + data.led_status_class;");

  // --- NEW Virtual LED Bar Logic ---
  html += String("let color = data.led_color_val.toString(16).padStart(6, '0');");
  html += String("let brightness = data.led_bright_val;");
  html += String("let opacity = (brightness / 255).toFixed(2);");
  html += String("let vleds = document.querySelectorAll('#virtual-bar .vled');");
  html += String("let numLeds = vleds.length;"); // Get count from rendered divs
  
  html += String("if (data.is_printing && data.print_percentage > 0 && numLeds > 0) {");
  html += String("  let leds_to_light = Math.ceil((data.print_percentage / 100) * numLeds);"); // Calculate based on actual led count
  html += String("  for (let i = 0; i < numLeds; i++) {");
  html += String("    if (i < leds_to_light) {");
  html += String("      vleds[i].style.backgroundColor = '#' + color;");
  html += String("      vleds[i].style.opacity = opacity;");
  html += String("    } else {");
  html += String("      vleds[i].style.backgroundColor = '#000';");
  html += String("      vleds[i].style.opacity = '1.0';"); // Off LEDs are black
  html += String("    }");
  html += String("  }");
  html += String("} else {");
  html += String("  vleds.forEach(led => {"); // Solid color for all other states
  html += String("    led.style.backgroundColor = '#' + color;");
  html += String("    led.style.opacity = opacity;");
  html += String("  });");
  html += String("}");
  // --- END Virtual LED Bar Logic ---

  html += String("} catch (e) { console.error('Error fetching status:', e); }");
  html += String("}");
  
  // Run on load and then every 3 seconds
  html += String("document.addEventListener('DOMContentLoaded', updateStatus);");
  html += String("setInterval(updateStatus, 3000);");
  
  html += String("</script>");
  // --- End JavaScript ---
  
  html += String("</body></html>");

  server.send(200, "text/html", html);
}
// --- END UPDATED handleRoot ---

// --- NEW: Handler for sending live status data as JSON ---
void handleStatusJson() {
  // Serial.println("Web Request: /status.json"); // Optional: for debugging
  DynamicJsonDocument doc(1024);

  // MQTT Status
  doc["mqtt_connected"] = client.connected();

  // Printer Status
  doc["gcode_state"] = current_gcode_state;
  doc["print_percentage"] = current_print_percentage;
  doc["time_remaining"] = current_time_remaining;
  doc["layer_num"] = current_layer; // <-- NEW
  doc["nozzle_temp"] = current_nozzle_temp;
  doc["nozzle_target_temp"] = current_nozzle_target_temp;
  doc["bed_temp"] = current_bed_temp;
  doc["bed_target_temp"] = current_bed_target_temp;
  doc["wifi_signal"] = current_wifi_signal;

  // Light Status
  // int pwm_duty = ledcRead(config.chamber_light_pin); // <-- OLD
  // bool is_on = (config.invert_output) ? (pwm_duty < 255) : (pwm_duty > 0); // <-- OLD
  doc["light_is_on"] = external_light_is_on; // <-- NEW
  doc["chamber_bright"] = config.chamber_pwm_brightness;
  doc["manual_control"] = manual_light_control;
  doc["bambu_light_mode"] = current_light_mode;
  
  String light_mode_extra = "";
  if (!manual_light_control && config.chamber_light_finish_timeout && finishTime > 0) {
    if (millis() - finishTime < FINISH_LIGHT_TIMEOUT) {
      light_mode_extra = " (Finish light ON - timing out...)";
    } else {
      light_mode_extra = " (Finish light OFF - timeout complete)";
    }
  }
  doc["light_mode_extra"] = light_mode_extra;

  // --- NEW: LED Status for Preview Bar ---
  uint32_t current_color_val = config.led_color_idle;
  int current_bright_val = config.led_bright_idle;
  bool is_printing = false;

  if (current_error_state) {
      current_color_val = config.led_color_error;
      current_bright_val = config.led_bright_error;
  } else if (current_gcode_state == "PAUSED") {
      current_color_val = config.led_color_pause;
      current_bright_val = config.led_bright_pause;
  } else if (current_gcode_state == "FINISH") {
      bool timeout_enabled = config.led_finish_timeout;
      bool timer_active = (finishTime > 0 && (millis() - finishTime < FINISH_LIGHT_TIMEOUT));
      if (!timeout_enabled || timer_active) {
          current_color_val = config.led_color_finish;
          current_bright_val = config.led_bright_finish;
      } else {
          // Already set to idle defaults
      }
  } else if (current_print_percentage > 0 && current_gcode_state != "IDLE") {
      current_color_val = config.led_color_print;
      current_bright_val = config.led_bright_print;
      is_printing = true; // Tell JS to render a progress bar
  }
  doc["led_color_val"] = current_color_val; // Send as a number
  doc["led_bright_val"] = current_bright_val;
  doc["is_printing"] = is_printing;
  // --- END NEW ---

  // LED Status (re-using logic from handleRoot)
  String led_status_str;
  String led_status_class;
  if (config.num_leds == 0) {
    led_status_str = "Disabled";
    led_status_class = "disconnected";
  }
  else if (current_error_state) {
    led_status_str = "Error (Red)";
    led_status_class = "error";
  }
  else if (current_gcode_state == "PAUSED") {
    led_status_str = "Paused (Orange)";
    led_status_class = "warning";
  }
  else if (current_gcode_state == "FINISH") {
    bool timeout_enabled = config.led_finish_timeout;
    bool timer_active = (finishTime > 0 && (millis() - finishTime < FINISH_LIGHT_TIMEOUT));
    if (!timeout_enabled || timer_active) {
        led_status_str = "Print Finished (Green)";
        if(timeout_enabled) led_status_str += " (Timing out...)";
        led_status_class = "connected";
    } else {
        led_status_str = "Idle (Finish Timeout)";
        led_status_class = "light-on";
    }
  }
  else if (current_print_percentage > 0) {
    led_status_str = "Printing Progress (" + String(current_print_percentage) + "%)";
    led_status_class = "warning";
  }
  else {
    led_status_str = "Idle/Off (No Light)";
    led_status_class = "light-on";
  }
  doc["led_status_str"] = led_status_str;
  doc["led_status_class"] = led_status_class;
  
  // Send the JSON
  String json_output;
  serializeJson(doc, json_output);
  server.send(200, "application/json", json_output);
}
// --- END NEW JSON HANDLER ---

// -----------------------------------------------------
// --- Web Server Handlers for Manual Control ---

// --- UPDATED HANDLER ---
void handleMqttJson() {
  Serial.println("Web Request: /mqtt (View JSON History)");
  
  String html = "<!DOCTYPE html><html><head><title>MQTT History</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body{font-family:monospace;margin:20px;background:#1a1a1b;color:#e0e0e0;}";
  html += "h1{color:#fff;font-family:Arial,sans-serif;}";
  html += "p{font-family:Arial,sans-serif;}";
  html += "pre{white-space:pre-wrap;word-wrap:break-word;font-size:0.9em;background:#2c2c2e;padding:10px;border-radius:5px;border:1px solid #444;}";
  html += "a{color:#58a6ff;text-decoration:none;font-family:Arial,sans-serif;}";
  html += "</style></head><body><h1>MQTT Message History</h1>";
  html += "<p>Showing the last " + String(mqtt_history.size()) + " of " + String(MAX_HISTORY_SIZE) + " messages (oldest first).</p>";
  html += "<a href='/'>&laquo; Back to Status</a><br><br>";

  html += "<pre>";
  if(mqtt_history.empty()) {
    html += "No data received yet.";
  } else {
    // Iterate forwards (from oldest to newest)
    for(auto it = mqtt_history.begin(); it != mqtt_history.end(); ++it) {
      String msg = *it;
      // Basic HTML escaping
      msg.replace("<", "&lt;");
      msg.replace(">", "&gt;");
      html += msg + "\n"; // Add the log line and a newline
    }
  }
  html += "</pre>";

  html += "</body></html>";
  server.send(200, "text/html", html);
}
// --- END UPDATED HANDLER ---


// --- NEW: Backup and Restore Handlers ---
void handleBackup() {
  Serial.println("Web Request: /backup");
  if (!LittleFS.exists("/config.json")) {
    server.send(404, "text/plain", "Config file not found.");
    return;
  }
  File configFile = LittleFS.open("/config.json", "r");
  server.sendHeader("Content-Disposition", "attachment; filename=\"config.json\"");
  server.streamFile(configFile, "application/json");
  configFile.close();
}

void handleRestorePage() {
  Serial.println("Web Request: GET /restore");
  String html = "<!DOCTYPE html><html><head><title>Restore Config</title>";
  html += "<style>body{font-family:Arial,sans-serif;margin:20px;background:#1a1a1b;color:#e0e0e0;}";
  html += "h1{color:#fff;} p{color:#e0e0e0;}";
  html += "button{background-color:#dc3545;color:white;padding:12px 20px;border:none;border-radius:5px;cursor:pointer;font-size:16px;}";
  html += "a{color:#58a6ff;}";
  html += "</style></head>";
  html += "<body><h1>Restore Configuration</h1>";
  html += "<p><b>WARNING:</b> This will overwrite your current settings and reboot the device.</p>";
  html += "<form action='/restore' method='POST' enctype='multipart/form-data'>";
  html += "<input type='file' name='restore' accept='.json' required>";
  html += "<br><br><button type='submit'>Upload and Restore</button>";
  html += "</form><br><br><a href='/config'>&laquo; Back to Settings</a></body></html>";
  server.send(200, "text/html", html);
}

void handleRestoreUpload() {
  HTTPUpload& upload = server.upload();
  if (server.uri() != "/restore") { // <-- *** FIX: Use server.uri() instead of upload.uri ***
    return; // This upload isn't for us
  }

  if (upload.status == UPLOAD_FILE_START) {
    // Check filename *before* opening
    if (upload.filename != "config.json") {
      Serial.printf("Invalid restore filename: %s. Aborting.\n", upload.filename.c_str());
      restoreSuccess = false; 
      return; // This doesn't stop the upload, just stops us from writing
    }
    
    Serial.printf("Restore Start: %s\n", upload.filename.c_str());
    restoreFile = LittleFS.open("/config.json", "w"); // Open the *real* file
    if (restoreFile) {
      restoreSuccess = true; // Flag that we've started successfully
    } else {
      Serial.println("Failed to open /config.json for restore.");
      restoreSuccess = false;
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (restoreSuccess && restoreFile) { // Only write if we're in a good state
      restoreFile.write(upload.buf, upload.currentSize);
      // Serial.printf("Restore Write: %u bytes\n", upload.currentSize); // Too noisy
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (restoreSuccess && restoreFile) {
      restoreFile.close();
      Serial.printf("Restore End: %u bytes total\n", upload.totalSize);
    } else {
      if(restoreFile) restoreFile.close();
      if(restoreSuccess) { // This means we *thought* we were good, but something failed
         Serial.println("Restore failed during write/end.");
         restoreSuccess = false; // Mark as failed
      }
    }
  }
}

void handleRestoreReboot() {
  if (restoreSuccess) {
    String html = "<!DOCTYPE html><html><head><title>Restore Complete</title>";
    html += "<meta http-equiv='refresh' content='3;url=/'><style>body{font-family:Arial,sans-serif;background:#1a1a1b;color:#e0e0e0;}</style></head>";
    html += "<body><h2>Restore Complete.</h2>";
    html += "<p>Device is rebooting to load new configuration. You will be redirected in 3 seconds...</p></body></html>";
    server.send(200, "text/html", html);
    delay(1000);
    ESP.restart();
  } else {
    String html = "<!DOCTYPE html><html><head><title>Restore Failed</title>";
    html += "<style>body{font-family:Arial,sans-serif;margin:20px;background:#1a1a1b;color:#e0e0e0;} a{color:#58a6ff;}</style></head>";
    html += "<body><h2>Restore Failed.</h2>";
    html += "<p>The file upload failed. This may be due to an invalid filename (must be 'config.json') or a file system error.</p>";
    html += "<a href='/restore'>Try again</a> | <a href='/'>Back to Status</a></body></html>";
    server.send(400, "text/html", html);
  }
  restoreSuccess = false; // Reset flag
}
// --- END NEW HANDLERS ---


void handleLightOn() {
  Serial.println("Web Request: /light/on");
  manual_light_control = true;
  set_chamber_light_state(true); // Turn light ON
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

void handleLightOff() {
  Serial.println("Web Request: /light/off");
  manual_light_control = true;
  set_chamber_light_state(false); // Turn light OFF
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

void handleLightAuto() {
  Serial.println("Web Request: /light/auto");
  manual_light_control = false;
  // Re-sync with the last known printer light state, considering finish timer
  bool lightShouldBeOn = (current_light_mode == "on" || current_light_mode == "flashing");
  bool finalLightState = lightShouldBeOn; // Default

  // --- UPDATED TIMER CHECK ---
  // Check for timer override
  if (config.chamber_light_finish_timeout && finishTime > 0) {
      if (millis() - finishTime < FINISH_LIGHT_TIMEOUT) {
          // Timer is active, force light ON
          finalLightState = true;
      } else {
          // Timer has expired, force light OFF
          finalLightState = false;
      }
  }
  // --- END UPDATED ---

  set_chamber_light_state(finalLightState);

  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

// --- NEW handleConfig Function (Replaces the old one) ---
// This version shows a simple HTML settings page
// It does NOT start the WiFiManager portal
void handleConfig() {

  // --- 1. Handle POST data (form submission) ---
  if (server.method() == HTTP_POST) {
    Serial.println("Web Request: POST /config - Saving settings...");

    Config tempConfig = config; // Work on a temporary copy

    // --- Parse Printer Settings ---
    if (server.hasArg("ip")) strlcpy(tempConfig.bbl_ip, server.arg("ip").c_str(), sizeof(tempConfig.bbl_ip));
    if (server.hasArg("serial")) strlcpy(tempConfig.bbl_serial, server.arg("serial").c_str(), sizeof(tempConfig.bbl_serial));
    if (server.hasArg("code")) strlcpy(tempConfig.bbl_access_code, server.arg("code").c_str(), sizeof(tempConfig.bbl_access_code));

    // --- NEW: Parse Time Settings ---
    if (server.hasArg("ntp_server")) strlcpy(tempConfig.ntp_server, server.arg("ntp_server").c_str(), sizeof(tempConfig.ntp_server));
    if (server.hasArg("timezone")) strlcpy(tempConfig.timezone, server.arg("timezone").c_str(), sizeof(tempConfig.timezone));

    // --- Parse External Light Settings ---
    if (server.hasArg("lightpin")) {
      int tempLightPin = server.arg("lightpin").toInt();
      if (isValidGpioPin(tempLightPin)) {
          tempConfig.chamber_light_pin = tempLightPin;
      } else {
          Serial.printf("ERROR: Invalid GPIO pin %d submitted. Retaining previous pin.\n", tempLightPin);
      }
    }
    tempConfig.invert_output = server.hasArg("invert"); // Checkbox: present if checked
    if (server.hasArg("chamber_bright")) tempConfig.chamber_pwm_brightness = constrain(server.arg("chamber_bright").toInt(), 0, 100);
    tempConfig.chamber_light_finish_timeout = server.hasArg("chamber_timeout");

    // --- Parse LED Status Bar Settings ---
    if (server.hasArg("numleds")) {
      int tempNumLeds = server.arg("numleds").toInt();
      if (tempNumLeds >= 0 && tempNumLeds <= MAX_LEDS) {
          tempConfig.num_leds = tempNumLeds;
      } else {
          Serial.printf("WARNING: Invalid LED count (%d). Setting to 0.\n", tempNumLeds);
          tempConfig.num_leds = 0;
      }
    }
    tempConfig.led_finish_timeout = server.hasArg("led_finish_timeout");

    // --- Parse LED Colors (convert hex string to uint32_t) ---
    // Use strtoul for robust hex conversion
    if (server.hasArg("idle_color")) tempConfig.led_color_idle = strtoul(server.arg("idle_color").c_str(), NULL, 16);
    if (server.hasArg("print_color")) tempConfig.led_color_print = strtoul(server.arg("print_color").c_str(), NULL, 16);
    if (server.hasArg("pause_color")) tempConfig.led_color_pause = strtoul(server.arg("pause_color").c_str(), NULL, 16);
    if (server.hasArg("error_color")) tempConfig.led_color_error = strtoul(server.arg("error_color").c_str(), NULL, 16);
    if (server.hasArg("finish_color")) tempConfig.led_color_finish = strtoul(server.arg("finish_color").c_str(), NULL, 16);

    // --- Parse LED Brightness ---
    if (server.hasArg("idle_bright")) tempConfig.led_bright_idle = constrain(server.arg("idle_bright").toInt(), 0, 255);
    if (server.hasArg("print_bright")) tempConfig.led_bright_print = constrain(server.arg("print_bright").toInt(), 0, 255);
    if (server.hasArg("pause_bright")) tempConfig.led_bright_pause = constrain(server.arg("pause_bright").toInt(), 0, 255);
    if (server.hasArg("error_bright")) tempConfig.led_bright_error = constrain(server.arg("error_bright").toInt(), 0, 255);
    if (server.hasArg("finish_bright")) tempConfig.led_bright_finish = constrain(server.arg("finish_bright").toInt(), 0, 255);

    // --- Apply and Save ---
    config = tempConfig; // Apply changes to global config
    saveConfig();        // Save to LittleFS

    // Send a response page and then reboot
    String html = "<!DOCTYPE html><html><head><title>Saving...</title>";
    html += "<meta http-equiv='refresh' content='3;url=/'><style>body{font-family:Arial,sans-serif;background:#1a1a1b;color:#e0e0e0;}</style></head>";
    html += "<body><h2>Configuration Saved.</h2>";
    html += "<p>Device is rebooting to apply settings. You will be redirected in 3 seconds...</p></body></html>";
    server.send(200, "text/html", html);
    delay(1000); // Give server time to send response
    ESP.restart();

  }
  // --- 2. Handle GET request (Show the form) ---
  else {
    Serial.println("Web Request: GET /config - Showing settings page...");
    // Buffers to format color hex codes (0xRRGGBB -> "RRGGBB")
    char idle_color_hex[7], print_color_hex[7], pause_color_hex[7], error_color_hex[7], finish_color_hex[7];
    snprintf(idle_color_hex, 7, "%06X", config.led_color_idle);
    snprintf(print_color_hex, 7, "%06X", config.led_color_print);
    snprintf(pause_color_hex, 7, "%06X", config.led_color_pause);
    snprintf(error_color_hex, 7, "%06X", config.led_color_error);
    snprintf(finish_color_hex, 7, "%06X", config.led_color_finish);

    String html = "<!DOCTYPE html><html><head>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>Bambu Light Config</title><style>";
    html += "body{font-family:Arial,sans-serif;margin:20px;background:#1a1a1b;color:#e0e0e0;}";
    html += "h1,h2,h3{color:#fff;} h2{border-bottom:2px solid #378cf0;padding-bottom:5px;}";
    html += "form{background:#2c2c2e;padding:20px;border-radius:8px;box-shadow:0 2px 5px rgba(0,0,0,0.1);}";
    html += "div{margin-bottom:15px;} label{display:block;margin-bottom:5px;font-weight:bold;color:#ccc;}";
    html += "input[type='text'],input[type='number'],input[type='password'],select{width:98%;padding:8px;border:1px solid #555;border-radius:4px;font-family:Arial,sans-serif;font-size:1em;background-color:#3a3a3c;color:#e0e0e0;}"; // <-- UPDATED STYLE
    html += "input[type='checkbox']{margin-right:10px;vertical-align:middle;}";
    html += "label[for='invert'], label[for='chamber_timeout'], label[for='led_finish_timeout'] { display:inline-block;font-weight:normal; }"; // Fix checkbox label alignment
    html += "button{background-color:#378cf0;color:#ffffff;padding:12px 20px;border:none;border-radius:5px;cursor:pointer;font-size:16px;}";
    html += "button:hover{background-color:#0056b3;}";
    html += ".color-input{width:100px;padding:8px;vertical-align:middle;margin-left:10px;border:1px solid #555;}";
    html += ".grid{display:grid;grid-template-columns:repeat(auto-fit, minmax(200px, 1fr));grid-gap:20px;}";
    html += ".card{background:#3a3a3c;padding:15px;border-radius:5px;border:1px solid #555;}";
    html += "small{color:#aaa;}";
    html += "a{color:#58a6ff;}";
    html += ".color-swatch{width: 20px; height: 20px; display: inline-block; vertical-align: middle; margin-left: 10px; border: 1px solid #555; border-radius: 4px; background-color: #000;}"; // <-- NEW
    html += "</style></head><body><h1>Bambu Light Controller Settings</h1>";
    html += "<p><small>To change Wi-Fi, use the 'Factory Reset' pin (GPIO 16) on boot.</small></p>";
    html += "<form action='/config' method='POST'>";

    // --- Printer Settings ---
    html += "<h2>Printer Settings</h2>";
    html += "<div><label for='ip'>Bambu Printer IP</label><input type='text' id='ip' name='ip' value='";
    html += String(config.bbl_ip);
    html += "'></div>";
    
    html += "<div><label for='serial'>Printer Serial</label><input type='text' id='serial' name='serial' value='";
    html += String(config.bbl_serial);
    html += "'></div>";

    html += "<div><label for='code'>Access Code (MQTT Pass)</label><input type='password' id='code' name='code' value='";
    html += String(config.bbl_access_code);
    html += "'></div>";

    // --- NEW: Time Settings ---
    html += "<h2>Time Settings</h2>";
    html += "<div class='grid'>";
    html += "<div class='card'><div><label for='ntp_server'>NTP Server</label><input type='text' id='ntp_server' name='ntp_server' value='";
    html += String(config.ntp_server);
    html += "'></div></div>";
    html += "<div class='card'><div><label for='timezone'>Timezone</label>";
    html += getTimezoneDropdown(String(config.timezone));
    html += "</div></div>";
    html += "</div>";
    // --- END NEW ---

    // --- External Light ---
    html += "<h2>External Light Settings</h2>";
    html += "<div class='grid'>";
    html += "<div class='card'><div><label for='lightpin'>External Light GPIO Pin</label><input type='number' id='lightpin' name='lightpin' min='0' max='39' value='";
    html += String(config.chamber_light_pin);
    html += "'></div>";
    
    html += "<div><label for='chamber_bright'>Brightness (0-100%)</label><input type='number' id='chamber_bright' name='chamber_bright' min='0' max='100' value='";
    html += String(config.chamber_pwm_brightness);
    html += "'></div></div>";

    html += "<div class='card'><div><input type='checkbox' id='invert' name='invert' value='1' ";
    html += (config.invert_output ? "checked" : "");
    html += "><label for='invert'>Invert Light Logic (Active LOW)</label></div>";
    
    html += "<div><input type='checkbox' id='chamber_timeout' name='chamber_timeout' value='1' ";
    html += (config.chamber_light_finish_timeout ? "checked" : "");
    html += "><label for='chamber_timeout'>Enable 2-Min Finish Timeout (Light OFF)</label></div></div>";

    html += "</div>";

    // --- LED Status Bar ---
    html += "<h2>LED Status Bar Settings</h2>";
    html += "<div><label for='numleds'>Number of WS2812B LEDs (Max 60)</label><input type='number' id='numleds' name='numleds' min='0' max='";
    html += String(MAX_LEDS);
    html += "' value='";
    html += String(config.num_leds);
    html += "'></div>";

    html += "<div><small>LED Data Pin is hardcoded to GPIO ";
    html += String(LED_PIN_CONST);
    html += " for FastLED.</small></div>";
    
    html += "<div><input type='checkbox' id='led_finish_timeout' name='led_finish_timeout' value='1' ";
    html += (config.led_finish_timeout ? "checked" : "");
    html += "><label for='led_finish_timeout'>Enable 2-Min Finish Timeout (LEDs return to Idle)</label></div>";

    // --- NEW: VIRTUAL LED PREVIEW ---
    html += "<h3>Virtual LED Preview</h3>";
    html += "<div class='card' style='padding: 20px; background-color: #2c2c2e; border: 1px solid #555; border-radius: 5px;'>"; // Darker card bg
    html += "<div id='virtual-bar-container'>";
    html += "<div id='virtual-bar' style='display: flex; width: 100%; height: 30px; background: #222; border-radius: 5px; overflow: hidden; border: 1px solid #555;'>";
    // Add 10 virtual LEDs
    for(int i=0; i<10; i++) {
      html += "<div class='vled' style='width: 10%; height: 100%;'></div>";
    }
    html += "</div></div>"; // end virtual-bar and container
    html += "<div id='preview-controls' style='margin-top: 15px; display: flex; flex-wrap: wrap; gap: 15px;'>";
    html += "<label style='display:inline-block; color:#e0e0e0;'><input type='radio' name='preview_state' value='idle' onchange='updatePreview()' checked> Idle</label>";
    html += "<label style='display:inline-block; color:#e0e0e0;'><input type='radio' name='preview_state' value='print' onchange='updatePreview()'> Printing</label>";
    html += "<label style='display:inline-block; color:#e0e0e0;'><input type='radio' name='preview_state' value='pause' onchange='updatePreview()'> Paused</label>";
    html += "<label style='display:inline-block; color:#e0e0e0;'><input type='radio' name='preview_state' value='error' onchange='updatePreview()'> Error</label>";
    html += "<label style='display:inline-block; color:#e0e0e0;'><input type='radio' name='preview_state' value='finish' onchange='updatePreview()'> Finish</label>";
    html += "</div></div>"; // end preview-controls and card
    // --- END NEW ---

    // --- LED Colors & Brightness ---
    html += "<h3>LED States</h3><div class='grid'>";
    html += "<div class='card'><h4>Idle Status</h4>";
    html += "<div><label for='idle_color'>Color (RRGGBB) <span id='idle_color_swatch' class='color-swatch'></span></label><input type='text' id='idle_color' name='idle_color' value='"; // <-- MODIFIED
    html += String(idle_color_hex);
    html += "' oninput='updatePreview(); try { document.getElementById(\"idle_color_picker\").value = \"#\" + this.value; } catch(e) {}'><input type='color' class='color-input' id='idle_color_picker' value='#";
    html += String(idle_color_hex);
    html += "' onchange='document.getElementById(\"idle_color\").value = this.value.substring(1).toUpperCase(); updatePreview();'></div>";
    
    html += "<div><label for='idle_bright'>Brightness (0-255)</label><input type='number' id='idle_bright' name='idle_bright' min='0' max='255' value='";
    html += String(config.led_bright_idle);
    html += "' oninput='updatePreview()'></div></div>"; // Added oninput


    html += "<div class='card'><h4>Printing Status</h4>";
    html += "<div><label for='print_color'>Color (RRGGBB) <span id='print_color_swatch' class='color-swatch'></span></label><input type='text' id='print_color' name='print_color' value='"; // <-- MODIFIED
    html += String(print_color_hex);
    html += "' oninput='updatePreview(); try { document.getElementById(\"print_color_picker\").value = \"#\" + this.value; } catch(e) {}'><input type='color' class='color-input' id='print_color_picker' value='#";
    html += String(print_color_hex);
    html += "' onchange='document.getElementById(\"print_color\").value = this.value.substring(1).toUpperCase(); updatePreview();'></div>";
    
    html += "<div><label for='print_bright'>Brightness (0-255)</label><input type='number' id='print_bright' name='print_bright' min='0' max='255' value='";
    html += String(config.led_bright_print);
    html += "' oninput='updatePreview()'></div></div>"; // Added oninput


    html += "<div class='card'><h4>Paused Status</h4>";
    html += "<div><label for='pause_color'>Color (RRGGBB) <span id='pause_color_swatch' class='color-swatch'></span></label><input type='text' id='pause_color' name='pause_color' value='"; // <-- MODIFIED
    html += String(pause_color_hex);
    html += "' oninput='updatePreview(); try { document.getElementById(\"pause_color_picker\").value = \"#\" + this.value; } catch(e) {}'><input type='color' class='color-input' id='pause_color_picker' value='#";
    html += String(pause_color_hex);
    html += "' onchange='document.getElementById(\"pause_color\").value = this.value.substring(1).toUpperCase(); updatePreview();'></div>";
    
    html += "<div><label for='pause_bright'>Brightness (0-255)</label><input type='number' id='pause_bright' name='pause_bright' min='0' max='255' value='";
    html += String(config.led_bright_pause);
    html += "' oninput='updatePreview()'></div></div>"; // Added oninput


    html += "<div class='card'><h4>Error Status</h4>";
    html += "<div><label for='error_color'>Color (RRGGBB) <span id='error_color_swatch' class='color-swatch'></span></label><input type='text' id='error_color' name='error_color' value='"; // <-- MODIFIED
    html += String(error_color_hex);
    html += "' oninput='updatePreview(); try { document.getElementById(\"error_color_picker\").value = \"#\" + this.value; } catch(e) {}'><input type='color' class='color-input' id='error_color_picker' value='#";
    html += String(error_color_hex);
    html += "' onchange='document.getElementById(\"error_color\").value = this.value.substring(1).toUpperCase(); updatePreview();'></div>";
    
    html += "<div><label for='error_bright'>Brightness (0-255)</label><input type='number' id='error_bright' name='error_bright' min='0' max='255' value='";
    html += String(config.led_bright_error);
    html += "' oninput='updatePreview()'></div></div>"; // Added oninput


    html += "<div class='card'><h4>Finish Status</h4>";
    html += "<div><label for='finish_color'>Color (RRGGBB) <span id='finish_color_swatch' class='color-swatch'></span></label><input type='text' id='finish_color' name='finish_color' value='"; // <-- MODIFIED
    html += String(finish_color_hex);
    html += "' oninput='updatePreview(); try { document.getElementById(\"finish_color_picker\").value = \"#\" + this.value; } catch(e) {}'><input type='color' class='color-input' id='finish_color_picker' value='#";
    html += String(finish_color_hex);
    html += "' onchange='document.getElementById(\"finish_color\").value = this.value.substring(1).toUpperCase(); updatePreview();'></div>";
    
    html += "<div><label for='finish_bright'>Brightness (0-255)</label><input type='number' id='finish_bright' name='finish_bright' min='0' max='255' value='";
    html += String(config.led_bright_finish);
    html += "' oninput='updatePreview()'></div></div>"; // Added oninput

    html += "</div>"; // End .grid

    // --- Submit Button ---
    html += "<br><div><button type='submit'>Save and Reboot</button></div>";
    html += "</form>";
    
    // --- NEW: Backup/Restore Buttons ---
    html += "<h2>Backup & Restore</h2>";
    html += "<div class='grid'>";
    html += "<div class='card'><p>Download a backup of your current settings.</p><a href='/backup'><button type='button' style='background-color:#17a2b8;'>Backup Configuration</button></a></div>";
    html += "<div class='card'><p>Upload a 'config.json' file to restore settings. <b>This will reboot the device.</b></p><a href='/restore'><button type='button' style='background-color:#dc3545;'>Restore Configuration</button></a></div>";
    html += "</div>";
    // --- END NEW ---

    html += "<br><p><a href='/'>&laquo; Back to Status Page</a></p>";

    // --- NEW: JAVASCRIPT FOR PREVIEW ---
    html += "<script>";
    html += "function updatePreview() {";
    html += "  try {";
    // Update selected state preview bar
    html += "    let state = document.querySelector('input[name=\"preview_state\"]:checked').value;";
    html += "    let color = document.getElementById(state + '_color').value;";
    html += "    if (!color.match(/^[0-9a-fA-F]{6}$/)) { color = '000000'; }"; // Basic hex validation
    html += "    let bright = parseInt(document.getElementById(state + '_bright').value, 10);";
    html += "    if (isNaN(bright) || bright < 0 || bright > 255) { bright = 0; }"; // Basic number validation
    html += "    let opacity = (bright / 255).toFixed(2);";
    html += "    let vleds = document.querySelectorAll('.vled');";
    html += "    vleds.forEach(led => {";
    html += "      led.style.backgroundColor = '#' + color;";
    html += "      led.style.opacity = opacity;";
    html += "    });";
    
    // --- NEW: Update all color swatches ---
    html += "    let states = ['idle', 'print', 'pause', 'error', 'finish'];";
    html += "    states.forEach(s => {";
    html += "      let c = document.getElementById(s + '_color').value;";
    html += "      if (!c.match(/^[0-9a-fA-F]{6}$/)) { c = '000000'; }";
    html += "      document.getElementById(s + '_color_swatch').style.backgroundColor = '#' + c;";
    html += "    });";
    // --- END NEW ---

    html += "  } catch (e) { console.error('Preview update failed:', e); }";
    html += "}";
    // Run on load to set initial state
    html += "document.addEventListener('DOMContentLoaded', updatePreview);";
    html += "</script>";
    // --- END NEW ---

    html += "</body></html>";

    server.send(200, "text/html", html);
  }
}
// --- END NEW handleConfig Function ---


// -----------------------------------------------------
// --- LED Control Function ---

void update_leds() {
  // Guard clause for safety if num_leds is invalid somehow
  if (config.num_leds <= 0 || config.num_leds > MAX_LEDS) {
     if(FastLED.getBrightness() != 0 || leds[0] != CRGB::Black) { // Avoid flicker if already off
        FastLED.clear();
        FastLED.show();
     }
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
  else if (current_gcode_state == "FINISH") {
    // Check for LED timeout logic
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
    // Printing (Progress Bar)
    FastLED.setBrightness(config.led_bright_print);
    int leds_to_light = map(current_print_percentage, 1, 100, 1, config.num_leds);
    leds_to_light = constrain(leds_to_light, 0, config.num_leds); // Ensure bounds

    // Optimization: Only update if state changes significantly
    // Check color of the last LED that should be lit
    CRGB targetColor = CRGB(config.led_color_print);
    if (leds_to_light > 0 && leds[leds_to_light - 1] != targetColor) {
        fill_solid(leds, leds_to_light, targetColor);
        fill_solid(leds + leds_to_light, config.num_leds - leds_to_light, CRGB::Black);
    } else if (leds_to_light == 0 && leds[0] != CRGB::Black) { // Case: 1% -> 0%
        fill_solid(leds, config.num_leds, CRGB::Black);
    }
  }
  else {
    // Idle state - only update if not already idle
    CRGB targetIdleColor = CRGB(config.led_color_idle);
     if (FastLED.getBrightness() != config.led_bright_idle || leds[0] != targetIdleColor ) {
        FastLED.setBrightness(config.led_bright_idle);
        fill_solid(leds, config.num_leds, targetIdleColor);
     }
  }

  FastLED.show();
}

// -----------------------------------------------------
// --- Configuration & Light Control Functions ---

// --- UPDATED: set_chamber_light_state ---
void set_chamber_light_state(bool lightShouldBeOn) {
  int pwm_value = 0; // Default to OFF
  if (lightShouldBeOn) {
    pwm_value = map(config.chamber_pwm_brightness, 0, 100, 0, 255);
  }
  int output_pwm = (config.invert_output) ? (255 - pwm_value) : pwm_value;
  // Ensure the value is within bounds (0-255)
  output_pwm = constrain(output_pwm, 0, 255);
  ledcWrite(config.chamber_light_pin, output_pwm); // <-- FIX: Use pin number
  external_light_is_on = lightShouldBeOn; // <-- NEW: Track state
}
// --- END UPDATED ---

// --- UPDATED: setup_chamber_light_pwm ---
void setup_chamber_light_pwm(int pin) {
    // Use the v3 API (which you confirmed is required)
    ledcAttach(pin, PWM_FREQ, PWM_RESOLUTION);

    // Set initial state (OFF)
    int off_value = config.invert_output ? 255 : 0;
    ledcWrite(pin, off_value); // Write directly to the pin
    external_light_is_on = false; // <-- NEW: Init state
    Serial.printf("PWM enabled on GPIO %d. OFF value: %d\n", pin, off_value);
}
// --- END UPDATED ---


bool saveConfig() {
  Serial.println("Saving configuration to LittleFS...");
  DynamicJsonDocument doc(2048);
  doc["bbl_ip"] = config.bbl_ip;
  doc["bbl_serial"] = config.bbl_serial;
  doc["bbl_access_code"] = config.bbl_access_code;
  doc["invert_output"] = config.invert_output;
  doc["chamber_light_pin"] = config.chamber_light_pin;
  doc["chamber_pwm_brightness"] = config.chamber_pwm_brightness;
  doc["chamber_light_finish_timeout"] = config.chamber_light_finish_timeout;

  doc["num_leds"] = config.num_leds;
  doc["led_color_idle"] = config.led_color_idle;
  doc["led_color_print"] = config.led_color_print;
  doc["led_color_pause"] = config.led_color_pause;
  doc["led_color_error"] = config.led_color_error;
  doc["led_color_finish"] = config.led_color_finish;
  doc["led_bright_idle"] = config.led_bright_idle;
  doc["led_bright_print"] = config.led_bright_print;
  doc["led_bright_pause"] = config.led_bright_pause;
  doc["led_bright_error"] = config.led_bright_error;
  doc["led_bright_finish"] = config.led_bright_finish;
  doc["led_finish_timeout"] = config.led_finish_timeout;
  
  doc["ntp_server"] = config.ntp_server; // <-- NEW
  doc["timezone"] = config.timezone; // <-- NEW

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
  if(!LittleFS.exists("/config.json")) {
      Serial.println("Config file not found.");
      return false;
  }

  File configFile = LittleFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file for reading.");
    return false;
  }

  size_t size = configFile.size();
  if (size == 0) {
      Serial.println("Config file is empty.");
      configFile.close();
      return false;
  }
  if (size > 2048) {
    Serial.println("Config file size is too large");
    configFile.close();
    return false;
  }

  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, configFile);
  configFile.close(); // Close file ASAP

  if (error) {
    Serial.print("Failed to parse config file: ");
    Serial.println(error.c_str());
    // Optionally delete the corrupted file
    // LittleFS.remove("/config.json");
    return false;
  }

  // Use temporary variables to avoid modifying config struct if parsing fails partially
  Config tempConfig = config; // Start with existing or default values

  strlcpy(tempConfig.bbl_ip, doc["bbl_ip"] | config.bbl_ip, sizeof(tempConfig.bbl_ip));
  strlcpy(tempConfig.bbl_serial, doc["bbl_serial"] | config.bbl_serial, sizeof(tempConfig.bbl_serial));
  strlcpy(tempConfig.bbl_access_code, doc["bbl_access_code"] | config.bbl_access_code, sizeof(tempConfig.bbl_access_code));
  tempConfig.invert_output = doc["invert_output"] | config.invert_output;
  tempConfig.chamber_light_pin = doc["chamber_light_pin"] | config.chamber_light_pin;
  tempConfig.chamber_pwm_brightness = doc["chamber_pwm_brightness"] | config.chamber_pwm_brightness;
  tempConfig.chamber_light_finish_timeout = doc["chamber_light_finish_timeout"] | config.chamber_light_finish_timeout;

  tempConfig.num_leds = doc["num_leds"] | config.num_leds;
  tempConfig.led_color_idle = doc["led_color_idle"] | config.led_color_idle;
  tempConfig.led_color_print = doc["led_color_print"] | config.led_color_print;
  tempConfig.led_color_pause = doc["led_color_pause"] | config.led_color_pause;
  tempConfig.led_color_error = doc["led_color_error"] | config.led_color_error;
  tempConfig.led_color_finish = doc["led_color_finish"] | config.led_color_finish;
  tempConfig.led_bright_idle = doc["led_bright_idle"] | config.led_bright_idle;
  tempConfig.led_bright_print = doc["led_bright_print"] | config.led_bright_print;
  tempConfig.led_bright_pause = doc["led_bright_pause"] | config.led_bright_pause;
  tempConfig.led_bright_error = doc["led_bright_error"] | config.led_bright_error;
  tempConfig.led_bright_finish = doc["led_bright_finish"] | config.led_bright_finish;
  tempConfig.led_finish_timeout = doc["led_finish_timeout"] | config.led_finish_timeout;

  // --- NEW: Load Time Settings ---
  strlcpy(tempConfig.ntp_server, doc["ntp_server"] | "pool.ntp.org", sizeof(tempConfig.ntp_server));
  strlcpy(tempConfig.timezone, doc["timezone"] | "GMT0BST,M3.5.0/1,M10.5.0", sizeof(tempConfig.timezone));
  // --- END NEW ---

  // Validate loaded values before applying
  if (!isValidGpioPin(tempConfig.chamber_light_pin)) {
      Serial.printf("WARNING: Loaded invalid chamber light pin (%d). Using default (%d).\n", tempConfig.chamber_light_pin, DEFAULT_CHAMBER_LIGHT_PIN);
      tempConfig.chamber_light_pin = DEFAULT_CHAMBER_LIGHT_PIN;
  }
   if (tempConfig.num_leds < 0 || tempConfig.num_leds > MAX_LEDS) {
       Serial.printf("WARNING: Loaded invalid number of LEDs (%d). Using default (%d).\n", tempConfig.num_leds, DEFAULT_NUM_LEDS);
       tempConfig.num_leds = DEFAULT_NUM_LEDS;
   }
   // Add checks for brightness/color ranges if necessary

   // Apply the loaded and validated config
   config = tempConfig;


  Serial.println("Configuration loaded successfully.");
  return true;
}

// Updated callback to handle reading from parameter objects
void saveConfigCallback() {
  Serial.println("WiFiManager signaled configuration save.");

  // 1. Copy new values from custom parameter objects using getValue()
  Config tempConfig = config; // Work on a temporary copy

  strlcpy(tempConfig.bbl_ip, custom_bbl_ip.getValue(), sizeof(tempConfig.bbl_ip));
  strlcpy(tempConfig.bbl_serial, custom_bbl_serial.getValue(), sizeof(tempConfig.bbl_serial));
  strlcpy(tempConfig.bbl_access_code, custom_bbl_access_code.getValue(), sizeof(tempConfig.bbl_access_code));
  tempConfig.invert_output = (strcmp(custom_bbl_invert.getValue(), "1") == 0);

  int tempLightPin = atoi(custom_bbl_pin.getValue());
  if (isValidGpioPin(tempLightPin)) {
      tempConfig.chamber_light_pin = tempLightPin;
  } else {
      Serial.print("ERROR: Light Pin ");
      Serial.print(tempLightPin);
      Serial.println(" is invalid or unsafe. Retaining previous pin.");
      // Keep existing config.chamber_light_pin by not updating tempConfig
  }

  tempConfig.chamber_pwm_brightness = atoi(custom_chamber_bright.getValue());
  tempConfig.chamber_light_finish_timeout = (strcmp(custom_chamber_finish_timeout.getValue(), "1") == 0);

  int tempNumLeds = atoi(custom_num_leds.getValue());
  if (tempNumLeds >= 0 && tempNumLeds <= MAX_LEDS) { // Allow 0 LEDs
      tempConfig.num_leds = tempNumLeds;
  } else {
      Serial.println("WARNING: Invalid LED count entered. Disabling LEDs (setting to 0).");
      tempConfig.num_leds = 0;
  }

  // Use strtoul for robust hex conversion
  tempConfig.led_color_idle = strtoul(custom_idle_color.getValue(), NULL, 16);
  tempConfig.led_color_print = strtoul(custom_print_color.getValue(), NULL, 16);
  tempConfig.led_color_pause = strtoul(custom_pause_color.getValue(), NULL, 16);
  tempConfig.led_color_error = strtoul(custom_error_color.getValue(), NULL, 16);
  tempConfig.led_color_finish = strtoul(custom_finish_color.getValue(), NULL, 16);

  // Use atoi and constrain for brightness
  tempConfig.led_bright_idle = constrain(atoi(custom_idle_bright.getValue()), 0, 255);
  tempConfig.led_bright_print = constrain(atoi(custom_print_bright.getValue()), 0, 255);
  tempConfig.led_bright_pause = constrain(atoi(custom_pause_bright.getValue()), 0, 255);
  tempConfig.led_bright_error = constrain(atoi(custom_error_bright.getValue()), 0, 255);
  tempConfig.led_bright_finish = constrain(atoi(custom_finish_bright.getValue()), 0, 255);
  tempConfig.led_finish_timeout = (strcmp(custom_led_finish_timeout.getValue(), "1") == 0);

  // --- NEW: Save Time Settings ---
  strlcpy(tempConfig.ntp_server, custom_ntp_server.getValue(), sizeof(tempConfig.ntp_server));
  strlcpy(tempConfig.timezone, custom_timezone.getValue(), sizeof(tempConfig.timezone));
  // --- END NEW ---

  // Apply the changes to the global config struct
  config = tempConfig;

  // 2. Save the updated config struct to LittleFS
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

// --- UPDATED reconnect_mqtt Function ---
bool reconnect_mqtt() {
  Serial.print("Attempting MQTT connection...");

  // Ensure WiFi is connected before attempting MQTT
  if(WiFi.status() != WL_CONNECTED){
      Serial.println("WiFi disconnected, cannot connect MQTT.");
      return false;
  }

  // --- NEW: Set client to insecure for MQTTS ---
  // Bambu printers use MQTTS (SSL) on port 8883.
  // We must skip certificate validation for a local LAN connection.
  espClient.setInsecure();
  // --- END NEW ---

  // Create a unique client ID from the MAC address
  String macAddress = WiFi.macAddress();
  macAddress.replace(":", "");
  String clientId = "BambuLight-" + macAddress;

  Serial.print(" (Client ID: ");
  Serial.print(clientId);
  Serial.print(")...");

  if (client.connect(clientId.c_str(), mqtt_user, config.bbl_access_code)) {
    Serial.println("connected");
    // Resubscribe upon reconnect
    if(client.subscribe(mqtt_topic_status.c_str())){
         Serial.print("Resubscribed to: ");
         Serial.println(mqtt_topic_status);
    } else {
         Serial.println("Resubscribe failed!");
    }
    return true;
  } else {
    Serial.print("failed, rc=");
    Serial.print(client.state());
    Serial.println("");
    // Possible MQTT error codes:
    // -4 : MQTT_CONNECTION_TIMEOUT - server didn't respond within وقت معين
    // -3 : MQTT_CONNECTION_LOST - the network connection was broken
    // -2 : MQTT_CONNECT_FAILED - the network connection failed
    // -1 : MQTT_DISCONNECTED - the client is disconnected cleanly
    //  1 : MQTT_CONNECT_BAD_PROTOCOL - the server doesn't support the requested version of MQTT
    //  2 : MQTT_CONNECT_BAD_CLIENT_ID - the server rejected the client identifier
    //  3 : MQTT_CONNECT_UNAVAILABLE - the server was unable to accept the connection
    //  4 : MQTT_CONNECT_BAD_CREDENTIALS - the username/password were rejected
    //  5 : MQTT_CONNECT_UNAUTHORIZED - the client was not authorized to connect
    return false;
  }
}
// --- END UPDATED reconnect_mqtt Function ---

// --- NEW: Main callback, now routes to helper functions ---
void callback(char* topic, byte* payload, unsigned int length) {

  // Create a buffer with guaranteed null termination
  char messageBuffer[length + 1];
  memcpy(messageBuffer, payload, length);
  messageBuffer[length] = '\0';

  // --- NEW: Store in history with timestamp ---
  String log_entry = getTimestamp() + " " + messageBuffer;
  mqtt_history.push_back(log_entry);
  if(mqtt_history.size() > MAX_HISTORY_SIZE) {
    mqtt_history.pop_front(); // Remove the oldest message
  }
  // --- END NEW ---

  // Increase JSON doc size slightly for safety margin
  DynamicJsonDocument doc(JSON_DOC_SIZE + 256);
  DeserializationError error = deserializeJson(doc, messageBuffer);

  if (error) {
    Serial.print("MQTT JSON Parse Error: ");
    Serial.println(error.c_str());
    return;
  }

  // --- NEW: Check JSON message type ---
  if (doc.is<JsonObject>()) {
      // This is likely the full "report"
      parseFullReport(doc.as<JsonObject>());
  } else if (doc.is<JsonArray>()) {
      // This is likely a delta update (like the light status)
      parseDeltaUpdate(doc.as<JsonArray>());
  } else {
      Serial.println("Received unknown JSON type.");
  }
}
// --- END NEW callback ---

// --- UPDATED: Function to parse the full status report ---
void parseFullReport(JsonObject doc) {
  // Serial.println("Parsing full report..."); // Optional: for debugging
  
  JsonObject data; // Create a new JsonObject to hold the data we care about
  JsonObject system_data; // Will hold the "system" object
  JsonObject print_data; // Will hold the "print" object

  if (doc.containsKey("report")) {
      // This is the full status report (Type 1)
      data = doc["report"];
      system_data = data["system"];
      print_data = data["print"];
  } else if (doc.containsKey("print")) {
      // This is the "push" update (Type 3)
      // The data is the doc itself.
      data = doc;
      system_data = data["system"]; // This will be null, which is fine
      print_data = data["print"];
  } else {
      // This is an unknown JSON object
      Serial.println("Received unknown JSON object.");
      return;
  }

  // --- From here on, we use the 'data' object ---
  // Check if the 'data' object is valid
  if(data.isNull()) {
      Serial.println("MQTT JSON Error: 'data' object is null.");
      return;
  }
  
  // We must have at least the "print" object
  if (print_data.isNull()) {
      Serial.println("MQTT JSON Error: 'print' object is null.");
      return;
  }

  // --- Extract all values ---
  
  // Default values (in case system or chamber_light is missing in a "push" update)
  const char* newChamberLightMode = current_light_mode.c_str(); // Use current state as default
  const char* newGcodeState = current_gcode_state.c_str();
  int newPrintPercentage = current_print_percentage;
  float newBedTemp = current_bed_temp;
  float newNozzleTemp = current_nozzle_temp;
  float newBedTargetTemp = current_bed_target_temp;
  float newNozzleTargetTemp = current_nozzle_target_temp;
  int newTimeRemaining = current_time_remaining;
  int newLayerNum = current_layer;
  const char* newWifiSignal = current_wifi_signal.c_str();
  bool lightModeFound = false;
  int newPrintSubStage = -1; // <-- NEW

  // --- Get Light Mode (NEW LOGIC) ---
  // 1. Check for "lights_report" array inside "print" (for Type 3 messages)
  if (!print_data.isNull() && print_data.containsKey("lights_report")) {
      JsonArray lightsReport = print_data["lights_report"].as<JsonArray>();
      if (!lightsReport.isNull()) {
          for (JsonObject node : lightsReport) {
              if (node.isNull()) continue;
              const char* nodeName = node["node"];
              if (nodeName && strcmp(nodeName, "chamber_light") == 0) {
                  newChamberLightMode = node["mode"] | current_light_mode.c_str();
                  lightModeFound = true;
                  Serial.println("Found light_mode in lights_report array.");
                  break; // Found it
              }
          }
      }
  }

  // 2. Fallback: Check for "chamber_light" object inside "system" (for Type 1 messages)
  if (!lightModeFound && !system_data.isNull()) {
      JsonObject chamber_light = system_data["chamber_light"];
      if (!chamber_light.isNull()) {
          newChamberLightMode = chamber_light["led_mode"] | current_light_mode.c_str();
          lightModeFound = true;
          // Serial.println("Found light_mode in system.chamber_light object."); // Optional debug
      }
  }
  // --- End Light Mode Logic ---


  // Get print data (we already know 'print' is not null)
  newGcodeState = print_data["gcode_state"] | current_gcode_state.c_str();
  
  // --- NEW: Check for both percentage keys ---
  if (print_data.containsKey("print_percentage")) {
    newPrintPercentage = print_data["print_percentage"] | current_print_percentage;
  } else if (print_data.containsKey("mc_percent")) {
    newPrintPercentage = print_data["mc_percent"] | current_print_percentage;
  }
  
  newBedTemp = print_data["bed_temper"] | current_bed_temp;
  newNozzleTemp = print_data["nozzle_temper"] | current_nozzle_temp;
  newBedTargetTemp = print_data["bed_target_temper"] | current_bed_target_temp;
  newNozzleTargetTemp = print_data["nozzle_target_temper"] | current_nozzle_target_temp;
  newTimeRemaining = print_data["mc_remaining_time"] | current_time_remaining;
  newLayerNum = print_data["layer_num"] | current_layer;
  newPrintSubStage = print_data["mc_print_sub_stage"] | -1; // <-- NEW

  // Get WiFi signal (can be in system or print)
  if (!system_data.isNull() && system_data.containsKey("wifi_signal")) {
      newWifiSignal = system_data["wifi_signal"] | current_wifi_signal.c_str();
  } else if (print_data.containsKey("wifi_signal")) { // As seen in "push" message
      newWifiSignal = print_data["wifi_signal"] | current_wifi_signal.c_str();
  }

  // --- NEW: Check for print sub-stage (which also turns on the light) ---
  if (newPrintSubStage == 1 && !lightModeFound) {
      newChamberLightMode = "on";
      lightModeFound = true; // Mark it as found
      Serial.println("Inferred light 'on' from mc_print_sub_stage: 1");
  }
  // --- End New ---

  // --- Call the state updater ---
  updatePrinterState(String(newGcodeState), newPrintPercentage, String(newChamberLightMode), newBedTemp, newNozzleTemp, String(newWifiSignal), newBedTargetTemp, newNozzleTargetTemp, newTimeRemaining, newLayerNum);
}
// --- END UPDATED function ---

// --- UPDATED: Function to parse the delta update array ---
void parseDeltaUpdate(JsonArray arr) {
  // Serial.println("Parsing delta update..."); // Optional: for debugging
  
  // Create temp variables to hold delta values
  String newGcodeState = current_gcode_state;
  int newPrintPercentage = current_print_percentage;
  String newChamberLightMode = current_light_mode;
  float newBedTemp = current_bed_temp;
  float newNozzleTemp = current_nozzle_temp;
  float newBedTargetTemp = current_bed_target_temp;
  float newNozzleTargetTemp = current_nozzle_target_temp;
  int newTimeRemaining = current_time_remaining;
  int newLayerNum = current_layer;
  String newWifiSignal = current_wifi_signal;

  // Iterate the array (can contain multiple updates)
  for (JsonObject node : arr) {
      if (node.isNull()) continue;

      const char* nodeName = node["node"];
      if (nodeName == nullptr) continue; // Not the format we expect

      // --- Found the chamber light! ---
      if (strcmp(nodeName, "chamber_light") == 0) {
          newChamberLightMode = node["mode"] | current_light_mode.c_str();
          Serial.print("Received chamber_light delta update. New mode: ");
          Serial.println(newChamberLightMode);
      }
      // --- Found bed temp! ---
      else if (strcmp(nodeName, "bed_temper") == 0) {
          newBedTemp = node["value"] | current_bed_temp;
          Serial.print("Received bed_temper delta update. New temp: ");
          Serial.println(newBedTemp);
      }
      // --- Found nozzle temp! ---
      else if (strcmp(nodeName, "nozzle_temper") == 0) {
          newNozzleTemp = node["value"] | current_nozzle_temp;
          Serial.print("Received nozzle_temper delta update. New temp: ");
          Serial.println(newNozzleTemp);
      }
      // --- Found bed target temp! ---
      else if (strcmp(nodeName, "bed_target_temper") == 0) {
          newBedTargetTemp = node["value"] | current_bed_target_temp;
          Serial.print("Received bed_target_temper delta update. New temp: ");
          Serial.println(newBedTargetTemp);
      }
      // --- Found nozzle target temp! ---
      else if (strcmp(nodeName, "nozzle_target_temper") == 0) {
          newNozzleTargetTemp = node["value"] | current_nozzle_target_temp;
          Serial.print("Received nozzle_target_temper delta update. New temp: ");
          Serial.println(newNozzleTargetTemp);
      }
      // --- Found gcode_state! ---
      else if (strcmp(nodeName, "gcode_state") == 0) {
          newGcodeState = node["value"] | current_gcode_state.c_str();
          Serial.print("Received gcode_state delta update. New state: ");
          Serial.println(newGcodeState);
      }
      // --- Found print_percentage! ---
      else if (strcmp(nodeName, "print_percentage") == 0) {
          newPrintPercentage = node["value"] | current_print_percentage;
          Serial.print("Received print_percentage delta update. New value: ");
          Serial.println(newPrintPercentage);
      }
      // --- Found mc_percent! ---
      else if (strcmp(nodeName, "mc_percent") == 0) {
          newPrintPercentage = node["value"] | current_print_percentage;
          Serial.print("Received mc_percent delta update. New value: ");
          Serial.println(newPrintPercentage);
      }
      // --- Found layer_num! ---
      else if (strcmp(nodeName, "layer_num") == 0) {
          newLayerNum = node["value"] | current_layer;
          Serial.print("Received layer_num delta update. New value: ");
          Serial.println(newLayerNum);
      }
      // --- Found time_remaining! ---
      else if (strcmp(nodeName, "mc_remaining_time") == 0) {
          newTimeRemaining = node["value"] | current_time_remaining;
          Serial.print("Received mc_remaining_time delta update. New value: ");
          Serial.println(newTimeRemaining);
      }
      // --- Found wifi_signal! ---
      else if (strcmp(nodeName, "wifi_signal") == 0) {
          newWifiSignal = node["value"] | current_wifi_signal.c_str();
          Serial.print("Received wifi_signal delta update. New value: ");
          Serial.println(newWifiSignal);
      }
      // --- Found print_sub_stage! ---
      else if (strcmp(nodeName, "mc_print_sub_stage") == 0) {
          int subStage = node["value"] | -1;
          if (subStage == 1) {
              newChamberLightMode = "on"; // Force light mode to 'on'
              Serial.println("Inferred light 'on' from mc_print_sub_stage delta update");
          }
      }
  }

  // --- Call the state updater ---
  // This function is now called *after* the loop, so it batches all delta changes
  updatePrinterState(newGcodeState, newPrintPercentage, newChamberLightMode, newBedTemp, newNozzleTemp, newWifiSignal, newBedTargetTemp, newNozzleTargetTemp, newTimeRemaining, newLayerNum);
}
// --- END UPDATED function ---


// --- UPDATED: Centralized function to update state and lights ---
void updatePrinterState(String gcodeState, int printPercentage, String chamberLightMode, float bedTemp, float nozzleTemp, String wifiSignal, float bedTargetTemp, float nozzleTargetTemp, int timeRemaining, int layerNum) { // <-- UPDATED

  // --- 1. Manage Finish Timer ---
  // Check for state *change* to FINISH
  if (gcodeState == "FINISH" && current_gcode_state != "FINISH") {
    finishTime = millis();
    Serial.println("Print finished, starting 2-minute timers.");
  } 
  // DO NOT reset finishTime here, the loop() timer will handle it

  // --- 2. Update global states ---
  current_gcode_state = gcodeState;
  current_print_percentage = printPercentage;
  current_light_mode = chamberLightMode;
  current_bed_temp = bedTemp;
  current_nozzle_temp = nozzleTemp;
  current_bed_target_temp = bedTargetTemp;
  current_nozzle_target_temp = nozzleTargetTemp;
  current_time_remaining = timeRemaining;
  current_layer = layerNum;
  current_wifi_signal = wifiSignal;
  current_error_state = (gcodeState == "FAILED" || gcodeState == "STOP");

  // --- 3. External Light Auto Logic ---
  if (!manual_light_control) {
    bool lightShouldBeOnBasedOnPrinter = (chamberLightMode == "on" || chamberLightMode == "flashing");
    bool finalLightState = lightShouldBeOnBasedOnPrinter; // Default to following printer

    // --- UPDATED TIMER CHECK ---
    // Check for timer override
    if (config.chamber_light_finish_timeout && finishTime > 0) {
        if (millis() - finishTime < FINISH_LIGHT_TIMEOUT) {
            // Timer is active, force light ON
            finalLightState = true;
        } else {
            // Timer has expired, force light OFF
            finalLightState = false;
            // The loop() will also catch this, but this is faster
        }
    }
    // --- END UPDATED ---
    
    // Only update if the calculated state is different from the current state
    if (finalLightState != external_light_is_on) { 
       set_chamber_light_state(finalLightState);
    }
  }

  // --- 4. LED Status Bar Control ---
  update_leds();
}
// --- END UPDATED function ---


// --- NEW: Helper function to build the timezone dropdown ---
String getTimezoneDropdown(String selectedTz) {
  String html = "<select id='timezone' name='timezone'>";
  
  // Helper to add <option>
  auto addOption = [&](const char* tz, const char* name) {
    html += "<option value='";
    html += tz;
    html += "'";
    if (selectedTz == tz) {
      html += " selected";
    }
    html += ">";
    html += name;
    html += "</option>";
  };

  // Add a selection of common timezones
  // (TZ format: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv)
  addOption("GMT0BST,M3.5.0/1,M10.5.0", "(GMT/BST) London, Dublin");
  addOption("CET-1CEST,M3.5.0,M10.5.0/3", "(CET/CEST) Berlin, Paris, Rome");
  addOption("EET-2EEST,M3.5.0/3,M10.5.0/4", "(EET/EEST) Athens, Helsinki, Kyiv");
  html += "<option disabled>--- Americas ---</option>";
  addOption("EST5EDT,M3.2.0,M11.1.0", "(EST/EDT) New York, Toronto");
  addOption("CST6CDT,M3.2.0,M11.1.0", "(CST/CDT) Chicago, Mexico City");
  addOption("MST7MDT,M3.2.0,M11.1.0", "(MST/MDT) Denver");
  addOption("PST8PDT,M3.2.0,M11.1.0", "(PST/PDT) Los Angeles, Vancouver");
  addOption("AST4ADT,M3.2.0,M11.1.0", "(AST/ADT) Halifax (Atlantic)");
  addOption("AKST9AKDT,M3.2.0,M11.1.0", "(AKST/AKDT) Alaska");
  addOption("HST10", "(HST) Hawaii (No DST)");
  html += "<option disabled>--- Asia/Pacific ---</option>";
  addOption("JST-9", "(JST) Tokyo, Seoul (No DST)");
  addOption("CST-8", "(CST) Beijing, Perth, Singapore (No DST)");
  addOption("AEST-10AEDT,M10.1.0,M4.1.0/3", "(AEST/AEDT) Sydney, Melbourne");
  addOption("ACST-9:30ACDT,M10.1.0,M4.1.0/3", "(ACST/ACDT) Adelaide");
  addOption("AWST-8", "(AWST) Perth (No DST)");
  addOption("NZST-12NZDT,M9.5.0,M4.1.0/3", "(NZST/NZDT) New Zealand");
  addOption("IST-5:30", "(IST) India (No DST)");
  html += "<option disabled>--- Other ---</option>";
  addOption("UTC0", "(UTC) Coordinated Universal Time");
  
  // Add the currently saved one if it's not in the list (custom)
  if (html.indexOf("selected") == -1) {
     addOption(selectedTz.c_str(), "(Custom)");
  }

  html += "</select>";
  return html;
}
