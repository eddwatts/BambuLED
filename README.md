# Bambu Lab MQTT Light Controller

This project transforms an ESP32 into a smart controller for your Bambu Lab 3D printer. It connects directly to the printer's local MQTT broker to monitor its status in real-time.

It provides two primary functions:
1.  **External Chamber Light:** Controls a dimmable external light (like a 5V/12V/24V LED strip) using a MOSFET, automatically syncing with the printer's light or manual overrides.
2.  **WS2812B Status Bar:** Drives a (FastLED) addressable LED strip to provide a clear visual indication of the printer's status (e.g., Idle, Printing, Paused, Error).

The device hosts a full web interface for live status monitoring, manual control, and deep configuration.



## 🌟 Key Features

* [cite_start]**Real-Time MQTT Monitoring:** Connects securely (MQTTS) to the printer's local broker to get instant status updates[cite: 40, 452].
* [cite_start]**Full Web Interface:** A responsive web UI to monitor printer status, control lights, and configure all settings[cite: 143, 301].
* **Dimmable External Light Control:**
    * [cite_start]Uses PWM for 0-100% brightness control of an external light[cite: 399].
    * [cite_start]Supports both Active HIGH and Active LOW logic (invert output) to work with any MOSFET or relay module[cite: 322, 400].
    * [cite_start]**Auto Mode:** Syncs with the printer's built-in light status[cite: 273, 532].
    * [cite_start]**Manual Control:** Override the light (On/Off) from the web UI[cite: 270, 271].
    * [cite_start]**Finish Timeout:** Optionally keeps the light ON when a print finishes, then automatically turns it OFF after 2 minutes [cite: 122-125, 285].
* **WS2812B (FastLED) Status Bar:**
    * [cite_start]Displays printer progress as a loading bar when printing [cite: 391-395].
    * [cite_start]Shows distinct colors for Idle, Paused, Error, and Finished states [cite: 384-390].
    * [cite_start]All colors and brightness levels are fully customizable via the web UI [cite: 290-295, 339-363].
    * [cite_start]Optional 2-minute "Finish" light timeout before reverting to Idle status [cite: 126, 386-390].
* **Easy Setup & Configuration:**
    * Uses **WiFiManager** for initial setup. [cite_start]Just connect to the "BambuLightSetup" WiFi portal[cite: 103].
    * [cite_start]All settings are stored in `config.json` on the ESP32's file system (LittleFS) [cite: 407-414].
    * [cite_start]**Configuration Web Page:** A dedicated `/config` page to adjust all settings after initial setup[cite: 301].
* **System & Debugging:**
    * [cite_start]**OTA Updates:** Supports Over-the-Air firmware updates (hostname: `bambu-light-controller`) [cite: 13, 118, 130-137].
    * [cite_start]**Factory Reset:** Grounding a specific pin (GPIO 16) on boot will wipe all settings [cite: 18, 58-60, 67].
    * [cite_start]**Backup/Restore:** Download or upload your `config.json` file from the config page [cite: 249, 251, 365-367].
    * [cite_start]**MQTT History:** A debug page (`/mqtt`) shows the last 100 raw messages received from the printer[cite: 240, 242].
    * [cite_start]**Time Syncing:** Uses an NTP server and configurable timezones to provide accurate timestamps in the logs [cite: 105-107, 141].

## 🔌 Hardware Requirements & Wiring

* **ESP32 Module:** This project is memory-intensive. **A board with 16MB of Flash (N16) and 8MB of PSRAM (R8), such as an ESP32-S3-N16R8, is highly recommended.**
    * [cite_start]**Why 16MB Flash (N16)?** The large flash provides ample space for the application, the LittleFS file system [cite: 10, 61][cite_start], and Over-the-Air (OTA) updates[cite: 13, 110], which requires partitions large enough to hold two full copies of the firmware.
    * [cite_start]**Why 8MB PSRAM (R8)?** The code uses `WiFiClientSecure` (SSL) [cite: 39] [cite_start]and parses large (4KB+) JSON payloads from MQTT[cite: 40, 462]. [cite_start]The firmware is **optimized to use this external PSRAM** [cite: 63] [cite_start]to store the MQTT message history[cite: 65, 461, 47], which prevents "Out of Memory" crashes and ensures stability. A standard ESP32 without PSRAM may be unstable.
* **External Light Hardware:**
    * A high-power 12V or 24V LED strip for chamber lighting.
    * A logic-level N-Channel MOSFET module (like an IRF520) or a Relay Module to safely control the high-voltage light from the ESP32's 3.3V pin.
* **Status Bar (Optional):** A WS2812B (e.g., NeoPixel) addressable LED strip.
* **Reset Button (Optional):** A momentary push button or jumper wire for the factory reset pin.

### Wiring Diagram

