#include "config.h"
#include <WiFiManager.h>

// --- Global Config Instance Definition ---
Config config;

// --- WiFiManager Parameter Definitions ---
WiFiManagerParameter custom_bbl_ip("ip", "Bambu Printer IP", config.bbl_ip, 40);
WiFiManagerParameter custom_bbl_serial("serial", "Printer Serial", config.bbl_serial, 40);
WiFiManagerParameter custom_bbl_access_code("code", "Access Code (MQTT Pass)", config.bbl_access_code, 50);
WiFiManagerParameter custom_bbl_pin("lightpin", "External Light GPIO Pin", "", 5, "type='number' min='0' max='39'");
WiFiManagerParameter custom_bbl_invert("invert", "Invert Light Logic (1=Active Low)", "", 2, "type='checkbox' value='1'");
WiFiManagerParameter custom_chamber_bright("chamber_bright", "External Light Brightness (0-100%)", "", 5, "type='number' min='0' max='100'");
WiFiManagerParameter custom_chamber_finish_timeout("chamber_timeout", "Enable 2-Min Finish Timeout (Light OFF)", "", 2, "type='checkbox' value='1'");
WiFiManagerParameter custom_num_leds("numleds", "Number of WS2812B LEDs (Max 60)", "", 5, "type='number' min='0' max='60'");
WiFiManagerParameter custom_led_order("ledorder", "LED Color Order (e.g. GRB)", config.led_color_order, 4);
WiFiManagerParameter custom_idle_color("idle_color", "Idle Color (RRGGBB)", "", 7, "placeholder='000000'");
WiFiManagerParameter custom_idle_bright("idle_bright", "Idle Brightness (0-255)", "", 5, "type='number' min='0' max='255'");
WiFiManagerParameter custom_print_color("print_color", "Print Color (RRGGBB)", "", 7, "placeholder='FFFFFF'");
WiFiManagerParameter custom_print_bright("print_bright", "Print Brightness (0-255)", "", 5, "type='number' min='0' max='255'");
WiFiManagerParameter custom_pause_color("pause_color", "Pause Color (RRGGBB)", "", 7, "placeholder='FFA500'");
WiFiManagerParameter custom_pause_bright("pause_bright", "Pause Brightness (0-255)", "", 5, "type='number' min='0' max='255'");
WiFiManagerParameter custom_error_color("error_color", "Error Color (RRGGBB)", "", 7, "placeholder='FF0000'");
WiFiManagerParameter custom_error_bright("error_bright", "Error Brightness (0-255)", "", 5, "type='number' min='0' max='255'");
WiFiManagerParameter custom_finish_color("finish_color", "Finish Color (RRGGBB)", "", 7, "placeholder='00FF00'");
WiFiManagerParameter custom_finish_bright("finish_bright", "Finish Brightness (0-255)", "", 5, "type='number' min='0' max='255'");
WiFiManagerParameter custom_led_finish_timeout("led_finish_timeout", "Enable 2-Min Finish Timeout (LEDs)", "", 2, "type='checkbox' value='1'");
WiFiManagerParameter custom_ntp_server("ntp", "NTP Server", config.ntp_server, 60);
WiFiManagerParameter custom_timezone("tz", "Timezone (TZ String)", config.timezone, 50);

// --- Parameter Buffer Definitions ---
char custom_pin_param_buffer[5] = "14";
char custom_invert_param_buffer[2] = "0";
char custom_chamber_bright_param_buffer[5] = "100";
char custom_chamber_finish_timeout_param_buffer[2] = "1";
char custom_num_leds_param_buffer[5] = "10";
char custom_led_order_buffer[4] = "GRB";
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
char custom_ntp_buffer[60] = "pool.ntp.org";
char custom_tz_buffer[50] = "GMT0BST,M3.5.0/1,M10.5.0";

// --- Function Implementations ---

bool checkFactoryReset() {
  pinMode(FORCE_RESET_PIN, INPUT_PULLUP);
  bool forceReset = (digitalRead(FORCE_RESET_PIN) == LOW); 
  if(forceReset) {
    Serial.println("!!! Factory reset pin (GPIO 16) is LOW. Wiping config. !!!");
  } else {
    Serial.println("Factory reset pin is HIGH. Booting normally.");
  }
  return forceReset;
}

bool initFileSystem() {
  Serial.println("Mounting LittleFS...");
  if (!LittleFS.begin(true)) {
    return false;
  }
  Serial.println("LittleFS mounted.");
  return true;
}

