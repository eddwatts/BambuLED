#include "web_handlers.h"
#include "config.h"
#include "light_controller.h"
#include "led_controller.h"
#include "mqtt_handler.h"
#include <ArduinoJson.h>
#include <WebSocketsServer.h> // <-- Added for WebSockets

// External declaration for the WebSocket server instance from the .ino file
extern WebSocketsServer webSocket;

// --- PROGMEM HTML for Root Page (Suggestion 4A + 5) ---
const char PAGE_ROOT[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Bambu Light Status</title>
  <!-- Suggestion 5: Favicon -->
  <link rel="icon" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'%3E%3Cpath fill='%23378cf0' d='M50 0C27.9 0 10 17.9 10 40c0 14.9 8.2 27.8 20 34.8V85c0 2.8 2.2 5 5 5h30c2.8 0 5-2.2 5-5v-10.2c11.8-7 20-19.9 20-34.8C90 17.9 72.1 0 50 0zM50 75h-0.1V67.8c-1.6 0.1-3.2 0.2-4.9 0.2s-3.3-0.1-4.9-0.2V75H25V62.4c-9.1-5.9-15-16.1-15-27.4C10 17.8 27.8 0 50 0s40 17.8 40 40c0 11.3-5.9 21.5-15 27.4V75h-15V67.8c-1.6 0.1-3.2 0.2-4.9 0.2s-3.3-0.1-4.9-0.2V75z'/%3E%3C/svg%3E">
  <!-- Suggestion 5: New CSS -->
  <style>
    :root {
      --bg-color: #1a1a1b;
      --card-color: #2c2c2e;
      --border-color: #444;
      --text-color: #e0e0e0;
      --text-color-muted: #aaa;
      --text-color-bright: #ffffff;
      --primary-color: #378cf0;
      --green-color: #28a745;
      --red-color: #dc3545;
      --grey-color: #6c757d;
    }
    body { 
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
      margin: 0; 
      padding: 1rem;
      background-color: var(--bg-color); 
      color: var(--text-color); 
      max-width: 900px;
      margin: 0 auto;
    }
    .logo_title_wrapper {
      display: flex;
      align-items: center;
      gap: 15px;
      margin-bottom: 1rem;
      color: var(--text-color-bright);
      border-bottom: 2px solid var(--primary-color);
      padding-bottom: 1rem;
    }
    .logo { flex-shrink: 0; }
    h1 { margin: 0; font-size: 1.75rem; }
    h2 { 
      color: var(--text-color-bright);
      border-bottom: 1px solid var(--border-color);
      padding-bottom: 8px;
      margin-top: 2rem;
    }
    .status_grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
      gap: 1rem;
    }
    .status { 
      padding: 15px; 
      margin-bottom: 0;
      border-radius: 8px; 
      transition: background-color 0.5s, border-color 0.5s; 
      background-color: var(--card-color); 
      border: 1px solid var(--border-color); 
    }
    .status strong {
      display: block;
      font-size: 0.9rem;
      color: var(--text-color-muted);
      margin-bottom: 5px;
      text-transform: uppercase;
    }
    span.data { 
      font-weight: 600; 
      font-size: 1.1rem;
      color: var(--text-color-bright);
    }
    .connected { background-color: #1a3a24; color: #8cda9b; border-color: #336d3f; }
    .disconnected { background-color: #401f22; color: #f0989f; border-color: #7c333a; }
    .warning { background-color: #423821; color: #f0d061; border-color: #7e6c33; }
    .light-on { background-color: #1c314a; color: #9cc2ef; border-color: #335d88; }
    .error { background-color: #401f22; color: #f0989f; border-color: #7c333a; font-weight: bold; }
    
    .button-group { display: flex; gap: 10px; flex-wrap: wrap; }
    button, .button { 
      background-color: var(--primary-color); 
      color: var(--text-color-bright); 
      padding: 10px 15px; 
      border: none; 
      border-radius: 5px; 
      cursor: pointer; 
      font-size: 1rem;
      font-weight: 500;
      text-decoration: none;
      transition: background-color 0.2s, transform 0.1s;
    }
    button:hover { background-color: #58a6ff; }
    button:active { transform: scale(0.98); }
    button.off, .button.off { background-color: var(--grey-color); }
    button.auto, .button.auto { background-color: var(--green-color); }
    button.danger, .button.danger { background-color: var(--red-color); }
    .vled { transition: background-color 0.5s, opacity 0.5s; }
    small { color: var(--text-color-muted); font-size: 0.9rem; }
  </style>
</head>
<body>
  <!-- Suggestion 5: Logo -->
  <div class="logo_title_wrapper">
    <svg class="logo" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100" fill="#378cf0" width="50" height="50">
      <path d="M50 0C27.9 0 10 17.9 10 40c0 14.9 8.2 27.8 20 34.8V85c0 2.8 2.2 5 5 5h30c2.8 0 5-2.2 5-5v-10.2c11.8-7 20-19.9 20-34.8C90 17.9 72.1 0 50 0zM50 75h-0.1V67.8c-1.6 0.1-3.2 0.2-4.9 0.2s-3.3-0.1-4.9-0.2V75H25V62.4c-9.1-5.9-15-16.1-15-27.4C10 17.8 27.8 0 50 0s40 17.8 40 40c0 11.3-5.9 21.5-15 27.4V75h-15V67.8c-1.6 0.1-3.2 0.2-4.9 0.2s-3.3-0.1-4.9-0.2V75z"/>
    </svg>
    <h1>Bambu Light Controller</h1>
  </div>
  
  <!-- Suggestion 5: New Grid Layout -->
  <div class="status_grid">
    <div id="wifi-status-div" class="status {{WIFI_STATUS_CLASS}}">
      <strong>WiFi Status</strong>
      <span class="data">{{WIFI_STATUS}}</span>
    </div>
    
    <div id="mqtt-status-div" class="status disconnected">
      <strong>MQTT Status</strong>
      <span id="mqtt-status" class="data">DISCONNECTED</span>
      <br><small><a href="/mqtt" target="_blank">View MQTT History</a></small>
    </div>
  </div>

  <h2>Printer Status</h2>
  <div class="status_grid">
    <div class="status"><strong>GCODE State</strong><span id="gcode-state" class="data">N/A</span></div>
    <div class="status"><strong>Print Progress</strong><span id="print-percent" class="data">0 %</span></div>
    <div class="status"><strong>Current Layer</strong><span id="layer-num" class="data">0</span></div>
    <div class="status"><strong>Time Remaining</strong><span id="print-time" class="data">--:--:--</span></div>
    <div class="status"><strong>Nozzle Temp</strong><span id="nozzle-temp" class="data">0.0 / 0.0 &deg;C</span></div>
    <div class="status"><strong>Bed Temp</strong><span id="bed-temp" class="data">0.0 / 0.0 &deg;C</span></div>
    <div class="status"><strong>Print Stage</strong><span id="stage" class="data">N/A</span></div>
    <div class="status"><strong>WiFi Signal</strong><span id="wifi-signal" class="data">N/A</span></div>
  </div>

  <h2>External Outputs</h2>
  <div class="status_grid">
    <div id="light-status-div" class="status disconnected">
      <strong>External Light (Pin {{LIGHT_PIN}})</strong>
      <span id="light-status" class="data">N/A</span>
      <br><small><strong>Mode:</strong> <span id="light-mode" class="data">N/A</span>
      | <strong>Logic:</strong> {{LIGHT_LOGIC}}
      | <strong>Bambu:</strong> <span id="bambu-light-mode" class="data">N/A</span></small>
    </div>

    <div id="led-status-div" class="status disconnected">
      <strong>LED Status Bar (Pin {{LED_PIN}} / {{LED_COUNT}} LEDs)</strong>
      <span id="led-status" class="data">N/A</span>
      <div id='virtual-bar-container' style='margin-top: 10px;'>
        <div id='virtual-bar' style='display: flex; width: 100%; height: 20px; background: #222; border-radius: 5px; overflow: hidden; border: 1px solid #444;'>
          {{VIRTUAL_LEDS}}
        </div>
      </div>
    </div>
  </div>

  <h2>Manual Control</h2>
  <div class="button-group">
    <!-- Suggestion 3: Updated buttons for WebSockets -->
    <button id="btn-on" class="button">Turn Light ON</button>
    <button id="btn-off" class="button off">Turn Light OFF</button>
    <button id="btn-auto" class="button auto">Set to AUTO</button>
  </div>

  <hr style="border: 0; border-top: 1px solid var(--border-color); margin: 2rem 0;">
  
  <div class="button-group">
    <a href="/config" class="button">Change Device Settings</a>
  </div>
  
  <!-- Suggestion 3: New WebSocket JavaScript -->
  <script>
    var ws;
    
    function formatTime(s) {
      if (s <= 0) return '--:--:--';
      let h = Math.floor(s / 3600); s %= 3600;
      let m = Math.floor(s / 60); s %= 60;
      let m_str = m < 10 ? '0' + m : m;
      let s_str = s < 10 ? '0' + s : s;
      return h > 0 ? h + ':' + m_str + ':' + s_str : m_str + ':' + s_str;
    }

    function updateUI(data) {
      try {
        document.getElementById('mqtt-status').innerText = data.mqtt_connected ? 'CONNECTED' : 'DISCONNECTED';
        document.getElementById('mqtt-status-div').className = 'status ' + (data.mqtt_connected ? 'connected' : 'disconnected');
        
        document.getElementById('gcode-state').innerText = data.gcode_state;
        document.getElementById('print-percent').innerText = data.print_percentage + ' %';
        document.getElementById('layer-num').innerText = data.layer_num;
        document.getElementById('stage').innerText = data.stage;
        document.getElementById('print-time').innerText = formatTime(data.time_remaining);
        document.getElementById('nozzle-temp').innerHTML = data.nozzle_temp.toFixed(1) + ' / ' + data.nozzle_target_temp.toFixed(1) + ' &deg;C';
        document.getElementById('bed-temp').innerHTML = data.bed_temp.toFixed(1) + ' / ' + data.bed_target_temp.toFixed(1) + ' &deg;C';
        document.getElementById('wifi-signal').innerText = data.wifi_signal;
        
        document.getElementById('light-status').innerText = data.light_is_on ? ('ON (' + data.chamber_bright + '%)') : 'OFF';
        document.getElementById('light-status-div').className = 'status ' + (data.light_is_on ? 'light-on' : 'disconnected');
        document.getElementById('light-mode').innerText = data.manual_control ? 'MANUAL' : ('AUTO' + data.light_mode_extra);
        document.getElementById('bambu-light-mode').innerText = data.bambu_light_mode;
        
        document.getElementById('led-status').innerText = data.led_status_str;
        document.getElementById('led-status-div').className = 'status ' + data.led_status_class;

        let color = data.led_color_val.toString(16).padStart(6, '0');
        let brightness = data.led_bright_val;
        let opacity = (brightness / 255).toFixed(2);
        let vleds = document.querySelectorAll('#virtual-bar .vled');
        let numLeds = vleds.length;
        
        if (data.is_printing && data.print_percentage > 0 && numLeds > 0) {
          let leds_to_light = Math.ceil((data.print_percentage / 100) * numLeds);
          for (let i = 0; i < numLeds; i++) {
            if (i < leds_to_light) {
              vleds[i].style.backgroundColor = '#' + color;
              vleds[i].style.opacity = opacity;
            } else {
              vleds[i].style.backgroundColor = '#000';
              vleds[i].style.opacity = '1.0';
            }
          }
        } else {
          vleds.forEach(led => {
            led.style.backgroundColor = '#' + color;
            led.style.opacity = opacity;
          });
        }
      } catch (e) { console.error('Error updating UI:', e); }
    }

    function connectWebSocket() {
      console.log('Connecting WebSocket...');
      ws = new WebSocket('ws://' + window.location.hostname + ':81/');
      ws.onopen = function() { console.log('WebSocket connected.'); };
      ws.onmessage = function(evt) {
        const data = JSON.parse(evt.data);
        updateUI(data);
      };
      ws.onclose = function() {
        console.log('WebSocket disconnected. Reconnecting in 3s...');
        setTimeout(connectWebSocket, 3000);
      };
      ws.onerror = function(err) {
        console.error('WebSocket Error:', err);
        ws.close();
      };
    }
    
    document.addEventListener('DOMContentLoaded', () => {
      connectWebSocket();
      
      // Add click listeners for WebSocket buttons
      document.getElementById('btn-on').addEventListener('click', (e) => {
        e.preventDefault();
        console.log('Sending LIGHT_ON');
        ws.send('LIGHT_ON');
      });
      document.getElementById('btn-off').addEventListener('click', (e) => {
        e.preventDefault();
        console.log('Sending LIGHT_OFF');
        ws.send('LIGHT_OFF');
      });
      document.getElementById('btn-auto').addEventListener('click', (e) => {
        e.preventDefault();
        console.log('Sending LIGHT_AUTO');
        ws.send('LIGHT_AUTO');
      });
    });
  </script>
</body>
</html>
)rawliteral";

// --- PROGMEM HTML for Config Page (Suggestion 4A + 5) ---
const char PAGE_CONFIG[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <title>Bambu Light Config</title><style>
    body{font-family:-apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;margin:20px;background:#1a1a1b;color:#e0e0e0;}
    h1,h2,h3{color:#fff;} h2{border-bottom:2px solid #378cf0;padding-bottom:5px;}
    form{background:#2c2c2e;padding:20px;border-radius:8px;}
    div{margin-bottom:15px;} label{display:block;margin-bottom:5px;font-weight:bold;color:#ccc;}
    input[type='text'],input[type='number'],input[type='password'],select{width:98%;max-width:400px;padding:8px;border:1px solid #555;border-radius:4px;font-family:Arial,sans-serif;font-size:1em;background-color:#3a3a3c;color:#e0e0e0;}
    input[type='checkbox']{margin-right:10px;vertical-align:middle;}
    label[for='invert'], label[for='chamber_timeout'], label[for='led_finish_timeout'] { display:inline-block;font-weight:normal; }
    button, .button { 
      background-color: #378cf0; color: #ffffff; padding: 12px 20px; border: none; 
      border-radius: 5px; cursor: pointer; font-size: 16px; text-decoration: none; display: inline-block;
      font-weight: 500;
    }
    button:hover{background-color:#0056b3;}
    .color-input{width:100px;padding:8px;vertical-align:middle;margin-left:10px;border:1px solid #555;}
    .grid{display:grid;grid-template-columns:repeat(auto-fit, minmax(200px, 1fr));grid-gap:20px;}
    .card{background:#3a3a3c;padding:15px;border-radius:5px;border:1px solid #555;}
    small{color:#aaa;} a{color:#58a6ff;}
    .color-swatch{width: 20px; height: 20px; display: inline-block; vertical-align: middle; margin-left: 10px; border: 1px solid #555; border-radius: 4px; background-color: #000;}
    button.danger, .button.danger { background-color: #dc3545; }
    </style></head><body><h1>Bambu Light Controller Settings</h1>
    <p><small>To change Wi-Fi, use the 'Factory Reset' pin (GPIO 16) on boot.</small></p>
    <form action='/config' method='POST'>
    <h2>Printer Settings</h2>
    <div><label for='ip'>Bambu Printer IP</label><input type='text' id='ip' name='ip' value='{{BBL_IP}}'></div>
    <div><label for='serial'>Printer Serial</label><input type='text' id='serial' name='serial' value='{{BBL_SERIAL}}'></div>
    <div><label for='code'>Access Code (MQTT Pass)</label><input type='password' id='code' name='code' value='{{BBL_CODE}}'></div>
    <h2>Time Settings</h2>
    <div class='grid'>
    <div class='card'><div><label for='ntp_server'>NTP Server</label><input type='text' id='ntp_server' name='ntp_server' value='{{NTP_SERVER}}'></div></div>
    <div class='card'><div><label for='timezone'>Timezone</label>{{TIMEZONE_DROPDOWN}}</div></div>
    </div>
    <h2>External Light Settings</h2>
    <div class='grid'>
    <div class='card'><div><label for='lightpin'>External Light GPIO Pin</label><input type='number' id='lightpin' name='lightpin' min='0' max='39' value='{{LIGHT_PIN}}'></div>
    <div><label for='chamber_bright'>Brightness (0-100%)</label><input type='number' id='chamber_bright' name='chamber_bright' min='0' max='100' value='{{CHAMBER_BRIGHT}}'></div></div>
    <div class='card'><div><input type='checkbox' id='invert' name='invert' value='1' {{INVERT_CHECKED}}><label for='invert'>Invert Light Logic (Active LOW)</label></div>
    <div><input type='checkbox' id='chamber_timeout' name='chamber_timeout' value='1' {{CHAMBER_TIMEOUT_CHECKED}}><label for='chamber_timeout'>Enable 2-Min Finish Timeout (Light OFF)</label></div></div>
    </div>
    <h2>LED Status Bar Settings</h2>
    <div class='grid'>
    <div class='card'><div><label for='numleds'>Number of WS2812B LEDs (Max {{MAX_LEDS}})</label><input type='number' id='numleds' name='numleds' min='0' max='{{MAX_LEDS}}' value='{{NUM_LEDS}}'></div></div>
    <div class='card'><div><label for='led_color_order'>LED Color Order</label>{{LED_ORDER_DROPDOWN}}</div></div>
    </div>
    <div><small>LED Data Pin is hardcoded to GPIO {{LED_DATA_PIN}} for FastLED.</small></div>
    <div><input type='checkbox' id='led_finish_timeout' name='led_finish_timeout' value='1' {{LED_TIMEOUT_CHECKED}}><label for='led_finish_timeout'>Enable 2-Min Finish Timeout (LEDs return to Idle)</label></div>
    <h3>Virtual LED Preview</h3>
    <div class='card' style='padding: 20px; background-color: #2c2c2e; border: 1px solid #555; border-radius: 5px;'>
    <div id='virtual-bar-container'>
    <div id='virtual-bar' style='display: flex; width: 100%; height: 30px; background: #222; border-radius: 5px; overflow: hidden; border: 1px solid #555;'>
    {{VIRTUAL_LEDS}}
    </div></div>
    <div id='preview-controls' style='margin-top: 15px; display: flex; flex-wrap: wrap; gap: 15px;'>
    <label style='display:inline-block; color:#e0e0e0;'><input type='radio' name='preview_state' value='idle' onchange='updatePreview()' checked> Idle</label>
    <label style='display:inline-block; color:#e0e0e0;'><input type='radio' name='preview_state' value='print' onchange='updatePreview()'> Printing</label>
    <label style='display:inline-block; color:#e0e0e0;'><input type='radio' name='preview_state' value='pause' onchange='updatePreview()'> Paused</label>
    <label style='display:inline-block; color:#e0e0e0;'><input type='radio' name='preview_state' value='error' onchange='updatePreview()'> Error</label>
    <label style='display:inline-block; color:#e0e0e0;'><input type='radio' name='preview_state' value='finish' onchange='updatePreview()'> Finish</label>
    </div></div>
    <h3>LED States</h3><div class='grid'>
    <div class='card'><h4>Idle Status</h4>
    <div><label for='idle_color'>Color (RRGGBB) <span id='idle_color_swatch' class='color-swatch'></span></label><input type='text' id='idle_color' name='idle_color' value='{{IDLE_COLOR}}' oninput='updatePreview(); try { document.getElementById("idle_color_picker").value = "#" + this.value; } catch(e) {}'><input type='color' class='color-input' id='idle_color_picker' value='#{{IDLE_COLOR}}' onchange='document.getElementById("idle_color").value = this.value.substring(1).toUpperCase(); updatePreview();'></div>
    <div><label for='idle_bright'>Brightness (0-255)</label><input type='number' id='idle_bright' name='idle_bright' min='0' max='255' value='{{IDLE_BRIGHT}}' oninput='updatePreview()'></div></div>
    <div class='card'><h4>Printing Status</h4>
    <div><label for='print_color'>Color (RRGGBB) <span id='print_color_swatch' class='color-swatch'></span></label><input type='text' id='print_color' name='print_color' value='{{PRINT_COLOR}}' oninput='updatePreview(); try { document.getElementById("print_color_picker").value = "#" + this.value; } catch(e) {}'><input type='color' class='color-input' id='print_color_picker' value='#{{PRINT_COLOR}}' onchange='document.getElementById("print_color").value = this.value.substring(1).toUpperCase(); updatePreview();'></div>
    <div><label for='print_bright'>Brightness (0-255)</label><input type='number' id='print_bright' name='print_bright' min='0' max='255' value='{{PRINT_BRIGHT}}' oninput='updatePreview()'></div></div>
    <div class='card'><h4>Paused Status</h4>
    <div><label for='pause_color'>Color (RRGGBB) <span id='pause_color_swatch' class='color-swatch'></span></label><input type='text' id='pause_color' name='pause_color' value='{{PAUSE_COLOR}}' oninput='updatePreview(); try { document.getElementById("pause_color_picker").value = "#" + this.value; } catch(e) {}'><input type='color' class='color-input' id='pause_color_picker' value='#{{PAUSE_COLOR}}' onchange='document.getElementById("pause_color").value = this.value.substring(1).toUpperCase(); updatePreview();'></div>
    <div><label for='pause_bright'>Brightness (0-255)</label><input type='number' id='pause_bright' name='pause_bright' min='0' max='255' value='{{PAUSE_BRIGHT}}' oninput='updatePreview()'></div></div>
    <div class='card'><h4>Error Status</h4>
    <div><label for='error_color'>Color (RRGGBB) <span id='error_color_swatch' class='color-swatch'></span></label><input type='text' id='error_color' name='error_color' value='{{ERROR_COLOR}}' oninput='updatePreview(); try { document.getElementById("error_color_picker").value = "#" + this.value; } catch(e) {}'><input type='color' class='color-input' id='error_color_picker' value='#{{ERROR_COLOR}}' onchange='document.getElementById("error_color").value = this.value.substring(1).toUpperCase(); updatePreview();'></div>
    <div><label for='error_bright'>Brightness (0-255)</label><input type='number' id='error_bright' name='error_bright' min='0' max='255' value='{{ERROR_BRIGHT}}' oninput='updatePreview()'></div></div>
    <div class='card'><h4>Finish Status</h4>
    <div><label for='finish_color'>Color (RRGGBB) <span id='finish_color_swatch' class='color-swatch'></span></label><input type='text' id='finish_color' name='finish_color' value='{{FINISH_COLOR}}' oninput='updatePreview(); try { document.getElementById("finish_color_picker").value = "#" + this.value; } catch(e) {}'><input type='color' class='color-input' id='finish_color_picker' value='#{{FINISH_COLOR}}' onchange='document.getElementById("finish_color").value = this.value.substring(1).toUpperCase(); updatePreview();'></div>
    <div><label for='finish_bright'>Brightness (0-255)</label><input type='number' id='finish_bright' name='finish_bright' min='0' max='255' value='{{FINISH_BRIGHT}}' oninput='updatePreview()'></div></div>
    </div>
    <br><div><button type='submit'>Save and Reboot</button></div>
    </form>
    <h2>Backup & Restore</h2>
    <div class='grid'>
    <div class='card'><p>Download a backup of your current settings.</p><a href='/backup' class='button' style='background-color:#17a2b8;'>Backup Configuration</a></div>
    <div class='card'><p>Upload a 'config.json' file to restore settings. <b>This will reboot the device.</b></p><a href='/restore' class='button danger'>Restore Configuration</a></div>
    </div>
    <!-- Suggestion 5: Device Management Card -->
    <h2>Device Management</h2>
    <div class='grid'>
      <div class='card'>
        <p>Reboot the device. This does not change settings.</p>
        <a href='/reboot' onclick="return confirm('Are you sure you want to reboot?');">
          <button type='button' style='background-color:#ffc107; color: #1a1a1b;'>Reboot Device</button>
        </a>
      </div>
      <div class='card'>
        <p><b>DANGER:</b> Wipes all settings (including Wi-Fi) and reboots.</p>
        <a href='/factory_reset' onclick="return confirm('WARNING: This will wipe ALL settings. Are you sure?');">
          <button type='button' class='danger'>Factory Reset</button>
        </a>
      </div>
    </div>
    <br><p><a href='/'>&laquo; Back to Status Page</a></p>
    <script>
    function updatePreview() {
      try {
        let state = document.querySelector('input[name=\"preview_state\"]:checked').value;
        let color = document.getElementById(state + '_color').value;
        if (!color.match(/^[0-9a-fA-F]{6}$/)) { color = '000000'; }
        let bright = parseInt(document.getElementById(state + '_bright').value, 10);
        if (isNaN(bright) || bright < 0 || bright > 255) { bright = 0; }
        let opacity = (bright / 255).toFixed(2);
        let vleds = document.querySelectorAll('.vled');
        vleds.forEach(led => {
          led.style.backgroundColor = '#' + color;
          led.style.opacity = opacity;
        });
        let states = ['idle', 'print', 'pause', 'error', 'finish'];
        states.forEach(s => {
          let c = document.getElementById(s + '_color').value;
          if (!c.match(/^[0-9a-fA-F]{6}$/)) { c = '000000'; }
          document.getElementById(s + '_color_swatch').style.backgroundColor = '#' + c;
        });
      } catch (e) { console.error('Preview update failed:', e); }
    }
    document.addEventListener('DOMContentLoaded', updatePreview);
    </script>
    </body></html>
)rawliteral";

// --- PROGMEM HTML for MQTT Page (Suggestion 4A) ---
const char PAGE_MQTT[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><title>MQTT History</title>
  <meta name='viewport' content='width=device-width, initial-scale=1'>
  <style>
  body{font-family:monospace;margin:20px;background:#1a1a1b;color:#e0e0e0;}
  h1{color:#fff;font-family:Arial,sans-serif;}
  p{font-family:Arial,sans-serif;}
  pre{white-space:pre-wrap;word-wrap:break-word;font-size:0.9em;background:#2c2c2e;padding:10px;border-radius:5px;border:1px solid #444;}
  a{color:#58a6ff;text-decoration:none;font-family:Arial,sans-serif;}
  </style></head><body><h1>MQTT Message History</h1>
  <p>Showing the last {{MSG_COUNT}} of {{MAX_HISTORY_SIZE}} messages (oldest first).</p>
  <a href='/'>&laquo; Back to Status</a><br><br>
  <pre>{{MQTT_HISTORY}}</pre>
  </body></html>
)rawliteral";

// --- PROGMEM HTML for Restore Page (Suggestion 4A) ---
const char PAGE_RESTORE[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><title>Restore Config</title>
  <style>body{font-family:Arial,sans-serif;margin:20px;background:#1a1a1b;color:#e0e0e0;}
  h1{color:#fff;} p{color:#e0e0e0;}
  button{background-color:#dc3545;color:white;padding:12px 20px;border:none;border-radius:5px;cursor:pointer;font-size:16px;}
  a{color:#58a6ff;}
  </style></head>
  <body><h1>Restore Configuration</h1>
  <p><b>WARNING:</b> This will overwrite your current settings and reboot the device.</p>
  <form action='/restore' method='POST' enctype='multipart/form-data'>
  <input type='file' name='restore' accept='.json' required>
  <br><br><button type='submit'>Upload and Restore</button>
  </form><br><br><a href='/config'>&laquo; Back to Settings</a></body></html>
)rawliteral";


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
  // Create a String from the PROGMEM data
  String html = FPSTR(PAGE_ROOT);

  // Do all dynamic replacements
  String wifi_status_str = (WiFi.status() == WL_CONNECTED) ? "CONNECTED (" + WiFi.SSID() + " / " + WiFi.localIP().toString() + ")" : "DISCONNECTED";
  html.replace("{{WIFI_STATUS_CLASS}}", (WiFi.status() == WL_CONNECTED ? "connected" : "disconnected"));
  html.replace("{{WIFI_STATUS}}", wifi_status_str);

  html.replace("{{LIGHT_PIN}}", String(config.chamber_light_pin));
  html.replace("{{LIGHT_LOGIC}}", (config.invert_output ? "Active LOW" : "Active HIGH"));
  
  html.replace("{{LED_PIN}}", String(LED_DATA_PIN));
  html.replace("{{LED_COUNT}}", String(config.num_leds));

  String vled_html = "";
  for(int i=0; i < config.num_leds && i < MAX_LEDS; i++) {
    vled_html += "<div class='vled' style='flex-grow: 1; height: 100%;'></div>";
  }
  html.replace("{{VIRTUAL_LEDS}}", vled_html);
  
  // Send the processed string
  server.send(200, "text/html", html);
}

// --- Suggestion 3: Split JSON creation from the HTTP handler ---
void createStatusJson(DynamicJsonDocument& doc) {
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
          // Idle color will be picked by default
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
}

// --- Suggestion 3: New HTTP handler ---
void handleStatusJson() {
  DynamicJsonDocument doc(1024);
  createStatusJson(doc); // Call the new function
  
  String json_output;
  serializeJson(doc, json_output);
  server.send(200, "application/json", json_output);
}

// --- Suggestion 3: New WebSocket broadcast function ---
void broadcastWebSocketStatus() {
  DynamicJsonDocument doc(1024);
  createStatusJson(doc); // Create the JSON
  
  String json_output;
  serializeJson(doc, json_output);
  
  // Send it to ALL connected web clients
  webSocket.broadcastTXT(json_output);
}

void handleMqttJson() {
  Serial.println("Web Request: /mqtt (View JSON History)");
  
  String html = FPSTR(PAGE_MQTT);
  html.replace("{{MSG_COUNT}}", String(mqtt_history.size()));
  html.replace("{{MAX_HISTORY_SIZE}}", String(MAX_HISTORY_SIZE));
  
  String history_html = "";
  if(mqtt_history.empty()) {
    history_html = "No data received yet.";
  } else {
    for(auto it = mqtt_history.begin(); it != mqtt_history.end(); ++it) {
      String msg = *it;
      msg.replace("<", "&lt;");
      msg.replace(">", "&gt;");
      history_html += msg + "\n";
    }
  }
  html.replace("{{MQTT_HISTORY}}", history_html);

  server.send(200, "text/html", html);
}

// --- Suggestion 3: Update button handlers to broadcast changes ---
void handleLightOn() {
  Serial.println("Web Request: /light/on");
  manual_light_control = true;
  setChamberLightState(true);
  
  if (server.client().localIP() == 0) { // Check if this is a WebSocket call (no client IP)
      broadcastWebSocketStatus(); // Send update to all clients
  } else {
      server.sendHeader("Location", "/");
      server.send(302, "text/plain", "");
  }
}

void handleLightOff() {
  Serial.println("Web Request: /light/off");
  manual_light_control = true;
  setChamberLightState(false);
  
  if (server.client().localIP() == 0) {
      broadcastWebSocketStatus();
  } else {
      server.sendHeader("Location", "/");
      server.send(302, "text/plain", "");
  }
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

  if (server.client().localIP() == 0) {
      broadcastWebSocketStatus();
  } else {
      server.sendHeader("Location", "/");
      server.send(302, "text/plain", "");
  }
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
    
    String html = FPSTR(PAGE_CONFIG);

    html.replace("{{BBL_IP}}", String(config.bbl_ip));
    html.replace("{{BBL_SERIAL}}", String(config.bbl_serial));
    html.replace("{{BBL_CODE}}", String(config.bbl_access_code));
    html.replace("{{NTP_SERVER}}", String(config.ntp_server));
    html.replace("{{TIMEZONE_DROPDOWN}}", getTimezoneDropdown(String(config.timezone)));
    html.replace("{{LIGHT_PIN}}", String(config.chamber_light_pin));
    html.replace("{{CHAMBER_BRIGHT}}", String(config.chamber_pwm_brightness));
    html.replace("{{INVERT_CHECKED}}", (config.invert_output ? "checked" : ""));
    html.replace("{{CHAMBER_TIMEOUT_CHECKED}}", (config.chamber_light_finish_timeout ? "checked" : ""));
    
    html.replace("{{MAX_LEDS}}", String(MAX_LEDS));
    html.replace("{{NUM_LEDS}}", String(config.num_leds));
    html.replace("{{LED_ORDER_DROPDOWN}}", getLedOrderDropdown(String(config.led_color_order)));
    html.replace("{{LED_DATA_PIN}}", String(LED_DATA_PIN));
    html.replace("{{LED_TIMEOUT_CHECKED}}", (config.led_finish_timeout ? "checked" : ""));

    String vled_html = "";
    for(int i=0; i < config.num_leds && i < MAX_LEDS; i++) {
      vled_html += "<div class='vled' style='flex-grow: 1; height: 100%;'></div>";
    }
    if (config.num_leds == 0) {
       vled_html += "<div style='flex-grow: 1; height: 100%; text-align: center; color: #888; padding-top: 5px; font-size: 0.9em;'>LEDs disabled (set count > 0)</div>";
    }
    html.replace("{{VIRTUAL_LEDS}}", vled_html);

    char hex[7];
    snprintf(hex, 7, "%06X", config.led_color_idle);
    html.replace("{{IDLE_COLOR}}", String(hex));
    html.replace("{{IDLE_BRIGHT}}", String(config.led_bright_idle));

    snprintf(hex, 7, "%06X", config.led_color_print);
    html.replace("{{PRINT_COLOR}}", String(hex));
    html.replace("{{PRINT_BRIGHT}}", String(config.led_bright_print));

    snprintf(hex, 7, "%06X", config.led_color_pause);
    html.replace("{{PAUSE_COLOR}}", String(hex));
    html.replace("{{PAUSE_BRIGHT}}", String(config.led_bright_pause));

    snprintf(hex, 7, "%06X", config.led_color_error);
    html.replace("{{ERROR_COLOR}}", String(hex));
    html.replace("{{ERROR_BRIGHT}}", String(config.led_bright_error));

    snprintf(hex, 7, "%06X", config.led_color_finish);
    html.replace("{{FINISH_COLOR}}", String(hex));
    html.replace("{{FINISH_BRIGHT}}", String(config.led_bright_finish));

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
  server.send(200, "text/html", FPSTR(PAGE_RESTORE));
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
