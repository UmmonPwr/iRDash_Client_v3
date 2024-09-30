# iRDash_Client v3
Displays live data of iRacing telemetry using a TFT display.

The purpose of this program is to display iRacing's live telemetry data on an Arduino TFT display.
As of now it can show:
- RPM
- Eight segment Shift Light Indicator
- /*Gear*/
- /*Fuel quantity*/
- /*Water temperature*/
- /*Speed*/
- /*Engine management lights*/

It supports multiple profiles to suit the different cars available in the sim. You can choose between them if you touch the middle 1/3 of the display.
Currently it has profile for:
- Skip Barber (Skippy)
- Cadillac CTS-V
- MX-5 (NC and ND)
- Formula Renault 2.0
- Dallara Formula 3
- Porsche 911 GT3 cup (992) // updated for 2024 season 4
- Toyota GR86
- Super Formula Lights
- BMW G82 M4

The program is developed on an ESP32-8048S043 display module
- Resolution: 800x480
- Capacitive touch sensing
- Display driver IC: Sitronics ST7262
- Touch controller IC: Goodix GT911

Arduino IDE board settings:
- Board: ESP32S3 Dev Module
- Flash mode: QIO 80Mhz
- Flash Size: 16 MB (128 Mb)
- PSRAM: OPI PSRAM

To display the live data of iRacing it needs the "iRDash Server" program running on Windows host and connected to the Arduino board via USB.
- https://github.com/UmmonPwr/iRDash-Server

To compile the program you need the below libraries:
- ESP_Panel_Library https://github.com/esp-arduino-libs/ESP32_Display_Panel
- ESP32 core library v3.x https://github.com/espressif/arduino-esp32
- LVGL v9.x https://github.com/lvgl/lvgl
- As a remark Arduino GFX library (https://github.com/moononournation/Arduino_GFX) is not compatible with "ESP32 core library v3.x" and above when the display uses RGB interface. Displays with SPI or I2C interface are still working with Arduino GFX library.