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
#include <WebSocketsServer.h>

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
WebSocketsServer webSocket = WebSocketsServer(81);

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
String mqtt_topic_status;
unsigned long lastAnimationUpdate = 0;

// Non-blocking reconnect timer
unsigned long lastReconnectAttempt = 0;
const unsigned long RECONNECT_INTERVAL = 5000;

// File upload state
File restoreFile;
bool restoreSuccess = false;

void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] WebSocket Disconnected!\n", num);
      break;
    case WStype_CONNECTED: {
      IPAddress ip = webSocket.remoteIP(num);
      Serial.printf("[%u] WebSocket Connected from %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);
      
      // When a new client connects, immediately send them the *current* status!
      // This requires `createStatusJson` to be defined in `web_handlers.h`
      DynamicJsonDocument doc(1024);
      createStatusJson(doc); 
      String json_output;
      serializeJson(doc, json_output);
      webSocket.sendTXT(num, json_output);
      break;
    }
    case WStype_TEXT:
      Serial.printf("[%u] got text: %s\n", num, payload);
      // Example: You could add logic here to control the light *from* the websocket
      if (strcmp((char*)payload, "LIGHT_ON") == 0) {
        Serial.println("WebSocket received LIGHT_ON command");
        handleLightOn(); // Reuse existing handler
      } else if (strcmp((char*)payload, "LIGHT_OFF") == 0) {
        Serial.println("WebSocket received LIGHT_OFF command");
        handleLightOff();
      } else if (strcmp((char*)payload, "LIGHT_AUTO") == 0) {
        Serial.println("WebSocket received LIGHT_AUTO command");
        handleLightAuto();
      }
      break;
  }
}

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

  
  server.on("/reboot", [](){
    server.send(200, "text/html", "<!DOCTYPE html><html><head><title>Rebooting...</title><meta http-equiv='refresh' content='5;url=/'></head><body style='font-family:sans-serif; background:#1a1a1b; color:#e0e0e0;'><h2>Rebooting...</h2><p>You will be redirected in 5 seconds.</p></body></html>");
    delay(1000);
    ESP.restart();
  });
  
  server.on("/factory_reset", [](){
    server.send(200, "text/html", "<!DOCTYPE html><html><head><title>Resetting...</title><meta http-equiv='refresh' content='10;url=/'></head><body style='font-family:sans-serif; background:#1a1a1b; color:#e0e0e0;'><h2>Factory Reset...</h2><p>Wiping config and rebooting. The device will restart in AP mode. You will be redirected in 10 seconds.</p></body></html>");
    performFactoryReset();
    delay(1000);
    wm.resetSettings(); // Also reset WiFi
    delay(1000);
    ESP.restart();
  });

  
  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);

  Serial.println("--- Setup Complete ---");
}

void loop() {
  // Handle OTA updates
  ArduinoOTA.handle();

  // Handle web client requests
  server.handleClient();
  
  webSocket.loop(); 
  
  // Handle MQTT connection
  handleMQTTConnection();

  // Handle finish timers
  handleFinishTimers();
}
