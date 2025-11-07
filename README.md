# Bambu Lab MQTT Light Controller

This project transforms an ESP32 into a smart controller for your Bambu Lab 3D printer. It connects directly to the printer's local MQTT broker to monitor its status in real-time.

It provides two primary functions:
1.  **External Chamber Light:** Controls a dimmable external light (like a 5V/12V/24V LED strip) using a MOSFET, automatically syncing with the printer's light or manual overrides.
2.  **WS2812B Status Bar:** Drives a (FastLED) addressable LED strip to provide a clear visual indication of the printer's status (e.g., Idle, Printing, Paused, Error).

The device hosts a full web interface for live status monitoring, manual control, and deep configuration.



## 🌟 Key Features

* **Real-Time MQTT Monitoring:** Connects securely (MQTTS) to the printer's local broker to get instant status updates.
* **Full Web Interface:** A responsive web UI to monitor printer status, control lights, and configure all settings.
* **Dimmable External Light Control:**
    *  Uses PWM for 0-100% brightness control of an external light.
    *  Supports both Active HIGH and Active LOW logic (invert output) to work with any MOSFET or relay module.
    *  **Auto Mode:** Syncs with the printer's built-in light status.
    *  **Manual Control:** Override the light (On/Off) from the web UI.
    *  **Finish Timeout:** Optionally keeps the light ON when a print finishes, then automatically turns it OFF after 2 minutes.
* **WS2812B (FastLED) Status Bar:**
    *  Displays printer progress as a loading bar when printing.
    *  Shows distinct colors for Idle, Paused, Error, and Finished states.
    *  All colors and brightness levels are fully customizable via the web UI.
    *  Optional 2-minute "Finish" light timeout before reverting to Idle status.
* **Easy Setup & Configuration:**
    * Uses **WiFiManager** for initial setup.  Just connect to the "BambuLightSetup" WiFi portal.
    *  All settings are stored in `config.json` on the ESP32's file system (LittleFS).
    *  **Configuration Web Page:** A dedicated `/config` page to adjust all settings after initial setup.
* **System & Debugging:**
    *  **OTA Updates:** Supports Over-the-Air firmware updates (hostname: `bambu-light-controller`).
    *  **Factory Reset:** Grounding a specific pin (GPIO 16) on boot will wipe all settings.
    *  **Backup/Restore:** Download or upload your `config.json` file from the config page.
    *  **MQTT History:** A debug page (`/mqtt`) shows the last 100 raw messages received from the printer.
    *  **Time Syncing:** Uses an NTP server and configurable timezones to provide accurate timestamps in the logs.

