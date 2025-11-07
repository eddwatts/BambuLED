#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <deque>
#include "config.h" 

// External declarations from main file
extern PubSubClient client;
extern WiFiClientSecure espClient; 
extern String mqtt_topic_status;
extern String current_light_mode;
extern bool manual_light_control;
extern bool external_light_is_on;
extern int current_print_percentage;
extern bool current_error_state;
extern String current_gcode_state;
extern float current_bed_temp;
extern float current_nozzle_temp;
extern float current_bed_target_temp;
extern float current_nozzle_target_temp;
extern int current_time_remaining;
extern int current_layer;
extern int current_stage;
extern String current_wifi_signal;
extern unsigned long finishTime;
extern const unsigned long FINISH_LIGHT_TIMEOUT;
extern unsigned long lastReconnectAttempt;
extern const unsigned long RECONNECT_INTERVAL;
extern Config config; 

// --- FIX for Highlighted Log ---
// 1. Define a struct to hold the log message and its state
struct MqttLogEntry {
  String message;
  bool highlight;
};

// 2. Change the deque to use this new struct
extern std::deque<MqttLogEntry> mqtt_history;

// 3. Set the new history size
const int MAX_HISTORY_SIZE = 500;  // Increased for 8MB RAM
// --- END FIX ---


// Function declarations
void setupMQTT();
void setupMQTTParams();
bool reconnectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void parseFullReport(JsonObject doc);
void parseDeltaUpdate(JsonArray arr);
void updatePrinterState(String gcodeState, int printPercentage, String chamberLightMode, 
                       float bedTemp, float nozzleTemp, String wifiSignal, 
                       float bedTargetTemp, float nozzleTargetTemp, 
                       int timeRemaining, int layerNum, int stage);
void handleMQTTConnection();

#endif