void performFactoryReset() {
  Serial.println("Factory reset triggered!");
  Serial.println("Erasing /config.json...");
  if (LittleFS.remove("/config.json")) {
    Serial.println("Config file erased.");
  } else {
    Serial.println("Config file not found or erase failed.");
  }
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
  configFile.close();

  if (error) {
    Serial.print("Failed to parse config file: ");
    Serial.println(error.c_str());
    return false;
  }

  Config tempConfig = config;

  strlcpy(tempConfig.bbl_ip, doc["bbl_ip"] | config.bbl_ip, sizeof(tempConfig.bbl_ip));
  strlcpy(tempConfig.bbl_serial, doc["bbl_serial"] | config.bbl_serial, sizeof(tempConfig.bbl_serial));
  strlcpy(tempConfig.bbl_access_code, doc["bbl_access_code"] | config.bbl_access_code, sizeof(tempConfig.bbl_access_code));
  tempConfig.invert_output = doc["invert_output"] | config.invert_output;
  tempConfig.chamber_light_pin = doc["chamber_light_pin"] | config.chamber_light_pin;
  tempConfig.chamber_pwm_brightness = doc["chamber_pwm_brightness"] | config.chamber_pwm_brightness;
  tempConfig.chamber_light_finish_timeout = doc["chamber_light_finish_timeout"] | config.chamber_light_finish_timeout;

  tempConfig.num_leds = doc["num_leds"] | config.num_leds;
  strlcpy(tempConfig.led_color_order, doc["led_color_order"] | "GRB", sizeof(tempConfig.led_color_order));
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

  strlcpy(tempConfig.ntp_server, doc["ntp_server"] | "pool.ntp.org", sizeof(tempConfig.ntp_server));
  strlcpy(tempConfig.timezone, doc["timezone"] | "GMT0BST,M3.5.0/1,M10.5.0", sizeof(tempConfig.timezone));

  if (!isValidGpioPin(tempConfig.chamber_light_pin)) {
      Serial.printf("WARNING: Loaded invalid chamber light pin (%d). Using default (%d).\n", tempConfig.chamber_light_pin, DEFAULT_CHAMBER_LIGHT_PIN);
      tempConfig.chamber_light_pin = DEFAULT_CHAMBER_LIGHT_PIN;
  }
   if (tempConfig.num_leds < 0 || tempConfig.num_leds > MAX_LEDS) {
       Serial.printf("WARNING: Loaded invalid number of LEDs (%d). Using default (%d).\n", tempConfig.num_leds, DEFAULT_NUM_LEDS);
       tempConfig.num_leds = DEFAULT_NUM_LEDS;
   }

   config = tempConfig;
   Serial.println("Configuration loaded successfully.");
   return true;
}

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
  doc["led_color_order"] = config.led_color_order;
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
  
  doc["ntp_server"] = config.ntp_server;
  doc["timezone"] = config.timezone;

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

void setupDefaultConfig() {
  strcpy(config.bbl_ip, "192.168.1.100");
  strcpy(config.bbl_serial, "012345678900000");
  strcpy(config.bbl_access_code, "AABBCCDD");
  config.chamber_light_pin = DEFAULT_CHAMBER_LIGHT_PIN;
  strcpy(config.ntp_server, "pool.ntp.org");
  strcpy(config.timezone, "GMT0BST,M3.5.0/1,M10.5.0");
  strcpy(config.led_color_order, "GRB");
}

void applyConfigFixes() {
  if (config.chamber_light_pin == 25) {
    Serial.println("!!! WARNING: Invalid GPIO 25 detected in saved config.");
    Serial.println("!!! Temporarily reverting to default pin 14 to allow boot.");
    config.chamber_light_pin = DEFAULT_CHAMBER_LIGHT_PIN;
  }
}

void printConfig() {
  Serial.println("--- Loaded Config Values ---");
  Serial.print("Printer IP: "); Serial.println(config.bbl_ip);
  Serial.print("Printer Serial: "); Serial.println(config.bbl_serial);
  Serial.print("NTP Server: "); Serial.println(config.ntp_server);
  Serial.print("Timezone: "); Serial.println(config.timezone);
  Serial.print("LED Color Order: "); Serial.println(config.led_color_order);
  Serial.println("------------------------------");
}

void setupWiFiManagerParams() {
  Serial.println("Setting up WiFiManager parameters...");
  snprintf(custom_pin_param_buffer, 5, "%d", config.chamber_light_pin);
  strcpy(custom_invert_param_buffer, config.invert_output ? "1" : "0");
  snprintf(custom_chamber_bright_param_buffer, 5, "%d", config.chamber_pwm_brightness);
  strcpy(custom_chamber_finish_timeout_param_buffer, config.chamber_light_finish_timeout ? "1" : "0");
  snprintf(custom_num_leds_param_buffer, 5, "%d", config.num_leds);
  snprintf(custom_led_order_buffer, 4, "%s", config.led_color_order);
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
  snprintf(custom_ntp_buffer, 60, "%s", config.ntp_server);
  snprintf(custom_tz_buffer, 50, "%s", config.timezone);

  custom_bbl_pin.setValue(custom_pin_param_buffer, 5);
  custom_bbl_invert.setValue(custom_invert_param_buffer, 2);
  custom_chamber_bright.setValue(custom_chamber_bright_param_buffer, 5);
  custom_chamber_finish_timeout.setValue(custom_chamber_finish_timeout_param_buffer, 2);
  custom_num_leds.setValue(custom_num_leds_param_buffer, 5);
  custom_led_order.setValue(custom_led_order_buffer, 4);
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
  custom_ntp_server.setValue(custom_ntp_buffer, 60);
  custom_timezone.setValue(custom_tz_buffer, 50);
  Serial.println("WiFiManager parameters set.");
}

