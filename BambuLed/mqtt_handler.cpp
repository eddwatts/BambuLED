#include "mqtt_handler.h"
#include "config.h"
#include "light_controller.h"
#include "led_controller.h"
#include <WiFi.h> // <-- ADDED for WiFi.status()

// MQTT History definition
 std::deque<String> mqtt_history;


void setupMQTT() {
  Serial.println("Setting up MQTT...");
  setupMQTTParams();
  client.setCallback(mqttCallback);
  Serial.println("MQTT OK.");
}

void setupMQTTParams() {
  client.setServer(config.bbl_ip, 8883);
  mqtt_topic_status = "device/" + String(config.bbl_serial) + "/report";
  Serial.print("MQTT Server: "); Serial.println(config.bbl_ip);
  Serial.print("Subscription Topic: "); Serial.println(mqtt_topic_status);
}

bool reconnectMQTT() {
  Serial.print("Attempting MQTT connection...");

  if(WiFi.status() != WL_CONNECTED){
      Serial.println("WiFi disconnected, cannot connect MQTT.");
      return false;
  }

  espClient.setInsecure();

  String macAddress = WiFi.macAddress();
  macAddress.replace(":", "");
  String clientId = "BambuLight-" + macAddress;

  Serial.print(" (Client ID: ");
  Serial.print(clientId);
  Serial.print(")...");

  if (client.connect(clientId.c_str(), "bblp", config.bbl_access_code)) {
    Serial.println("connected");
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
    return false;
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char messageBuffer[length + 1];
  memcpy(messageBuffer, payload, length);
  messageBuffer[length] = '\0';

  String log_entry = getTimestamp() + " " + messageBuffer;
  mqtt_history.push_back(log_entry);
  if(mqtt_history.size() > MAX_HISTORY_SIZE) {
    mqtt_history.pop_front();
  }

  DynamicJsonDocument doc(JSON_DOC_SIZE + 256);
  DeserializationError error = deserializeJson(doc, messageBuffer);

  if (error) {
    Serial.print("MQTT JSON Parse Error: ");
    Serial.println(error.c_str());
    return;
  }

  if (doc.is<JsonObject>()) {
      parseFullReport(doc.as<JsonObject>());
  } else if (doc.is<JsonArray>()) {
      parseDeltaUpdate(doc.as<JsonArray>());
  } else {
      Serial.println("Received unknown JSON type.");
  }
}

void parseFullReport(JsonObject doc) {
  JsonObject data;
  JsonObject system_data;
  JsonObject print_data;

  if (doc.containsKey("report")) {
      data = doc["report"];
      system_data = data["system"];
      print_data = data["print"];
  } else if (doc.containsKey("print")) {
      data = doc;
      system_data = data["system"];
      print_data = data["print"];
  } else {
      Serial.println("Received unknown JSON object.");
      return;
  }

  if(data.isNull()) {
      Serial.println("MQTT JSON Error: 'data' object is null.");
      return;
  }
  
  if (print_data.isNull()) {
      Serial.println("MQTT JSON Error: 'print' object is null.");
      return;
  }

  const char* newChamberLightMode = current_light_mode.c_str();
  const char* newGcodeState = current_gcode_state.c_str();
  int newPrintPercentage = current_print_percentage;
  float newBedTemp = current_bed_temp;
  float newNozzleTemp = current_nozzle_temp;
  float newBedTargetTemp = current_bed_target_temp;
  float newNozzleTargetTemp = current_nozzle_target_temp;
  int newTimeRemaining = current_time_remaining;
  int newLayerNum = current_layer;
  int newPrintStage = current_stage;
  const char* newWifiSignal = current_wifi_signal.c_str();
  bool lightModeFound = false;
  bool gcodeStateFound = false;

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
                  break;
              }
          }
      }
  }

  if (!lightModeFound && !system_data.isNull()) {
      JsonObject chamber_light = system_data["chamber_light"];
      if (!chamber_light.isNull()) {
          newChamberLightMode = chamber_light["led_mode"] | current_light_mode.c_str();
          lightModeFound = true;
      }
  }

  if (print_data.containsKey("gcode_state")) {
      newGcodeState = print_data["gcode_state"] | current_gcode_state.c_str();
      gcodeStateFound = true;
  }
  
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
  newPrintStage = print_data["stg_cur"] | current_stage;

  if (!system_data.isNull() && system_data.containsKey("wifi_signal")) {
      newWifiSignal = system_data["wifi_signal"] | current_wifi_signal.c_str();
  } else if (print_data.containsKey("wifi_signal")) {
      newWifiSignal = print_data["wifi_signal"] | current_wifi_signal.c_str();
  }

  int subStage = print_data["mc_print_sub_stage"] | -1;
  if (subStage == 1 && !lightModeFound) {
      newChamberLightMode = "on";
      lightModeFound = true;
      Serial.println("Inferred light 'on' from mc_print_sub_stage: 1");
  }

  if (!gcodeStateFound && (newPrintPercentage > 0 || newLayerNum > 0) && strcmp(newGcodeState, "IDLE") == 0) {
      newGcodeState = "RUNNING";
      Serial.println("Inferred state 'RUNNING' from print progress.");
  }

  updatePrinterState(String(newGcodeState), newPrintPercentage, String(newChamberLightMode), newBedTemp, newNozzleTemp, String(newWifiSignal), newBedTargetTemp, newNozzleTargetTemp, newTimeRemaining, newLayerNum, newPrintStage);
}

