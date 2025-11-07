#include "web_handlers.h"
#include "config.h"
#include "light_controller.h"
#include "led_controller.h"
#include "mqtt_handler.h"
#include <ArduinoJson.h>

void setupWebServer() {
  Serial.println("Setting up Web Server...");
  server.on("/", handleRoot);
  server.on("/status.json", handleStatusJson);
  server.on("/light/on", handleLightOn);
  server.on("/light/off", handleLightOff);
  server.on("/light/auto", handleLightAuto);
  server.on("/config", handleConfig);
  server.on("/mqtt", handleMqttJson);
  server.on("/backup", HTTP_GET, handleBackup);
  server.on("/restore", HTTP_GET, handleRestorePage);
  server.on("/restore", HTTP_POST, handleRestoreReboot);
  server.onFileUpload(handleRestoreUpload);
  server.begin();
  Serial.print("Status page available at http://");
  Serial.println(WiFi.localIP());
}

bool connectWiFi(bool forceReset) {
  if (forceReset) {
      Serial.println("Clearing saved Wi-Fi settings...");
      wm.resetSettings();
  }

  wm.setSaveConfigCallback(saveConfigCallback);

  WiFiManagerParameter p_time_heading("<h2>Time Settings</h2>");
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
  wm.addParameter(&p_time_heading);
  wm.addParameter(&custom_ntp_server);
  wm.addParameter(&custom_timezone);
  wm.addParameter(&p_light_heading);
  wm.addParameter(&custom_bbl_pin);
  wm.addParameter(&custom_bbl_invert);
  wm.addParameter(&custom_chamber_bright);
  wm.addParameter(&custom_chamber_finish_timeout);
  wm.addParameter(&p_led_heading);
  wm.addParameter(&custom_num_leds);
  wm.addParameter(&custom_led_order);
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
  yield();
  
  wm.setConfigPortalTimeout(180);

  return wm.autoConnect("BambuLightSetup", "password");
}