void saveConfigCallback() {
  Serial.println("WiFiManager signaled configuration save.");

  Config tempConfig = config;

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
  }

  tempConfig.chamber_pwm_brightness = atoi(custom_chamber_bright.getValue());
  tempConfig.chamber_light_finish_timeout = (strcmp(custom_chamber_finish_timeout.getValue(), "1") == 0);

  int tempNumLeds = atoi(custom_num_leds.getValue());
  if (tempNumLeds >= 0 && tempNumLeds <= MAX_LEDS) {
      tempConfig.num_leds = tempNumLeds;
  } else {
      Serial.println("WARNING: Invalid LED count entered. Disabling LEDs (setting to 0).");
      tempConfig.num_leds = 0;
  }

  strlcpy(tempConfig.led_color_order, custom_led_order.getValue(), sizeof(tempConfig.led_color_order));

  tempConfig.led_color_idle = strtoul(custom_idle_color.getValue(), NULL, 16);
  tempConfig.led_color_print = strtoul(custom_print_color.getValue(), NULL, 16);
  tempConfig.led_color_pause = strtoul(custom_pause_color.getValue(), NULL, 16);
  tempConfig.led_color_error = strtoul(custom_error_color.getValue(), NULL, 16);
  tempConfig.led_color_finish = strtoul(custom_finish_color.getValue(), NULL, 16);

  tempConfig.led_bright_idle = constrain(atoi(custom_idle_bright.getValue()), 0, 255);
  tempConfig.led_bright_print = constrain(atoi(custom_print_bright.getValue()), 0, 255);
  tempConfig.led_bright_pause = constrain(atoi(custom_pause_bright.getValue()), 0, 255);
  tempConfig.led_bright_error = constrain(atoi(custom_error_bright.getValue()), 0, 255);
  tempConfig.led_bright_finish = constrain(atoi(custom_finish_bright.getValue()), 0, 255);
  tempConfig.led_finish_timeout = (strcmp(custom_led_finish_timeout.getValue(), "1") == 0);

  strlcpy(tempConfig.ntp_server, custom_ntp_server.getValue(), sizeof(tempConfig.ntp_server));
  strlcpy(tempConfig.timezone, custom_timezone.getValue(), sizeof(tempConfig.timezone));

  config = tempConfig;
  saveConfig();
}

bool isValidGpioPin(int pin) {
    if (pin < 0 || pin > 39) return false;
    if (pin >= 6 && pin <= 11) return false; // SPI Flash
    if (pin >= 34) return false; // Input-only
    
    // --- FIX 5 ---
    // Add checks to prevent pin conflicts with other hardware
    if (pin == LED_DATA_PIN) {
        Serial.printf("Pin %d is reserved for LED Data.\n", LED_DATA_PIN);
        return false;
    }
    if (pin == FORCE_RESET_PIN) {
        Serial.printf("Pin %d is reserved for Factory Reset.\n", FORCE_RESET_PIN);
        return false;
    }
    // --- END FIX 5 ---

    return true;
}

void configureTime() {
  Serial.println("Configuring time...");
  configTime(0, 0, config.ntp_server);
  setenv("TZ", config.timezone, 1);
  tzset();
  Serial.printf("Timezone set to: %s\n", config.timezone);
}

String getTimestamp() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    return "[--:--:--]";
  }
  char buffer[30];
  strftime(buffer, sizeof(buffer), "[%Y-%m-%d %H:%M:%S]", &timeinfo);
  return String(buffer);
}

String getTimezoneDropdown(String selectedTz) {
  String html = "<select id='timezone' name='timezone'>";
  
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
  
  if (html.indexOf("selected") == -1) {
     addOption(selectedTz.c_str(), "(Custom)");
  }

  html += "</select>";
  return html;
}

String getLedOrderDropdown(String selectedOrder) {
  String html = "<select id='led_color_order' name='led_color_order'>";
  
  auto addOption = [&](const char* order, const char* name) {
    html += "<option value='";
    html += order;
    html += "'";
    if (selectedOrder == order) {
      html += " selected";
    }
    html += ">";
    html += name;
    html += "</option>";
  };

  addOption("GRB", "GRB (Most Common WS2812B)");
  addOption("RGB", "RGB");
  addOption("BRG", "BRG");
  addOption("GBR", "GBR");
  addOption("RBG", "RBG");
  addOption("BGR", "BGR");

  if (html.indexOf("selected") == -1) {
     addOption(selectedOrder.c_str(), "(Custom)");
  }

  html += "</select>";
  return html;
}