A Light controler for Bammu Labs printers, tested on a P1S
External light that mirrors the internal chamber light, but is also dimable using PWM - uses mosfet to control LED power.
Status bar LED lights using ws2812 leds.
All information is gathered from MQTT in read only mode (do not need to set printer to LAN only mode)
MQTT log file collected for debugging, timestamped (NTP to get time/date)
Sataus of printer and config ewb pages avalibe to view at any time.
Uses a Esp32 s3 n16r8 board.