## 🔌 Hardware Requirements & Wiring
![alt text](https://github.com/eddwatts/BambuLED/blob/ba640855261b71b3a1040bf42925f67e103e8231/esp32.png "ESP32 S3 N16R8") 
* **ESP32 Module:** This project is memory-intensive. **A board with 8MB or 16MB of Flash (N8 or N16) and 8MB of PSRAM (R8), such as an ESP32-S3-N8R8 or ESP32-S3-N16R8, is highly recommended.**
    *  **Why 8MB (N8) or 16MB Flash (N16)?** The large flash provides ample space for the application, the LittleFS file system , and Over-the-Air (OTA) updates, which requires partitions large enough to hold two full copies of the firmware.
    *  **Why 8MB PSRAM (R8)?** The code uses `WiFiClientSecure` (SSL) and parses large (4KB+) JSON payloads from MQTT.  The firmware is **optimized to use this external PSRAM** to store the MQTT message history, which prevents "Out of Memory" crashes and ensures stability. A standard ESP32 without PSRAM may be unstable.
* **External Light Hardware:**
    * A 5V or high-power 12V or 24V LED strip for chamber lighting.
    * A logic-level N-Channel MOSFET module (like an IRF520 or AOD4184) or a Relay Module (do not use dimer settings if using a relay) to safely control the high-voltage light from the ESP32's 3.3V pin.
* **Status Bar (Optional):** A WS2812B (e.g., NeoPixel) addressable LED strip.
* **Reset Button (Optional):** A momentary push button or jumper wire for the factory reset pin.

### Wiring Diagram

| ESP32 Pin | Connects To | Purpose |
| :--- | :--- | :--- |
| **GPIO 4** (Hardcoded) | **WS2812B Data In** |  FastLED Status Bar Data |
| **GPIO 14** (Default) | **MOSFET/Relay Signal Pin** | External Chamber Light Control.  *This pin is configurable in the web UI.* |
| **GPIO 16** (Hardcoded) | **Momentary Button to GND** | Factory Reset Pin.  *Ground this pin **during boot** to wipe settings.* |
| **5V / VIN** | MOSFET/Relay VCC, WS2812B +5V | Power for modules (if 5V) |
| **GND** | MOSFET/Relay GND, WS2812B GND | Common Ground |

> **Warning:** Do **NOT** power a WS2812B strip or a high-power chamber light directly from the ESP32's 5V or 3.3V pins. They require a separate, appropriately rated power supply. Always connect the grounds together.

## 🚀 Installation & Setup

### 1. Find Your Printer's Network Info

You need three pieces of information from your Bambu Lab printer:
1.  **IP Address:** (e.g., `192.168.1.100`)
2.  **Serial Number:** (e.g., `01S00A000000000`)
3.  **Access Code:** (Found in the printer's settings menu under `Network` -> `WLAN` -> `Access Code`)

### 2. Flash the ESP32

1.  Open the `BambuLed.ino` file in the Arduino IDE.
2.  Select your ESP32 board (e.g., "ESP32-S3 DEV Module") from the "Tools" menu.
3.  **Important:** Ensure your Partition Scheme provides space for OTA and LittleFS (e.g., "16M Flash (3M APP/9.9M FATFS)" or a similar "OTA" variant). Ensure PSRAM is enabled in the settings.
4.  Ensure you have the required libraries installed (e.g., `WiFiManager`, `PubSubClient`, `ArduinoJson`, `FastLED`, `LittleFS`).
5.  Compile and upload the sketch to your ESP32.

### 3. Initial WiFi & MQTT Configuration

On its first boot (or after a factory reset), the ESP32 will not find any saved WiFi credentials.
1.  On your phone or computer, scan for WiFi networks.
2.   Connect to the Access Point named **"BambuLightSetup"**.
3.   Use the password **"password"**.
4.  A captive portal (like a hotel WiFi login) should pop up automatically. If not, open a browser and go to `http://192.168.4.1`.
5.  Click **"Configure WiFi"**.
6.  Select your home WiFi network (SSID) and enter its password.
7.   **Crucially, fill in the custom parameters:**
    * Bambu Printer IP
    * Printer Serial
    * Access Code (MQTT Pass)
8.  Click **"Save"**.  The ESP32 will save all settings, connect to your WiFi, and reboot.
9.  You can find the device's IP address from your router or by monitoring the Serial output in the Arduino IDE.

## 🖥️ Using the Web Interface

Once connected, you can access the controller by visiting its IP address in a web browser.

### Status Page (`/`)
![alt text](https://github.com/eddwatts/BambuLED/blob/a28694ac57b4b747e026ee08147ecbdc9329c466/Screenshot-Status.png "Status Page")

 This is the main dashboard, which auto-refreshes every 3 seconds. It shows:
*  **Connection Status:** WiFi network, device IP, and MQTT connection status.
*  **Printer Status:** Live GCODE state, print percentage, layer, time remaining, and temperatures.
*  **Light Status:** The current state of your external light (On/Off, brightness) and the control mode (Auto/Manual).
*  **LED Status:** A text description of the LED bar's current state (e.g., "Printing (50%)") and a virtual preview bar.
*  **Manual Control:** Buttons to turn the external light **On**, **Off**, or set it back to **Auto** mode.

### Configuration Page (`/config`)
![alt text](https://github.com/eddwatts/BambuLED/blob/8445dd3f65be4448c6a89b5107ff864ef8ba02c7/Screenshot%20config.png "Config Page")

 This page allows you to change *all* device settings (except for WiFi) *after* the initial setup.
*  **Printer Settings:** Change the IP, Serial, or Access Code.
*  **Time Settings:** Set your NTP server and select your local Timezone for accurate logs.
*  **External Light Settings:**
    *  `GPIO Pin`: Change the pin used to control your light.
    *  `Brightness`: Set the brightness (0-100%) for when the light is "On".
    *  `Invert Logic`: Check this if your MOSFET/relay is Active LOW.
    *  `Enable 2-Min Finish Timeout`: Check to have the light turn off 2 minutes after a print finishes.
*  **LED Status Bar Settings:**
    *  `Number of LEDs`: Set how many WS2812B LEDs are in your strip.
    *  `Enable 2-Min Finish Timeout`: Check to have the green "Finish" color revert to "Idle" after 2 minutes.
    *  **Live Preview:** A virtual bar shows you what your LED settings will look like in each state.
    *  **LED States:** Configure the hex color code (RRGGBB) and brightness (0-255) for all five printer states: Idle, Printing, Paused, Error, and Finish.

 **Saving:** Clicking **"Save and Reboot"** will store your new settings to `config.json` and reboot the ESP32 to apply them.

### Backup & Restore

At the bottom of the `/config` page, you can:
*  **Backup Configuration:** Download a `config.json` file of all your current settings.
* **Restore Configuration:** Upload a `config.json` file to restore settings.  This will reboot the device.

### Debugging Pages

*  **/mqtt:** Visit this page to see a history of the last 100 JSON messages received from the printer, with timestamps. This is extremely useful for debugging connection issues.
*  **/status.json:** This page provides the raw JSON data used to build the main status page.

## 💡 Troubleshooting & Notes

* **How to Change WiFi:** You cannot change the WiFi network from the `/config` page. You must perform a **Factory Reset**.
*  **Factory Reset:** To wipe all settings (WiFi, MQTT, pins, colors) and restart the WiFiManager portal, connect **GPIO 16 to GND** and then power on or reset the ESP32.
*  **Over-the-Air (OTA) Updates:** The device will appear in the Arduino IDE's "Network Ports" list with the hostname **`bambu-light-controller`**. You can upload new firmware over WiFi.  The LED strip will turn blue during the update.
*  **Invalid GPIO Pin:** The code includes checks to prevent using unsafe GPIO pins (like input-only or flash-reserved pins). If you enter an invalid pin, it will be ignored or reverted to the default.

---