void parseDeltaUpdate(JsonArray arr) {
  String newGcodeState = current_gcode_state;
  int newPrintPercentage = current_print_percentage;
  String newChamberLightMode = current_light_mode;
  float newBedTemp = current_bed_temp;
  float newNozzleTemp = current_nozzle_temp;
  float newBedTargetTemp = current_bed_target_temp;
  float newNozzleTargetTemp = current_nozzle_target_temp;
  int newTimeRemaining = current_time_remaining;
  int newLayerNum = current_layer;
  int newPrintStage = current_stage;
  String newWifiSignal = current_wifi_signal;
  bool gcodeStateFound = false;
  bool progressFound = false;

  for (JsonObject node : arr) {
      if (node.isNull()) continue;

      const char* nodeName = node["node"];
      if (nodeName == nullptr) continue;

      if (strcmp(nodeName, "chamber_light") == 0) {
          newChamberLightMode = node["mode"] | current_light_mode.c_str();
          Serial.print("Received chamber_light delta update. New mode: ");
          Serial.println(newChamberLightMode);
      }
      else if (strcmp(nodeName, "bed_temper") == 0) {
          newBedTemp = node["value"] | current_bed_temp;
      }
      else if (strcmp(nodeName, "nozzle_temper") == 0) {
          newNozzleTemp = node["value"] | current_nozzle_temp;
      }
      else if (strcmp(nodeName, "bed_target_temper") == 0) {
          newBedTargetTemp = node["value"] | current_bed_target_temp;
      }
      else if (strcmp(nodeName, "nozzle_target_temper") == 0) {
          newNozzleTargetTemp = node["value"] | current_nozzle_target_temp;
      }
      else if (strcmp(nodeName, "gcode_state") == 0) {
          newGcodeState = node["value"] | current_gcode_state.c_str();
          gcodeStateFound = true;
          Serial.print("Received gcode_state delta update. New state: ");
          Serial.println(newGcodeState);
      }
      else if (strcmp(nodeName, "print_percentage") == 0) {
          newPrintPercentage = node["value"] | current_print_percentage;
          progressFound = true;
      }
      else if (strcmp(nodeName, "mc_percent") == 0) {
          newPrintPercentage = node["value"] | current_print_percentage;
          progressFound = true;
      }
      else if (strcmp(nodeName, "layer_num") == 0) {
          newLayerNum = node["value"] | current_layer;
          progressFound = true;
      }
      else if (strcmp(nodeName, "mc_remaining_time") == 0) {
          newTimeRemaining = node["value"] | current_time_remaining;
      }
      else if (strcmp(nodeName, "wifi_signal") == 0) {
          newWifiSignal = node["value"] | current_wifi_signal.c_str();
      }
      else if (strcmp(nodeName, "mc_print_sub_stage") == 0) {
          int subStage = node["value"] | -1;
          if (subStage == 1) {
              newChamberLightMode = "on";
              Serial.println("Inferred light 'on' from mc_print_sub_stage delta update");
          }
      }
      else if (strcmp(nodeName, "stg_cur") == 0) {
          newPrintStage = node["value"] | current_stage;
      }
  }

  if (!gcodeStateFound && progressFound && newGcodeState == "IDLE") {
      newGcodeState = "RUNNING";
      Serial.println("Inferred state 'RUNNING' from delta print progress.");
  }

  updatePrinterState(newGcodeState, newPrintPercentage, newChamberLightMode, newBedTemp, newNozzleTemp, newWifiSignal, newBedTargetTemp, newNozzleTargetTemp, newTimeRemaining, newLayerNum, newPrintStage);
}

void updatePrinterState(String gcodeState, int printPercentage, String chamberLightMode, float bedTemp, float nozzleTemp, String wifiSignal, float bedTargetTemp, float nozzleTargetTemp, int timeRemaining, int layerNum, int stage) {
  if ( (gcodeState == "RUNNING" || gcodeState == "PAUSED") && (stage == 255 || printPercentage == 100) ) 
  {
      if (current_gcode_state != "FINISH") {
          Serial.printf("Inferred state 'FINISH' from stg_cur=%d or percentage=%d.\n", stage, printPercentage);
          gcodeState = "FINISH";
      }
  }

  if (gcodeState == "FINISH" && current_gcode_state != "FINISH") {
    finishTime = millis();
    Serial.println("Print finished, starting 2-minute timers.");
  }

  current_gcode_state = gcodeState;
  current_print_percentage = printPercentage;
  current_light_mode = chamberLightMode;
  current_bed_temp = bedTemp;
  current_nozzle_temp = nozzleTemp;
  current_bed_target_temp = bedTargetTemp;
  current_nozzle_target_temp = nozzleTargetTemp;
  current_time_remaining = timeRemaining;
  current_layer = layerNum;
  current_stage = stage;
  current_wifi_signal = wifiSignal;
  current_error_state = (gcodeState == "FAILED" || gcodeState == "STOP");

  if (!manual_light_control) {
    bool lightShouldBeOnBasedOnPrinter = (chamberLightMode == "on" || chamberLightMode == "flashing");
    bool finalLightState = lightShouldBeOnBasedOnPrinter;

    if (config.chamber_light_finish_timeout && finishTime > 0) {
        if (millis() - finishTime < FINISH_LIGHT_TIMEOUT) {
            finalLightState = true;
        } else {
            finalLightState = false;
        }
    }
    
    if (finalLightState != external_light_is_on) { 
       setChamberLightState(finalLightState);
    }
  }

  updateLEDs();
}

void handleMQTTConnection() {
  if (!client.connected()) {
    if (millis() - lastReconnectAttempt > RECONNECT_INTERVAL) {
      lastReconnectAttempt = millis();
      if (reconnectMQTT()) {
        lastReconnectAttempt = 0;
      }
    }
  } else {
    client.loop();
  }
}