void handleRoot() {
  String html = String("<!DOCTYPE html><html><head>");
  html += String("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
  html += String("<title>Bambu Light Status</title><style>");
  html += String("body { font-family: Arial, sans-serif; margin: 20px; background-color: #1a1a1b; color: #e0e0e0; }");
  html += String("h1, h2 { color: #ffffff; }");
  html += String("a { color: #58a6ff; }");
  html += String(".status { padding: 10px; margin-bottom: 10px; border-radius: 5px; transition: background-color 0.5s, border-color 0.5s; background-color: #2c2c2e; border: 1px solid #444; }");
  html += String(".connected { background-color: #1a3a24; color: #8cda9b; border: 1px solid #336d3f; }");
  html += String(".disconnected { background-color: #401f22; color: #f0989f; border: 1px solid #7c333a; }");
  html += String(".warning { background-color: #423821; color: #f0d061; border: 1px solid #7e6c33; }");
  html += String(".light-on { background-color: #1c314a; color: #9cc2ef; border: 1px solid #335d88; }");
  html += String(".error { background-color: #401f22; color: #f0989f; border: 1px solid #7c333a; font-weight: bold; }");
  html += String("button { background-color: #378cf0; color: #ffffff; padding: 10px 15px; border: none; border-radius: 5px; cursor: pointer; margin-right: 5px; margin-bottom: 5px; }");
  html += String("button.off { background-color: #6c757d; } button.auto { background-color: #28a745; }");
  html += String("span.data { font-weight: normal; }");
  html += String("</style></head><body><h1>Bambu Chamber Light Controller</h1>");

  html += String("<div class=\"status ");
  html += (WiFi.status() == WL_CONNECTED ? "connected" : "disconnected");
  html += String("\"><strong>WiFi Status:</strong> ");
  String wifi_status = (WiFi.status() == WL_CONNECTED ? "CONNECTED (" + WiFi.SSID() + " / " + WiFi.localIP().toString() + ")" : "DISCONNECTED");
  html += wifi_status;
  html += String("</div>");

  html += String("<div id=\"mqtt-status-div\" class=\"status disconnected\">");
  html += String("<strong>MQTT Status:</strong> <span id=\"mqtt-status\" class=\"data\">DISCONNECTED</span>");
  html += String(" | <a href=\"/mqtt\" target=\"_blank\">View MQTT History</a>");
  html += String("</div>");

  html += String("<h2>Printer Status</h2>");
  html += String("<p><strong>GCODE State:</strong> <span id=\"gcode-state\" class=\"data\">N/A</span></p>");
  html += String("<p><strong>Print Percentage:</strong> <span id=\"print-percent\" class=\"data\">0 %</span></p>");
  html += String("<p><strong>Current Layer:</strong> <span id=\"layer-num\" class=\"data\">0</span></p>");
  html += String("<p><strong>Print Stage:</strong> <span id=\"stage\" class=\"data\">N/A</span></p>");
  html += String("<p><strong>Time Remaining:</strong> <span id=\"print-time\" class=\"data\">--:--:--</span></p>");
  html += String("<p><strong>Nozzle Temp:</strong> <span id=\"nozzle-temp\" class=\"data\">0.0 / 0.0 &deg;C</span></p>");
  html += String("<p><strong>Bed Temp:</strong> <span id=\"bed-temp\" class=\"data\">0.0 / 0.0 &deg;C</span></p>");
  html += String("<p><strong>Printer WiFi Signal:</strong> <span id=\"wifi-signal\" class=\"data\">N/A</span></p>");

  html += String("<h2>External Outputs</h2>");
  html += String("<div id=\"light-status-div\" class=\"status disconnected\">");
  html += String("<strong>External Light (Pin ") + String(config.chamber_light_pin) + String("):</strong> ");
  html += String("<span id=\"light-status\" class=\"data\">N/A</span>");
  html += String("<br><small><strong>Control Mode:</strong> <span id=\"light-mode\" class=\"data\">N/A</span>");
  html += String(" | (Logic: ") + (config.invert_output ? "Active LOW" : "Active HIGH");
  html += String(" | Bambu Light Mode: <span id=\"bambu-light-mode\" class=\"data\">N/A</span>)</small></div>");

  html += String("<div id=\"led-status-div\" class=\"status disconnected\">");
  // --- FIX 1 ---
  // Replaced LED_PIN_CONST with LED_DATA_PIN
  html += String("<strong>LED Status Bar (Pin ") + String(LED_DATA_PIN) + String(" / ") + String(config.num_leds) + String(" LEDs):</strong> ");
  html += String("<span id=\"led-status\" class=\"data\">N/A</span>");
  
  html += String("<div id='virtual-bar-container' style='margin-top: 10px;'>");
  html += String("<div id='virtual-bar' style='display: flex; width: 100%; height: 20px; background: #222; border-radius: 5px; overflow: hidden; border: 1px solid #444;'>");
  for(int i=0; i < config.num_leds && i < MAX_LEDS; i++) {
    html += "<div class='vled' style='flex-grow: 1; height: 100%; transition: background-color 0.5s, opacity 0.5s;'></div>";
  }
  html += String("</div></div>");
  // --- FIX 2 ---
  // Replaced LED_PIN_CONST with LED_DATA_PIN
  html += String("<br><small>Data Pin is hardcoded to GPIO ") + String(LED_DATA_PIN) + String(" for FastLED compatibility.</small></div>");

  html += String("<h2>Manual Control</h2>");
  html += String("<p><a href=\"/light/on\"><button>Turn Light ON</button></a>");
  html += String("<a href=\"/light/off\"><button class=\"off\">Turn Light OFF</button></a>");
  html += String("<a href=\"/light/auto\"><button class=\"auto\">Set to AUTO</button></a></p>");

  html += String("<hr><p><a href=\"/config\"><button>Change Device Settings</button></a></p>");
  
  html += String("<script>");
  html += String("function formatTime(s) {");
  html += String("if (s <= 0) return '--:--:--';");
  html += String("let h = Math.floor(s / 3600); s %= 3600;");
  html += String("let m = Math.floor(s / 60); s %= 60;");
  html += String("let m_str = m < 10 ? '0' + m : m;");
  html += String("let s_str = s < 10 ? '0' + s : s;");
  html += String("return h > 0 ? h + ':' + m_str + ':' + s_str : m_str + ':' + s_str;");
  html += String("}");
  
  html += String("async function updateStatus() {");
  html += String("try {");
  html += String("const response = await fetch('/status.json');");
  html += String("if (!response.ok) return;");
  html += String("const data = await response.json();");
  
  html += String("document.getElementById('mqtt-status').innerText = data.mqtt_connected ? 'CONNECTED' : 'DISCONNECTED';");
  html += String("document.getElementById('mqtt-status-div').className = 'status ' + (data.mqtt_connected ? 'connected' : 'disconnected');");
  
  html += String("document.getElementById('gcode-state').innerText = data.gcode_state;");
  html += String("document.getElementById('print-percent').innerText = data.print_percentage + ' %';");
  html += String("document.getElementById('layer-num').innerText = data.layer_num;");
  html += String("document.getElementById('stage').innerText = data.stage;");
  html += String("document.getElementById('print-time').innerText = formatTime(data.time_remaining);");
  html += String("document.getElementById('nozzle-temp').innerHTML = data.nozzle_temp.toFixed(1) + ' / ' + data.nozzle_target_temp.toFixed(1) + ' &deg;C';");
  html += String("document.getElementById('bed-temp').innerHTML = data.bed_temp.toFixed(1) + ' / ' + data.bed_target_temp.toFixed(1) + ' &deg;C';");
  html += String("document.getElementById('wifi-signal').innerText = data.wifi_signal;");
  
  html += String("document.getElementById('light-status').innerText = data.light_is_on ? ('ON (' + data.chamber_bright + '%)') : 'OFF';");
  html += String("document.getElementById('light-status-div').className = 'status ' + (data.light_is_on ? 'light-on' : 'disconnected');");
  html += String("document.getElementById('light-mode').innerText = data.manual_control ? 'MANUAL' : ('AUTO' + data.light_mode_extra);");
  html += String("document.getElementById('bambu-light-mode').innerText = data.bambu_light_mode;");
  
  html += String("document.getElementById('led-status').innerText = data.led_status_str;");
  html += String("document.getElementById('led-status-div').className = 'status ' + data.led_status_class;");

  html += String("let color = data.led_color_val.toString(16).padStart(6, '0');");
  html += String("let brightness = data.led_bright_val;");
  html += String("let opacity = (brightness / 255).toFixed(2);");
  html += String("let vleds = document.querySelectorAll('#virtual-bar .vled');");
  html += String("let numLeds = vleds.length;");
  
  html += String("if (data.is_printing && data.print_percentage > 0 && numLeds > 0) {");
  html += String("  let leds_to_light = Math.ceil((data.print_percentage / 100) * numLeds);");
  html += String("  for (let i = 0; i < numLeds; i++) {");
  html += String("    if (i < leds_to_light) {");
  html += String("      vleds[i].style.backgroundColor = '#' + color;");
  html += String("      vleds[i].style.opacity = opacity;");
  html += String("    } else {");
  html += String("      vleds[i].style.backgroundColor = '#000';");
  html += String("      vleds[i].style.opacity = '1.0';");
  html += String("    }");
  html += String("  }");
  html += String("} else {");
  html += String("  vleds.forEach(led => {");
  html += String("    led.style.backgroundColor = '#' + color;");
  html += String("    led.style.opacity = opacity;");
  html += String("  });");
  html += String("}");

  html += String("} catch (e) { console.error('Error fetching status:', e); }");
  html += String("}");
  
  html += String("document.addEventListener('DOMContentLoaded', updateStatus);");
  html += String("setInterval(updateStatus, 3000);");
  
  html += String("</script>");
  
  html += String("</body></html>");

  server.send(200, "text/html", html);
}

void handleStatusJson() {
  DynamicJsonDocument doc(1024);

  doc["mqtt_connected"] = client.connected();

  doc["gcode_state"] = current_gcode_state;
  doc["print_percentage"] = current_print_percentage;
  doc["time_remaining"] = current_time_remaining;
  doc["layer_num"] = current_layer;
  doc["stage"] = current_stage;
  doc["nozzle_temp"] = current_nozzle_temp;
  doc["nozzle_target_temp"] = current_nozzle_target_temp;
  doc["bed_temp"] = current_bed_temp;
  doc["bed_target_temp"] = current_bed_target_temp;
  doc["wifi_signal"] = current_wifi_signal;

  doc["light_is_on"] = external_light_is_on;
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
      }
  } else if (current_print_percentage > 0 && current_gcode_state != "IDLE") {
      current_color_val = config.led_color_print;
      current_bright_val = config.led_bright_print;
      is_printing = true;
  }
  doc["led_color_val"] = current_color_val;
  doc["led_bright_val"] = current_bright_val;
  doc["is_printing"] = is_printing;

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
  
  String json_output;
  serializeJson(doc, json_output);
  server.send(200, "application/json", json_output);
}

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
    for(auto it = mqtt_history.begin(); it != mqtt_history.end(); ++it) {
      String msg = *it;
      msg.replace("<", "&lt;");
      msg.replace(">", "&gt;");
      html += msg + "\n";
    }
  }
  html += "</pre>";

  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleLightOn() {
  Serial.println("Web Request: /light/on");
  manual_light_control = true;
  setChamberLightState(true);
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

void handleLightOff() {
  Serial.println("Web Request: /light/off");
  manual_light_control = true;
  setChamberLightState(false);
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

void handleLightAuto() {
  Serial.println("Web Request: /light/auto");
  manual_light_control = false;
  bool lightShouldBeOn = (current_light_mode == "on" || current_light_mode == "flashing");
  bool finalLightState = lightShouldBeOn;

  if (config.chamber_light_finish_timeout && finishTime > 0) {
      if (millis() - finishTime < FINISH_LIGHT_TIMEOUT) {
          finalLightState = true;
      } else {
          finalLightState = false;
      }
  }

  setChamberLightState(finalLightState);

  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

void handleConfig() {
  if (server.method() == HTTP_POST) {
    Serial.println("Web Request: POST /config - Saving settings...");

    Config tempConfig = config;

    if (server.hasArg("ip")) strlcpy(tempConfig.bbl_ip, server.arg("ip").c_str(), sizeof(tempConfig.bbl_ip));
    if (server.hasArg("serial")) strlcpy(tempConfig.bbl_serial, server.arg("serial").c_str(), sizeof(tempConfig.bbl_serial));
    if (server.hasArg("code")) strlcpy(tempConfig.bbl_access_code, server.arg("code").c_str(), sizeof(tempConfig.bbl_access_code));

    if (server.hasArg("ntp_server")) strlcpy(tempConfig.ntp_server, server.arg("ntp_server").c_str(), sizeof(tempConfig.ntp_server));
    if (server.hasArg("timezone")) strlcpy(tempConfig.timezone, server.arg("timezone").c_str(), sizeof(tempConfig.timezone));

    if (server.hasArg("lightpin")) {
      int tempLightPin = server.arg("lightpin").toInt();
      if (isValidGpioPin(tempLightPin)) {
          tempConfig.chamber_light_pin = tempLightPin;
      } else {
          Serial.printf("ERROR: Invalid GPIO pin %d submitted. Retaining previous pin.\n", tempLightPin);
      }
    }
    tempConfig.invert_output = server.hasArg("invert");
    if (server.hasArg("chamber_bright")) tempConfig.chamber_pwm_brightness = constrain(server.arg("chamber_bright").toInt(), 0, 100);
    tempConfig.chamber_light_finish_timeout = server.hasArg("chamber_timeout");

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
    if (server.hasArg("led_color_order")) strlcpy(tempConfig.led_color_order, server.arg("led_color_order").c_str(), sizeof(tempConfig.led_color_order));

    if (server.hasArg("idle_color")) tempConfig.led_color_idle = strtoul(server.arg("idle_color").c_str(), NULL, 16);
    if (server.hasArg("print_color")) tempConfig.led_color_print = strtoul(server.arg("print_color").c_str(), NULL, 16);
    if (server.hasArg("pause_color")) tempConfig.led_color_pause = strtoul(server.arg("pause_color").c_str(), NULL, 16);
    if (server.hasArg("error_color")) tempConfig.led_color_error = strtoul(server.arg("error_color").c_str(), NULL, 16);
    if (server.hasArg("finish_color")) tempConfig.led_color_finish = strtoul(server.arg("finish_color").c_str(), NULL, 16);

    if (server.hasArg("idle_bright")) tempConfig.led_bright_idle = constrain(server.arg("idle_bright").toInt(), 0, 255);
    if (server.hasArg("print_bright")) tempConfig.led_bright_print = constrain(server.arg("print_bright").toInt(), 0, 255);
    if (server.hasArg("pause_bright")) tempConfig.led_bright_pause = constrain(server.arg("pause_bright").toInt(), 0, 255);
    if (server.hasArg("error_bright")) tempConfig.led_bright_error = constrain(server.arg("error_bright").toInt(), 0, 255);
    if (server.hasArg("finish_bright")) tempConfig.led_bright_finish = constrain(server.arg("finish_bright").toInt(), 0, 255);

    config = tempConfig;
    saveConfig();

    String html = "<!DOCTYPE html><html><head><title>Saving...</title>";
    html += "<meta http-equiv='refresh' content='3;url=/'><style>body{font-family:Arial,sans-serif;background:#1a1a1b;color:#e0e0e0;}</style></head>";
    html += "<body><h2>Configuration Saved.</h2>";
    html += "<p>Device is rebooting to apply settings. You will be redirected in 3 seconds...</p></body></html>";
    server.send(200, "text/html", html);
    delay(1000);
    ESP.restart();
  }
  else {
    Serial.println("Web Request: GET /config - Showing settings page...");
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
    html += "input[type='text'],input[type='number'],input[type='password'],select{width:98%;padding:8px;border:1px solid #555;border-radius:4px;font-family:Arial,sans-serif;font-size:1em;background-color:#3a3a3c;color:#e0e0e0;}";
    html += "input[type='checkbox']{margin-right:10px;vertical-align:middle;}";
    html += "label[for='invert'], label[for='chamber_timeout'], label[for='led_finish_timeout'] { display:inline-block;font-weight:normal; }";
    html += "button{background-color:#378cf0;color:#ffffff;padding:12px 20px;border:none;border-radius:5px;cursor:pointer;font-size:16px;}";
    html += "button:hover{background-color:#0056b3;}";
    html += ".color-input{width:100px;padding:8px;vertical-align:middle;margin-left:10px;border:1px solid #555;}";
    html += ".grid{display:grid;grid-template-columns:repeat(auto-fit, minmax(200px, 1fr));grid-gap:20px;}";
    html += ".card{background:#3a3a3c;padding:15px;border-radius:5px;border:1px solid #555;}";
    html += "small{color:#aaa;}";
    html += "a{color:#58a6ff;}";
    html += ".color-swatch{width: 20px; height: 20px; display: inline-block; vertical-align: middle; margin-left: 10px; border: 1px solid #555; border-radius: 4px; background-color: #000;}";
    html += "</style></head><body><h1>Bambu Light Controller Settings</h1>";
    html += "<p><small>To change Wi-Fi, use the 'Factory Reset' pin (GPIO 16) on boot.</small></p>";
    html += "<form action='/config' method='POST'>";

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

    html += "<h2>Time Settings</h2>";
    html += "<div class='grid'>";
    html += "<div class='card'><div><label for='ntp_server'>NTP Server</label><input type='text' id='ntp_server' name='ntp_server' value='";
    html += String(config.ntp_server);
    html += "'></div></div>";
    html += "<div class='card'><div><label for='timezone'>Timezone</label>";
    html += getTimezoneDropdown(String(config.timezone));
    html += "</div></div>";
    html += "</div>";

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

    html += "<h2>LED Status Bar Settings</h2>";
    html += "<div class='grid'>";
    html += "<div class='card'><div><label for='numleds'>Number of WS2812B LEDs (Max 60)</label><input type='number' id='numleds' name='numleds' min='0' max='";
    html += String(MAX_LEDS);
    html += "' value='";
    html += String(config.num_leds);
    html += "'></div></div>";

    html += "<div class='card'><div><label for='led_color_order'>LED Color Order</label>";
    html += getLedOrderDropdown(String(config.led_color_order));
    html += "</div></div>";
    html += "</div>";

    html += "<div><small>LED Data Pin is hardcoded to GPIO ";
    // --- FIX 3 ---
    // Replaced LED_PIN_CONST with LED_DATA_PIN
    html += String(LED_DATA_PIN);
    html += " for FastLED.</small></div>";
    
    html += "<div><input type='checkbox' id='led_finish_timeout' name='led_finish_timeout' value='1' ";
    html += (config.led_finish_timeout ? "checked" : "");
    html += "><label for='led_finish_timeout'>Enable 2-Min Finish Timeout (LEDs return to Idle)</label></div>";

    html += "<h3>Virtual LED Preview</h3>";
    html += "<div class='card' style='padding: 20px; background-color: #2c2c2e; border: 1px solid #555; border-radius: 5px;'>";
    html += "<div id='virtual-bar-container'>";
    html += "<div id='virtual-bar' style='display: flex; width: 100%; height: 30px; background: #222; border-radius: 5px; overflow: hidden; border: 1px solid #555;'>";
    
    // --- FIX 4 ---
    // Replaced hardcoded 10-LED preview with one that respects config.num_leds
    // and uses flex-grow, just like the one on the main status page.
    for(int i=0; i < config.num_leds && i < MAX_LEDS; i++) {
      html += "<div class='vled' style='flex-grow: 1; height: 100%;'></div>";
    }
    if (config.num_leds == 0) {
       html += "<div style='flex-grow: 1; height: 100%; text-align: center; color: #888; padding-top: 5px; font-size: 0.9em;'>LEDs disabled (set count > 0)</div>";
    }
    // --- END FIX 4 ---

    html += "</div></div>";
    html += "<div id='preview-controls' style='margin-top: 15px; display: flex; flex-wrap: wrap; gap: 15px;'>";
    html += "<label style='display:inline-block; color:#e0e0e0;'><input type='radio' name='preview_state' value='idle' onchange='updatePreview()' checked> Idle</label>";
    html += "<label style='display:inline-block; color:#e0e0e0;'><input type='radio' name='preview_state' value='print' onchange='updatePreview()'> Printing</label>";
    html += "<label style='display:inline-block; color:#e0e0e0;'><input type='radio' name='preview_state' value='pause' onchange='updatePreview()'> Paused</label>";
    html += "<label style='display:inline-block; color:#e0e0e0;'><input type='radio' name='preview_state' value='error' onchange='updatePreview()'> Error</label>";
    html += "<label style='display:inline-block; color:#e0e0e0;'><input type='radio' name='preview_state' value='finish' onchange='updatePreview()'> Finish</label>";
    html += "</div></div>";

    html += "<h3>LED States</h3><div class='grid'>";
    html += "<div class='card'><h4>Idle Status</h4>";
    html += "<div><label for='idle_color'>Color (RRGGBB) <span id='idle_color_swatch' class='color-swatch'></span></label><input type='text' id='idle_color' name='idle_color' value='";
    html += String(idle_color_hex);
    html += "' oninput='updatePreview(); try { document.getElementById(\"idle_color_picker\").value = \"#\" + this.value; } catch(e) {}'><input type='color' class='color-input' id='idle_color_picker' value='#";
    html += String(idle_color_hex);
    html += "' onchange='document.getElementById(\"idle_color\").value = this.value.substring(1).toUpperCase(); updatePreview();'></div>";
    
    html += "<div><label for='idle_bright'>Brightness (0-255)</label><input type='number' id='idle_bright' name='idle_bright' min='0' max='255' value='";
    html += String(config.led_bright_idle);
    html += "' oninput='updatePreview()'></div></div>";

    html += "<div class='card'><h4>Printing Status</h4>";
    html += "<div><label for='print_color'>Color (RRGGBB) <span id='print_color_swatch' class='color-swatch'></span></label><input type='text' id='print_color' name='print_color' value='";
    html += String(print_color_hex);
    html += "' oninput='updatePreview(); try { document.getElementById(\"print_color_picker\").value = \"#\" + this.value; } catch(e) {}'><input type='color' class='color-input' id='print_color_picker' value='#";
    html += String(print_color_hex);
    html += "' onchange='document.getElementById(\"print_color\").value = this.value.substring(1).toUpperCase(); updatePreview();'></div>";
    
    html += "<div><label for='print_bright'>Brightness (0-255)</label><input type='number' id='print_bright' name='print_bright' min='0' max='255' value='";
    html += String(config.led_bright_print);
    html += "' oninput='updatePreview()'></div></div>";

    html += "<div class='card'><h4>Paused Status</h4>";
    html += "<div><label for='pause_color'>Color (RRGGBB) <span id='pause_color_swatch' class='color-swatch'></span></label><input type='text' id='pause_color' name='pause_color' value='";
    html += String(pause_color_hex);
    html += "' oninput='updatePreview(); try { document.getElementById(\"pause_color_picker\").value = \"#\" + this.value; } catch(e) {}'><input type='color' class='color-input' id='pause_color_picker' value='#";
    html += String(pause_color_hex);
    html += "' onchange='document.getElementById(\"pause_color\").value = this.value.substring(1).toUpperCase(); updatePreview();'></div>";
    
    html += "<div><label for='pause_bright'>Brightness (0-255)</label><input type='number' id='pause_bright' name='pause_bright' min='0' max='255' value='";
    html += String(config.led_bright_pause);
    html += "' oninput='updatePreview()'></div></div>";

    html += "<div class='card'><h4>Error Status</h4>";
    html += "<div><label for='error_color'>Color (RRGGBB) <span id='error_color_swatch' class='color-swatch'></span></label><input type='text' id='error_color' name='error_color' value='";
    html += String(error_color_hex);
    html += "' oninput='updatePreview(); try { document.getElementById(\"error_color_picker\").value = \"#\" + this.value; } catch(e) {}'><input type='color' class='color-input' id='error_color_picker' value='#";
    html += String(error_color_hex);
    html += "' onchange='document.getElementById(\"error_color\").value = this.value.substring(1).toUpperCase(); updatePreview();'></div>";
    
    html += "<div><label for='error_bright'>Brightness (0-255)</label><input type='number' id='error_bright' name='error_bright' min='0' max='255' value='";
    html += String(config.led_bright_error);
    html += "' oninput='updatePreview()'></div></div>";

    html += "<div class='card'><h4>Finish Status</h4>";
    html += "<div><label for='finish_color'>Color (RRGGBB) <span id='finish_color_swatch' class='color-swatch'></span></label><input type='text' id='finish_color' name='finish_color' value='";
    html += String(finish_color_hex);
    html += "' oninput='updatePreview(); try { document.getElementById(\"finish_color_picker\").value = \"#\" + this.value; } catch(e) {}'><input type='color' class='color-input' id='finish_color_picker' value='#";
    html += String(finish_color_hex);
    html += "' onchange='document.getElementById(\"finish_color\").value = this.value.substring(1).toUpperCase(); updatePreview();'></div>";
    
    html += "<div><label for='finish_bright'>Brightness (0-255)</label><input type='number' id='finish_bright' name='finish_bright' min='0' max='255' value='";
    html += String(config.led_bright_finish);
    html += "' oninput='updatePreview()'></div></div>";

    html += "</div>";

    html += "<br><div><button type='submit'>Save and Reboot</button></div>";
    html += "</form>";
    
    html += "<h2>Backup & Restore</h2>";
    html += "<div class='grid'>";
    html += "<div class='card'><p>Download a backup of your current settings.</p><a href='/backup'><button type='button' style='background-color:#17a2b8;'>Backup Configuration</button></a></div>";
    html += "<div class='card'><p>Upload a 'config.json' file to restore settings. <b>This will reboot the device.</b></p><a href='/restore'><button type='button' style='background-color:#dc3545;'>Restore Configuration</button></a></div>";
    html += "</div>";

    html += "<br><p><a href='/'>&laquo; Back to Status Page</a></p>";

    html += "<script>";
    html += "function updatePreview() {";
    html += "  try {";
    html += "    let state = document.querySelector('input[name=\"preview_state\"]:checked').value;";
    html += "    let color = document.getElementById(state + '_color').value;";
    html += "    if (!color.match(/^[0-9a-fA-F]{6}$/)) { color = '000000'; }";
    html += "    let bright = parseInt(document.getElementById(state + '_bright').value, 10);";
    html += "    if (isNaN(bright) || bright < 0 || bright > 255) { bright = 0; }";
    html += "    let opacity = (bright / 255).toFixed(2);";
    html += "    let vleds = document.querySelectorAll('.vled');";
    html += "    vleds.forEach(led => {";
    html += "      led.style.backgroundColor = '#' + color;";
    html += "      led.style.opacity = opacity;";
    html += "    });";
    
    html += "    let states = ['idle', 'print', 'pause', 'error', 'finish'];";
    html += "    states.forEach(s => {";
    html += "      let c = document.getElementById(s + '_color').value;";
    html += "      if (!c.match(/^[0-9a-fA-F]{6}$/)) { c = '000000'; }";
    html += "      document.getElementById(s + '_color_swatch').style.backgroundColor = '#' + c;";
    html += "    });";

    html += "  } catch (e) { console.error('Preview update failed:', e); }";
    html += "}";
    html += "document.addEventListener('DOMContentLoaded', updatePreview);";
    html += "</script>";

    html += "</body></html>";

    server.send(200, "text/html", html);
  }
}

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
  if (server.uri() != "/restore") {
    return;
  }

  if (upload.status == UPLOAD_FILE_START) {
    if (upload.filename != "config.json") {
      Serial.printf("Invalid restore filename: %s. Aborting.\n", upload.filename.c_str());
      restoreSuccess = false; 
      return;
    }
    
    Serial.printf("Restore Start: %s\n", upload.filename.c_str());
    restoreFile = LittleFS.open("/config.json", "w");
    if (restoreFile) {
      restoreSuccess = true;
    } else {
      Serial.println("Failed to open /config.json for restore.");
      restoreSuccess = false;
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (restoreSuccess && restoreFile) {
      restoreFile.write(upload.buf, upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (restoreSuccess && restoreFile) {
      restoreFile.close();
      Serial.printf("Restore End: %u bytes total\n", upload.totalSize);
    } else {
      if(restoreFile) restoreFile.close();
      if(restoreSuccess) {
         Serial.println("Restore failed during write/end.");
         restoreSuccess = false;
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
  restoreSuccess = false;
}