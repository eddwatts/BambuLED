#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include "FS.h"
#include "LittleFS.h"
#include <FastLED.h>
#include <driver/ledc.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <deque>
#include <time.h>

// Include all our module headers
#include "config.h"
#include "mqtt_handler.h"
#include "web_handlers.h"
#include "led_controller.h"
#include "light_controller.h"
#include "ota_handler.h"
// #include "utils.h" // This file is obsolete

// Global instances
WebServer server(80);
WiFiManager wm;
WiFiClientSecure espClient;
PubSubClient client(espClient);

// Global state variables
String current_light_mode = "UNKNOWN";
bool manual_light_control = false;
bool external_light_is_on = false;
int current_print_percentage = 0;
bool current_error_state = false;
String current_gcode_state = "IDLE";
float current_bed_temp = 0.0;
float current_nozzle_temp = 0.0;
float current_bed_target_temp = 0.0;
float current_nozzle_target_temp = 0.0;
int current_time_remaining = 0;
int current_layer = 0;
int current_stage = -1;
String current_wifi_signal = "N/A";
unsigned long finishTime = 0;
const unsigned long FINISH_LIGHT_TIMEOUT = 120000;

// --- FIX ---
// Added the definition for the MQTT topic string
String mqtt_topic_status;
// --- END FIX ---

// Non-blocking reconnect timer
unsigned long lastReconnectAttempt = 0;
const unsigned long RECONNECT_INTERVAL = 5000;

// File upload state
File restoreFile;
bool restoreSuccess = false;

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n\nBooting Bambu Light Controller...");
  yield();
  // Check for factory reset
  bool forceReset = checkFactoryReset();
  // Initialize file system
  if (!initFileSystem()) {
    Serial.println("LITTLEFS MOUNT FAILED! Restarting...");
    delay(2000);
    ESP.restart();
  }

  // Add initial boot message to log
  mqtt_history.push_back("[--:--:--] System Booted. Initializing...");
  // Handle factory reset
  if (forceReset) {
    performFactoryReset();
  }

  // Load configuration
  if (!loadConfig()) {
    Serial.println("No saved configuration found. Using hardcoded defaults.");
    setupDefaultConfig();
  }

  // Apply temporary fixes for invalid config
  applyConfigFixes();

  // Print loaded config
  printConfig();
  // Initialize hardware
  initChamberLight();
  initLEDStrip();

  // Setup WiFiManager parameters
  setupWiFiManagerParams();
  // Set lower WiFi power
  Serial.println("Setting WiFi Tx Power to 11dBm to reduce current spike...");
  WiFi.setTxPower(WIFI_POWER_11dBm);
  // Connect to WiFi
  if (!connectWiFi(forceReset)) {
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
  // Configure time
  configureTime();

  // Re-initialize pins if changed in portal
  reinitHardwareIfNeeded();

  // Setup OTA
  setupOTA();
  // Setup MQTT
  setupMQTT();

  // Setup web server
  setupWebServer();

  Serial.println("--- Setup Complete ---");
}

void loop() {
  // Handle OTA updates
  ArduinoOTA.handle();

  // Handle web client requests
  server.handleClient();
  // Handle MQTT connection
  handleMQTTConnection();

  // Handle finish timers
  handleFinishTimers();
}