| ESP32 Pin | Connects To | Purpose |
| :--- | :--- | :--- |
| **GPIO 4** (Hardcoded) | **WS2812B Data In** | [cite_start]FastLED Status Bar Data [cite: 3, 83] |
| **GPIO 14** (Default) | **MOSFET/Relay Signal Pin** | External Chamber Light Control. [cite_start]*This pin is configurable in the web UI.* [cite: 17, 35, 71] |
| **GPIO 16** (Hardcoded) | **Momentary Button to GND** | Factory Reset Pin. [cite_start]*Ground this pin **during boot** to wipe settings.* [cite: 18, 58] |
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
2.  [cite_start]Connect to the Access Point named **"BambuLightSetup"**[cite: 103].
3.  [cite_start]Use the password **"password"**[cite: 103].
4.  A captive portal (like a hotel WiFi login) should pop up automatically. If not, open a browser and go to `http://192.168.4.1`.
5.  Click **"Configure WiFi"**.
6.  Select your home WiFi network (SSID) and enter its password.
7.  [cite_start]**Crucially, fill in the custom parameters:** [cite: 16, 17]
    * Bambu Printer IP
    * Printer Serial
    * Access Code (MQTT Pass)
8.  Click **"Save"**. [cite_start]The ESP32 will save all settings[cite: 96, 432], connect to your WiFi, and reboot.
9.  You can find the device's IP address from your router or by monitoring the Serial output in the Arduino IDE.

## 🖥️ Using the Web Interface

Once connected, you can access the controller by visiting its IP address in a web browser.

### Status Page (`/`)

[cite_start]This is the main dashboard, which auto-refreshes every 3 seconds[cite: 209]. It shows:
* [cite_start]**Connection Status:** WiFi network, device IP, and MQTT connection status [cite: 156-159, 185].
* [cite_start]**Printer Status:** Live GCODE state, print percentage, layer, time remaining, and temperatures [cite: 161-164, 187-190].
* [cite_start]**Light Status:** The current state of your external light (On/Off, brightness) and the control mode (Auto/Manual) [cite: 166, 191-192].
* [cite_start]**LED Status:** A text description of the LED bar's current state (e.g., "Printing (50%)") and a virtual preview bar [cite: 170-173, 194-207].
* [cite_start]**Manual Control:** Buttons to turn the external light **On**, **Off**, or set it back to **Auto** mode [cite: 175, 270-276].

### Configuration Page (`/config`)

[cite_start]This page allows you to change *all* device settings (except for WiFi) *after* the initial setup[cite: 301].
* [cite_start]**Printer Settings:** Change the IP, Serial, or Access Code [cite: 312-315, 278].
* [cite_start]**Time Settings:** Set your NTP server and select your local Timezone for accurate logs [cite: 316-318, 279-280, 537-547].
* [cite_start]**External Light Settings:** [cite: 319-324]
    * [cite_start]`GPIO Pin`: Change the pin used to control your light [cite: 281-282].
    * [cite_start]`Brightness`: Set the brightness (0-100%) for when the light is "On"[cite: 284].
    * [cite_start]`Invert Logic`: Check this if your MOSFET/relay is Active LOW[cite: 284].
    * [cite_start]`Enable 2-Min Finish Timeout`: Check to have the light turn off 2 minutes after a print finishes[cite: 285].
* [cite_start]**LED Status Bar Settings:** [cite: 325-329]
    * [cite_start]`Number of LEDs`: Set how many WS2812B LEDs are in your strip[cite: 286].
    * [cite_start]`Enable 2-Min Finish Timeout`: Check to have the green "Finish" color revert to "Idle" after 2 minutes[cite: 289].
    * [cite_start]**Live Preview:** A virtual bar shows you what your LED settings will look like in each state [cite: 330-338, 369-381].
    * [cite_start]**LED States:** Configure the hex color code (RRGGBB) and brightness (0-255) for all five printer states: Idle, Printing, Paused, Error, and Finish [cite: 290-295, 339-363].

[cite_start]**Saving:** Clicking **"Save and Reboot"** will store your new settings to `config.json` [cite: 297] [cite_start]and reboot the ESP32 to apply them [cite: 299-300].

### Backup & Restore

At the bottom of the `/config` page, you can:
* [cite_start]**Backup Configuration:** Download a `config.json` file of all your current settings [cite: 249-250, 366].
* **Restore Configuration:** Upload a `config.json` file to restore settings. [cite_start]This will reboot the device [cite: 251-269, 367].

### Debugging Pages

* [cite_start]**/mqtt:** Visit this page to see a history of the last 100 JSON messages received from the printer, with timestamps [cite: 240-248]. This is extremely useful for debugging connection issues.
* [cite_start]**/status.json:** This page provides the raw JSON data used to build the main status page [cite: 211-239].

## 💡 Troubleshooting & Notes

* **How to Change WiFi:** You cannot change the WiFi network from the `/config` page. You must perform a **Factory Reset**.
* [cite_start]**Factory Reset:** To wipe all settings (WiFi, MQTT, pins, colors) and restart the WiFiManager portal, connect **GPIO 16 to GND** and then power on or reset the ESP32 [cite: 18, 58-60, 67, 96].
* [cite_start]**Over-the-Air (OTA) Updates:** The device will appear in the Arduino IDE's "Network Ports" list with the hostname **`bambu-light-controller`**[cite: 130]. You can upload new firmware over WiFi. [cite_start]The LED strip will turn blue during the update [cite: 131-132].
* [cite_start]**Invalid GPIO Pin:** The code includes checks to prevent using unsafe GPIO pins (like input-only or flash-reserved pins) [cite: 138-140, 283]. If you enter an invalid pin, it will be ignored or reverted to the default.

---
